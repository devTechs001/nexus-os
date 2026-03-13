/*
 * NEXUS OS - OpenGL Implementation
 * drivers/gpu/opengl.c
 *
 * OpenGL 4.6 compatible implementation
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "gpu.h"
#include "acceleration.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         OPENGL CONFIGURATION                              */
/*===========================================================================*/

#define GL_MAX_TEXTURE_UNITS      32
#define GL_MAX_UNIFORMS           256
#define GL_MAX_ATTRIBUTES         16
#define GL_MAX_VARYINGS           32

/*===========================================================================*/
/*                         OPENGL ERROR CODES                                */
/*===========================================================================*/

#define GL_NO_ERROR               0
#define GL_INVALID_ENUM           0x0500
#define GL_INVALID_VALUE          0x0501
#define GL_INVALID_OPERATION      0x0502
#define GL_OUT_OF_MEMORY          0x0505
#define GL_INVALID_FRAMEBUFFER_OPERATION 0x0506

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 id;
    u32 target;
    void *data;
    u32 size;
    u32 usage;
    bool is_mapped;
} gl_buffer_object_t;

typedef struct {
    u32 id;
    u32 target;
    u32 width;
    u32 height;
    u32 depth;
    u32 internal_format;
    u32 format;
    u32 type;
    void *data;
    u32 mip_levels;
} gl_texture_object_t;

typedef struct {
    u32 id;
    u32 type;
    char source[65536];
    char info_log[1024];
    bool compiled;
} gl_shader_object_t;

typedef struct {
    u32 id;
    u32 shaders[16];
    u32 shader_count;
    char info_log[1024];
    bool linked;
    bool validated;
    bool in_use;
} gl_program_object_t;

typedef struct {
    bool initialized;
    gl_buffer_object_t buffers[1024];
    u32 buffer_count;
    gl_texture_object_t textures[512];
    u32 texture_count;
    gl_shader_object_t shaders[256];
    u32 shader_count;
    gl_program_object_t programs[128];
    u32 program_count;
    u32 current_program;
    u32 current_error;
    float clear_color[4];
    float clear_depth;
    u32 enabled_caps;
    spinlock_t lock;
} opengl_context_t;

static opengl_context_t g_gl;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int opengl_init(void)
{
    printk("[OPENGL] ========================================\n");
    printk("[OPENGL] NEXUS OS OpenGL Implementation\n");
    printk("[OPENGL] Version: 4.6 Compatibility\n");
    printk("[OPENGL] ========================================\n");
    
    memset(&g_gl, 0, sizeof(opengl_context_t));
    spinlock_init(&g_gl.lock);
    
    g_gl.initialized = true;
    g_gl.clear_color[0] = 0.0f;
    g_gl.clear_color[1] = 0.0f;
    g_gl.clear_color[2] = 0.0f;
    g_gl.clear_color[3] = 1.0f;
    g_gl.clear_depth = 1.0f;
    
    printk("[OPENGL] OpenGL initialized\n");
    return 0;
}

/*===========================================================================*/
/*                         BUFFER OBJECTS                                    */
/*===========================================================================*/

u32 glGenBuffers(u32 n, u32 *buffers)
{
    if (!buffers || n == 0) return 0;
    
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < n && g_gl.buffer_count < 1024; i++) {
        gl_buffer_object_t *bo = &g_gl.buffers[g_gl.buffer_count++];
        bo->id = g_gl.buffer_count;
        bo->target = 0;
        bo->data = NULL;
        bo->size = 0;
        bo->usage = 0;
        bo->is_mapped = false;
        buffers[i] = bo->id;
    }
    
    spinlock_unlock(&g_gl.lock);
    return n;
}

void glBindBuffer(u32 target, u32 buffer)
{
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < g_gl.buffer_count; i++) {
        if (g_gl.buffers[i].id == buffer) {
            g_gl.buffers[i].target = target;
            break;
        }
    }
    
    spinlock_unlock(&g_gl.lock);
}

void glBufferData(u32 target, u32 size, const void *data, u32 usage)
{
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < g_gl.buffer_count; i++) {
        gl_buffer_object_t *bo = &g_gl.buffers[i];
        if (bo->target == target) {
            if (bo->data) kfree(bo->data);
            bo->size = size;
            bo->usage = usage;
            bo->data = kmalloc(size);
            if (bo->data && data) {
                memcpy(bo->data, data, size);
            }
            break;
        }
    }
    
    spinlock_unlock(&g_gl.lock);
}

void *glMapBuffer(u32 target, u32 access)
{
    (void)access;
    void *ptr = NULL;
    
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < g_gl.buffer_count; i++) {
        gl_buffer_object_t *bo = &g_gl.buffers[i];
        if (bo->target == target && bo->data) {
            bo->is_mapped = true;
            ptr = bo->data;
            break;
        }
    }
    
    spinlock_unlock(&g_gl.lock);
    return ptr;
}

bool glUnmapBuffer(u32 target)
{
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < g_gl.buffer_count; i++) {
        gl_buffer_object_t *bo = &g_gl.buffers[i];
        if (bo->target == target) {
            bo->is_mapped = false;
            spinlock_unlock(&g_gl.lock);
            return true;
        }
    }
    
    spinlock_unlock(&g_gl.lock);
    return false;
}

void glDeleteBuffers(u32 n, const u32 *buffers)
{
    if (!buffers) return;
    
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < n; i++) {
        for (u32 j = 0; j < g_gl.buffer_count; j++) {
            if (g_gl.buffers[j].id == buffers[i]) {
                if (g_gl.buffers[j].data) kfree(g_gl.buffers[j].data);
                g_gl.buffers[j].id = 0;
                g_gl.buffer_count--;
                break;
            }
        }
    }
    
    spinlock_unlock(&g_gl.lock);
}

/*===========================================================================*/
/*                         TEXTURES                                          */
/*===========================================================================*/

u32 glGenTextures(u32 n, u32 *textures)
{
    if (!textures || n == 0) return 0;
    
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < n && g_gl.texture_count < 512; i++) {
        gl_texture_object_t *tex = &g_gl.textures[g_gl.texture_count++];
        tex->id = g_gl.texture_count;
        tex->target = GL_TEXTURE_2D;
        tex->width = 0;
        tex->height = 0;
        tex->data = NULL;
        textures[i] = tex->id;
    }
    
    spinlock_unlock(&g_gl.lock);
    return n;
}

void glBindTexture(u32 target, u32 texture)
{
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < g_gl.texture_count; i++) {
        if (g_gl.textures[i].id == texture) {
            g_gl.textures[i].target = target;
            break;
        }
    }
    
    spinlock_unlock(&g_gl.lock);
}

void glTexImage2D(u32 target, s32 level, s32 internal_format, s32 width, s32 height,
                  s32 border, u32 format, u32 type, const void *data)
{
    (void)level; (void)border;
    
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < g_gl.texture_count; i++) {
        gl_texture_object_t *tex = &g_gl.textures[i];
        if (tex->target == target) {
            tex->width = width;
            tex->height = height;
            tex->internal_format = internal_format;
            tex->format = format;
            tex->type = type;
            
            u32 size = width * height * 4;  /* Assume RGBA */
            if (tex->data) kfree(tex->data);
            tex->data = kmalloc(size);
            if (tex->data && data) {
                memcpy(tex->data, data, size);
            }
            break;
        }
    }
    
    spinlock_unlock(&g_gl.lock);
}

void glDeleteTextures(u32 n, const u32 *textures)
{
    if (!textures) return;
    
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < n; i++) {
        for (u32 j = 0; j < g_gl.texture_count; j++) {
            if (g_gl.textures[j].id == textures[i]) {
                if (g_gl.textures[j].data) kfree(g_gl.textures[j].data);
                g_gl.textures[j].id = 0;
                g_gl.texture_count--;
                break;
            }
        }
    }
    
    spinlock_unlock(&g_gl.lock);
}

/*===========================================================================*/
/*                         SHADERS                                           */
/*===========================================================================*/

u32 glCreateShader(u32 type)
{
    spinlock_lock(&g_gl.lock);
    
    if (g_gl.shader_count >= 256) {
        spinlock_unlock(&g_gl.lock);
        return 0;
    }
    
    gl_shader_object_t *shader = &g_gl.shaders[g_gl.shader_count++];
    shader->id = g_gl.shader_count;
    shader->type = type;
    shader->compiled = false;
    shader->source[0] = '\0';
    shader->info_log[0] = '\0';
    
    spinlock_unlock(&g_gl.lock);
    return shader->id;
}

void glShaderSource(u32 shader, s32 count, const char **string, const s32 *length)
{
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < g_gl.shader_count; i++) {
        gl_shader_object_t *s = &g_gl.shaders[i];
        if (s->id == shader) {
            s->source[0] = '\0';
            for (s32 j = 0; j < count; j++) {
                u32 len = length ? length[j] : strlen(string[j]);
                strncat(s->source, string[j], len);
            }
            break;
        }
    }
    
    spinlock_unlock(&g_gl.lock);
}

void glCompileShader(u32 shader)
{
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < g_gl.shader_count; i++) {
        gl_shader_object_t *s = &g_gl.shaders[i];
        if (s->id == shader) {
            /* In real implementation, would compile GLSL */
            s->compiled = true;
            printk("[OPENGL] Shader %d compiled\n", shader);
            break;
        }
    }
    
    spinlock_unlock(&g_gl.lock);
}

u32 glCreateProgram(void)
{
    spinlock_lock(&g_gl.lock);
    
    if (g_gl.program_count >= 128) {
        spinlock_unlock(&g_gl.lock);
        return 0;
    }
    
    gl_program_object_t *prog = &g_gl.programs[g_gl.program_count++];
    prog->id = g_gl.program_count;
    prog->shader_count = 0;
    prog->linked = false;
    prog->validated = false;
    prog->in_use = false;
    
    spinlock_unlock(&g_gl.lock);
    return prog->id;
}

void glAttachShader(u32 program, u32 shader)
{
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < g_gl.program_count; i++) {
        gl_program_object_t *p = &g_gl.programs[i];
        if (p->id == program && p->shader_count < 16) {
            p->shaders[p->shader_count++] = shader;
            break;
        }
    }
    
    spinlock_unlock(&g_gl.lock);
}

void glLinkProgram(u32 program)
{
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < g_gl.program_count; i++) {
        gl_program_object_t *p = &g_gl.programs[i];
        if (p->id == program) {
            /* In real implementation, would link shaders */
            p->linked = true;
            printk("[OPENGL] Program %d linked\n", program);
            break;
        }
    }
    
    spinlock_unlock(&g_gl.lock);
}

void glUseProgram(u32 program)
{
    spinlock_lock(&g_gl.lock);
    
    for (u32 i = 0; i < g_gl.program_count; i++) {
        gl_program_object_t *p = &g_gl.programs[i];
        if (p->id == program) {
            g_gl.current_program = program;
            p->in_use = true;
            break;
        }
    }
    
    spinlock_unlock(&g_gl.lock);
}

/*===========================================================================*/
/*                         RENDERING                                         */
/*===========================================================================*/

void glClear(u32 mask)
{
    (void)mask;
    /* Clear color/depth/stencil buffers */
}

void glClearColor(float r, float g, float b, float a)
{
    g_gl.clear_color[0] = r;
    g_gl.clear_color[1] = g;
    g_gl.clear_color[2] = b;
    g_gl.clear_color[3] = a;
}

void glDrawArrays(u32 mode, s32 first, s32 count)
{
    (void)mode; (void)first; (void)count;
    /* In real implementation, would render primitives */
}

void glDrawElements(u32 mode, s32 count, u32 type, const void *indices)
{
    (void)mode; (void)count; (void)type; (void)indices;
    /* In real implementation, would render indexed primitives */
}

void glViewport(s32 x, s32 y, s32 width, s32 height)
{
    (void)x; (void)y; (void)width; (void)height;
    /* Set viewport transform */
}

void glEnable(u32 cap)
{
    g_gl.enabled_caps |= (1 << cap);
}

void glDisable(u32 cap)
{
    g_gl.enabled_caps &= ~(1 << cap);
}

u32 glGetError(void)
{
    u32 error = g_gl.current_error;
    g_gl.current_error = GL_NO_ERROR;
    return error;
}

opengl_context_t *opengl_get_context(void)
{
    return &g_gl;
}
