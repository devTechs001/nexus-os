/*
 * NEXUS OS - AI/ML Framework
 * ai_ml/inference/neural_engine.c
 *
 * Neural Network Inference Engine
 *
 * This module implements the core neural network inference engine for NEXUS OS.
 * It provides optimized execution of neural network models with support for
 * multiple accelerators (CPU, GPU, NPU).
 */

#include "../ai_ml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================*/
/*                         INFERENCE ENGINE STATE                            */
/*===========================================================================*/

static struct {
    bool initialized;
    inference_config_t config;
    inference_stats_t stats;
    
    /* Thread pool */
    void *thread_pool;
    u32 num_threads;
    
    /* Memory pools */
    void *workspace_pool;
    size_t workspace_size;
    
    /* Model cache */
    nn_model_t *cached_models;
    u32 num_cached;
    
    /* Synchronization */
    spinlock_t lock;
} g_inference_engine = {
    .initialized = false,
    .lock = { .lock = 0 }
};

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static int inference_execute_layer(nn_layer_t *layer);
static int inference_execute_dense(nn_layer_t *layer);
static int inference_execute_conv2d(nn_layer_t *layer);
static int inference_execute_pooling(nn_layer_t *layer);
static int inference_execute_activation(nn_layer_t *layer);
static int inference_execute_bn(nn_layer_t *layer);
static int inference_execute_softmax(nn_layer_t *layer);

static void update_stats(u64 time_us, bool success);
static u64 get_time_us(void);

/*===========================================================================*/
/*                         INFERENCE ENGINE INITIALIZATION                   */
/*===========================================================================*/

/**
 * inference_init - Initialize the inference engine
 * @config: Configuration parameters
 *
 * Initializes the inference engine with the specified configuration.
 * Sets up thread pools, memory pools, and accelerator backends.
 *
 * Returns: INFERENCE_SUCCESS on success, error code on failure
 */
int inference_init(inference_config_t *config)
{
    if (g_inference_engine.initialized) {
        pr_debug("Inference engine already initialized\n");
        return INFERENCE_SUCCESS;
    }

    spin_lock(&g_inference_engine.lock);

    /* Copy configuration */
    if (config) {
        memcpy(&g_inference_engine.config, config, sizeof(inference_config_t));
    } else {
        /* Default configuration */
        memset(&g_inference_engine.config, 0, sizeof(inference_config_t));
        g_inference_engine.config.batch_size = 1;
        g_inference_engine.config.num_threads = INFERENCE_THREAD_POOL_SIZE;
        g_inference_engine.config.accelerator = ACCEL_CPU;
        g_inference_engine.config.precision = PRECISION_FP32;
        g_inference_engine.config.timeout_ms = 5000;
    }

    /* Initialize statistics */
    memset(&g_inference_engine.stats, 0, sizeof(inference_stats_t));
    g_inference_engine.stats.min_time_us = U64_MAX;

    /* Initialize thread pool */
    g_inference_engine.num_threads = g_inference_engine.config.num_threads;
    pr_info("Inference engine: %u worker threads\n", g_inference_engine.num_threads);

    /* Allocate workspace pool */
    g_inference_engine.workspace_size = MB(64);
    g_inference_engine.workspace_pool = kmalloc(g_inference_engine.workspace_size);
    if (!g_inference_engine.workspace_pool) {
        pr_err("Failed to allocate workspace pool\n");
        spin_unlock(&g_inference_engine.lock);
        return INFERENCE_ERR_MEMORY;
    }
    memset(g_inference_engine.workspace_pool, 0, g_inference_engine.workspace_size);

    /* Initialize NPU if enabled */
    if (g_inference_engine.config.use_npu) {
        pr_info("Initializing NPU backend...\n");
        if (npu_init() != 0) {
            pr_warn("NPU initialization failed, falling back to CPU\n");
            g_inference_engine.config.use_npu = false;
        }
    }

    g_inference_engine.initialized = true;

    pr_info("Inference engine initialized\n");
    pr_info("  Batch size: %u\n", g_inference_engine.config.batch_size);
    pr_info("  Precision: %s\n", 
            g_inference_engine.config.precision == PRECISION_FP16 ? "FP16" :
            g_inference_engine.config.precision == PRECISION_INT8 ? "INT8" : "FP32");
    pr_info("  Accelerator: %s\n",
            g_inference_engine.config.accelerator == ACCEL_NPU ? "NPU" :
            g_inference_engine.config.accelerator == ACCEL_GPU ? "GPU" : "CPU");

    spin_unlock(&g_inference_engine.lock);

    return INFERENCE_SUCCESS;
}

/**
 * inference_shutdown - Shutdown the inference engine
 *
 * Releases all resources allocated by the inference engine.
 */
int inference_shutdown(void)
{
    if (!g_inference_engine.initialized) {
        return INFERENCE_SUCCESS;
    }

    spin_lock(&g_inference_engine.lock);

    /* Free workspace pool */
    if (g_inference_engine.workspace_pool) {
        kfree(g_inference_engine.workspace_pool);
        g_inference_engine.workspace_pool = NULL;
    }

    /* Shutdown NPU */
    if (g_inference_engine.config.use_npu) {
        npu_shutdown();
    }

    /* Reset state */
    memset(&g_inference_engine.config, 0, sizeof(inference_config_t));
    memset(&g_inference_engine.stats, 0, sizeof(inference_stats_t));
    g_inference_engine.initialized = false;

    pr_info("Inference engine shutdown complete\n");

    spin_unlock(&g_inference_engine.lock);

    return INFERENCE_SUCCESS;
}

/*===========================================================================*/
/*                         INFERENCE EXECUTION                               */
/*===========================================================================*/

/**
 * inference_run - Run inference on a model
 * @model: Neural network model
 * @inputs: Input tensors
 * @num_inputs: Number of input tensors
 * @outputs: Output tensors
 * @num_outputs: Number of output tensors
 *
 * Executes the neural network model with the given inputs and produces outputs.
 * This is a synchronous call that blocks until inference is complete.
 *
 * Returns: INFERENCE_SUCCESS on success, error code on failure
 */
int inference_run(nn_model_t *model, tensor_t **inputs, u32 num_inputs,
                  tensor_t **outputs, u32 num_outputs)
{
    u64 start_time;
    u64 elapsed_time;
    nn_layer_t *layer;
    int ret;

    if (!g_inference_engine.initialized) {
        pr_err("Inference engine not initialized\n");
        return INFERENCE_ERR_INIT;
    }

    if (!model || !model->loaded) {
        pr_err("Invalid or unloaded model\n");
        return INFERENCE_ERR_MODEL;
    }

    if (!inputs || num_inputs == 0) {
        pr_err("No input tensors provided\n");
        return INFERENCE_ERR_INPUT;
    }

    spin_lock(&g_inference_engine.lock);

    /* Set model inputs */
    for (u32 i = 0; i < num_inputs && i < model->num_inputs; i++) {
        model->inputs[i] = inputs[i];
    }

    /* Record start time */
    start_time = get_time_us();

    /* Execute layers sequentially */
    layer = model->layers;
    while (layer) {
        ret = inference_execute_layer(layer);
        if (ret != 0) {
            pr_err("Layer execution failed: %s (type: %u)\n", layer->name, layer->type);
            update_stats(0, false);
            spin_unlock(&g_inference_engine.lock);
            return ret;
        }
        layer = layer->next;
    }

    /* Get elapsed time */
    elapsed_time = get_time_us() - start_time;

    /* Copy outputs */
    for (u32 i = 0; i < num_outputs && i < model->num_outputs; i++) {
        if (outputs[i] && model->outputs[i]) {
            tensor_copy(outputs[i], model->outputs[i]);
        }
    }

    /* Update statistics */
    update_stats(elapsed_time, true);

    pr_debug("Inference completed in %llu us\n", (unsigned long long)elapsed_time);

    spin_unlock(&g_inference_engine.lock);

    return INFERENCE_SUCCESS;
}

/**
 * inference_run_async - Run inference asynchronously
 * @model: Neural network model
 * @inputs: Input tensors
 * @num_inputs: Number of input tensors
 * @outputs: Output tensors
 * @num_outputs: Number of output tensors
 * @callback: Completion callback
 *
 * Executes inference asynchronously and calls the callback when complete.
 *
 * Returns: INFERENCE_SUCCESS on success, error code on failure
 */
int inference_run_async(nn_model_t *model, tensor_t **inputs, u32 num_inputs,
                        tensor_t **outputs, u32 num_outputs, void *callback)
{
    /* For now, just call synchronous version */
    /* TODO: Implement proper async execution with thread pool */
    return inference_run(model, inputs, num_inputs, outputs, num_outputs);
}

/*===========================================================================*/
/*                         LAYER EXECUTION                                   */
/*===========================================================================*/

/**
 * inference_execute_layer - Execute a single neural network layer
 * @layer: Layer to execute
 *
 * Dispatches to the appropriate layer-specific execution function.
 *
 * Returns: 0 on success, error code on failure
 */
static int inference_execute_layer(nn_layer_t *layer)
{
    if (!layer) {
        return INFERENCE_ERR_INPUT;
    }

    switch (layer->type) {
        case LAYER_DENSE:
            return inference_execute_dense(layer);
        
        case LAYER_CONV2D:
            return inference_execute_conv2d(layer);
        
        case LAYER_POOLING:
            return inference_execute_pooling(layer);
        
        case LAYER_ACTIVATION:
        case LAYER_RELU:
        case LAYER_SIGMOID:
        case LAYER_TANH:
            return inference_execute_activation(layer);
        
        case LAYER_BN:
            return inference_execute_bn(layer);
        
        case LAYER_SOFTMAX:
            return inference_execute_softmax(layer);
        
        default:
            pr_warn("Unsupported layer type: %u\n", layer->type);
            return INFERENCE_ERR_MODEL;
    }
}

/**
 * inference_execute_dense - Execute a dense (fully connected) layer
 * @layer: Dense layer
 *
 * Computes: output = activation(input * weights + biases)
 *
 * Returns: 0 on success, error code on failure
 */
static int inference_execute_dense(nn_layer_t *layer)
{
    tensor_t *input = layer->input;
    tensor_t *output = layer->output;
    tensor_t *weights = layer->weights;
    tensor_t *biases = layer->biases;
    float *in_data, *out_data, *w_data, *b_data;
    u32 batch_size, in_features, out_features;
    
    if (!input || !output || !weights) {
        return INFERENCE_ERR_INPUT;
    }

    in_data = (float *)input->data;
    out_data = (float *)output->data;
    w_data = (float *)weights->data;
    b_data = biases ? (float *)biases->data : NULL;

    batch_size = input->shape[0];
    in_features = layer->input_channels;
    out_features = layer->output_channels;

    /* Matrix multiplication: output = input * weights */
    for (u32 b = 0; b < batch_size; b++) {
        for (u32 out = 0; out < out_features; out++) {
            float sum = 0.0f;
            
            for (u32 in = 0; in < in_features; in++) {
                sum += in_data[b * in_features + in] * w_data[in * out_features + out];
            }
            
            /* Add bias */
            if (b_data) {
                sum += b_data[out];
            }
            
            /* Apply activation */
            switch (layer->activation) {
                case ACTIVATION_RELU:
                    sum = fmaxf(0.0f, sum);
                    break;
                case ACTIVATION_SIGMOID:
                    sum = 1.0f / (1.0f + expf(-sum));
                    break;
                case ACTIVATION_TANH:
                    sum = tanhf(sum);
                    break;
                case ACTIVATION_LEAKY_RELU:
                    sum = (sum > 0) ? sum : 0.01f * sum;
                    break;
                case ACTIVATION_GELU:
                    sum = sum * 0.5f * (1.0f + tanhf(sqrtf(2.0f / M_PI) * (sum + 0.044715f * sum * sum * sum)));
                    break;
                default:
                    break;
            }
            
            out_data[b * out_features + out] = sum;
        }
    }

    return 0;
}

/**
 * inference_execute_conv2d - Execute a 2D convolution layer
 * @layer: Conv2D layer
 *
 * Performs 2D convolution with optional activation.
 *
 * Returns: 0 on success, error code on failure
 */
static int inference_execute_conv2d(nn_layer_t *layer)
{
    tensor_t *input = layer->input;
    tensor_t *output = layer->output;
    tensor_t *weights = layer->weights;
    float *in_data, *out_data, *w_data;
    u32 batch, in_h, in_w, in_c;
    u32 out_h, out_w, out_c;
    u32 kernel_size, stride, padding;
    
    if (!input || !output || !weights) {
        return INFERENCE_ERR_INPUT;
    }

    in_data = (float *)input->data;
    out_data = (float *)output->data;
    w_data = (float *)weights->data;

    /* Get dimensions */
    batch = input->shape[0];
    in_h = input->shape[1];
    in_w = input->shape[2];
    in_c = input->shape[3];
    
    out_h = output->shape[1];
    out_w = output->shape[2];
    out_c = output->shape[3];

    kernel_size = layer->kernel_size;
    stride = layer->stride;
    padding = layer->padding;

    /* Perform convolution */
    for (u32 n = 0; n < batch; n++) {
        for (u32 oc = 0; oc < out_c; oc++) {
            for (u32 oh = 0; oh < out_h; oh++) {
                for (u32 ow = 0; ow < out_w; ow++) {
                    float sum = 0.0f;
                    
                    /* Convolution window */
                    for (u32 ic = 0; ic < in_c; ic++) {
                        for (u32 kh = 0; kh < kernel_size; kh++) {
                            for (u32 kw = 0; kw < kernel_size; kw++) {
                                s32 ih = oh * stride + kh - padding;
                                s32 iw = ow * stride + kw - padding;
                                
                                /* Handle padding */
                                if (ih >= 0 && ih < in_h && iw >= 0 && iw < in_w) {
                                    u32 in_idx = ((n * in_h + ih) * in_w + iw) * in_c + ic;
                                    u32 w_idx = ((oc * in_c + ic) * kernel_size + kh) * kernel_size + kw;
                                    sum += in_data[in_idx] * w_data[w_idx];
                                }
                            }
                        }
                    }
                    
                    /* Apply activation */
                    if (layer->activation == ACTIVATION_RELU) {
                        sum = fmaxf(0.0f, sum);
                    }
                    
                    u32 out_idx = ((n * out_h + oh) * out_w + ow) * out_c + oc;
                    out_data[out_idx] = sum;
                }
            }
        }
    }

    return 0;
}

/**
 * inference_execute_pooling - Execute a pooling layer
 * @layer: Pooling layer
 *
 * Performs max or average pooling.
 *
 * Returns: 0 on success, error code on failure
 */
static int inference_execute_pooling(nn_layer_t *layer)
{
    tensor_t *input = layer->input;
    tensor_t *output = layer->output;
    float *in_data, *out_data;
    u32 batch, in_h, in_w, channels;
    u32 out_h, out_w;
    u32 kernel_size, stride;
    
    if (!input || !output) {
        return INFERENCE_ERR_INPUT;
    }

    in_data = (float *)input->data;
    out_data = (float *)output->data;

    batch = input->shape[0];
    in_h = input->shape[1];
    in_w = input->shape[2];
    channels = input->shape[3];
    
    out_h = output->shape[1];
    out_w = output->shape[2];

    kernel_size = layer->kernel_size;
    stride = layer->stride;

    /* Perform pooling */
    for (u32 n = 0; n < batch; n++) {
        for (u32 c = 0; c < channels; c++) {
            for (u32 oh = 0; oh < out_h; oh++) {
                for (u32 ow = 0; ow < out_w; ow++) {
                    float value;
                    
                    if (layer->type == LAYER_POOLING) {
                        /* Max pooling */
                        value = -INFINITY;
                        
                        for (u32 kh = 0; kh < kernel_size; kh++) {
                            for (u32 kw = 0; kw < kernel_size; kw++) {
                                u32 ih = oh * stride + kh;
                                u32 iw = ow * stride + kw;
                                
                                if (ih < in_h && iw < in_w) {
                                    u32 in_idx = ((n * in_h + ih) * in_w + iw) * channels + c;
                                    value = fmaxf(value, in_data[in_idx]);
                                }
                            }
                        }
                    } else {
                        /* Average pooling */
                        value = 0.0f;
                        u32 count = 0;
                        
                        for (u32 kh = 0; kh < kernel_size; kh++) {
                            for (u32 kw = 0; kw < kernel_size; kw++) {
                                u32 ih = oh * stride + kh;
                                u32 iw = ow * stride + kw;
                                
                                if (ih < in_h && iw < in_w) {
                                    u32 in_idx = ((n * in_h + ih) * in_w + iw) * channels + c;
                                    value += in_data[in_idx];
                                    count++;
                                }
                            }
                        }
                        
                        if (count > 0) {
                            value /= count;
                        }
                    }
                    
                    u32 out_idx = ((n * out_h + oh) * out_w + ow) * channels + c;
                    out_data[out_idx] = value;
                }
            }
        }
    }

    return 0;
}

/**
 * inference_execute_activation - Execute an activation layer
 * @layer: Activation layer
 *
 * Applies element-wise activation function.
 *
 * Returns: 0 on success, error code on failure
 */
static int inference_execute_activation(nn_layer_t *layer)
{
    tensor_t *input = layer->input;
    tensor_t *output = layer->output;
    float *in_data, *out_data;
    size_t size;
    
    if (!input || !output) {
        return INFERENCE_ERR_INPUT;
    }

    in_data = (float *)input->data;
    out_data = (float *)output->data;
    size = input->size / sizeof(float);

    for (size_t i = 0; i < size; i++) {
        float x = in_data[i];
        
        switch (layer->activation) {
            case ACTIVATION_RELU:
                out_data[i] = fmaxf(0.0f, x);
                break;
            
            case ACTIVATION_SIGMOID:
                out_data[i] = 1.0f / (1.0f + expf(-x));
                break;
            
            case ACTIVATION_TANH:
                out_data[i] = tanhf(x);
                break;
            
            case ACTIVATION_LEAKY_RELU:
                out_data[i] = (x > 0) ? x : 0.01f * x;
                break;
            
            case ACTIVATION_GELU:
                out_data[i] = x * 0.5f * (1.0f + tanhf(sqrtf(2.0f / M_PI) * (x + 0.044715f * x * x * x)));
                break;
            
            case ACTIVATION_SWISH:
                out_data[i] = x / (1.0f + expf(-x));
                break;
            
            default:
                out_data[i] = x;
                break;
        }
    }

    return 0;
}

/**
 * inference_execute_bn - Execute batch normalization layer
 * @layer: Batch normalization layer
 *
 * Applies batch normalization: output = gamma * (input - mean) / sqrt(var + eps) + beta
 *
 * Returns: 0 on success, error code on failure
 */
static int inference_execute_bn(nn_layer_t *layer)
{
    tensor_t *input = layer->input;
    tensor_t *output = layer->output;
    tensor_t *weights = layer->weights;  /* gamma */
    tensor_t *biases = layer->biases;    /* beta */
    float *in_data, *out_data, *gamma, *beta;
    u32 channels;
    size_t size;
    
    if (!input || !output || !weights) {
        return INFERENCE_ERR_INPUT;
    }

    in_data = (float *)input->data;
    out_data = (float *)output->data;
    gamma = (float *)weights->data;
    beta = biases ? (float *)biases->data : NULL;

    channels = layer->input_channels;
    size = input->size / sizeof(float) / channels;

    /* Apply batch normalization per channel */
    for (u32 c = 0; c < channels; c++) {
        float g = gamma[c];
        float b = beta ? beta[c] : 0.0f;
        
        for (size_t i = 0; i < size; i++) {
            u32 idx = i * channels + c;
            out_data[idx] = g * in_data[idx] + b;
        }
    }

    return 0;
}

/**
 * inference_execute_softmax - Execute softmax layer
 * @layer: Softmax layer
 *
 * Applies softmax: output[i] = exp(input[i]) / sum(exp(input))
 *
 * Returns: 0 on success, error code on failure
 */
static int inference_execute_softmax(nn_layer_t *layer)
{
    tensor_t *input = layer->input;
    tensor_t *output = layer->output;
    float *in_data, *out_data;
    u32 batch, classes;
    
    if (!input || !output) {
        return INFERENCE_ERR_INPUT;
    }

    in_data = (float *)input->data;
    out_data = (float *)output->data;

    batch = input->shape[0];
    classes = input->shape[1];

    for (u32 b = 0; b < batch; b++) {
        /* Find max for numerical stability */
        float max_val = -INFINITY;
        for (u32 c = 0; c < classes; c++) {
            max_val = fmaxf(max_val, in_data[b * classes + c]);
        }
        
        /* Compute exp and sum */
        float sum = 0.0f;
        for (u32 c = 0; c < classes; c++) {
            float exp_val = expf(in_data[b * classes + c] - max_val);
            out_data[b * classes + c] = exp_val;
            sum += exp_val;
        }
        
        /* Normalize */
        for (u32 c = 0; c < classes; c++) {
            out_data[b * classes + c] /= sum;
        }
    }

    return 0;
}

/*===========================================================================*/
/*                         STATISTICS MANAGEMENT                             */
/*===========================================================================*/

/**
 * update_stats - Update inference statistics
 * @time_us: Inference time in microseconds
 * @success: Whether inference was successful
 */
static void update_stats(u64 time_us, bool success)
{
    g_inference_engine.stats.total_inferences++;
    
    if (success) {
        g_inference_engine.stats.successful_inferences++;
        
        if (time_us > 0) {
            g_inference_engine.stats.total_time_us += time_us;
            
            if (time_us < g_inference_engine.stats.min_time_us) {
                g_inference_engine.stats.min_time_us = time_us;
            }
            if (time_us > g_inference_engine.stats.max_time_us) {
                g_inference_engine.stats.max_time_us = time_us;
            }
            
            g_inference_engine.stats.avg_time_us = 
                g_inference_engine.stats.total_time_us / 
                g_inference_engine.stats.successful_inferences;
        }
    } else {
        g_inference_engine.stats.failed_inferences++;
    }
}

/**
 * inference_get_stats - Get inference statistics
 * @stats: Output statistics structure
 *
 * Returns: INFERENCE_SUCCESS on success, error code on failure
 */
int inference_get_stats(inference_stats_t *stats)
{
    if (!stats) {
        return INFERENCE_ERR_INPUT;
    }

    if (!g_inference_engine.initialized) {
        return INFERENCE_ERR_INIT;
    }

    spin_lock(&g_inference_engine.lock);
    memcpy(stats, &g_inference_engine.stats, sizeof(inference_stats_t));
    spin_unlock(&g_inference_engine.lock);

    return INFERENCE_SUCCESS;
}

/**
 * inference_reset_stats - Reset inference statistics
 *
 * Returns: INFERENCE_SUCCESS on success
 */
int inference_reset_stats(void)
{
    if (!g_inference_engine.initialized) {
        return INFERENCE_ERR_INIT;
    }

    spin_lock(&g_inference_engine.lock);
    memset(&g_inference_engine.stats, 0, sizeof(inference_stats_t));
    g_inference_engine.stats.min_time_us = U64_MAX;
    spin_unlock(&g_inference_engine.lock);

    return INFERENCE_SUCCESS;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * get_time_us - Get current time in microseconds
 */
static u64 get_time_us(void)
{
    return get_time_ms() * 1000;
}
