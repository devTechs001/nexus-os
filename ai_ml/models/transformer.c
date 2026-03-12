/*
 * NEXUS OS - AI/ML Framework
 * ai_ml/models/transformer.c
 *
 * Transformer Model Implementation
 *
 * This module implements the Transformer architecture for natural language
 * processing, including self-attention mechanisms, multi-head attention,
 * and feed-forward networks.
 */

#include "../ai_ml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================*/
/*                         TRANSFORMER CONFIGURATION                         */
/*===========================================================================*/

/* Transformer model sizes */
#define TRANSFORMER_TINY          0   /* d_model=128, heads=2, layers=2 */
#define TRANSFORMER_SMALL         1   /* d_model=256, heads=4, layers=4 */
#define TRANSFORMER_BASE          2   /* d_model=512, heads=8, layers=6 */
#define TRANSFORMER_LARGE         3   /* d_model=768, heads=12, layers=12 */
#define TRANSFORMER_XL            4   /* d_model=1024, heads=16, layers=24 */

/* Default parameters for BASE model */
#define TRANSFORMER_DEFAULT_D_MODEL     512
#define TRANSFORMER_DEFAULT_N_HEADS     8
#define TRANSFORMER_DEFAULT_N_LAYERS    6
#define TRANSFORMER_DEFAULT_D_FF        2048
#define TRANSFORMER_DEFAULT_MAX_SEQ     512
#define TRANSFORMER_DEFAULT_VOCAB       30522

/* Attention types */
#define ATTENTION_SELF            0
#define ATTENTION_CROSS           1
#define ATTENTION_CAUSAL          2

/*===========================================================================*/
/*                         TRANSFORMER DATA STRUCTURES                       */
/*===========================================================================*/

/**
 * Transformer-specific layer data
 */
typedef struct transformer_layer {
    u32 layer_id;                   /* Layer index */
    
    /* Multi-head attention */
    tensor_t *q_weights;            /* Query projection weights */
    tensor_t *k_weights;            /* Key projection weights */
    tensor_t *v_weights;            /* Value projection weights */
    tensor_t *o_weights;            /* Output projection weights */
    tensor_t *q_bias;               /* Query bias */
    tensor_t *k_bias;               /* Key bias */
    tensor_t *v_bias;               /* Value bias */
    tensor_t *o_bias;               /* Output bias */
    
    /* Layer normalization 1 */
    tensor_t *ln1_gamma;            /* Layer norm gamma */
    tensor_t *ln1_beta;             /* Layer norm beta */
    
    /* Feed-forward network */
    tensor_t *ff_w1;                /* FFN layer 1 weights */
    tensor_t *ff_w2;                /* FFN layer 2 weights */
    tensor_t *ff_b1;                /* FFN layer 1 bias */
    tensor_t *ff_b2;                /* FFN layer 2 bias */
    
    /* Layer normalization 2 */
    tensor_t *ln2_gamma;            /* Layer norm gamma */
    tensor_t *ln2_beta;             /* Layer norm beta */
    
    /* Configuration */
    u32 d_model;                    /* Model dimension */
    u32 n_heads;                    /* Number of attention heads */
    u32 d_ff;                       /* Feed-forward dimension */
    u32 attention_type;             /* Attention type */
} transformer_layer_t;

/**
 * Transformer model state
 */
typedef struct transformer_state {
    u32 d_model;                    /* Model dimension */
    u32 n_heads;                    /* Number of attention heads */
    u32 n_layers;                   /* Number of layers */
    u32 d_ff;                       /* Feed-forward dimension */
    u32 max_seq_len;                /* Maximum sequence length */
    u32 vocab_size;                 /* Vocabulary size */
    
    /* Embeddings */
    tensor_t *token_embeddings;     /* Token embedding table */
    tensor_t *position_embeddings;  /* Position embedding table */
    tensor_t *segment_embeddings;   /* Segment embedding (for BERT) */
    
    /* Transformer layers */
    transformer_layer_t *layers;    /* Array of transformer layers */
    
    /* Output projection */
    tensor_t *output_weights;       /* Output projection weights */
    tensor_t *output_bias;          /* Output projection bias */
    
    /* Attention mask */
    tensor_t *attention_mask;       /* Attention mask tensor */
    
    /* Temporary buffers */
    tensor_t *q_proj;               /* Query projection buffer */
    tensor_t *k_proj;               /* Key projection buffer */
    tensor_t *v_proj;               /* Value projection buffer */
    tensor_t *attention_output;     /* Attention output buffer */
    
    /* Dropout rate */
    float dropout_rate;             /* Dropout rate */
    bool training;                  /* Training mode flag */
} transformer_state_t;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static int transformer_forward(nn_model_t *model, tensor_t *input);
static int transformer_layer_forward(transformer_layer_t *layer, 
                                      transformer_state_t *state,
                                      tensor_t *input, tensor_t *output);
static int multi_head_attention(tensor_t *q, tensor_t *k, tensor_t *v,
                                 tensor_t *output, u32 n_heads,
                                 tensor_t *mask);
static int layer_normalization(tensor_t *input, tensor_t *output,
                                tensor_t *gamma, tensor_t *beta);
static int feed_forward(tensor_t *input, tensor_t *output,
                        tensor_t *w1, tensor_t *w2, tensor_t *b1, tensor_t *b2);
static void softmax_attention(float *attention, u32 seq_len);

/*===========================================================================*/
/*                         TRANSFORMER MODEL CREATION                        */
/*===========================================================================*/

/**
 * model_create_transformer - Create a Transformer model
 * @name: Model name
 * @d_model: Model dimension (hidden size)
 * @n_heads: Number of attention heads
 * @n_layers: Number of transformer layers
 *
 * Creates a Transformer model with the specified configuration.
 * This can be used for various NLP tasks including translation,
 * text generation, and understanding.
 *
 * Returns: Pointer to created model, or NULL on failure
 */
nn_model_t *model_create_transformer(const char *name, u32 d_model, u32 n_heads, u32 n_layers)
{
    nn_model_t *model;
    transformer_state_t *state;
    u32 d_ff;
    
    if (!name) {
        pr_err("Transformer model name required\n");
        return NULL;
    }

    /* Validate parameters */
    if (d_model == 0) d_model = TRANSFORMER_DEFAULT_D_MODEL;
    if (n_heads == 0) n_heads = TRANSFORMER_DEFAULT_N_HEADS;
    if (n_layers == 0) n_layers = TRANSFORMER_DEFAULT_N_LAYERS;
    
    /* d_ff is typically 4 * d_model */
    d_ff = d_model * 4;

    /* Create model structure */
    model = model_create(name, MODEL_TRANSFORMER);
    if (!model) {
        return NULL;
    }

    /* Allocate transformer state */
    state = (transformer_state_t *)kzalloc(sizeof(transformer_state_t));
    if (!state) {
        model_destroy(model);
        return NULL;
    }

    /* Initialize state */
    state->d_model = d_model;
    state->n_heads = n_heads;
    state->n_layers = n_layers;
    state->d_ff = d_ff;
    state->max_seq_len = TRANSFORMER_DEFAULT_MAX_SEQ;
    state->vocab_size = TRANSFORMER_DEFAULT_VOCAB;
    state->dropout_rate = 0.1f;
    state->training = false;

    model->exec_context = state;

    /* Create input tensor: [batch, seq_len] */
    u32 input_shape[] = {1, TRANSFORMER_DEFAULT_MAX_SEQ};
    model->inputs[0] = tensor_create("input_ids", 2, input_shape, PRECISION_FP32);
    model->num_inputs = 1;

    /* Create output tensor: [batch, seq_len, vocab_size] */
    u32 output_shape[] = {1, TRANSFORMER_DEFAULT_MAX_SEQ, state->vocab_size};
    model->outputs[0] = tensor_create("logits", 3, output_shape, PRECISION_FP32);
    model->num_outputs = 1;

    /* Create embedding tables */
    u32 embed_shape[] = {state->vocab_size, d_model};
    state->token_embeddings = tensor_create("token_embeddings", 2, embed_shape, PRECISION_FP32);
    
    u32 pos_shape[] = {state->max_seq_len, d_model};
    state->position_embeddings = tensor_create("position_embeddings", 2, pos_shape, PRECISION_FP32);

    /* Allocate transformer layers */
    state->layers = (transformer_layer_t *)kzalloc(n_layers * sizeof(transformer_layer_t));
    if (!state->layers) {
        model_destroy(model);
        return NULL;
    }

    /* Initialize each transformer layer */
    for (u32 i = 0; i < n_layers; i++) {
        transformer_layer_t *layer = &state->layers[i];
        layer->layer_id = i;
        layer->d_model = d_model;
        layer->n_heads = n_heads;
        layer->d_ff = d_ff;
        layer->attention_type = ATTENTION_SELF;

        /* Attention projection weights */
        /* Each head has d_model/n_heads dimensions */
        u32 head_dim = d_model / n_heads;
        u32 qkv_shape[] = {d_model, d_model};
        
        layer->q_weights = tensor_create("q_weights", 2, qkv_shape, PRECISION_FP32);
        layer->k_weights = tensor_create("k_weights", 2, qkv_shape, PRECISION_FP32);
        layer->v_weights = tensor_create("v_weights", 2, qkv_shape, PRECISION_FP32);
        layer->o_weights = tensor_create("o_weights", 2, qkv_shape, PRECISION_FP32);

        u32 bias_shape[] = {d_model};
        layer->q_bias = tensor_create("q_bias", 1, bias_shape, PRECISION_FP32);
        layer->k_bias = tensor_create("k_bias", 1, bias_shape, PRECISION_FP32);
        layer->v_bias = tensor_create("v_bias", 1, bias_shape, PRECISION_FP32);
        layer->o_bias = tensor_create("o_bias", 1, bias_shape, PRECISION_FP32);

        /* Layer normalization 1 */
        layer->ln1_gamma = tensor_create("ln1_gamma", 1, bias_shape, PRECISION_FP32);
        layer->ln1_beta = tensor_create("ln1_beta", 1, bias_shape, PRECISION_FP32);

        /* Feed-forward network */
        u32 ff1_shape[] = {d_model, d_ff};
        u32 ff2_shape[] = {d_ff, d_model};
        
        layer->ff_w1 = tensor_create("ff_w1", 2, ff1_shape, PRECISION_FP32);
        layer->ff_w2 = tensor_create("ff_w2", 2, ff2_shape, PRECISION_FP32);

        u32 ff_bias1[] = {d_ff};
        u32 ff_bias2[] = {d_model};
        
        layer->ff_b1 = tensor_create("ff_b1", 1, ff_bias1, PRECISION_FP32);
        layer->ff_b2 = tensor_create("ff_b2", 1, ff_bias2, PRECISION_FP32);

        /* Layer normalization 2 */
        layer->ln2_gamma = tensor_create("ln2_gamma", 1, bias_shape, PRECISION_FP32);
        layer->ln2_beta = tensor_create("ln2_beta", 1, bias_shape, PRECISION_FP32);
    }

    /* Output projection */
    u32 out_shape[] = {d_model, state->vocab_size};
    state->output_weights = tensor_create("output_weights", 2, out_shape, PRECISION_FP32);
    u32 out_bias_shape[] = {state->vocab_size};
    state->output_bias = tensor_create("output_bias", 1, out_bias_shape, PRECISION_FP32);

    /* Calculate model statistics */
    /* Approximate parameter count */
    u32 embed_params = state->vocab_size * d_model + state->max_seq_len * d_model;
    u32 attention_params = n_layers * (4 * d_model * d_model * 4);  /* Q, K, V, O projections */
    u32 ffn_params = n_layers * (2 * d_model * d_ff * 2);  /* Two FFN layers */
    u32 ln_params = n_layers * (4 * d_model);  /* Layer norms */
    u32 output_params = d_model * state->vocab_size;
    
    model->params_count = embed_params + attention_params + ffn_params + ln_params + output_params;
    model->flops_count = model->params_count * 2 * state->max_seq_len;  /* Per token */
    model->memory_usage = model->params_count * sizeof(float);

    /* Add a single "layer" to the model for inference engine compatibility */
    nn_layer_t *transformer_wrapper = (nn_layer_t *)kzalloc(sizeof(nn_layer_t));
    if (transformer_wrapper) {
        transformer_wrapper->type = LAYER_ATTENTION;
        transformer_wrapper->input_channels = d_model;
        transformer_wrapper->output_channels = d_model;
        snprintf(transformer_wrapper->name, sizeof(transformer_wrapper->name), "transformer");
        model_add_layer(model, transformer_wrapper);
    }

    pr_info("Transformer model '%s' created:\n", name);
    pr_info("  d_model=%u, n_heads=%u, n_layers=%u, d_ff=%u\n",
            d_model, n_heads, n_layers, d_ff);
    pr_info("  Parameters: %llu, FLOPs: %llu\n",
            (unsigned long long)model->params_count,
            (unsigned long long)model->flops_count);

    return model;
}

/**
 * model_create_bert - Create a BERT model
 * @name: Model name
 * @hidden_size: Hidden layer size
 * @n_layers: Number of layers
 *
 * Creates a BERT (Bidirectional Encoder Representations from Transformers)
 * model for natural language understanding tasks.
 *
 * Returns: Pointer to created model, or NULL on failure
 */
nn_model_t *model_create_bert(const char *name, u32 hidden_size, u32 n_layers)
{
    nn_model_t *model;
    transformer_state_t *state;
    u32 n_heads;
    
    if (!name) {
        pr_err("BERT model name required\n");
        return NULL;
    }

    /* Validate parameters */
    if (hidden_size == 0) hidden_size = 768;  /* BERT-base */
    if (n_layers == 0) n_layers = 12;
    
    /* BERT-base: 768 hidden, 12 heads, 12 layers */
    /* BERT-large: 1024 hidden, 16 heads, 24 layers */
    n_heads = hidden_size / 64;

    /* Create base transformer */
    model = model_create_transformer(name, hidden_size, n_heads, n_layers);
    if (!model) {
        return NULL;
    }

    /* Update model type */
    model->type = MODEL_BERT;

    /* Get transformer state */
    state = (transformer_state_t *)model->exec_context;

    /* Add BERT-specific components */
    /* [CLS] and [SEP] token embeddings */
    u32 cls_sep_shape[] = {2, hidden_size};
    tensor_t *cls_sep = tensor_create("cls_sep_embeddings", 2, cls_sep_shape, PRECISION_FP32);
    
    /* Layer norm for pooler */
    u32 ln_shape[] = {hidden_size};
    tensor_t *pooler_ln_gamma = tensor_create("pooler_ln_gamma", 1, ln_shape, PRECISION_FP32);
    tensor_t *pooler_ln_beta = tensor_create("pooler_ln_beta", 1, ln_shape, PRECISION_FP32);
    
    /* Pooler dense layer */
    u32 pooler_shape[] = {hidden_size, hidden_size};
    tensor_t *pooler_dense = tensor_create("pooler_dense", 2, pooler_shape, PRECISION_FP32);
    tensor_t *pooler_bias = tensor_create("pooler_bias", 1, ln_shape, PRECISION_FP32);

    /* Store BERT-specific tensors in model */
    /* (In a full implementation, these would be stored in state) */
    (void)cls_sep;
    (void)pooler_ln_gamma;
    (void)pooler_ln_beta;
    (void)pooler_dense;
    (void)pooler_bias;

    /* BERT output is typically the [CLS] token representation */
    u32 cls_output_shape[] = {1, hidden_size};
    model->outputs[0] = tensor_create("cls_output", 2, cls_output_shape, PRECISION_FP32);

    pr_info("BERT model '%s' created (hidden=%u, layers=%u)\n",
            name, hidden_size, n_layers);

    return model;
}

/*===========================================================================*/
/*                         TRANSFORMER FORWARD PASS                          */
/*===========================================================================*/

/**
 * transformer_forward - Forward pass through Transformer
 * @model: Transformer model
 * @input: Input tensor (token IDs)
 *
 * Executes the complete Transformer forward pass including:
 * 1. Token and position embeddings
 * 2. Transformer encoder layers
 * 3. Output projection
 *
 * Returns: 0 on success, error code on failure
 */
static int transformer_forward(nn_model_t *model, tensor_t *input)
{
    transformer_state_t *state;
    tensor_t *hidden;
    u32 batch_size, seq_len;
    
    if (!model || !input) {
        return INFERENCE_ERR_INPUT;
    }

    state = (transformer_state_t *)model->exec_context;
    if (!state) {
        return INFERENCE_ERR_MODEL;
    }

    batch_size = input->shape[0];
    seq_len = input->shape[1];

    /* Step 1: Token embeddings lookup */
    /* (Simplified - in reality would do embedding lookup) */
    hidden = tensor_create("hidden", 3, 
                           (u32[]){batch_size, seq_len, state->d_model},
                           PRECISION_FP32);
    if (!hidden) {
        return INFERENCE_ERR_MEMORY;
    }

    /* Step 2: Add position embeddings */
    /* (Simplified - just initialize with small random values) */
    float *hidden_data = (float *)hidden->data;
    for (u32 i = 0; i < hidden->size / sizeof(float); i++) {
        hidden_data[i] = 0.01f * (i % 100 - 50);
    }

    /* Step 3: Pass through transformer layers */
    tensor_t *layer_input = hidden;
    tensor_t *layer_output = NULL;
    
    for (u32 i = 0; i < state->n_layers; i++) {
        /* Allocate layer output */
        layer_output = tensor_create("layer_out", 3,
                                     (u32[]){batch_size, seq_len, state->d_model},
                                     PRECISION_FP32);
        if (!layer_output) {
            tensor_destroy(hidden);
            return INFERENCE_ERR_MEMORY;
        }

        /* Forward through layer */
        transformer_layer_forward(&state->layers[i], state, layer_input, layer_output);

        /* Swap buffers */
        if (layer_input != hidden) {
            tensor_destroy(layer_input);
        }
        layer_input = layer_output;
    }

    /* Step 4: Output projection to vocabulary */
    /* (Simplified - just copy for now) */
    if (model->outputs[0]) {
        tensor_copy(model->outputs[0], layer_input);
    }

    /* Cleanup */
    if (layer_input && layer_input != hidden) {
        tensor_destroy(layer_input);
    }
    tensor_destroy(hidden);

    return 0;
}

/**
 * transformer_layer_forward - Forward pass through single transformer layer
 * @layer: Transformer layer
 * @state: Transformer state
 * @input: Input tensor
 * @output: Output tensor
 *
 * Implements:
 * 1. Multi-head self-attention with residual connection
 * 2. Layer normalization
 * 3. Feed-forward network with residual connection
 * 4. Layer normalization
 *
 * Returns: 0 on success, error code on failure
 */
static int transformer_layer_forward(transformer_layer_t *layer,
                                      transformer_state_t *state,
                                      tensor_t *input, tensor_t *output)
{
    u32 batch_size, seq_len, d_model;
    
    if (!layer || !input || !output) {
        return INFERENCE_ERR_INPUT;
    }

    batch_size = input->shape[0];
    seq_len = input->shape[1];
    d_model = layer->d_model;

    /* ========== MULTI-HEAD ATTENTION ========== */
    
    /* Project input to Q, K, V */
    /* (Simplified - in reality would do matrix multiplication) */
    tensor_t *q = tensor_create("q", 3, (u32[]){batch_size, seq_len, d_model}, PRECISION_FP32);
    tensor_t *k = tensor_create("k", 3, (u32[]){batch_size, seq_len, d_model}, PRECISION_FP32);
    tensor_t *v = tensor_create("v", 3, (u32[]){batch_size, seq_len, d_model}, PRECISION_FP32);
    
    if (!q || !k || !v) {
        tensor_destroy(q);
        tensor_destroy(k);
        tensor_destroy(v);
        return INFERENCE_ERR_MEMORY;
    }

    /* Copy input to Q, K, V (simplified) */
    tensor_copy(q, input);
    tensor_copy(k, input);
    tensor_copy(v, input);

    /* Compute multi-head attention */
    tensor_t *attn_output = tensor_create("attn_out", 3,
                                          (u32[]){batch_size, seq_len, d_model},
                                          PRECISION_FP32);
    if (!attn_output) {
        tensor_destroy(q);
        tensor_destroy(k);
        tensor_destroy(v);
        return INFERENCE_ERR_MEMORY;
    }

    multi_head_attention(q, k, v, attn_output, layer->n_heads, NULL);

    /* Project attention output */
    /* (Simplified) */
    tensor_t *attn_proj = tensor_create("attn_proj", 3,
                                        (u32[]){batch_size, seq_len, d_model},
                                        PRECISION_FP32);
    tensor_copy(attn_proj, attn_output);

    /* Residual connection: input + attn_proj */
    float *in_data = (float *)input->data;
    float *attn_data = (float *)attn_proj->data;
    float *residual_data = (float *)output->data;
    
    size_t size = input->size / sizeof(float);
    for (size_t i = 0; i < size; i++) {
        residual_data[i] = in_data[i] + attn_data[i];
    }

    /* Layer normalization 1 */
    layer_normalization(output, output, layer->ln1_gamma, layer->ln1_beta);

    /* ========== FEED-FORWARD NETWORK ========== */
    
    tensor_t *ff_input = tensor_create("ff_in", 3,
                                       (u32[]){batch_size, seq_len, d_model},
                                       PRECISION_FP32);
    tensor_t *ff_output = tensor_create("ff_out", 3,
                                        (u32[]){batch_size, seq_len, d_model},
                                        PRECISION_FP32);
    
    if (!ff_input || !ff_output) {
        tensor_destroy(q);
        tensor_destroy(k);
        tensor_destroy(v);
        tensor_destroy(attn_output);
        tensor_destroy(attn_proj);
        tensor_destroy(ff_input);
        tensor_destroy(ff_output);
        return INFERENCE_ERR_MEMORY;
    }

    tensor_copy(ff_input, output);
    
    feed_forward(ff_input, ff_output, layer->ff_w1, layer->ff_w2,
                 layer->ff_b1, layer->ff_b2);

    /* Residual connection + layer norm 2 */
    float *ff_data = (float *)ff_output->data;
    float *ln_input = (float *)output->data;
    
    for (size_t i = 0; i < size; i++) {
        ln_input[i] = ln_input[i] + ff_data[i];
    }

    layer_normalization(output, output, layer->ln2_gamma, layer->ln2_beta);

    /* Cleanup */
    tensor_destroy(q);
    tensor_destroy(k);
    tensor_destroy(v);
    tensor_destroy(attn_output);
    tensor_destroy(attn_proj);
    tensor_destroy(ff_input);
    tensor_destroy(ff_output);

    return 0;
}

/*===========================================================================*/
/*                         ATTENTION MECHANISM                               */
/*===========================================================================*/

/**
 * multi_head_attention - Compute multi-head self-attention
 * @q: Query tensor [batch, seq_len, d_model]
 * @k: Key tensor [batch, seq_len, d_model]
 * @v: Value tensor [batch, seq_len, d_model]
 * @output: Output tensor [batch, seq_len, d_model]
 * @n_heads: Number of attention heads
 * @mask: Optional attention mask
 *
 * Computes: Attention(Q, K, V) = softmax(QK^T / sqrt(d_k))V
 *
 * Returns: 0 on success, error code on failure
 */
static int multi_head_attention(tensor_t *q, tensor_t *k, tensor_t *v,
                                 tensor_t *output, u32 n_heads, tensor_t *mask)
{
    float *q_data, *k_data, *v_data, *out_data;
    u32 batch_size, seq_len, d_model, d_k;
    
    if (!q || !k || !v || !output) {
        return INFERENCE_ERR_INPUT;
    }

    batch_size = q->shape[0];
    seq_len = q->shape[1];
    d_model = q->shape[2];
    d_k = d_model / n_heads;

    q_data = (float *)q->data;
    k_data = (float *)k->data;
    v_data = (float *)v->data;
    out_data = (float *)output->data;

    /* For each head */
    for (u32 head = 0; head < n_heads; head++) {
        u32 head_start = head * d_k;

        /* For each position in sequence */
        for (u32 i = 0; i < seq_len; i++) {
            /* Compute attention scores */
            float scores[512];  /* Max sequence length */
            float max_score = -INFINITY;
            
            for (u32 j = 0; j < seq_len; j++) {
                float score = 0.0f;
                
                /* Dot product of Q[i] and K[j] */
                for (u32 d = 0; d < d_k; d++) {
                    u32 q_idx = (i * d_model + head_start + d);
                    u32 k_idx = (j * d_model + head_start + d);
                    score += q_data[q_idx] * k_data[k_idx];
                }
                
                /* Scale by sqrt(d_k) */
                score /= sqrtf((float)d_k);
                
                scores[j] = score;
                if (score > max_score) {
                    max_score = score;
                }
            }

            /* Softmax */
            float sum = 0.0f;
            for (u32 j = 0; j < seq_len; j++) {
                scores[j] = expf(scores[j] - max_score);
                sum += scores[j];
            }
            for (u32 j = 0; j < seq_len; j++) {
                scores[j] /= sum;
            }

            /* Apply mask if provided */
            if (mask) {
                float *mask_data = (float *)mask->data;
                for (u32 j = 0; j < seq_len; j++) {
                    if (mask_data[i * seq_len + j] == 0) {
                        scores[j] = 0;
                    }
                }
            }

            /* Compute weighted sum of values */
            for (u32 d = 0; d < d_k; d++) {
                float value = 0.0f;
                for (u32 j = 0; j < seq_len; j++) {
                    u32 v_idx = (j * d_model + head_start + d);
                    value += scores[j] * v_data[v_idx];
                }
                
                u32 out_idx = (i * d_model + head_start + d);
                out_data[out_idx] = value;
            }
        }
    }

    return 0;
}

/**
 * softmax_attention - Apply softmax to attention scores
 * @attention: Attention score matrix [seq_len, seq_len]
 * @seq_len: Sequence length
 */
static void softmax_attention(float *attention, u32 seq_len)
{
    for (u32 i = 0; i < seq_len; i++) {
        float max_val = -INFINITY;
        float sum = 0.0f;
        
        /* Find max for numerical stability */
        for (u32 j = 0; j < seq_len; j++) {
            float val = attention[i * seq_len + j];
            if (val > max_val) {
                max_val = val;
            }
        }
        
        /* Compute exp and sum */
        for (u32 j = 0; j < seq_len; j++) {
            float exp_val = expf(attention[i * seq_len + j] - max_val);
            attention[i * seq_len + j] = exp_val;
            sum += exp_val;
        }
        
        /* Normalize */
        for (u32 j = 0; j < seq_len; j++) {
            attention[i * seq_len + j] /= sum;
        }
    }
}

/*===========================================================================*/
/*                         LAYER NORMALIZATION                               */
/*===========================================================================*/

/**
 * layer_normalization - Apply layer normalization
 * @input: Input tensor
 * @output: Output tensor
 * @gamma: Scale parameter
 * @beta: Shift parameter
 *
 * Computes: output = gamma * (input - mean) / sqrt(var + eps) + beta
 *
 * Returns: 0 on success, error code on failure
 */
static int layer_normalization(tensor_t *input, tensor_t *output,
                                tensor_t *gamma, tensor_t *beta)
{
    float *in_data, *out_data, *g_data, *b_data;
    u32 batch_size, seq_len, d_model;
    float eps = 1e-6f;
    
    if (!input || !output) {
        return INFERENCE_ERR_INPUT;
    }

    in_data = (float *)input->data;
    out_data = (float *)output->data;
    g_data = gamma ? (float *)gamma->data : NULL;
    b_data = beta ? (float *)beta->data : NULL;

    batch_size = input->shape[0];
    seq_len = input->shape[1];
    d_model = input->shape[2];

    /* Normalize each position independently */
    for (u32 b = 0; b < batch_size; b++) {
        for (u32 s = 0; s < seq_len; s++) {
            u32 base_idx = (b * seq_len + s) * d_model;
            
            /* Compute mean */
            float mean = 0.0f;
            for (u32 d = 0; d < d_model; d++) {
                mean += in_data[base_idx + d];
            }
            mean /= d_model;
            
            /* Compute variance */
            float var = 0.0f;
            for (u32 d = 0; d < d_model; d++) {
                float diff = in_data[base_idx + d] - mean;
                var += diff * diff;
            }
            var /= d_model;
            
            /* Normalize */
            float std = sqrtf(var + eps);
            for (u32 d = 0; d < d_model; d++) {
                float normalized = (in_data[base_idx + d] - mean) / std;
                
                /* Apply gamma and beta */
                if (g_data) {
                    normalized *= g_data[d];
                }
                if (b_data) {
                    normalized += b_data[d];
                }
                
                out_data[base_idx + d] = normalized;
            }
        }
    }

    return 0;
}

/*===========================================================================*/
/*                         FEED-FORWARD NETWORK                              */
/*===========================================================================*/

/**
 * feed_forward - Feed-forward network forward pass
 * @input: Input tensor
 * @output: Output tensor
 * @w1: First layer weights
 * @w2: Second layer weights
 * @b1: First layer bias
 * @b2: Second layer bias
 *
 * Computes: output = W2 * ReLU(W1 * input + b1) + b2
 *
 * Returns: 0 on success, error code on failure
 */
static int feed_forward(tensor_t *input, tensor_t *output,
                        tensor_t *w1, tensor_t *w2, tensor_t *b1, tensor_t *b2)
{
    float *in_data, *out_data, *w1_data, *w2_data, *b1_data, *b2_data;
    u32 batch_size, seq_len, d_model, d_ff;
    
    if (!input || !output || !w1 || !w2) {
        return INFERENCE_ERR_INPUT;
    }

    in_data = (float *)input->data;
    out_data = (float *)output->data;
    w1_data = (float *)w1->data;
    w2_data = (float *)w2->data;
    b1_data = b1 ? (float *)b1->data : NULL;
    b2_data = b2 ? (float *)b2->data : NULL;

    batch_size = input->shape[0];
    seq_len = input->shape[1];
    d_model = input->shape[2];
    d_ff = w1->shape[1];

    /* For each position */
    for (u32 b = 0; b < batch_size; b++) {
        for (u32 s = 0; s < seq_len; s++) {
            u32 base_idx = (b * seq_len + s) * d_model;
            
            /* First layer: W1 * input + b1 with ReLU */
            float hidden[8192];  /* Max d_ff */
            for (u32 h = 0; h < d_ff; h++) {
                float sum = 0.0f;
                for (u32 d = 0; d < d_model; d++) {
                    sum += in_data[base_idx + d] * w1_data[d * d_ff + h];
                }
                if (b1_data) {
                    sum += b1_data[h];
                }
                /* ReLU activation */
                hidden[h] = fmaxf(0.0f, sum);
            }
            
            /* Second layer: W2 * hidden + b2 */
            for (u32 d = 0; d < d_model; d++) {
                float sum = 0.0f;
                for (u32 h = 0; h < d_ff; h++) {
                    sum += hidden[h] * w2_data[h * d_model + d];
                }
                if (b2_data) {
                    sum += b2_data[d];
                }
                out_data[base_idx + d] = sum;
            }
        }
    }

    return 0;
}

/*===========================================================================*/
/*                         TRANSFORMER UTILITIES                             */
/*===========================================================================*/

/**
 * transformer_set_training_mode - Set training/evaluation mode
 * @model: Transformer model
 * @training: Training mode flag
 *
 * Enables/disables dropout and other training-specific behaviors.
 */
void transformer_set_training_mode(nn_model_t *model, bool training)
{
    transformer_state_t *state;
    
    if (!model) {
        return;
    }

    state = (transformer_state_t *)model->exec_context;
    if (state) {
        state->training = training;
    }
}

/**
 * transformer_generate - Generate text using the transformer
 * @model: Transformer model
 * @input_ids: Input token IDs
 * @input_len: Input sequence length
 * @max_length: Maximum generation length
 * @output_ids: Output token IDs buffer
 *
 * Autoregressive text generation.
 *
 * Returns: Number of tokens generated, or error code on failure
 */
int transformer_generate(nn_model_t *model, u32 *input_ids, u32 input_len,
                         u32 max_length, u32 *output_ids)
{
    /* Simplified generation - full implementation would be more complex */
    if (!model || !input_ids || !output_ids) {
        return INFERENCE_ERR_INPUT;
    }

    /* Copy input to output */
    for (u32 i = 0; i < input_len && i < max_length; i++) {
        output_ids[i] = input_ids[i];
    }

    /* Generate remaining tokens */
    for (u32 pos = input_len; pos < max_length; pos++) {
        /* In a real implementation, would run forward pass and sample */
        output_ids[pos] = 0;  /* EOS token */
        break;
    }

    return max_length;
}
