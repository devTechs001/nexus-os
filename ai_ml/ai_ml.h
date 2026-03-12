/*
 * NEXUS OS - AI/ML Framework
 * Copyright (c) 2026 NEXUS Development Team
 *
 * ai_ml.h - AI/ML Framework Header
 *
 * This header defines the core AI/ML framework interfaces for NEXUS OS.
 * It provides unified access to neural network inference, NPU acceleration,
 * and pre-built model architectures.
 */

#ifndef _NEXUS_AI_ML_H
#define _NEXUS_AI_ML_H

#include "../kernel/include/types.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         AI/ML FRAMEWORK VERSION                           */
/*===========================================================================*/

#define AI_ML_VERSION_MAJOR     1
#define AI_ML_VERSION_MINOR     0
#define AI_ML_VERSION_PATCH     0
#define AI_ML_VERSION_STRING    "1.0.0"
#define AI_ML_CODENAME          "NeuralCore"

/*===========================================================================*/
/*                         FRAMEWORK CONFIGURATION                           */
/*===========================================================================*/

/* Maximum supported neural network layers */
#define MAX_NN_LAYERS           256

/* Maximum tensor dimensions */
#define MAX_TENSOR_DIMS         8

/* Maximum model inputs/outputs */
#define MAX_MODEL_INPUTS        32
#define MAX_MODEL_OUTPUTS       32

/* NPU configuration */
#define MAX_NPU_CORES           1024
#define NPU_MEMORY_POOL_SIZE    (256 * MB(1))

/* Inference engine configuration */
#define INFERENCE_THREAD_POOL_SIZE  8
#define INFERENCE_BATCH_SIZE_MAX    64

/* Precision modes */
#define PRECISION_FP32          0
#define PRECISION_FP16          1
#define PRECISION_INT8          2
#define PRECISION_INT4          3

/* Accelerator types */
#define ACCEL_CPU               0
#define ACCEL_GPU               1
#define ACCEL_NPU               2
#define ACCEL_TPU               3
#define ACCEL_DSP               4

/* Model types */
#define MODEL_CNN               0x01
#define MODEL_RNN               0x02
#define MODEL_LSTM              0x03
#define MODEL_TRANSFORMER       0x04
#define MODEL_GAN               0x05
#define MODEL_AUTOENCODER       0x06
#define MODEL_YOLO              0x07
#define MODEL_RESNET            0x08
#define MODEL_VIT               0x09
#define MODEL_BERT              0x0A

/* Layer types */
#define LAYER_DENSE             0x01
#define LAYER_CONV2D            0x02
#define LAYER_CONV3D            0x03
#define LAYER_POOLING           0x04
#define LAYER_BN                0x05
#define LAYER_DROPOUT           0x06
#define LAYER_ACTIVATION        0x07
#define LAYER_SOFTMAX           0x08
#define LAYER_LSTM              0x09
#define LAYER_GRU               0x0A
#define LAYER_ATTENTION         0x0B
#define LAYER_NORMALIZATION     0x0C
#define LAYER_EMBEDDING         0x0D

/* Activation functions */
#define ACTIVATION_NONE         0
#define ACTIVATION_RELU         1
#define ACTIVATION_SIGMOID      2
#define ACTIVATION_TANH         3
#define ACTIVATION_SOFTMAX      4
#define ACTIVATION_LEAKY_RELU   5
#define ACTIVATION_GELU         6
#define ACTIVATION_SWISH        7
#define ACTIVATION_MISH         8

/* Inference status codes */
#define INFERENCE_SUCCESS       0
#define INFERENCE_ERR_INIT      -1
#define INFERENCE_ERR_MEMORY    -2
#define INFERENCE_ERR_INPUT     -3
#define INFERENCE_ERR_MODEL     -4
#define INFERENCE_ERR_ACCEL     -5
#define INFERENCE_ERR_TIMEOUT   -6

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Tensor - Multi-dimensional array structure
 *
 * Represents a multi-dimensional tensor used in neural network operations.
 * Supports various data types and memory layouts.
 */
typedef struct tensor {
    u32 id;                         /* Unique tensor identifier */
    char name[64];                  /* Tensor name */
    
    /* Dimensions */
    u32 ndim;                       /* Number of dimensions */
    u32 shape[MAX_TENSOR_DIMS];     /* Shape of each dimension */
    u32 strides[MAX_TENSOR_DIMS];   /* Strides for each dimension */
    
    /* Data */
    void *data;                     /* Pointer to tensor data */
    void *data_device;              /* Pointer to device memory (GPU/NPU) */
    size_t size;                    /* Total size in bytes */
    u32 dtype;                      /* Data type (FP32, FP16, INT8, etc.) */
    
    /* Memory management */
    bool owns_data;                 /* Whether tensor owns the data */
    bool is_device;                 /* Whether data is on device */
    
    /* Quantization */
    float scale;                    /* Quantization scale */
    s32 zero_point;                 /* Quantization zero point */
    
    /* Reference counting */
    atomic_t refcount;              /* Reference count */
    
    /* Links for graph */
    struct tensor *inputs[MAX_MODEL_INPUTS];
    struct tensor *outputs[MAX_MODEL_OUTPUTS];
    u32 num_inputs;
    u32 num_outputs;
} tensor_t;

/**
 * Neural Network Layer
 *
 * Represents a single layer in a neural network.
 */
typedef struct nn_layer {
    u32 id;                         /* Layer identifier */
    char name[64];                  /* Layer name */
    u32 type;                       /* Layer type (LAYER_*) */
    
    /* Layer parameters */
    u32 input_channels;             /* Number of input channels */
    u32 output_channels;            /* Number of output channels */
    u32 kernel_size;                /* Kernel size (for conv layers) */
    u32 stride;                     /* Stride */
    u32 padding;                    /* Padding */
    u32 dilation;                   /* Dilation rate */
    
    /* Weights and biases */
    tensor_t *weights;              /* Layer weights */
    tensor_t *biases;               /* Layer biases */
    
    /* Activation */
    u32 activation;                 /* Activation function */
    
    /* Quantization */
    bool quantized;                 /* Whether layer is quantized */
    u32 quant_bits;                 /* Quantization bit width */
    
    /* Runtime state */
    tensor_t *input;                /* Input tensor */
    tensor_t *output;               /* Output tensor */
    void *workspace;                /* Temporary workspace */
    size_t workspace_size;          /* Workspace size */
    
    /* Links */
    struct nn_layer *next;          /* Next layer */
    struct nn_layer *prev;          /* Previous layer */
} nn_layer_t;

/**
 * Neural Network Model
 *
 * Represents a complete neural network model.
 */
typedef struct nn_model {
    u32 id;                         /* Model identifier */
    char name[64];                  /* Model name */
    u32 type;                       /* Model type (MODEL_*) */
    
    /* Model architecture */
    nn_layer_t *layers;             /* Linked list of layers */
    u32 num_layers;                 /* Number of layers */
    
    /* Inputs and outputs */
    tensor_t *inputs[MAX_MODEL_INPUTS];
    tensor_t *outputs[MAX_MODEL_OUTPUTS];
    u32 num_inputs;
    u32 num_outputs;
    
    /* Model metadata */
    char version[32];               /* Model version */
    char framework[32];             /* Source framework (PyTorch, TF, etc.) */
    u64 created_at;                 /* Creation timestamp */
    u64 modified_at;                /* Last modification timestamp */
    
    /* Performance metrics */
    u64 params_count;               /* Number of parameters */
    u64 flops_count;                /* Floating point operations */
    u64 memory_usage;               /* Memory usage in bytes */
    
    /* Acceleration */
    u32 preferred_accel;            /* Preferred accelerator */
    u32 precision;                  /* Preferred precision */
    
    /* Runtime state */
    bool loaded;                    /* Whether model is loaded */
    bool compiled;                  /* Whether model is compiled */
    bool quantized;                 /* Whether model is quantized */
    
    /* Execution context */
    void *exec_context;             /* Execution context */
    
    /* Reference counting */
    atomic_t refcount;              /* Reference count */
} nn_model_t;

/**
 * Inference Engine Configuration
 */
typedef struct inference_config {
    u32 batch_size;                 /* Batch size for inference */
    u32 num_threads;                /* Number of CPU threads */
    u32 accelerator;                /* Accelerator type */
    u32 precision;                  /* Precision mode */
    bool use_npu;                   /* Use NPU acceleration */
    bool use_gpu;                   /* Use GPU acceleration */
    bool use_fp16;                  /* Use FP16 precision */
    bool use_int8;                  /* Use INT8 quantization */
    bool enable_profiling;          /* Enable performance profiling */
    bool enable_caching;            /* Enable result caching */
    u32 cache_size;                 /* Cache size in MB */
    u32 timeout_ms;                 /* Inference timeout in ms */
} inference_config_t;

/**
 * Inference Engine Statistics
 */
typedef struct inference_stats {
    u64 total_inferences;           /* Total inference count */
    u64 successful_inferences;      /* Successful inference count */
    u64 failed_inferences;          /* Failed inference count */
    
    /* Timing */
    u64 total_time_us;              /* Total inference time */
    u64 min_time_us;                /* Minimum inference time */
    u64 max_time_us;                /* Maximum inference time */
    u64 avg_time_us;                /* Average inference time */
    
    /* Resource usage */
    u64 memory_allocated;           /* Memory allocated */
    u64 memory_peak;                /* Peak memory usage */
    u32 cpu_usage;                  /* CPU usage percentage */
    u32 npu_usage;                  /* NPU usage percentage */
    u32 gpu_usage;                  /* GPU usage percentage */
    
    /* Cache statistics */
    u64 cache_hits;                 /* Cache hit count */
    u64 cache_misses;               /* Cache miss count */
    u32 cache_hit_rate;             /* Cache hit rate percentage */
} inference_stats_t;

/**
 * NPU Device Information
 */
typedef struct npu_info {
    u32 device_id;                  /* Device identifier */
    char name[64];                  /* Device name */
    char vendor[64];                /* Vendor name */
    u32 num_cores;                  /* Number of NPU cores */
    u64 memory_size;                /* Device memory size */
    u64 memory_used;                /* Memory currently used */
    u32 max_batch_size;             /* Maximum batch size */
    u32 supported_precisions;       /* Bitmask of supported precisions */
    u32 temperature;                /* Current temperature */
    u32 power_usage;                /* Power usage in mW */
    u32 utilization;                /* Utilization percentage */
    bool available;                 /* Device availability */
} npu_info_t;

/*===========================================================================*/
/*                         AI/ML FRAMEWORK STATE                             */
/*===========================================================================*/

typedef struct ai_ml_framework {
    bool initialized;               /* Framework initialized */
    u32 version;                    /* Framework version */
    
    /* NPU devices */
    npu_info_t npu_devices[MAX_NPU_CORES];
    u32 num_npu_devices;
    u32 active_npu;                 /* Active NPU device */
    
    /* Inference engine */
    inference_config_t default_config;
    inference_stats_t global_stats;
    
    /* Model registry */
    nn_model_t *models;             /* Registered models */
    u32 num_models;                 /* Number of registered models */
    
    /* Memory pools */
    void *tensor_memory_pool;       /* Tensor memory pool */
    void *npu_memory_pool;          /* NPU memory pool */
    size_t pool_size;               /* Pool size */
    
    /* Thread pool */
    void *thread_pool;              /* Thread pool handle */
    u32 num_worker_threads;         /* Number of worker threads */
    
    /* Synchronization */
    spinlock_t lock;                /* Framework lock */
} ai_ml_framework_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Framework initialization */
int ai_ml_framework_init(void);
void ai_ml_framework_shutdown(void);
bool ai_ml_framework_is_initialized(void);

/* NPU management */
int npu_init(void);
int npu_shutdown(void);
int npu_get_device_count(void);
int npu_get_device_info(u32 device_id, npu_info_t *info);
int npu_select_device(u32 device_id);
int npu_allocate_memory(size_t size, void **ptr);
int npu_free_memory(void *ptr);
int npu_copy_to_device(void *dst, const void *src, size_t size);
int npu_copy_from_device(void *dst, const void *src, size_t size);

/* Tensor operations */
tensor_t *tensor_create(const char *name, u32 ndim, const u32 *shape, u32 dtype);
void tensor_destroy(tensor_t *tensor);
tensor_t *tensor_clone(tensor_t *tensor);
int tensor_copy(tensor_t *dst, const tensor_t *src);
int tensor_reshape(tensor_t *tensor, u32 ndim, const u32 *shape);
int tensor_transpose(tensor_t *tensor, const u32 *perm);
int tensor_fill(tensor_t *tensor, float value);
int tensor_zero(tensor_t *tensor);

/* Model management */
nn_model_t *model_create(const char *name, u32 type);
void model_destroy(nn_model_t *model);
int model_add_layer(nn_model_t *model, nn_layer_t *layer);
int model_load(nn_model_t *model, const char *path);
int model_save(nn_model_t *model, const char *path);
int model_compile(nn_model_t *model, inference_config_t *config);
int model_quantize(nn_model_t *model, u32 precision);

/* Layer operations */
nn_layer_t *layer_create_dense(u32 input_size, u32 output_size, u32 activation);
nn_layer_t *layer_create_conv2d(u32 in_ch, u32 out_ch, u32 kernel, u32 stride, u32 padding);
nn_layer_t *layer_create_pooling(u32 kernel, u32 stride, u32 type);
nn_layer_t *layer_create_bn(u32 num_features);
nn_layer_t *layer_create_dropout(float rate);
nn_layer_t *layer_create_activation(u32 activation);
void layer_destroy(nn_layer_t *layer);

/* Inference engine */
int inference_init(inference_config_t *config);
int inference_shutdown(void);
int inference_run(nn_model_t *model, tensor_t **inputs, u32 num_inputs,
                  tensor_t **outputs, u32 num_outputs);
int inference_run_async(nn_model_t *model, tensor_t **inputs, u32 num_inputs,
                        tensor_t **outputs, u32 num_outputs, void *callback);
int inference_get_stats(inference_stats_t *stats);
int inference_reset_stats(void);

/* Pre-built models */
nn_model_t *model_create_cnn(const char *name, u32 num_classes);
nn_model_t *model_create_resnet(const char *name, u32 depth);
nn_model_t *model_create_transformer(const char *name, u32 d_model, u32 n_heads, u32 n_layers);
nn_model_t *model_create_yolo(const char *name, u32 num_classes);
nn_model_t *model_create_bert(const char *name, u32 hidden_size, u32 n_layers);

/* Utility functions */
const char *ai_ml_get_version(void);
const char *ai_ml_get_codename(void);
u32 ai_ml_get_supported_precisions(void);
u32 ai_ml_get_supported_accelerators(void);

#endif /* _NEXUS_AI_ML_H */
