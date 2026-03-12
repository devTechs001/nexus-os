/*
 * NEXUS OS - AI/ML Framework
 * ai_ml/models/cnn.c
 *
 * Convolutional Neural Network Model
 *
 * This module implements Convolutional Neural Network (CNN) architectures
 * for image classification, object detection, and feature extraction.
 */

#include "../ai_ml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================*/
/*                         CNN MODEL CONFIGURATION                           */
/*===========================================================================*/

/* Common CNN architectures */
#define CNN_ARCH_SIMPLE         0
#define CNN_ARCH_VGG11          1
#define CNN_ARCH_VGG16          2
#define CNN_ARCH_RESNET18       3
#define CNN_ARCH_RESNET50       4
#define CNN_ARCH_MOBILE_NET     5

/* Default CNN parameters */
#define CNN_DEFAULT_INPUT_SIZE  224
#define CNN_DEFAULT_NUM_CLASSES 1000

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static nn_layer_t *create_conv_block(u32 in_ch, u32 out_ch, u32 kernel, 
                                      u32 stride, bool use_bn, u32 activation);
static nn_layer_t *create_residual_block(u32 channels, u32 stride);
static int cnn_forward(nn_model_t *model, tensor_t *input);

/*===========================================================================*/
/*                         CNN MODEL CREATION                                */
/*===========================================================================*/

/**
 * model_create_cnn - Create a CNN model
 * @name: Model name
 * @num_classes: Number of output classes
 *
 * Creates a CNN model with a default architecture suitable for
 * image classification tasks.
 *
 * Returns: Pointer to created model, or NULL on failure
 */
nn_model_t *model_create_cnn(const char *name, u32 num_classes)
{
    nn_model_t *model;
    nn_layer_t *layer;
    
    if (!name) {
        pr_err("CNN model name required\n");
        return NULL;
    }

    /* Create model structure */
    model = model_create(name, MODEL_CNN);
    if (!model) {
        return NULL;
    }

    /* Default to 224x224 input with 3 channels */
    u32 input_shape[] = {1, 224, 224, 3};
    model->inputs[0] = tensor_create("input", 4, input_shape, PRECISION_FP32);
    model->num_inputs = 1;

    /* Build CNN architecture: Simple 8-layer CNN */
    
    /* Layer 1: Conv2D 3->32, kernel=3, stride=1, padding=1 */
    layer = layer_create_conv2d(3, 32, 3, 1, 1);
    layer->activation = ACTIVATION_RELU;
    snprintf(layer->name, sizeof(layer->name), "conv1");
    model_add_layer(model, layer);

    /* Layer 2: MaxPool 2x2 */
    layer = layer_create_pooling(2, 2, 0);
    snprintf(layer->name, sizeof(layer->name), "pool1");
    model_add_layer(model, layer);

    /* Layer 3: Conv2D 32->64, kernel=3, stride=1, padding=1 */
    layer = layer_create_conv2d(32, 64, 3, 1, 1);
    layer->activation = ACTIVATION_RELU;
    snprintf(layer->name, sizeof(layer->name), "conv2");
    model_add_layer(model, layer);

    /* Layer 4: MaxPool 2x2 */
    layer = layer_create_pooling(2, 2, 0);
    snprintf(layer->name, sizeof(layer->name), "pool2");
    model_add_layer(model, layer);

    /* Layer 5: Conv2D 64->128, kernel=3, stride=1, padding=1 */
    layer = layer_create_conv2d(64, 128, 3, 1, 1);
    layer->activation = ACTIVATION_RELU;
    snprintf(layer->name, sizeof(layer->name), "conv3");
    model_add_layer(model, layer);

    /* Layer 6: MaxPool 2x2 */
    layer = layer_create_pooling(2, 2, 0);
    snprintf(layer->name, sizeof(layer->name), "pool3");
    model_add_layer(model, layer);

    /* Layer 7: Conv2D 128->256, kernel=3, stride=1, padding=1 */
    layer = layer_create_conv2d(128, 256, 3, 1, 1);
    layer->activation = ACTIVATION_RELU;
    snprintf(layer->name, sizeof(layer->name), "conv4");
    model_add_layer(model, layer);

    /* Layer 8: Global Average Pool */
    layer = layer_create_pooling(7, 7, 0);  /* For 224x224 input */
    snprintf(layer->name, sizeof(layer->name), "gap");
    model_add_layer(model, layer);

    /* Layer 9: Flatten (implicit) */
    
    /* Layer 10: Dense 256->num_classes */
    layer = layer_create_dense(256, num_classes, ACTIVATION_NONE);
    snprintf(layer->name, sizeof(layer->name), "fc");
    model_add_layer(model, layer);

    /* Layer 11: Softmax */
    layer = layer_create_activation(ACTIVATION_SOFTMAX);
    snprintf(layer->name, sizeof(layer->name), "softmax");
    model_add_layer(model, layer);

    /* Set output tensor */
    u32 output_shape[] = {1, num_classes};
    model->outputs[0] = tensor_create("output", 2, output_shape, PRECISION_FP32);
    model->num_outputs = 1;

    /* Calculate model statistics */
    model->params_count = 3*32*3*3 + 32 +           /* Conv1 */
                          32*64*3*3 + 64 +          /* Conv2 */
                          64*128*3*3 + 128 +        /* Conv3 */
                          128*256*3*3 + 256 +       /* Conv4 */
                          256*num_classes + num_classes;  /* FC */
    
    model->flops_count = model->params_count * 2;  /* Approximate */
    model->memory_usage = model->params_count * sizeof(float);

    pr_info("CNN model '%s' created: %u layers, %llu params\n",
            name, model->num_layers, (unsigned long long)model->params_count);

    return model;
}

/**
 * model_create_resnet - Create a ResNet model
 * @name: Model name
 * @depth: ResNet depth (18, 34, 50, 101, 152)
 *
 * Creates a Residual Network model for image classification.
 *
 * Returns: Pointer to created model, or NULL on failure
 */
nn_model_t *model_create_resnet(const char *name, u32 depth)
{
    nn_model_t *model;
    nn_layer_t *layer;
    u32 num_blocks;
    
    if (!name) {
        pr_err("ResNet model name required\n");
        return NULL;
    }

    /* Create model structure */
    model = model_create(name, MODEL_RESNET);
    if (!model) {
        return NULL;
    }

    /* Default input */
    u32 input_shape[] = {1, 224, 224, 3};
    model->inputs[0] = tensor_create("input", 4, input_shape, PRECISION_FP32);
    model->num_inputs = 1;

    /* Initial conv layer */
    layer = layer_create_conv2d(3, 64, 7, 2, 3);
    layer->activation = ACTIVATION_RELU;
    snprintf(layer->name, sizeof(layer->name), "conv1_init");
    model_add_layer(model, layer);

    /* Max pool */
    layer = layer_create_pooling(3, 2, 0);
    snprintf(layer->name, sizeof(layer->name), "pool1_init");
    model_add_layer(model, layer);

    /* Determine number of blocks based on depth */
    switch (depth) {
        case 18:
            num_blocks = 2;
            break;
        case 34:
            num_blocks = 3;
            break;
        case 50:
            num_blocks = 3;
            break;
        default:
            num_blocks = 2;
            break;
    }

    /* Residual blocks - simplified version */
    for (u32 i = 0; i < num_blocks; i++) {
        char block_name[32];
        
        /* Block 1: 64 channels */
        snprintf(block_name, sizeof(block_name), "res2a_%u", i);
        layer = create_residual_block(64, (i == 0) ? 1 : 2);
        snprintf(layer->name, sizeof(layer->name), "%s", block_name);
        model_add_layer(model, layer);
    }

    /* Global average pooling */
    layer = layer_create_pooling(7, 1, 0);
    snprintf(layer->name, sizeof(layer->name), "gap");
    model_add_layer(model, layer);

    /* Fully connected */
    layer = layer_create_dense(512, 1000, ACTIVATION_NONE);
    snprintf(layer->name, sizeof(layer->name), "fc");
    model_add_layer(model, layer);

    /* Softmax */
    layer = layer_create_activation(ACTIVATION_SOFTMAX);
    snprintf(layer->name, sizeof(layer->name), "softmax");
    model_add_layer(model, layer);

    /* Set output */
    u32 output_shape[] = {1, 1000};
    model->outputs[0] = tensor_create("output", 2, output_shape, PRECISION_FP32);
    model->num_outputs = 1;

    /* Calculate parameters (approximate for ResNet-18) */
    model->params_count = 11689512;  /* ResNet-18 */
    model->flops_count = model->params_count * 2;
    model->memory_usage = model->params_count * sizeof(float);

    pr_info("ResNet-%u model '%s' created: %u layers, %llu params\n",
            depth, name, model->num_layers, (unsigned long long)model->params_count);

    return model;
}

/**
 * model_create_yolo - Create a YOLO object detection model
 * @name: Model name
 * @num_classes: Number of object classes
 *
 * Creates a YOLO-style object detection model.
 *
 * Returns: Pointer to created model, or NULL on failure
 */
nn_model_t *model_create_yolo(const char *name, u32 num_classes)
{
    nn_model_t *model;
    nn_layer_t *layer;
    
    if (!name) {
        pr_err("YOLO model name required\n");
        return NULL;
    }

    /* Create model structure */
    model = model_create(name, MODEL_YOLO);
    if (!model) {
        return NULL;
    }

    /* YOLO input: 416x416x3 */
    u32 input_shape[] = {1, 416, 416, 3};
    model->inputs[0] = tensor_create("input", 4, input_shape, PRECISION_FP32);
    model->num_inputs = 1;

    /* Darknet-style backbone - simplified */
    u32 filters[] = {32, 64, 128, 256, 512, 1024};
    
    for (int i = 0; i < 6; i++) {
        char layer_name[32];
        u32 in_ch = (i == 0) ? 3 : filters[i-1];
        u32 out_ch = filters[i];
        
        /* Conv block */
        snprintf(layer_name, sizeof(layer_name), "conv_%d", i);
        layer = layer_create_conv2d(in_ch, out_ch, 3, 1, 1);
        layer->activation = ACTIVATION_LEAKY_RELU;
        snprintf(layer->name, sizeof(layer->name), "%s", layer_name);
        model_add_layer(model, layer);

        /* Max pool (except last) */
        if (i < 5) {
            snprintf(layer_name, sizeof(layer_name), "pool_%d", i);
            layer = layer_create_pooling(2, 2, 0);
            snprintf(layer->name, sizeof(layer->name), "%s", layer_name);
            model_add_layer(model, layer);
        }
    }

    /* Detection head */
    /* Output: (num_classes + 5) * num_anchors per grid cell */
    u32 num_anchors = 3;
    u32 output_channels = (num_classes + 5) * num_anchors;
    
    layer = layer_create_conv2d(1024, output_channels, 1, 1, 0);
    snprintf(layer->name, sizeof(layer->name), "detection_head");
    model_add_layer(model, layer);

    /* Set output: [batch, grid_h, grid_w, output_channels] */
    u32 grid_size = 13;  /* For 416x416 input */
    u32 output_shape[] = {1, grid_size, grid_size, output_channels};
    model->outputs[0] = tensor_create("output", 4, output_shape, PRECISION_FP32);
    model->num_outputs = 1;

    /* Calculate parameters */
    model->params_count = 57000000;  /* Approximate for YOLOv3-tiny */
    model->flops_count = model->params_count * 2;
    model->memory_usage = model->params_count * sizeof(float);

    pr_info("YOLO model '%s' created: %u layers, %u classes\n",
            name, model->num_layers, num_classes);

    return model;
}

/*===========================================================================*/
/*                         HELPER FUNCTIONS                                  */
/*===========================================================================*/

/**
 * create_conv_block - Create a convolution block
 * @in_ch: Input channels
 * @out_ch: Output channels
 * @kernel: Kernel size
 * @stride: Stride
 * @use_bn: Use batch normalization
 * @activation: Activation function
 *
 * Returns: Created layer
 */
static nn_layer_t *create_conv_block(u32 in_ch, u32 out_ch, u32 kernel,
                                      u32 stride, bool use_bn, u32 activation)
{
    nn_layer_t *layer;
    
    layer = layer_create_conv2d(in_ch, out_ch, kernel, stride, kernel / 2);
    layer->activation = activation;
    
    return layer;
}

/**
 * create_residual_block - Create a residual block
 * @channels: Number of channels
 * @stride: Stride for downsampling
 *
 * Returns: Created layer (simplified - real implementation would need multiple layers)
 */
static nn_layer_t *create_residual_block(u32 channels, u32 stride)
{
    nn_layer_t *layer;
    
    /* Simplified: just return a conv layer */
    /* Real ResNet would have 2-3 conv layers with skip connection */
    layer = layer_create_conv2d(channels, channels, 3, stride, 1);
    layer->activation = ACTIVATION_RELU;
    
    return layer;
}

/**
 * cnn_forward - Forward pass through CNN
 * @model: CNN model
 * @input: Input tensor
 *
 * Returns: 0 on success, error code on failure
 */
static int cnn_forward(nn_model_t *model, tensor_t *input)
{
    if (!model || !input) {
        return INFERENCE_ERR_INPUT;
    }

    /* Set input tensor */
    model->inputs[0] = input;

    /* Run inference */
    return inference_run(model, &input, 1, model->outputs, model->num_outputs);
}

/*===========================================================================*/
/*                         CNN UTILITIES                                     */
/*===========================================================================*/

/**
 * cnn_preprocess_image - Preprocess image for CNN input
 * @image: Input image data
 * @width: Image width
 * @height: Image height
 * @channels: Number of channels
 * @output: Output tensor
 * @normalize: Whether to normalize pixel values
 *
 * Resizes and normalizes image data for CNN input.
 *
 * Returns: 0 on success, error code on failure
 */
int cnn_preprocess_image(const void *image, u32 width, u32 height, u32 channels,
                         tensor_t *output, bool normalize)
{
    float *out_data;
    const u8 *in_data;
    u32 target_size = 224;
    float scale;
    
    if (!image || !output) {
        return INFERENCE_ERR_INPUT;
    }

    out_data = (float *)output->data;
    in_data = (const u8 *)image;

    /* Simple resize (nearest neighbor) */
    scale = (float)width / target_size;
    
    for (u32 y = 0; y < target_size; y++) {
        for (u32 x = 0; x < target_size; x++) {
            u32 src_x = (u32)(x * scale);
            u32 src_y = (u32)(y * scale);
            
            if (src_x >= width) src_x = width - 1;
            if (src_y >= height) src_y = height - 1;
            
            for (u32 c = 0; c < channels && c < 3; c++) {
                u32 src_idx = (src_y * width + src_x) * channels + c;
                u32 dst_idx = (y * target_size + x) * 3 + c;
                
                float value = in_data[src_idx];
                
                if (normalize) {
                    /* Normalize to [-1, 1] */
                    value = (value / 255.0f) * 2.0f - 1.0f;
                } else {
                    /* Normalize to [0, 1] */
                    value = value / 255.0f;
                }
                
                out_data[dst_idx] = value;
            }
        }
    }

    return 0;
}

/**
 * cnn_get_prediction - Get top prediction from CNN output
 * @output: Output tensor from CNN
 * @top_k: Number of top predictions to return
 * @indices: Output array for class indices
 * @probabilities: Output array for probabilities
 *
 * Returns: 0 on success, error code on failure
 */
int cnn_get_prediction(tensor_t *output, u32 top_k, u32 *indices, float *probabilities)
{
    float *data;
    u32 num_classes;
    
    if (!output || !indices || !probabilities) {
        return INFERENCE_ERR_INPUT;
    }

    data = (float *)output->data;
    num_classes = output->shape[output->ndim - 1];

    if (top_k > num_classes) {
        top_k = num_classes;
    }

    /* Find top-k predictions */
    for (u32 k = 0; k < top_k; k++) {
        u32 max_idx = 0;
        float max_val = -1.0f;
        
        for (u32 c = 0; c < num_classes; c++) {
            /* Check if already selected */
            bool selected = false;
            for (u32 j = 0; j < k; j++) {
                if (indices[j] == c) {
                    selected = true;
                    break;
                }
            }
            
            if (!selected && data[c] > max_val) {
                max_val = data[c];
                max_idx = c;
            }
        }
        
        indices[k] = max_idx;
        probabilities[k] = max_val;
    }

    return 0;
}
