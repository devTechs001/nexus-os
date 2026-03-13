/*
 * NEXUS OS - GPU Scheduler & Command Buffer
 * drivers/gpu/gpu_scheduler.c
 *
 * GPU command scheduling, memory management, command buffer
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "gpu.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GPU SCHEDULER CONFIGURATION                       */
/*===========================================================================*/

#define GPU_SCHED_MAX_QUEUES        32
#define GPU_SCHED_MAX_JOBS          1024
#define GPU_SCHED_MAX_FENCES        2048
#define GPU_SCHED_MAX_BO            512
#define GPU_SCHED_BO_MAX_SIZE       (256 * 1024 * 1024)

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef enum {
    GPU_JOB_TYPE_NONE = 0,
    GPU_JOB_TYPE_DRAW,
    GPU_JOB_TYPE_COMPUTE,
    GPU_JOB_TYPE_COPY,
    GPU_JOB_TYPE_CLEAR,
    GPU_JOB_TYPE_PRESENT,
} gpu_job_type_t;

typedef enum {
    GPU_JOB_STATE_IDLE = 0,
    GPU_JOB_STATE_PENDING,
    GPU_JOB_STATE_RUNNING,
    GPU_JOB_STATE_COMPLETE,
    GPU_JOB_STATE_ERROR,
} gpu_job_state_t;

typedef struct {
    u32 job_id;
    gpu_job_type_t type;
    gpu_job_state_t state;
    u32 priority;
    u64 submit_time;
    u64 start_time;
    u64 end_time;
    u32 context_id;
    u32 ring;
    void *command_buffer;
    u32 cmd_size;
    u32 fence_id;
    s32 result;
    void *user_data;
} gpu_job_t;

typedef struct {
    u32 queue_id;
    char name[32];
    u32 type;                       /* GRAPHICS/COMPUTE/DMA */
    u32 priority;
    gpu_job_t *jobs[GPU_SCHED_MAX_JOBS];
    u32 job_count;
    u32 head;
    u32 tail;
    bool suspended;
} gpu_queue_t;

typedef struct {
    u32 fence_id;
    u32 seqno;
    gpu_job_t *job;
    bool signaled;
    u64 timestamp;
    void (*callback)(void *);
    void *user_data;
} gpu_fence_t;

typedef struct {
    u32 bo_id;
    void *cpu_addr;
    phys_addr_t gpu_addr;
    u32 size;
    u32 flags;
    u32 refcount;
    bool is_mapped;
    bool is_pinned;
} gpu_bo_t;

typedef struct {
    bool initialized;
    gpu_queue_t queues[GPU_SCHED_MAX_QUEUES];
    u32 queue_count;
    gpu_job_t jobs[GPU_SCHED_MAX_JOBS];
    u32 job_count;
    gpu_fence_t fences[GPU_SCHED_MAX_FENCES];
    u32 fence_count;
    gpu_bo_t bos[GPU_SCHED_MAX_BO];
    u32 bo_count;
    u32 current_seqno;
    u32 active_queue;
    bool scheduler_running;
    void *scheduler_thread;
    spinlock_t lock;
} gpu_scheduler_t;

static gpu_scheduler_t g_gpu_sched;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int gpu_sched_init(void)
{
    printk("[GPU_SCHED] Initializing GPU scheduler...\n");
    
    memset(&g_gpu_sched, 0, sizeof(gpu_scheduler_t));
    g_gpu_sched.initialized = true;
    spinlock_init(&g_gpu_sched.lock);
    
    /* Create default queues */
    /* Graphics queue */
    gpu_queue_t *gfx = &g_gpu_sched.queues[g_gpu_sched.queue_count++];
    gfx->queue_id = 1;
    strcpy(gfx->name, "Graphics");
    gfx->type = 1;  /* GRAPHICS */
    gfx->priority = 0;
    
    /* Compute queue */
    gpu_queue_t *compute = &g_gpu_sched.queues[g_gpu_sched.queue_count++];
    compute->queue_id = 2;
    strcpy(compute->name, "Compute");
    compute->type = 2;  /* COMPUTE */
    compute->priority = 1;
    
    /* DMA queue */
    gpu_queue_t *dma = &g_gpu_sched.queues[g_gpu_sched.queue_count++];
    dma->queue_id = 3;
    strcpy(dma->name, "DMA");
    dma->type = 3;  /* DMA */
    dma->priority = 2;
    
    printk("[GPU_SCHED] Created %d queues\n", g_gpu_sched.queue_count);
    return 0;
}

int gpu_sched_shutdown(void)
{
    printk("[GPU_SCHED] Shutting down GPU scheduler...\n");
    
    g_gpu_sched.scheduler_running = false;
    g_gpu_sched.initialized = false;
    
    /* Free all jobs */
    for (u32 i = 0; i < g_gpu_sched.job_count; i++) {
        gpu_job_t *job = &g_gpu_sched.jobs[i];
        if (job->command_buffer) {
            kfree(job->command_buffer);
        }
    }
    
    /* Free all BOs */
    for (u32 i = 0; i < g_gpu_sched.bo_count; i++) {
        gpu_bo_t *bo = &g_gpu_sched.bos[i];
        if (bo->cpu_addr && !bo->is_pinned) {
            kfree(bo->cpu_addr);
        }
    }
    
    return 0;
}

/*===========================================================================*/
/*                         JOB MANAGEMENT                                    */
/*===========================================================================*/

gpu_job_t *gpu_sched_create_job(gpu_job_type_t type, u32 priority)
{
    spinlock_lock(&g_gpu_sched.lock);
    
    if (g_gpu_sched.job_count >= GPU_SCHED_MAX_JOBS) {
        spinlock_unlock(&g_gpu_sched.lock);
        return NULL;
    }
    
    gpu_job_t *job = &g_gpu_sched.jobs[g_gpu_sched.job_count++];
    job->job_id = g_gpu_sched.job_count;
    job->type = type;
    job->state = GPU_JOB_STATE_IDLE;
    job->priority = priority;
    job->submit_time = 0;
    job->start_time = 0;
    job->end_time = 0;
    job->context_id = 0;
    job->ring = 0;
    job->command_buffer = NULL;
    job->cmd_size = 0;
    job->fence_id = 0;
    job->result = 0;
    job->user_data = NULL;
    
    spinlock_unlock(&g_gpu_sched.lock);
    return job;
}

int gpu_sched_submit_job(gpu_job_t *job, u32 queue_id)
{
    if (!job) return -EINVAL;
    
    gpu_queue_t *queue = NULL;
    for (u32 i = 0; i < g_gpu_sched.queue_count; i++) {
        if (g_gpu_sched.queues[i].queue_id == queue_id) {
            queue = &g_gpu_sched.queues[i];
            break;
        }
    }
    
    if (!queue) return -ENOENT;
    if (queue->job_count >= GPU_SCHED_MAX_JOBS) return -ENOMEM;
    
    spinlock_lock(&g_gpu_sched.lock);
    
    /* Create fence */
    u32 fence_id = ++g_gpu_sched.current_seqno;
    gpu_fence_t *fence = &g_gpu_sched.fences[g_gpu_sched.fence_count++];
    fence->fence_id = fence_id;
    fence->seqno = fence_id;
    fence->job = job;
    fence->signaled = false;
    fence->timestamp = 0;
    
    job->fence_id = fence_id;
    job->state = GPU_JOB_STATE_PENDING;
    job->submit_time = ktime_get_us();
    
    /* Add to queue */
    queue->jobs[queue->job_count++] = job;
    
    spinlock_unlock(&g_gpu_sched.lock);
    
    return fence_id;
}

int gpu_sched_wait_job(u32 fence_id, u32 timeout_ms)
{
    gpu_fence_t *fence = NULL;
    for (u32 i = 0; i < g_gpu_sched.fence_count; i++) {
        if (g_gpu_sched.fences[i].fence_id == fence_id) {
            fence = &g_gpu_sched.fences[i];
            break;
        }
    }
    
    if (!fence) return -ENOENT;
    
    u64 start = ktime_get_ms();
    while (!fence->signaled) {
        if (timeout_ms > 0 && (ktime_get_ms() - start) > timeout_ms) {
            return -ETIMEDOUT;
        }
        cpu_relax();
    }
    
    return fence->job->result;
}

/*===========================================================================*/
/*                         COMMAND BUFFER                                    */
/*===========================================================================*/

typedef struct {
    u32 type;
    u32 size;
    union {
        struct {
            u32 vertex_count;
            u32 first_vertex;
            u32 instance_count;
        } draw;
        struct {
            u32 index_count;
            u32 index_type;
            u64 index_buffer;
        } draw_indexed;
        struct {
            u32 workgroup_x;
            u32 workgroup_y;
            u32 workgroup_z;
        } dispatch;
        struct {
            u64 src;
            u64 dst;
            u32 size;
        } copy;
        struct {
            u32 color;
            u32 x;
            u32 y;
            u32 w;
            u32 h;
        } clear;
    };
} gpu_command_t;

void *gpu_cmd_buffer_create(u32 size)
{
    return kmalloc(size);
}

void gpu_cmd_buffer_destroy(void *buffer)
{
    if (buffer) kfree(buffer);
}

int gpu_cmd_begin(void *buffer, u32 *offset)
{
    if (!buffer || !offset) return -EINVAL;
    *offset = 0;
    return 0;
}

int gpu_cmd_draw(void *buffer, u32 *offset, u32 vertex_count, u32 first_vertex)
{
    gpu_command_t *cmd = (gpu_command_t *)((u8 *)buffer + *offset);
    cmd->type = 1;  /* DRAW */
    cmd->size = sizeof(gpu_command_t);
    cmd->draw.vertex_count = vertex_count;
    cmd->draw.first_vertex = first_vertex;
    cmd->draw.instance_count = 1;
    
    *offset += sizeof(gpu_command_t);
    return 0;
}

int gpu_cmd_draw_indexed(void *buffer, u32 *offset, u32 index_count, 
                         u32 index_type, u64 index_buffer)
{
    gpu_command_t *cmd = (gpu_command_t *)((u8 *)buffer + *offset);
    cmd->type = 2;  /* DRAW_INDEXED */
    cmd->size = sizeof(gpu_command_t);
    cmd->draw_indexed.index_count = index_count;
    cmd->draw_indexed.index_type = index_type;
    cmd->draw_indexed.index_buffer = index_buffer;
    
    *offset += sizeof(gpu_command_t);
    return 0;
}

int gpu_cmd_dispatch(void *buffer, u32 *offset, u32 x, u32 y, u32 z)
{
    gpu_command_t *cmd = (gpu_command_t *)((u8 *)buffer + *offset);
    cmd->type = 3;  /* DISPATCH */
    cmd->size = sizeof(gpu_command_t);
    cmd->dispatch.workgroup_x = x;
    cmd->dispatch.workgroup_y = y;
    cmd->dispatch.workgroup_z = z;
    
    *offset += sizeof(gpu_command_t);
    return 0;
}

int gpu_cmd_copy(void *buffer, u32 *offset, u64 src, u64 dst, u32 size)
{
    gpu_command_t *cmd = (gpu_command_t *)((u8 *)buffer + *offset);
    cmd->type = 4;  /* COPY */
    cmd->size = sizeof(gpu_command_t);
    cmd->copy.src = src;
    cmd->copy.dst = dst;
    cmd->copy.size = size;
    
    *offset += sizeof(gpu_command_t);
    return 0;
}

int gpu_cmd_clear(void *buffer, u32 *offset, u32 color, u32 x, u32 y, u32 w, u32 h)
{
    gpu_command_t *cmd = (gpu_command_t *)((u8 *)buffer + *offset);
    cmd->type = 5;  /* CLEAR */
    cmd->size = sizeof(gpu_command_t);
    cmd->clear.color = color;
    cmd->clear.x = x;
    cmd->clear.y = y;
    cmd->clear.w = w;
    cmd->clear.h = h;
    
    *offset += sizeof(gpu_command_t);
    return 0;
}

int gpu_cmd_end(void *buffer, u32 *offset)
{
    gpu_command_t *cmd = (gpu_command_t *)((u8 *)buffer + *offset);
    cmd->type = 0;  /* END */
    cmd->size = 0;
    *offset += sizeof(gpu_command_t);
    return 0;
}

/*===========================================================================*/
/*                         BUFFER OBJECT MANAGEMENT                          */
/*===========================================================================*/

gpu_bo_t *gpu_bo_create(u32 size, u32 flags)
{
    if (size > GPU_SCHED_BO_MAX_SIZE) {
        return NULL;
    }
    
    spinlock_lock(&g_gpu_sched.lock);
    
    if (g_gpu_sched.bo_count >= GPU_SCHED_MAX_BO) {
        spinlock_unlock(&g_gpu_sched.lock);
        return NULL;
    }
    
    gpu_bo_t *bo = &g_gpu_sched.bos[g_gpu_sched.bo_count++];
    bo->bo_id = g_gpu_sched.bo_count;
    bo->size = size;
    bo->flags = flags;
    bo->cpu_addr = kmalloc(size);
    bo->gpu_addr = (phys_addr_t)bo->cpu_addr;  /* Simplified */
    bo->refcount = 1;
    bo->is_mapped = false;
    bo->is_pinned = false;
    
    if (!bo->cpu_addr) {
        g_gpu_sched.bo_count--;
        spinlock_unlock(&g_gpu_sched.lock);
        return NULL;
    }
    
    memset(bo->cpu_addr, 0, size);
    
    spinlock_unlock(&g_gpu_sched.lock);
    return bo;
}

int gpu_bo_destroy(gpu_bo_t *bo)
{
    if (!bo) return -EINVAL;
    
    spinlock_lock(&g_gpu_sched.lock);
    
    bo->refcount--;
    if (bo->refcount > 0) {
        spinlock_unlock(&g_gpu_sched.lock);
        return 0;
    }
    
    if (bo->cpu_addr && !bo->is_pinned) {
        kfree(bo->cpu_addr);
    }
    
    bo->bo_id = 0;
    g_gpu_sched.bo_count--;
    
    spinlock_unlock(&g_gpu_sched.lock);
    return 0;
}

void *gpu_bo_map(gpu_bo_t *bo)
{
    if (!bo) return NULL;
    
    spinlock_lock(&g_gpu_sched.lock);
    
    if (!bo->is_mapped) {
        bo->is_mapped = true;
    }
    
    spinlock_unlock(&g_gpu_sched.lock);
    return bo->cpu_addr;
}

int gpu_bo_unmap(gpu_bo_t *bo)
{
    if (!bo) return -EINVAL;
    
    spinlock_lock(&g_gpu_sched.lock);
    bo->is_mapped = false;
    spinlock_unlock(&g_gpu_sched.lock);
    
    return 0;
}

/*===========================================================================*/
/*                         MEMORY MANAGEMENT                                 */
/*===========================================================================*/

typedef struct {
    u32 start;
    u32 end;
    bool free;
    gpu_bo_t *bo;
} gpu_mem_block_t;

#define GPU_MEM_MAX_BLOCKS 256

typedef struct {
    gpu_mem_block_t blocks[GPU_MEM_MAX_BLOCKS];
    u32 block_count;
    u32 total_size;
    u32 used_size;
} gpu_mem_heap_t;

static gpu_mem_heap_t g_gpu_heap;

int gpu_mem_init(u32 size)
{
    memset(&g_gpu_heap, 0, sizeof(gpu_mem_heap_t));
    g_gpu_heap.total_size = size;
    g_gpu_heap.block_count = 1;
    
    /* Create initial free block */
    g_gpu_heap.blocks[0].start = 0;
    g_gpu_heap.blocks[0].end = size;
    g_gpu_heap.blocks[0].free = true;
    g_gpu_heap.blocks[0].bo = NULL;
    
    return 0;
}

gpu_bo_t *gpu_mem_alloc(u32 size, u32 alignment)
{
    (void)alignment;
    
    for (u32 i = 0; i < g_gpu_heap.block_count; i++) {
        gpu_mem_block_t *block = &g_gpu_heap.blocks[i];
        
        if (block->free && (block->end - block->start) >= size) {
            /* Allocate from this block */
            u32 alloc_start = block->start;
            u32 alloc_end = block->start + size;
            
            block->start = alloc_end;
            if (block->start == block->end) {
                /* Block fully consumed */
                block->free = false;
            }
            
            gpu_bo_t *bo = gpu_bo_create(size, 0);
            if (!bo) return NULL;
            
            bo->gpu_addr = alloc_start;
            g_gpu_heap.used_size += size;
            
            return bo;
        }
    }
    
    return NULL;  /* Out of memory */
}

int gpu_mem_free(gpu_bo_t *bo)
{
    if (!bo) return -EINVAL;
    
    /* Find and free the block */
    for (u32 i = 0; i < g_gpu_heap.block_count; i++) {
        gpu_mem_block_t *block = &g_gpu_heap.blocks[i];
        
        if (!block->free && block->start == bo->gpu_addr) {
            block->free = true;
            g_gpu_heap.used_size -= bo->size;
            
            /* Coalesce with next block if free */
            if (i + 1 < g_gpu_heap.block_count && 
                g_gpu_heap.blocks[i + 1].free) {
                block->end = g_gpu_heap.blocks[i + 1].end;
                /* Shift blocks */
                for (u32 j = i + 1; j < g_gpu_heap.block_count - 1; j++) {
                    g_gpu_heap.blocks[j] = g_gpu_heap.blocks[j + 1];
                }
                g_gpu_heap.block_count--;
            }
            
            return gpu_bo_destroy(bo);
        }
    }
    
    return -ENOENT;
}

/*===========================================================================*/
/*                         SCHEDULER THREAD                                  */
/*===========================================================================*/

static void gpu_sched_run(void *arg)
{
    (void)arg;
    
    while (g_gpu_sched.scheduler_running) {
        /* Process each queue */
        for (u32 q = 0; q < g_gpu_sched.queue_count; q++) {
            gpu_queue_t *queue = &g_gpu_sched.queues[q];
            
            if (queue->suspended || queue->job_count == 0) {
                continue;
            }
            
            /* Get next job */
            gpu_job_t *job = queue->jobs[queue->head];
            
            if (job->state == GPU_JOB_STATE_PENDING) {
                /* Execute job */
                job->state = GPU_JOB_STATE_RUNNING;
                job->start_time = ktime_get_us();
                
                /* In real implementation, would submit to hardware */
                /* Simulate execution */
                job->state = GPU_JOB_STATE_COMPLETE;
                job->end_time = ktime_get_us();
                job->result = 0;
                
                /* Signal fence */
                gpu_fence_t *fence = NULL;
                for (u32 i = 0; i < g_gpu_sched.fence_count; i++) {
                    if (g_gpu_sched.fences[i].fence_id == job->fence_id) {
                        fence = &g_gpu_sched.fences[i];
                        break;
                    }
                }
                
                if (fence) {
                    fence->signaled = true;
                    fence->timestamp = job->end_time;
                    if (fence->callback) {
                        fence->callback(fence->user_data);
                    }
                }
                
                /* Remove from queue */
                queue->head = (queue->head + 1) % GPU_SCHED_MAX_JOBS;
                queue->job_count--;
            }
        }
        
        /* Check hotplug */
        if (g_gpu_sched.initialized) {
            check_hotplug();
        }
        
        cpu_relax();
    }
}

int gpu_sched_start(void)
{
    if (g_gpu_sched.scheduler_running) {
        return -EBUSY;
    }
    
    g_gpu_sched.scheduler_running = true;
    
    /* In real implementation, would create kernel thread */
    printk("[GPU_SCHED] Scheduler started\n");
    return 0;
}

int gpu_sched_stop(void)
{
    g_gpu_sched.scheduler_running = false;
    printk("[GPU_SCHED] Scheduler stopped\n");
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

gpu_scheduler_t *gpu_sched_get(void)
{
    return &g_gpu_sched;
}

int gpu_sched_get_queue_count(void)
{
    return g_gpu_sched.queue_count;
}

int gpu_sched_get_pending_jobs(void)
{
    u32 count = 0;
    for (u32 i = 0; i < g_gpu_sched.job_count; i++) {
        if (g_gpu_sched.jobs[i].state == GPU_JOB_STATE_PENDING ||
            g_gpu_sched.jobs[i].state == GPU_JOB_STATE_RUNNING) {
            count++;
        }
    }
    return count;
}
