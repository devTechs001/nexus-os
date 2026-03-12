/*
 * NEXUS OS - IoT Framework
 * iot/edge_computing/edge_runtime.c
 *
 * Edge Computing Runtime
 *
 * This module implements the edge computing runtime for executing
 * functions and workflows at the network edge, close to IoT devices.
 */

#include "../iot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================*/
/*                         EDGE RUNTIME STATE                                */
/*===========================================================================*/

static struct {
    bool initialized;
    edge_node_t *nodes;             /* Linked list of edge nodes */
    u32 num_nodes;
    u32 next_node_id;
    
    edge_function_t *functions;     /* Registered functions */
    u32 num_functions;
    u32 next_function_id;
    
    edge_workflow_t *workflows;     /* Registered workflows */
    u32 num_workflows;
    u32 next_workflow_id;
    
    /* Execution statistics */
    u64 total_executions;
    u64 successful_executions;
    u64 failed_executions;
    u64 total_execution_time_us;
    
    /* Runtime configuration */
    u32 max_concurrent_executions;
    u32 current_executions;
    u32 default_timeout_ms;
    
    /* Synchronization */
    spinlock_t lock;
} g_edge_runtime = {
    .initialized = false,
    .lock = { .lock = 0 }
};

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static u32 generate_node_id(void);
static u32 generate_function_id(void);
static u32 generate_workflow_id(void);
static int execute_function_internal(edge_function_t *func,
                                      const void *input, size_t input_len,
                                      void *output, size_t *output_len);
static int execute_workflow_internal(edge_workflow_t *workflow,
                                      const void *input, size_t input_len,
                                      void *output, size_t *output_len);

/*===========================================================================*/
/*                         RUNTIME INITIALIZATION                            */
/*===========================================================================*/

/**
 * edge_runtime_init - Initialize the edge computing runtime
 *
 * Returns: 0 on success, error code on failure
 */
int edge_runtime_init(void)
{
    if (g_edge_runtime.initialized) {
        pr_debug("Edge runtime already initialized\n");
        return 0;
    }

    spin_lock(&g_edge_runtime.lock);

    g_edge_runtime.next_node_id = 1;
    g_edge_runtime.next_function_id = 1;
    g_edge_runtime.next_workflow_id = 1;
    g_edge_runtime.num_nodes = 0;
    g_edge_runtime.num_functions = 0;
    g_edge_runtime.num_workflows = 0;
    
    g_edge_runtime.total_executions = 0;
    g_edge_runtime.successful_executions = 0;
    g_edge_runtime.failed_executions = 0;
    g_edge_runtime.total_execution_time_us = 0;
    
    g_edge_runtime.max_concurrent_executions = 100;
    g_edge_runtime.current_executions = 0;
    g_edge_runtime.default_timeout_ms = 5000;

    g_edge_runtime.initialized = true;

    pr_info("Edge computing runtime initialized\n");

    spin_unlock(&g_edge_runtime.lock);

    return 0;
}

/**
 * edge_runtime_shutdown - Shutdown the edge runtime
 *
 * Releases all edge runtime resources.
 */
void edge_runtime_shutdown(void)
{
    if (!g_edge_runtime.initialized) {
        return;
    }

    spin_lock(&g_edge_runtime.lock);

    /* Destroy all nodes */
    edge_node_t *node = g_edge_runtime.nodes;
    while (node) {
        edge_node_t *next = node->next;
        edge_node_destroy(node);
        node = next;
    }
    g_edge_runtime.nodes = NULL;

    /* Destroy all functions */
    edge_function_t *func = g_edge_runtime.functions;
    while (func) {
        edge_function_t *next = func->next;
        edge_function_destroy(func);
        func = next;
    }
    g_edge_runtime.functions = NULL;

    /* Destroy all workflows */
    edge_workflow_t *workflow = g_edge_runtime.workflows;
    while (workflow) {
        edge_workflow_t *next = workflow->next;
        edge_workflow_destroy(workflow);
        workflow = next;
    }
    g_edge_runtime.workflows = NULL;

    g_edge_runtime.initialized = false;

    pr_info("Edge runtime shutdown complete\n");

    spin_unlock(&g_edge_runtime.lock);
}

/*===========================================================================*/
/*                         EDGE NODE MANAGEMENT                              */
/*===========================================================================*/

/**
 * edge_node_create - Create an edge node
 * @name: Node name
 * @location: Physical location
 *
 * Returns: Pointer to created node, or NULL on failure
 */
edge_node_t *edge_node_create(const char *name, const char *location)
{
    edge_node_t *node;

    if (!name) {
        pr_err("Edge node name required\n");
        return NULL;
    }

    node = (edge_node_t *)kzalloc(sizeof(edge_node_t));
    if (!node) {
        pr_err("Failed to allocate edge node\n");
        return NULL;
    }

    node->node_id = generate_node_id();
    strncpy(node->name, name, sizeof(node->name) - 1);
    
    if (location) {
        strncpy(node->location, location, sizeof(node->location) - 1);
    }

    /* Default resources */
    node->cpu_cores = 4;
    node->memory_total = GB(8);
    node->memory_used = 0;
    node->storage_total = GB(64);
    node->storage_used = 0;

    node->status = DEVICE_STATUS_ONLINE;
    node->cpu_usage = 0;
    node->memory_usage = 0;
    node->temperature = 35;

    node->functions = NULL;
    node->workflows = NULL;
    node->num_functions = 0;
    node->num_workflows = 0;

    node->bandwidth_up = 100000;  /* 100 Mbps */
    node->bandwidth_down = 100000;

    node->next = NULL;

    pr_debug("Edge node created: %s (ID: %u, Location: %s)\n",
             name, node->node_id, location ? location : "unknown");

    return node;
}

/**
 * edge_node_destroy - Destroy an edge node
 * @node: Node to destroy
 */
void edge_node_destroy(edge_node_t *node)
{
    if (!node) {
        return;
    }

    /* Unregister from runtime */
    if (g_edge_runtime.initialized) {
        spin_lock(&g_edge_runtime.lock);
        
        edge_node_t *prev = NULL;
        edge_node_t *curr = g_edge_runtime.nodes;
        while (curr) {
            if (curr == node) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    g_edge_runtime.nodes = curr->next;
                }
                g_edge_runtime.num_nodes--;
                break;
            }
            prev = curr;
            curr = curr->next;
        }
        
        spin_unlock(&g_edge_runtime.lock);
    }

    /* Destroy deployed functions */
    edge_function_t *func = node->functions;
    while (func) {
        edge_function_t *next = func->next;
        edge_function_destroy(func);
        func = next;
    }

    /* Destroy workflows */
    edge_workflow_t *workflow = node->workflows;
    while (workflow) {
        edge_workflow_t *next = workflow->next;
        edge_workflow_destroy(workflow);
        workflow = next;
    }

    kfree(node);

    pr_debug("Edge node destroyed: %u\n", node->node_id);
}

/**
 * edge_node_register - Register an edge node
 * @node: Node to register
 *
 * Returns: 0 on success, error code on failure
 */
int edge_node_register(edge_node_t *node)
{
    if (!node) {
        return -1;
    }

    if (!g_edge_runtime.initialized) {
        pr_err("Edge runtime not initialized\n");
        return -1;
    }

    spin_lock(&g_edge_runtime.lock);

    /* Add to list */
    node->next = g_edge_runtime.nodes;
    g_edge_runtime.nodes = node;
    g_edge_runtime.num_nodes++;

    pr_info("Edge node registered: %s (ID: %u)\n", node->name, node->node_id);

    spin_unlock(&g_edge_runtime.lock);

    return 0;
}

/**
 * edge_node_find - Find an edge node by ID
 * @node_id: Node ID
 *
 * Returns: Pointer to node, or NULL if not found
 */
edge_node_t *edge_node_find(u32 node_id)
{
    if (!g_edge_runtime.initialized) {
        return NULL;
    }

    spin_lock(&g_edge_runtime.lock);

    edge_node_t *node = g_edge_runtime.nodes;
    while (node) {
        if (node->node_id == node_id) {
            spin_unlock(&g_edge_runtime.lock);
            return node;
        }
        node = node->next;
    }

    spin_unlock(&g_edge_runtime.lock);
    return NULL;
}

/**
 * edge_node_select - Select best edge node for execution
 * @requirements: Resource requirements
 *
 * Selects the best edge node based on resource availability and load.
 *
 * Returns: Pointer to selected node, or NULL if none available
 */
edge_node_t *edge_node_select(u32 memory_required, u32 cpu_required)
{
    edge_node_t *best_node = NULL;
    u32 best_score = 0;

    if (!g_edge_runtime.initialized) {
        return NULL;
    }

    spin_lock(&g_edge_runtime.lock);

    edge_node_t *node = g_edge_runtime.nodes;
    while (node) {
        if (node->status != DEVICE_STATUS_ONLINE) {
            node = node->next;
            continue;
        }

        /* Check resource availability */
        u64 available_memory = node->memory_total - node->memory_used;
        if (available_memory < memory_required) {
            node = node->next;
            continue;
        }

        /* Calculate score (higher is better) */
        u32 score = 100 - node->cpu_usage;
        score += (available_memory > MB(512)) ? 20 : 10;
        score += (node->bandwidth_up > 50000) ? 10 : 0;

        if (score > best_score) {
            best_score = score;
            best_node = node;
        }

        node = node->next;
    }

    spin_unlock(&g_edge_runtime.lock);

    if (best_node) {
        pr_debug("Selected edge node: %s (score: %u)\n",
                 best_node->name, best_score);
    }

    return best_node;
}

/*===========================================================================*/
/*                         EDGE FUNCTION MANAGEMENT                          */
/*===========================================================================*/

/**
 * edge_function_create - Create an edge function
 * @name: Function name
 * @runtime: Runtime environment (e.g., "javascript", "python", "wasm")
 * @code: Function code
 *
 * Returns: Pointer to created function, or NULL on failure
 */
edge_function_t *edge_function_create(const char *name, const char *runtime,
                                       const char *code)
{
    edge_function_t *func;
    size_t code_len;

    if (!name) {
        pr_err("Function name required\n");
        return NULL;
    }

    func = (edge_function_t *)kzalloc(sizeof(edge_function_t));
    if (!func) {
        pr_err("Failed to allocate edge function\n");
        return NULL;
    }

    func->function_id = generate_function_id();
    strncpy(func->name, name, sizeof(func->name) - 1);
    
    if (runtime) {
        strncpy(func->runtime, runtime, sizeof(func->runtime) - 1);
    } else {
        strncpy(func->runtime, "javascript", sizeof(func->runtime) - 1);
    }

    if (code) {
        code_len = strlen(code);
        func->code = (char *)kmalloc(code_len + 1);
        if (func->code) {
            memcpy(func->code, code, code_len + 1);
            func->code_size = code_len;
        }
    }

    func->timeout_ms = g_edge_runtime.default_timeout_ms;
    func->memory_limit = MB(128);
    func->max_instances = 10;
    func->enabled = true;
    func->execution_count = 0;
    func->last_execution = 0;
    func->total_execution_time = 0;

    func->next = NULL;

    pr_debug("Edge function created: %s (ID: %u, Runtime: %s)\n",
             name, func->function_id, func->runtime);

    return func;
}

/**
 * edge_function_destroy - Destroy an edge function
 * @func: Function to destroy
 */
void edge_function_destroy(edge_function_t *func)
{
    if (!func) {
        return;
    }

    /* Unregister from runtime */
    if (g_edge_runtime.initialized) {
        spin_lock(&g_edge_runtime.lock);
        
        edge_function_t *prev = NULL;
        edge_function_t *curr = g_edge_runtime.functions;
        while (curr) {
            if (curr == func) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    g_edge_runtime.functions = curr->next;
                }
                g_edge_runtime.num_functions--;
                break;
            }
            prev = curr;
            curr = curr->next;
        }
        
        spin_unlock(&g_edge_runtime.lock);
    }

    /* Free code */
    if (func->code) {
        kfree(func->code);
    }

    /* Free triggers */
    if (func->triggers) {
        for (u32 i = 0; i < func->num_triggers; i++) {
            kfree(func->triggers[i]);
        }
        kfree(func->triggers);
    }

    kfree(func);

    pr_debug("Edge function destroyed: %u\n", func->function_id);
}

/**
 * edge_function_register - Register an edge function
 * @func: Function to register
 *
 * Returns: 0 on success, error code on failure
 */
int edge_function_register(edge_function_t *func)
{
    if (!func) {
        return -1;
    }

    if (!g_edge_runtime.initialized) {
        pr_err("Edge runtime not initialized\n");
        return -1;
    }

    spin_lock(&g_edge_runtime.lock);

    /* Add to list */
    func->next = g_edge_runtime.functions;
    g_edge_runtime.functions = func;
    g_edge_runtime.num_functions++;

    pr_info("Edge function registered: %s (ID: %u)\n",
            func->name, func->function_id);

    spin_unlock(&g_edge_runtime.lock);

    return 0;
}

/**
 * edge_function_deploy - Deploy a function to an edge node
 * @node: Target node
 * @func: Function to deploy
 *
 * Returns: 0 on success, error code on failure
 */
int edge_function_deploy(edge_node_t *node, edge_function_t *func)
{
    if (!node || !func) {
        return -1;
    }

    spin_lock(&g_edge_runtime.lock);

    /* Check if already deployed */
    edge_function_t *existing = node->functions;
    while (existing) {
        if (existing->function_id == func->function_id) {
            pr_debug("Function %u already deployed on node %u\n",
                     func->function_id, node->node_id);
            spin_unlock(&g_edge_runtime.lock);
            return 0;
        }
        existing = existing->next;
    }

    /* Add to node's function list */
    func->next = node->functions;
    node->functions = func;
    node->num_functions++;

    /* Update node resource usage */
    node->memory_used += func->code_size + KB(64);  /* Code + runtime overhead */

    pr_info("Function %s deployed to node %s\n", func->name, node->name);

    spin_unlock(&g_edge_runtime.lock);

    return 0;
}

/**
 * edge_function_execute - Execute an edge function
 * @func: Function to execute
 * @input: Input data
 * @input_len: Input length
 * @output: Output buffer
 * @output_len: Output length (input/output)
 *
 * Executes the function with the given input and produces output.
 *
 * Returns: 0 on success, error code on failure
 */
int edge_function_execute(edge_function_t *func, const void *input,
                          size_t input_len, void *output, size_t *output_len)
{
    u64 start_time;
    u64 elapsed_time;
    int ret;

    if (!func || !output || !output_len) {
        return -1;
    }

    if (!func->enabled) {
        pr_err("Function %u is disabled\n", func->function_id);
        return -1;
    }

    spin_lock(&g_edge_runtime.lock);

    /* Check concurrent execution limit */
    if (g_edge_runtime.current_executions >= g_edge_runtime.max_concurrent_executions) {
        pr_err("Maximum concurrent executions reached\n");
        spin_unlock(&g_edge_runtime.lock);
        return -1;
    }

    g_edge_runtime.current_executions++;

    spin_unlock(&g_edge_runtime.lock);

    /* Record start time */
    start_time = get_time_ms() * 1000;  /* Convert to microseconds */

    /* Execute function */
    ret = execute_function_internal(func, input, input_len, output, output_len);

    /* Record elapsed time */
    elapsed_time = get_time_ms() * 1000 - start_time;

    spin_lock(&g_edge_runtime.lock);

    g_edge_runtime.current_executions--;
    g_edge_runtime.total_executions++;

    if (ret == 0) {
        g_edge_runtime.successful_executions++;
        func->execution_count++;
        func->last_execution = iot_get_timestamp();
        func->total_execution_time += elapsed_time;
    } else {
        g_edge_runtime.failed_executions++;
    }

    g_edge_runtime.total_execution_time_us += elapsed_time;

    spin_unlock(&g_edge_runtime.lock);

    pr_debug("Function %s executed in %llu us (result: %s)\n",
             func->name, (unsigned long long)elapsed_time,
             ret == 0 ? "success" : "failure");

    return ret;
}

/**
 * execute_function_internal - Internal function execution
 * @func: Function to execute
 * @input: Input data
 * @input_len: Input length
 * @output: Output buffer
 * @output_len: Output length
 *
 * This is a simplified implementation. A real implementation would:
 * - Create a sandboxed execution environment
 * - Load the appropriate runtime (JS engine, Python interpreter, WASM runtime)
 * - Execute the function code
 * - Capture and return the output
 *
 * Returns: 0 on success, error code on failure
 */
static int execute_function_internal(edge_function_t *func,
                                      const void *input, size_t input_len,
                                      void *output, size_t *output_len)
{
    /* Simplified implementation - just echo input as output */
    /* In a real implementation, this would execute the actual function code */

    if (!func->code) {
        pr_err("Function %u has no code\n", func->function_id);
        return -1;
    }

    /* For demonstration, copy input to output */
    if (input && input_len > 0 && *output_len >= input_len) {
        memcpy(output, input, input_len);
        *output_len = input_len;
    } else {
        /* Return a simple response */
        const char *response = "{\"status\":\"executed\",\"function\":\"";
        size_t response_len = strlen(response) + strlen(func->name) + 2;
        
        if (*output_len >= response_len) {
            snprintf((char *)output, *output_len, "%s%s\"}", response, func->name);
            *output_len = strlen((char *)output);
        }
    }

    return 0;
}

/*===========================================================================*/
/*                         EDGE WORKFLOW MANAGEMENT                          */
/*===========================================================================*/

/**
 * edge_workflow_create - Create an edge workflow
 * @name: Workflow name
 *
 * Returns: Pointer to created workflow, or NULL on failure
 */
edge_workflow_t *edge_workflow_create(const char *name)
{
    edge_workflow_t *workflow;

    if (!name) {
        pr_err("Workflow name required\n");
        return NULL;
    }

    workflow = (edge_workflow_t *)kzalloc(sizeof(edge_workflow_t));
    if (!workflow) {
        pr_err("Failed to allocate edge workflow\n");
        return NULL;
    }

    workflow->workflow_id = generate_workflow_id();
    strncpy(workflow->name, name, sizeof(workflow->name) - 1);

    workflow->enabled = true;
    workflow->execution_count = 0;
    workflow->num_steps = 0;
    workflow->step_function_ids = NULL;
    workflow->step_conditions = NULL;

    workflow->next = NULL;

    pr_debug("Edge workflow created: %s (ID: %u)\n",
             name, workflow->workflow_id);

    return workflow;
}

/**
 * edge_workflow_destroy - Destroy an edge workflow
 * @workflow: Workflow to destroy
 */
void edge_workflow_destroy(edge_workflow_t *workflow)
{
    if (!workflow) {
        return;
    }

    /* Unregister from runtime */
    if (g_edge_runtime.initialized) {
        spin_lock(&g_edge_runtime.lock);
        
        edge_workflow_t *prev = NULL;
        edge_workflow_t *curr = g_edge_runtime.workflows;
        while (curr) {
            if (curr == workflow) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    g_edge_runtime.workflows = curr->next;
                }
                g_edge_runtime.num_workflows--;
                break;
            }
            prev = curr;
            curr = curr->next;
        }
        
        spin_unlock(&g_edge_runtime.lock);
    }

    /* Free step data */
    if (workflow->step_function_ids) {
        kfree(workflow->step_function_ids);
    }
    if (workflow->step_conditions) {
        for (u32 i = 0; i < workflow->num_steps; i++) {
            kfree(workflow->step_conditions[i]);
        }
        kfree(workflow->step_conditions);
    }

    kfree(workflow);

    pr_debug("Edge workflow destroyed: %u\n", workflow->workflow_id);
}

/**
 * edge_workflow_add_step - Add a step to a workflow
 * @workflow: Workflow
 * @function_id: Function ID for this step
 * @condition: Optional condition for step execution
 *
 * Returns: 0 on success, error code on failure
 */
int edge_workflow_add_step(edge_workflow_t *workflow, u32 function_id,
                           const char *condition)
{
    u32 *new_ids;
    char **new_conditions;

    if (!workflow) {
        return -1;
    }

    spin_lock(&g_edge_runtime.lock);

    /* Reallocate function IDs array */
    new_ids = (u32 *)krealloc(workflow->step_function_ids,
                               (workflow->num_steps + 1) * sizeof(u32));
    if (!new_ids) {
        spin_unlock(&g_edge_runtime.lock);
        return -1;
    }
    workflow->step_function_ids = new_ids;

    /* Reallocate conditions array */
    new_conditions = (char **)krealloc(workflow->step_conditions,
                                        (workflow->num_steps + 1) * sizeof(char *));
    if (!new_conditions) {
        spin_unlock(&g_edge_runtime.lock);
        return -1;
    }
    workflow->step_conditions = new_conditions;

    /* Add step */
    workflow->step_function_ids[workflow->num_steps] = function_id;
    workflow->step_conditions[workflow->num_steps] = condition ? kstrdup(condition) : NULL;
    workflow->num_steps++;

    pr_debug("Step added to workflow %u: function=%u, condition=%s\n",
             workflow->workflow_id, function_id, condition ? condition : "none");

    spin_unlock(&g_edge_runtime.lock);

    return 0;
}

/**
 * edge_workflow_execute - Execute a workflow
 * @workflow: Workflow to execute
 * @input: Input data
 * @input_len: Input length
 * @output: Output buffer
 * @output_len: Output length
 *
 * Executes all steps in the workflow sequentially.
 *
 * Returns: 0 on success, error code on failure
 */
int edge_workflow_execute(edge_workflow_t *workflow, const void *input,
                          size_t input_len, void *output, size_t *output_len)
{
    void *step_output;
    size_t step_output_size;
    size_t current_output_len;
    int ret;

    if (!workflow || !output || !output_len) {
        return -1;
    }

    if (!workflow->enabled) {
        pr_err("Workflow %u is disabled\n", workflow->workflow_id);
        return -1;
    }

    if (workflow->num_steps == 0) {
        pr_err("Workflow %u has no steps\n", workflow->workflow_id);
        return -1;
    }

    pr_info("Executing workflow %s (%u steps)\n",
            workflow->name, workflow->num_steps);

    /* Allocate intermediate output buffer */
    step_output_size = MAX(*output_len, KB(64));
    step_output = kmalloc(step_output_size);
    if (!step_output) {
        return -1;
    }

    current_output_len = input_len;
    if (input && current_output_len > 0 && current_output_len <= *output_len) {
        memcpy(output, input, current_output_len);
    }

    /* Execute each step */
    for (u32 i = 0; i < workflow->num_steps; i++) {
        u32 func_id = workflow->step_function_ids[i];
        
        /* Find function */
        edge_function_t *func = g_edge_runtime.functions;
        while (func) {
            if (func->function_id == func_id) {
                break;
            }
            func = func->next;
        }

        if (!func) {
            pr_err("Function %u not found for workflow step %u\n",
                   func_id, i);
            kfree(step_output);
            return -1;
        }

        /* Check condition */
        if (workflow->step_conditions[i]) {
            /* In a real implementation, evaluate the condition */
            pr_debug("Evaluating condition: %s\n", workflow->step_conditions[i]);
        }

        /* Execute function */
        size_t func_output_len = step_output_size;
        ret = edge_function_execute(func, output, current_output_len,
                                    step_output, &func_output_len);
        if (ret != 0) {
            pr_err("Workflow step %u failed\n", i);
            kfree(step_output);
            return ret;
        }

        /* Copy output for next step */
        current_output_len = func_output_len;
        if (current_output_len > *output_len) {
            current_output_len = *output_len;
        }
        memcpy(output, step_output, current_output_len);
    }

    kfree(step_output);

    *output_len = current_output_len;
    workflow->execution_count++;

    pr_info("Workflow %s completed successfully\n", workflow->name);

    return 0;
}

/**
 * execute_workflow_internal - Internal workflow execution (stub)
 */
static int execute_workflow_internal(edge_workflow_t *workflow,
                                      const void *input, size_t input_len,
                                      void *output, size_t *output_len)
{
    return edge_workflow_execute(workflow, input, input_len, output, output_len);
}

/*===========================================================================*/
/*                         RUNTIME STATISTICS                                */
/*===========================================================================*/

/**
 * edge_runtime_get_stats - Get runtime statistics
 * @stats: Output statistics structure
 */
void edge_runtime_get_stats(struct {
    u32 num_nodes;
    u32 num_functions;
    u32 num_workflows;
    u64 total_executions;
    u64 successful_executions;
    u64 failed_executions;
    u32 current_executions;
    u64 avg_execution_time_us;
} *stats)
{
    if (!stats) {
        return;
    }

    spin_lock(&g_edge_runtime.lock);

    stats->num_nodes = g_edge_runtime.num_nodes;
    stats->num_functions = g_edge_runtime.num_functions;
    stats->num_workflows = g_edge_runtime.num_workflows;
    stats->total_executions = g_edge_runtime.total_executions;
    stats->successful_executions = g_edge_runtime.successful_executions;
    stats->failed_executions = g_edge_runtime.failed_executions;
    stats->current_executions = g_edge_runtime.current_executions;

    if (g_edge_runtime.successful_executions > 0) {
        stats->avg_execution_time_us = 
            g_edge_runtime.total_execution_time_us / 
            g_edge_runtime.successful_executions;
    } else {
        stats->avg_execution_time_us = 0;
    }

    spin_unlock(&g_edge_runtime.lock);
}

/*===========================================================================*/
/*                         HELPER FUNCTIONS                                  */
/*===========================================================================*/

static u32 generate_node_id(void)
{
    return g_edge_runtime.next_node_id++;
}

static u32 generate_function_id(void)
{
    return g_edge_runtime.next_function_id++;
}

static u32 generate_workflow_id(void)
{
    return g_edge_runtime.next_workflow_id++;
}
