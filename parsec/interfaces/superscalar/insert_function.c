/**
 * Copyright (c) 2009-2016 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

/* **************************************************************************** */
/**
 * @file insert_function.c
 *
 * @version 2.0.0
 * @author Reazul Hoque
 *
 */

/* Define a group for Doxygen documentation */
/**
 * @defgroup DTD_INTERFACE Dynamic Task Discovery interface for PaRSEC
 * @ingroup parsec_public
 *
 * These functions are available from the PaRSEC library for the
 * scheduling of kernel routines.
 */

/* Define a group for Doxygen documentation */
/**
 * @defgroup DTD_INTERFACE_INTERNAL Dynamic Task Discovery functions for PaRSEC
 * @ingroup parsec_internal
 *
 * These functions are not available from the PaRSEC library for the
 * scheduling of kernel routines.
 */

#include <stdlib.h>
#include <sys/time.h>

#include "parsec_config.h"
#include "parsec/parsec_internal.h"
#include "parsec/scheduling.h"
#include "parsec/remote_dep.h"
#include "parsec/devices/device.h"
#include "parsec/constants.h"
#include "parsec/vpmap.h"
#include "parsec/utils/mca_param.h"
#include "parsec/mca/sched/sched.h"
#include "parsec/interfaces/interface.h"
#include "parsec/interfaces/superscalar/insert_function_internal.h"
#include "parsec/parsec_prof_grapher.h"
#include "parsec/mca/pins/pins.h"

int dtd_window_size             =  2048; /**< Default window size */
uint32_t dtd_threshold_size     =  2048; /**< Default threshold size of tasks for master thread to wait on */
static int task_hash_table_size = (10+1); /**< Default task hash table size */
static int tile_hash_table_size =  104729; /**< Default tile hash table size */

int my_rank = -1;
int dump_traversal_info; /**< For printing traversal info */
int dump_function_info; /**< For printing function_structure info */

extern parsec_sched_module_t *current_scheduler;

/* Global mempool for all the parsec handles that will be created for a run */
parsec_mempool_t *handle_mempool = NULL;

/**
 * All the static functions should be declared before being defined.
 */
static int
hook_of_dtd_task(parsec_execution_unit_t *context,
                      parsec_execution_context_t *this_task);

static int
dtd_is_ready(const parsec_dtd_task_t *dest);

static void
iterate_successors_of_dtd_task(parsec_execution_unit_t *eu,
                               const parsec_execution_context_t *this_task,
                               uint32_t action_mask,
                               parsec_ontask_function_t *ontask,
                               void *ontask_arg);
static int
release_deps_of_dtd(parsec_execution_unit_t *,
                    parsec_execution_context_t *,
                    uint32_t, parsec_remote_deps_t *);

static parsec_hook_return_t
complete_hook_of_dtd(parsec_execution_unit_t *,
                     parsec_execution_context_t *);

/* Copied from parsec/scheduling.c, will need to be exposed */
#define TIME_STEP 5410
#define MIN(x, y) ( (x)<(y)?(x):(y) )
static inline unsigned long exponential_backoff(uint64_t k)
{
    unsigned int n = MIN( 64, k );
    unsigned int r = (unsigned int) ((double)n * ((double)rand()/(double)RAND_MAX));
    return r * TIME_STEP;
}

/* To create object of class parsec_dtd_task_t that inherits parsec_execution_context_t
 * class
 */
OBJ_CLASS_INSTANCE(parsec_dtd_task_t, parsec_execution_context_t,
                   NULL, NULL);

/* To create object of class .list_itemdtd_tile_t that inherits parsec_list_item_t
 * class
 */
OBJ_CLASS_INSTANCE(parsec_dtd_tile_t, parsec_hashtable_item_t,
                   NULL, NULL);

/* To create object of class parsec_handle_t that inherits parsec_list_t
 * class
 */
OBJ_CLASS_INSTANCE(parsec_handle_t, parsec_list_item_t,
                   NULL, NULL);

/***************************************************************************//**
 *
 * Constructor of PaRSEC's DTD handle.
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to handle which will be constructed
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 *
 ******************************************************************************/
void
parsec_dtd_handle_constructor
(parsec_dtd_handle_t *parsec_handle)
{
    int i;

    parsec_handle->startup_list = (parsec_execution_context_t**)calloc( vpmap_get_nb_vp(), sizeof(parsec_execution_context_t*));

    parsec_handle->function_counter          = 0;

    parsec_handle->task_h_table              = OBJ_NEW(hash_table);
    hash_table_init(parsec_handle->task_h_table,
                    task_hash_table_size,
                    &hash_key);
    parsec_handle->tile_h_table              = OBJ_NEW(hash_table);
    hash_table_init(parsec_handle->tile_h_table,
                    tile_hash_table_size,
                    &hash_key);
    parsec_handle->function_h_table          = OBJ_NEW(hash_table);
    hash_table_init(parsec_handle->function_h_table,
                    PARSEC_dtd_NB_FUNCTIONS,
                    &hash_key);

    parsec_handle->super.startup_hook        = dtd_startup;
    parsec_handle->super.destructor          = (parsec_destruct_fn_t) parsec_dtd_handle_destruct;
    parsec_handle->super.functions_array     = (const parsec_function_t **) malloc( PARSEC_dtd_NB_FUNCTIONS * sizeof(parsec_function_t *));

    for(i=0; i<PARSEC_dtd_NB_FUNCTIONS; i++) {
        parsec_handle->super.functions_array[i] = NULL;
    }

    parsec_handle->super.dependencies_array  = calloc(PARSEC_dtd_NB_FUNCTIONS, sizeof(parsec_dependencies_t *));
    parsec_handle->arenas_size               = 1;
    parsec_handle->arenas = (parsec_arena_t **) malloc(parsec_handle->arenas_size * sizeof(parsec_arena_t *));

    for (i = 0; i < parsec_handle->arenas_size; i++) {
        parsec_handle->arenas[i] = (parsec_arena_t *) calloc(1, sizeof(parsec_arena_t));
    }

#if defined(PARSEC_PROF_TRACE)
    parsec_handle->super.profiling_array     = calloc (2 * PARSEC_dtd_NB_FUNCTIONS , sizeof(int));
#endif /* defined(PARSEC_PROF_TRACE) */

    /* Initializing the tile mempool and attaching it to the parsec_handle */
    parsec_dtd_tile_t fake_tile;
    parsec_handle->tile_mempool          = (parsec_mempool_t*) malloc (sizeof(parsec_mempool_t));
    parsec_mempool_construct( parsec_handle->tile_mempool,
                             OBJ_CLASS(parsec_dtd_tile_t), sizeof(parsec_dtd_tile_t),
                             ((char*)&fake_tile.super.mempool_owner) - ((char*)&fake_tile),
                             1/* no. of threads*/ );

    /* Initializing hash_table_bucket mempool and attaching it to the parsec_handle */
    parsec_generic_bucket_t fake_bucket;
    parsec_handle->hash_table_bucket_mempool = (parsec_mempool_t*) malloc (sizeof(parsec_mempool_t));
    parsec_mempool_construct( parsec_handle->hash_table_bucket_mempool,
                             OBJ_CLASS(parsec_generic_bucket_t), sizeof(parsec_generic_bucket_t),
                             ((char*)&fake_bucket.super.mempool_owner) - ((char*)&fake_bucket),
                             1/* no. of threads*/ );

}

/***************************************************************************//**
 *
 * Destructor of PaRSEC's DTD handle.
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to handle which will be destroyed
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 *
 ******************************************************************************/
void
parsec_dtd_handle_destructor
(parsec_dtd_handle_t *parsec_handle)
{
    int i;
#if defined(PARSEC_PROF_TRACE)
    free((void *)parsec_handle->super.profiling_array);
#endif /* defined(PARSEC_PROF_TRACE) */

    free(parsec_handle->super.functions_array);
    parsec_handle->super.nb_functions = 0;

    for (i = 0; i < parsec_handle->arenas_size; i++) {
        if (parsec_handle->arenas[i] != NULL) {
            free(parsec_handle->arenas[i]);
            parsec_handle->arenas[i] = NULL;
        }
    }

    free(parsec_handle->arenas);
    parsec_handle->arenas      = NULL;
    parsec_handle->arenas_size = 0;

    /* Destroy the data repositories for this object */
    for (i = 0; i <PARSEC_dtd_NB_FUNCTIONS; i++) {
        parsec_destruct_dependencies(parsec_handle->super.dependencies_array[i]);
        parsec_handle->super.dependencies_array[i] = NULL;
    }

    free(parsec_handle->super.dependencies_array);
    parsec_handle->super.dependencies_array = NULL;

    /* Unregister the handle from the devices */
    for (i = 0; i < (int)parsec_nb_devices; i++) {
        if (!(parsec_handle->super.devices_mask & (1 << i)))
            continue;
        parsec_handle->super.devices_mask ^= (1 << i);
        parsec_device_t *device = parsec_devices_get(i);
        if ((NULL == device) || (NULL == device->device_handle_unregister))
            continue;
        if (PARSEC_SUCCESS != device->device_handle_unregister(device, &parsec_handle->super))
            continue;
    }

    /* dtd_handle specific */
    parsec_mempool_destruct(parsec_handle->tile_mempool);
    free (parsec_handle->tile_mempool);
    parsec_mempool_destruct(parsec_handle->hash_table_bucket_mempool);
    free (parsec_handle->hash_table_bucket_mempool);
    free(parsec_handle->startup_list);

    hash_table_fini(parsec_handle->task_h_table, parsec_handle->task_h_table->size);
    hash_table_fini(parsec_handle->tile_h_table, parsec_handle->tile_h_table->size);
    hash_table_fini(parsec_handle->function_h_table, parsec_handle->function_h_table->size);
}

/* To create object of class parsec_dtd_handle_t that inherits parsec_handle_t
 * class
 */
OBJ_CLASS_INSTANCE(parsec_dtd_handle_t, parsec_handle_t,
                   parsec_dtd_handle_constructor, parsec_dtd_handle_destructor);


/* **************************************************************************** */
/**
 * Init function of Dynamic Task Discovery Interface.
 *
 * Here a global(per node/process) handle mempool for PaRSEC's DTD handle
 * is constructed. The mca_params passed to the runtime are also scanned
 * and set here.
 * List of mca options available for DTD interface are:
 *  - dtd_traversal_info (default=0 off):   This prints the DAG travesal
 *                                          info for each node in the DAG.
 *  - dtd_function_info (default=0 off):    This prints the DOT compliant
 *                                          output to check the relationship
 *                                          between the master structure,
 *                                          which represent each task class.
 *  - dtd_tile_hash_size (default=104729):  This sets the tile hash table
 *                                          size.
 *  - dtd_task_hash_size (default=11):      This sets the size of task hash
 *                                          table.
 *  - dtd_window_size (default:2048):       To set the window size for the
 *                                          execution.
 *  - dtd_threshold_size (default:2048):    This sets the threshold task
 *                                          size up to which the master
 *                                          thread will wait before going
 *                                          back and inserting task into the
 *                                          engine.
 * @ingroup DTD_INTERFACE
 */
void
parsec_dtd_init()
{
    parsec_dtd_handle_t  fake_handle, *parsec_handle;

    handle_mempool          = (parsec_mempool_t*) malloc (sizeof(parsec_mempool_t));
    parsec_mempool_construct( handle_mempool,
                             OBJ_CLASS(parsec_dtd_handle_t), sizeof(parsec_dtd_handle_t),
                             ((char*)&fake_handle.mempool_owner) - ((char*)&fake_handle),
                             1/* no. of threads*/ );

    parsec_handle = (parsec_dtd_handle_t *)parsec_thread_mempool_allocate(handle_mempool->thread_mempools);
    parsec_thread_mempool_free( handle_mempool->thread_mempools, parsec_handle );

    /* Registering mca param for printing out traversal info */
    (void)parsec_mca_param_reg_int_name("dtd", "traversal_info",
                                       "Show graph traversal info",
                                       false, false, 0, &dump_traversal_info);

    /* Registering mca param for printing out function_structure info */
    (void)parsec_mca_param_reg_int_name("dtd", "function_info",
                                       "Show master structure info",
                                       false, false, 0, &dump_function_info);

    /* Registering mca param for tile hash table size */
    (void)parsec_mca_param_reg_int_name("dtd", "tile_hash_size",
                                       "Registers the supplied size overriding the default size of tile hash table",
                                       false, false, tile_hash_table_size, &tile_hash_table_size);

    /* Registering mca param for task hash table size */
    (void)parsec_mca_param_reg_int_name("dtd", "task_hash_size",
                                       "Registers the supplied size overriding the default size of task hash table",
                                       false, false, task_hash_table_size, &task_hash_table_size);

    /* Registering mca param for window size */
    (void)parsec_mca_param_reg_int_name("dtd", "window_size",
                                       "Registers the supplied size overriding the default size of window size",
                                       false, false, dtd_window_size, &dtd_window_size);

    /* Registering mca param for threshold size */
    (void)parsec_mca_param_reg_int_name("dtd", "threshold_size",
                                       "Registers the supplied size overriding the default size of threshold size",
                                       false, false, dtd_threshold_size, (int *)&dtd_threshold_size);
}

/* **************************************************************************** */
/**
 * Fini function of Dynamic Task Discovery Interface.
 *
 * The global mempool of parsec_dtd_handle is destroyed here.
 *
 * @ingroup DTD_INTERFACE
 */
void
parsec_dtd_fini()
{
#if defined(PARSEC_DEBUG_PARANOID)
    assert(handle_mempool != NULL);
#endif

    parsec_mempool_destruct(handle_mempool);
    free (handle_mempool);
}

/* **************************************************************************** */
/**
 * Master thread calls this to join worker threads in executing tasks.
 *
 * Master thread, at the end of each window, calls this function to
 * join the worker thread(s) in executing tasks and takes a break
 * from inserting tasks. It(master thread) remains in this function
 * till the total number of pending tasks in the engine reaches a
 * threshold (see dtd_threshold_size). It goes back to inserting task
 * once the number of pending tasks in the engine reaches the
 * threshold size.
 *
 * @param[in]   context
 *                  The PaRSEC context (pointer to the runtime instance)
 * @param[in]   parsec_handle
 *                  PaRSEC dtd handle
 *
 * @ingroup     DTD_INTERFACE_INTERNAL
 */
void
parsec_execute_and_come_back(parsec_context_t *context,
                            parsec_handle_t *parsec_handle)
{
    parsec_dtd_handle_t *dtd_handle = (parsec_dtd_handle_t *)parsec_handle;
    uint64_t misses_in_a_row;
    parsec_execution_unit_t* eu_context = context->virtual_processes[0]->execution_units[0];
    parsec_execution_context_t* exec_context;
    int rc, nbiterations = 0, distance;
    struct timespec rqtp;

    rqtp.tv_sec = 0;
    misses_in_a_row = 1;

    /* Checking if the context has been started or not */
    /* The master thread might not have to trigger the barrier if the other
     * threads have been activated by a previous start.
     */
    if( PARSEC_CONTEXT_FLAG_CONTEXT_ACTIVE & context->flags ) {

    } else {
        (void)parsec_remote_dep_on(context);
        /* Mark the context so that we will skip the initial barrier during the _wait */
        context->flags |= PARSEC_CONTEXT_FLAG_CONTEXT_ACTIVE;
        /* Wake up the other threads */
        parsec_barrier_wait( &(context->barrier) );
    }

    while(parsec_handle->nb_tasks > dtd_handle->task_threshold_size ) {
        if( misses_in_a_row > 1 ) {
            rqtp.tv_nsec = exponential_backoff(misses_in_a_row);
            nanosleep(&rqtp, NULL);
        }

        exec_context = current_scheduler->module.select(eu_context, &distance);

        if( exec_context != NULL ) {
            PINS(eu_context, SELECT_END, exec_context);
            misses_in_a_row = 0;

#if defined(PARSEC_SCHED_REPORT_STATISTICS)
            {
                uint32_t my_idx = parsec_atomic_inc_32b(&sched_priority_trace_counter);
                if(my_idx < PARSEC_SCHED_MAX_PRIORITY_TRACE_COUNTER ) {
                    sched_priority_trace[my_idx].step = eu_context->sched_nb_tasks_done++;
                    sched_priority_trace[my_idx].thread_id = eu_context->th_id;
                    sched_priority_trace[my_idx].vp_id     = eu_context->virtual_process->vp_id;
                    sched_priority_trace[my_idx].priority  = exec_context->priority;
                }
            }
#endif

            rc = PARSEC_HOOK_RETURN_DONE;
            if(exec_context->status <= PARSEC_TASK_STATUS_PREPARE_INPUT) {
                PINS(eu_context, PREPARE_INPUT_BEGIN, exec_context);
                rc = exec_context->function->prepare_input(eu_context, exec_context);
                PINS(eu_context, PREPARE_INPUT_END, exec_context);
            }
            switch(rc) {
            case PARSEC_HOOK_RETURN_DONE: {
                if(exec_context->status <= PARSEC_TASK_STATUS_HOOK) {
                    rc = __parsec_execute( eu_context, exec_context );
                }
                /* We're good to go ... */
                switch(rc) {
                case PARSEC_HOOK_RETURN_DONE:    /* This execution succeeded */
                    exec_context->status = PARSEC_TASK_STATUS_COMPLETE;
                    __parsec_complete_execution( eu_context, exec_context );
                    break;
                case PARSEC_HOOK_RETURN_AGAIN:   /* Reschedule later */
                    exec_context->status = PARSEC_TASK_STATUS_HOOK;
                    if(0 == exec_context->priority) {
                        SET_LOWEST_PRIORITY(exec_context, parsec_execution_context_priority_comparator);
                    } else
                        exec_context->priority /= 10;  /* demote the task */
                    __parsec_schedule(eu_context, exec_context, distance + 1);
                    exec_context = NULL;
                    break;
                case PARSEC_HOOK_RETURN_ASYNC:   /* The task is outside our reach we should not
                                                 * even try to change it's state, the completion
                                                 * will be triggered asynchronously. */
                    break;
                case PARSEC_HOOK_RETURN_NEXT:    /* Try next variant [if any] */
                case PARSEC_HOOK_RETURN_DISABLE: /* Disable the device, something went wrong */
                case PARSEC_HOOK_RETURN_ERROR:   /* Some other major error happened */
                    assert( 0 ); /* Internal error: invalid return value */
                }
                nbiterations++;
                break;
            }
            case PARSEC_HOOK_RETURN_ASYNC:   /* The task is outside our reach we should not
                                             * even try to change it's state, the completion
                                             * will be triggered asynchronously. */
                break;
            case PARSEC_HOOK_RETURN_AGAIN:   /* Reschedule later */
                if(0 == exec_context->priority) {
                    SET_LOWEST_PRIORITY(exec_context, parsec_execution_context_priority_comparator);
                } else
                    exec_context->priority /= 10;  /* demote the task */
                PARSEC_LIST_ITEM_SINGLETON(exec_context);
                __parsec_schedule(eu_context, exec_context, distance + 1);
                exec_context = NULL;
                break;
            default:
                assert( 0 ); /* Internal error: invalid return value for data_lookup function */
            }

            // subsequent select begins
            PINS(eu_context, SELECT_BEGIN, NULL);
        } else {
            misses_in_a_row++;
        }
    }
}

/* **************************************************************************** */
/**
 * Function to end the execution of a DTD handle
 *
 * This function should be called exactly once for each handle, and only after
 * all the tasks initial have been generated. Once inside, new tasks can be
 * created and attached to the handle, and this function will only returns
 * once all tasks have been completed.
 * Upon return the handle is released, and should not be used anymore.
 *
 * @param[in]       parsec
 *                      The PaRSEC context
 * @param[in,out]   parsec_handle
 *                      PaRSEC dtd handle
 *
 * @ingroup         DTD_INTERFACE
 */
void
parsec_dtd_context_wait_on_handle( parsec_context_t     *parsec,
                                  parsec_dtd_handle_t  *dtd_handle )
{
    (void)parsec;

    /* First wait until no tasks are ready */
    parsec_dtd_handle_wait(parsec, dtd_handle);

    /* Now we  can therefore release the extra dependency we had on the handle,
     * and if the handle is complete move on.
     */
    if( 0 == parsec_atomic_dec_32b((uint32_t*)&dtd_handle->super.nb_tasks) ) {
        if( parsec_atomic_cas_32b((uint32_t*)&(dtd_handle->super.nb_tasks), 0, PARSEC_RUNTIME_RESERVED_NB_TASKS) )
            parsec_handle_update_runtime_nbtask((parsec_handle_t*)dtd_handle, -1);
        parsec_context_wait(parsec);
    }
}

/* **************************************************************************** */
/**
 * Function to call when PaRSEC context should wait on a specific handle.
 *
 * This function is called to execute a task collection attached to the
 * handle by the user. This function will schedule all the initially ready
 * tasks in the engine and return when all the pending tasks are executed.
 * Users should call this function everytime they insert a bunch of tasks.
 * Calling this function on a DTD handle for which
 * parsec_dtd_context_wait_on_handle() has been already called will result
 * in undefined behavior. Users can call this function any number of times
 * on a DTD handle before parsec_dtd_context_wait_on_handle() is called for
 * that DTD handle.
 *
 * @param[in]       parsec
 *                      The PaRSEC context
 * @param[in,out]   parsec_handle
 *                      PaRSEC dtd handle
 *
 * @ingroup         DTD_INTERFACE
 */
void
parsec_dtd_handle_wait( parsec_context_t     *parsec,
                       parsec_dtd_handle_t  *parsec_handle )
{
    (void)parsec;
    /* Scheduling all the remaining tasks */
    schedule_tasks (parsec_handle);

    uint32_t tmp_threshold = parsec_handle->task_threshold_size;
    parsec_handle->task_threshold_size = 1;
    parsec_execute_and_come_back (parsec_handle->super.context, &parsec_handle->super);
    parsec_handle->task_threshold_size = tmp_threshold;
}

/* **************************************************************************** */
/**
 * This function unpacks the parameters of a task
 *
 * Unpacks all the parameters of a task, the variables(in which the
 * actual values will be copied) are passed from the body(function that does
 * what this_task is supposed to compute) of this task and the parameters of each
 * task is copied back on the passed variables
 *
 * @param[in]   this_task
 *                  The task we are trying to unpack the parameters for
 * @param[out]  variadic paramter
 *                  The variables where the paramters will be unpacked
 *
 * @ingroup DTD_INTERFACE
 */
void
parsec_dtd_unpack_args(parsec_execution_context_t *this_task, ...)
{
    parsec_dtd_task_t *current_task = (parsec_dtd_task_t *)this_task;
    parsec_dtd_task_param_t *current_param = current_task->param_list;
    int next_arg;
    int i = 0;
    void **tmp;
    va_list arguments;
    va_start(arguments, this_task);
    next_arg = va_arg(arguments, int);

    while (current_param != NULL) {
        tmp = va_arg(arguments, void**);
        if(UNPACK_VALUE == next_arg) {
            *tmp = current_param->pointer_to_tile;
        }else if (UNPACK_DATA == next_arg) {
            /* Let's return directly the usable pointer to the user */
            *tmp = PARSEC_DATA_COPY_GET_PTR(this_task->data[i].data_in);
            i++;
        }else if (UNPACK_SCRATCH == next_arg) {
            *tmp = current_param->pointer_to_tile;
        }
        next_arg = va_arg(arguments, int);
        current_param = current_param->next;
    }
    va_end(arguments);
}

#if defined(PARSEC_PROF_TRACE)
/* **************************************************************************** */
/**
 * This function returns a unique color
 *
 * This function takes a index and a colorspace and queries unique_color()
 * for a hex color code and prepends "fills:" to that color string.
 *
 * @param[in]   index
 * @param[in]   colorspace
 * @return
 *                  String containing "fill:" followed by color code returned
 *                  by unique_color()
 *
 * @ingroup     DTD_INTERFACE_INTERNAL
 */
static inline char*
fill_color(int index, int colorspace)
{
    char *str, *color;
    str = (char *)calloc(12,sizeof(char));
    color = unique_color(index, colorspace);
    snprintf(str,12,"fill:%s",color);
    free(color);
    return str;
}

/* **************************************************************************** */
/**
 * This function adds info about a task class into a global dictionary
 * used for profiling.
 *
 * @param[in]   __parsec_handle
 *                  The pointer to the DTD handle
 * @param[in]   function
 *                  Pointer to the master structure representing a
 *                  task class
 * @param[in]   name
 *                  Name of the task class
 * @param[in]   flow_count
 *                  Total number of flows of the task class
 *
 * @ingroup     DTD_INTERFACE_INTERNAL
 */
void
add_profiling_info( parsec_dtd_handle_t *__parsec_handle,
                    parsec_function_t *function, char* name,
                    int flow_count )
{
    char *str = fill_color(function->function_id, PARSEC_dtd_NB_FUNCTIONS);
    parsec_profiling_add_dictionary_keyword(name, str,
                                           sizeof(parsec_profile_ddesc_info_t) + flow_count * sizeof(assignment_t),
                                           PARSEC_PROFILE_DDESC_INFO_CONVERTOR,
                                           (int *) &__parsec_handle->super.profiling_array[0 +
                                                                                          2 *
                                                                                          function->function_id
                                                                                          /* start key */
                                                                                          ],
                                           (int *) &__parsec_handle->super.profiling_array[1 +
                                                                                          2 *
                                                                                          function->function_id
                                                                                          /*  end key */
                                                                                          ]);
    free(str);

}
#endif /* defined(PARSEC_PROF_TRACE) */

/* **************************************************************************** */
/**
 * This function produces a hash from a key and a size
 *
 * This function returns a hash for a key. The hash is produced
 * by the following operation key % size
 *
 * @param[in]   key
 *                  The key to be hashed
 * @param[in]   size
 *                  Size of the hash table
 * @return
 *              The hash for the key provided
 *
 * @ingroup     DTD_INTERFACE_INTERNAL
 */
uint32_t
hash_key (uintptr_t key, int size)
{
    uint32_t hash_val = key % size;
    return hash_val;
}

/* **************************************************************************** */
/**
 * This function inserts a DTD task into task hash table(for distributed)
 *
 * @param[in,out]  parsec_handle
 *                      Pointer to DTD handle, the task hash table
 *                      is attached to the handle
 * @param[in]       value
 *                      The task to be inserted
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
void
parsec_dtd_task_insert( parsec_dtd_handle_t   *parsec_handle,
                       parsec_dtd_task_t     *value )
{
    uint64_t    key         = value->super.super.key;
    hash_table *hash_table  =  parsec_handle->task_h_table;
    uint32_t    hash        =  hash_table->hash ( key, hash_table->size );

    hash_table_insert ( hash_table, (parsec_hashtable_item_t *)value, hash );
}

/* **************************************************************************** */
/**
 * This function removes a DTD task from a task hash table(for distributed)
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the task hash table
 *                      is attached to the handle
 * @param[in]       key
 *                      The key of the task to be removed
 * @return
 *                  The pointer to the task removed from the hash table
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
void *
parsec_dtd_task_remove( parsec_dtd_handle_t  *parsec_handle,
                       uint32_t             key )
{
    hash_table *hash_table      =  parsec_handle->task_h_table;
    uint32_t    hash            =  hash_table->hash ( key, hash_table->size );

    return hash_table_remove ( hash_table, key, hash );
}

/* **************************************************************************** */
/**
 * This function searches for a DTD task in task hash table(for distributed)
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the task hash table
 *                      is attached to the handle
 * @param[in]       key
 *                      The key of the task to be removed
 * @return
 *                  The pointer to the task if found, NULL otherwise
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
parsec_hashtable_item_t *
parsec_dtd_task_find_internal( parsec_dtd_handle_t  *parsec_handle,
                              uint32_t             key )
{
    hash_table *hash_table      =  parsec_handle->task_h_table;
    uint32_t    hash            =  hash_table->hash( key, hash_table->size );

    return hash_table_nolock_find ( hash_table, key, hash );
}

/* **************************************************************************** */
/**
 * This function calls API to find a DTD task in task hash table(for distributed)
 *
 * @see             parsec_dtd_task_find_internal()
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the task hash table
 *                      is attached to the handle
 * @param[in]       key
 *                      The key of the task to be removed
 * @return
 *                  The pointer to the task if found, NULL otherwise
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
parsec_dtd_task_t *
parsec_dtd_task_find( parsec_dtd_handle_t  *parsec_handle,
                     uint32_t             key )
{
    parsec_dtd_task_t *task = (parsec_dtd_task_t *)parsec_dtd_task_find_internal ( parsec_handle, key );
    return task;
}

/* **************************************************************************** */
/**
 * This function inserts master structure in hash table
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the hash table
 *                      is attached to the handle
 * @param[in]       key
 *                      The function-pointer to the body of task-class
 *                      is treated as the key
 * @param[in]       value
 *                      The pointer to the master structure
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
void
parsec_dtd_function_insert( parsec_dtd_handle_t   *parsec_handle,
                           parsec_dtd_funcptr_t  *key,
                           parsec_dtd_function_t *value )
{
    parsec_generic_bucket_t *bucket  =  (parsec_generic_bucket_t *)parsec_thread_mempool_allocate(parsec_handle->hash_table_bucket_mempool->thread_mempools);

    hash_table *hash_table          =  parsec_handle->function_h_table;
    uint32_t    hash                =  hash_table->hash ( (uint64_t)key, hash_table->size );

    bucket->super.key   = (uint64_t)key;
    bucket->value       = (void *)value;

    hash_table_insert ( hash_table, (parsec_hashtable_item_t *)bucket, hash );
}

/* **************************************************************************** */
/**
 * This function removes master structure from hash table
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the hash table
 *                      is attached to the handle
 * @param[in]       key
 *                      The function-pointer to the body of task-class
 *                      is treated as the key
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
void
parsec_dtd_function_remove( parsec_dtd_handle_t  *parsec_handle,
                           parsec_dtd_funcptr_t *key )
{
    hash_table *hash_table      =  parsec_handle->function_h_table;
    uint32_t    hash            =  hash_table->hash ( (uint64_t)key, hash_table->size );

    hash_table_remove ( hash_table, (uint64_t)key, hash );
}

/* **************************************************************************** */
/**
 * This function searches for master-structure in hash table
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the hash table
 *                      is attached to the handle
 * @param[in]       key
 *                      The function-pointer to the body of task-class
 *                      is treated as the key
 * @return
 *                  The pointer to the master-structure is returned if found,
 *                  NULL otherwise
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
parsec_generic_bucket_t *
parsec_dtd_function_find_internal( parsec_dtd_handle_t  *parsec_handle,
                                  parsec_dtd_funcptr_t *key )
{
    hash_table *hash_table      =  parsec_handle->function_h_table;
    uint32_t    hash            =  hash_table->hash( (uint64_t)key, hash_table->size );

    return (parsec_generic_bucket_t *)hash_table_nolock_find ( hash_table, (uint64_t)key, hash );
}

/* **************************************************************************** */
/**
 * This function internal API to search for master-structure in hash table
 *
 * @see             parsec_dtd_function_find_internal()
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the hash table
 *                      is attached to the handle
 * @param[in]       key
 *                      The function-pointer to the body of task-class
 *                      is treated as the key
 * @return
 *                  The pointer to the master-structure is returned if found,
 *                  NULL otherwise
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
parsec_dtd_function_t *
parsec_dtd_function_find( parsec_dtd_handle_t  *parsec_handle,
                         parsec_dtd_funcptr_t *key )
{
    parsec_generic_bucket_t *bucket = parsec_dtd_function_find_internal ( parsec_handle, key );
    if( bucket != NULL ) {
        return (parsec_dtd_function_t *)bucket->value;
    } else {
        return NULL;
    }
}

/* **************************************************************************** */
/**
 * This function inserts DTD tile into tile hash table
 *
 * The actual key for each tile is formed by OR'ing the last 32 bits of
 * address of the data descriptor (ddesc) with the 32 bit key. So, the actual
 * key is of 64 bits,
 * first 32 bits: last 32 bits of ddesc pointer + last 32 bits: 32 bit key.
 * This is done as PaRSEC key for tiles are unique per ddesc.
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the tile hash table
 *                      is attached to the handle
 * @param[in]       key
 *                      The key of the tile
 * @param[in]       tile
 *                      Pointer to the tile structure
 * @param[in]       ddesc
 *                      Pointer to the ddesc the tile belongs to
 *
 * @ingroup         DTD_ITERFACE_INTERNAL
 */
void
parsec_dtd_tile_insert( parsec_dtd_handle_t *parsec_handle, uint32_t key,
                       parsec_dtd_tile_t   *tile,
                       parsec_ddesc_t      *ddesc )
{
    hash_table *hash_table   =  parsec_handle->tile_h_table;
    uint64_t    combined_key = (uint64_t)ddesc << 32 | (uint64_t)key;
    uint32_t    hash         =  hash_table->hash ( combined_key, hash_table->size );

    tile->super.key = combined_key;

    hash_table_insert ( hash_table, (parsec_hashtable_item_t *)tile, hash );
}

/* **************************************************************************** */
/**
 * This function removes DTD tile from tile hash table
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the tile hash table
 *                      is attached to this handle
 * @param[in]       key
 *                      The key of the tile
 * @param[in]       ddesc
 *                      Pointer to the ddesc the tile belongs to
 *
 * @ingroup         DTD_ITERFACE_INTERNAL
 */
void
parsec_dtd_tile_remove( parsec_dtd_handle_t *parsec_handle, uint32_t key,
                       parsec_ddesc_t      *ddesc )
{
    hash_table *hash_table   =  parsec_handle->tile_h_table;
    uint64_t    combined_key = (uint64_t)ddesc << 32 | (uint64_t)key;
    uint32_t    hash         =  hash_table->hash ( combined_key, hash_table->size );

    parsec_list_t *list = hash_table->item_list[hash];

    parsec_list_lock( list );
    parsec_dtd_tile_t *tile = hash_table_nolock_remove( hash_table, combined_key, hash );
    if( tile->super.list_item.super.obj_reference_count == 1 ) {
#if defined(PARSEC_DEBUG_PARANOID)
        assert(tile->super.list_item.refcount == 0);
#endif
        parsec_thread_mempool_free( parsec_handle->tile_mempool->thread_mempools, tile );
    }

    parsec_list_unlock( list );
}

/* **************************************************************************** */
/**
 * This function searches a DTD tile in the tile hash table
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the tile hash table
 *                      is attached to this handle
 * @param[in]       key
 *                      The key of the tile
 * @param[in]       ddesc
 *                      Pointer to the ddesc the tile belongs to
 *
 * @ingroup         DTD_ITERFACE_INTERNAL
 */
parsec_dtd_tile_t *
parsec_dtd_tile_find( parsec_dtd_handle_t *parsec_handle, uint32_t key,
                     parsec_ddesc_t      *ddesc )
{
    hash_table *hash_table   =  parsec_handle->tile_h_table;
    uint64_t    combined_key = (uint64_t)ddesc << 32 | (uint64_t)key;
    uint32_t    hash         =  hash_table->hash ( combined_key, hash_table->size );

    parsec_list_t *list = hash_table->item_list[hash];

    parsec_list_lock( list );
    parsec_dtd_tile_t *tile = hash_table_nolock_find ( hash_table, combined_key, hash );
    if( NULL != tile ) {
        OBJ_RETAIN(tile);
    }
    parsec_list_unlock( list );
    return tile;
}

/* **************************************************************************** */
/**
 * This function releases tile from tile hash table and pushes them back in
 * tile mempool
 *
 * Here we at first we try to remove the tile from the tile hash table,
 * if we are successful we push the tile back in the tile mempool.
 * This function is called for each flow of a task, after the task is
 * completed.
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the tile hash table
 *                      is attached to this handle
 * @param[in]       tile
 *                      Tile to be released
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
void
parsec_dtd_tile_release(parsec_dtd_handle_t *parsec_handle, parsec_dtd_tile_t *tile)
{
    assert(tile->super.list_item.super.obj_reference_count>1);
    parsec_dtd_tile_remove ( parsec_handle, tile->key, tile->ddesc );
}

/* **************************************************************************** */
/**
 * This function releases the master-structure and pushes them back in mempool
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the tile hash table
 *                      is attached to this handle
 * @param[in]       key
 *                      The function pointer to the body of the task class
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
void
parsec_dtd_function_release( parsec_dtd_handle_t  *parsec_handle,
                            parsec_dtd_funcptr_t *key )
{
    parsec_generic_bucket_t *bucket = parsec_dtd_function_find_internal ( parsec_handle, key );
#if defined(PARSEC_DEBUG_PARANOID)
    assert (bucket != NULL);
#endif
    parsec_dtd_function_remove ( parsec_handle, key );
    parsec_thread_mempool_free( parsec_handle->hash_table_bucket_mempool->thread_mempools, bucket );
}

/* **************************************************************************** */
/**
 * Function to release the task structures inserted into the hash table
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to DTD handle, the tile hash table
 *                      is attached to this handle
 * @param[in]       key
 *                      The unique key of the task
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
void
parsec_dtd_task_release( parsec_dtd_handle_t  *parsec_handle,
                        uint32_t             key )
{
    parsec_dtd_task_remove ( parsec_handle, key );
}

/* **************************************************************************** */
/**
 * Function to recover tiles inserted by insert_task()
 *
 * This function search for a tile if already inserted in the system,
 * and if not returns the freshly created tile.
 *
 * @param[in,out]   parsec handle
 *                      Pointer to the DTD handle
 * @param[in]       ddesc
 *                      Data descriptor
 * @param[in]       i,j
 *                      The co-ordinates of the tile in the matrix
 * @return
 *                  The tile representing the data in specified co-ordinate
 *
 * @ingroup         DTD_INTERFACE
 */
parsec_dtd_tile_t*
parsec_dtd_tile_of(parsec_dtd_handle_t *parsec_dtd_handle,
                  parsec_ddesc_t *ddesc, int i, int j)
{
    parsec_dtd_tile_t *tmp = parsec_dtd_tile_find ( parsec_dtd_handle, ddesc->data_key(ddesc, i, j),
                                                  ddesc );
    if( NULL == tmp ) {
        /* Creating Tile object */
        parsec_dtd_tile_t *temp_tile = (parsec_dtd_tile_t *) parsec_thread_mempool_allocate
                                                          (parsec_dtd_handle->tile_mempool->thread_mempools);
#if defined(PARSEC_DEBUG_PARANOID)
        assert(temp_tile->super.list_item.refcount == 0);
#endif
        temp_tile->key                   = ddesc->data_key(ddesc, i, j);
        temp_tile->rank                  = ddesc->rank_of_key(ddesc, temp_tile->key);
        temp_tile->vp_id                 = ddesc->vpid_of_key(ddesc, temp_tile->key);
        temp_tile->data                  = ddesc->data_of_key(ddesc, temp_tile->key);
        temp_tile->data_copy             = temp_tile->data->device_copies[0];
        temp_tile->ddesc                 = ddesc;
        temp_tile->last_user.flow_index  = -1;
        temp_tile->last_user.op_type     = -1;
        temp_tile->last_user.task        = NULL;
        temp_tile->last_user.alive       = TASK_IS_NOT_ALIVE;
        parsec_atomic_unlock(&temp_tile->last_user.atomic_lock);

        parsec_dtd_tile_insert ( parsec_dtd_handle, temp_tile->key,
                                temp_tile, ddesc );
#if defined(PARSEC_HAVE_CUDA)
        temp_tile->data_copy->readers    = 0;
#endif
        return temp_tile;
    }else {
#if defined(PARSEC_DEBUG_PARANOID)
        assert(tmp->super.list_item.super.obj_reference_count > 0);
#endif
        return tmp;
    }
}

/* **************************************************************************** */
/**
 * This function acts as the hook to connect the PaRSEC task with
 * the actual task.
 *
 * The function users passed while inserting task in PaRSEC is called
 * in this procedure.
 * Called internally by the scheduler
 *
 * @param[in]   context
 *                  The execution unit
 * @param[in]   this_task
 *                  The DTD task to be executed
 * @return
 *              Returns PARSEC_HOOK_RETURN_DONE if the task was
 *              successfully executed, anything else otherwise
 *
 * @ingroup     DTD_INTERFACE_INTERNAL
 */
static int
hook_of_dtd_task( parsec_execution_unit_t    *context,
                  parsec_execution_context_t *this_task )
{
    parsec_dtd_task_t   *dtd_task   = (parsec_dtd_task_t*)this_task;
    int rc = 0;

    PARSEC_TASK_PROF_TRACE(context->eu_profile,
                          this_task->parsec_handle->profiling_array[2 * this_task->function->function_id],
                          this_task);

    rc = ((parsec_dtd_function_t *)(dtd_task->super.function))->fpointer(context, this_task);
#if defined(PARSEC_DEBUG_PARANOID)
    assert( rc == PARSEC_HOOK_RETURN_DONE );
#endif

    return rc;
}

/* chores and parsec_function_t structure initialization */
static const __parsec_chore_t dtd_chore[] = {
    {.type      = PARSEC_DEV_CPU,
     .evaluate  = NULL,
     .hook      = hook_of_dtd_task },
    {.type      = PARSEC_DEV_NONE,
     .evaluate  = NULL,
     .hook      = NULL},             /* End marker */
};

/* for GRAPHER purpose */
static symbol_t symb_dtd_taskid = {
    .name           = "task_id",
    .context_index  = 0,
    .min            = NULL,
    .max            = NULL,
    .cst_inc        = 1,
    .expr_inc       = NULL,
    .flags          = 0x0
};

/**
 * To make it consistent with PaRSEC we need to intialize and have this function
 * we do not use this at this point
 */
static inline uint64_t DTD_identity_hash(const parsec_dtd_handle_t * __parsec_handle,
                                         const assignment_t * assignments)
{
    (void)__parsec_handle;
    return (uint64_t)assignments[0].value;
}

/* **************************************************************************** */
/**
 * Intializes all the needed members and returns the DTD handle
 *
 * For correct profiling the task_class_counter should be correct
 *
 * @param[in]   context
 *                  The PARSEC context
 * @param[in]   arena_count
 *                  The count of the task class DTD handle will deal with
 * @return
 *              The PARSEC DTD handle
 *
 * @ingroup     DTD_INTERFACE
 */
parsec_dtd_handle_t *
parsec_dtd_handle_new( parsec_context_t *context)
{
    if (dump_traversal_info) {
        parsec_output(parsec_debug_output, "\n\n------ New Handle -----\n\n\n");
    }

    my_rank = context->my_rank;
    parsec_dtd_handle_t *__parsec_handle;
    int i;

#if defined(PARSEC_DEBUG_PARANOID)
    assert( handle_mempool != NULL );
#endif
    __parsec_handle = (parsec_dtd_handle_t *)parsec_thread_mempool_allocate(handle_mempool->thread_mempools);

    __parsec_handle->super.context             = context;
    __parsec_handle->super.on_enqueue          = NULL;
    __parsec_handle->super.on_enqueue_data     = NULL;
    __parsec_handle->super.on_complete         = NULL;
    __parsec_handle->super.on_complete_data    = NULL;
    __parsec_handle->super.devices_mask        = PARSEC_DEVICES_ALL;
    __parsec_handle->super.nb_tasks            = 1;  /* For the bounded window, starting with +1 task */
    __parsec_handle->super.nb_pending_actions  = 1;  /* For the future tasks that will be inserted */
    __parsec_handle->super.nb_functions        = 0;

    for(i = 0; i < vpmap_get_nb_vp(); i++) {
        __parsec_handle->startup_list[i] = NULL;
    }

    /* Keeping track of total tasks to be executed per handle for the window */
    for (i=0; i<PARSEC_dtd_NB_FUNCTIONS; i++) {
        __parsec_handle->flow_set_flag[i]  = 0;
        /* Added new */
        __parsec_handle->super.functions_array[i] = NULL;
    }

    __parsec_handle->task_id               = 0;
    __parsec_handle->task_window_size      = 1;
    __parsec_handle->task_threshold_size   = dtd_threshold_size;
    __parsec_handle->function_counter      = 0;
    __parsec_handle->function_counter      = 0;
#if defined (OVERLAP)
    __parsec_handle->mode                  = OVERLAPPED;
#else
    __parsec_handle->mode                  = NOT_OVERLAPPED;
#endif

    (void)parsec_handle_reserve_id((parsec_handle_t *) __parsec_handle);
    (void)parsec_handle_enable((parsec_handle_t *)__parsec_handle, NULL, NULL, NULL, __parsec_handle->super.nb_pending_actions);

    return (parsec_dtd_handle_t*) __parsec_handle;
}

/* **************************************************************************** */
/**
 * Clean up function to clean memory allocated dynamically for the run
 *
 * @param[in,out]   parsec_handle
 *                      Pointer to the DTD handle
 *
 * @ingroup         DTD_INTERFACE
 */
void
parsec_dtd_handle_destruct(parsec_dtd_handle_t *parsec_handle)
{
    int i;
    for (i=0; i<PARSEC_dtd_NB_FUNCTIONS; i++) {
        const parsec_function_t   *func = parsec_handle->super.functions_array[i];
        parsec_dtd_function_t *dtd_func = (parsec_dtd_function_t *)func;

        if( func != NULL ) {
            int j, k;

            parsec_dtd_function_release( parsec_handle, dtd_func->fpointer );

            for (j=0; j< func->nb_flows; j++) {
                if(func->in[j] != NULL ) {
                    for(k=0; k<MAX_DEP_IN_COUNT; k++) {
                        if (func->in[j]->dep_in[k] != NULL) {
                            free((void*)func->in[j]->dep_in[k]);
                        }
                    }
                    for(k=0; k<MAX_DEP_OUT_COUNT; k++) {
                        if (func->in[j]->dep_out[k] != NULL) {
                            free((void*)func->in[j]->dep_out[k]);
                        }
                    }
                    free((void*)func->in[j]);
                }
                /*if(func->out[j] != NULL) {
                    for(k=0; k<MAX_DEP_IN_COUNT; k++) {
                        if (func->out[j]->dep_in[k] != NULL) {
                            free((void*)func->out[j]->dep_in[k]);
                        }
                    }
                    for(k=0; k<MAX_DEP_OUT_COUNT; k++) {
                        if (func->out[j]->dep_out[k] != NULL) {
                            free((void*)func->out[j]->dep_out[k]);
                        }
                    }
                    free((void*)func->out[j]);
                }*/
            }
            parsec_mempool_destruct(dtd_func->context_mempool);
            free(dtd_func->context_mempool);
            free((void*)func);
        }
    }

    parsec_handle_unregister( &parsec_handle->super );
    parsec_thread_mempool_free( handle_mempool->thread_mempools, parsec_handle );
}

/* **************************************************************************** */
/**
 * This is the hook that connects the function to start initial ready
 * tasks with the context. Called internally by PaRSEC.
 *
 * @param[in]   context
 *                  PARSEC context
 * @param[in]   parsec_handle
 *                  Pointer to DTD handle
 * @param[in]   pready_list
 *                  Lists of ready tasks for each core
 *
 * @ingroup     DTD_INTERFACE_INTERNAL
 */
void
dtd_startup( parsec_context_t            *context,
             parsec_handle_t             *parsec_handle,
             parsec_execution_context_t **pready_list )
{
    uint32_t supported_dev = 0;
    parsec_dtd_handle_t *__parsec_handle = (parsec_dtd_handle_t *) parsec_handle;

    /* Create the PINS DATA pointers if PINS is enabled */
#if defined(PINS_ENABLE)
    __parsec_handle->super.context = context;
#endif /* defined(PINS_ENABLE) */

    uint32_t wanted_devices = parsec_handle->devices_mask;
    parsec_handle->devices_mask = 0;

    for (uint32_t _i = 0; _i < parsec_nb_devices; _i++) {
        if (!(wanted_devices & (1 << _i)))
            continue;
        parsec_device_t *device = parsec_devices_get(_i);

        if (NULL == device)
            continue;
        if (NULL != device->device_handle_register)
            if (PARSEC_SUCCESS != device->device_handle_register(device, (parsec_handle_t *) parsec_handle))
                continue;

        supported_dev |= device->type;
        parsec_handle->devices_mask |= (1 << _i);
    }
    (void)pready_list;
}

/* **************************************************************************** */
/**
 * Function that checks if a task is ready or not
 *
 * @param[in,out]   dest
 *                      The task we are performing the ready check for
 * @return
 *                  If the task is ready we return 1, 0 otherwise
 *
 * @ingroup         DTD_ITERFACE_INTERNAL
 */
static int
dtd_is_ready(const parsec_dtd_task_t *dest)
{
    parsec_dtd_task_t *dest_task = (parsec_dtd_task_t*)dest;
    if ( 0 == parsec_atomic_add_32b((int *)&(dest_task->flow_count), -1) ) {
        return 1;
    }
    return 0;
}

/* **************************************************************************** */
/**
 * This function checks the readyness of a task and if ready pushes it
 * in a list of ready task
 *
 * This function will have more functionality in the implementation
 * for distributed memory
 *
 * @param[in]   eu
 *                  Execution unit
 * @param[in]   new_context
 *                  Pointer to DTD task we are trying to activate
 * @param[in]   old_context
 *                  Pointer to DTD task activating it's successor(new_context)
 * @param       deps,data,src_rank,dst_rank,dst_vpid
 *                  Parameters we will use in distributed memory implementation
 * @param[out]  param
 *                  Pointer to list in which we will push if the task is ready
 * @return
 *              Instruction on how to iterate over the successors of old_context
 *
 * @ingroup     DTD_INTERFACE_INTERNAL
 */
parsec_ontask_iterate_t
dtd_release_dep_fct( parsec_execution_unit_t *eu,
                     const parsec_execution_context_t *new_context,
                     const parsec_execution_context_t *old_context,
                     const dep_t *deps,
                     parsec_dep_data_description_t *data,
                     int src_rank, int dst_rank, int dst_vpid,
                     void *param )
{
    (void)eu; (void)data; (void)src_rank; (void)dst_rank; (void)old_context;
    parsec_release_dep_fct_arg_t *arg = (parsec_release_dep_fct_arg_t *)param;
    parsec_dtd_task_t *current_task = (parsec_dtd_task_t*) new_context;
    int is_ready = 0;

    is_ready = dtd_is_ready(current_task);

#if defined(PARSEC_PROF_GRAPHER)
    parsec_dtd_task_t *parent_task  = (parsec_dtd_task_t*)old_context;
    /* Check to not print stuff redundantly */
    if(!parent_task->dont_skip_releasing_data[deps->dep_index]) {
        parsec_flow_t *origin_flow = (parsec_flow_t*) calloc(1, sizeof(parsec_flow_t));
        parsec_flow_t *dest_flow = (parsec_flow_t*) calloc(1, sizeof(parsec_flow_t));

        origin_flow->name = "A";
        dest_flow->name = "A";
        dest_flow->flow_flags = FLOW_ACCESS_RW;

        parsec_prof_grapher_dep(old_context, new_context, is_ready, origin_flow, dest_flow);

        free(origin_flow);
        free(dest_flow);
    }
#else
    (void)deps;
#endif

    if(is_ready) {
        if(dump_traversal_info) {
            parsec_output(parsec_debug_output, "------\ntask Ready: %s \t %" PRIu64 "\nTotal flow: %d  flow_count:"
                   "%d\n-----\n", current_task->super.function->name, current_task->super.super.key,
                   current_task->super.function->nb_flows, current_task->flow_count);
        }

#if defined(DEBUG_HEAVY)
            parsec_dtd_task_release( (parsec_dtd_handle_t *)current_task->super.parsec_handle,
                                    current_task->super.super.key );
#endif

            arg->ready_lists[dst_vpid] = (parsec_execution_context_t*)
                parsec_list_item_ring_push_sorted( (parsec_list_item_t*)arg->ready_lists[dst_vpid],
                                                  &current_task->super.super.list_item,
                                                  parsec_execution_context_priority_comparator );
            return PARSEC_ITERATE_CONTINUE; /* Returns the status of the task being activated */
    } else {
        return PARSEC_ITERATE_STOP;
    }
}

/* **************************************************************************** */
/**
 * This function iterates over all the successors of a task and
 * activates them and builds a list of the ones that got ready
 * by this activation
 *
 * The actual implementation is done in ordering_correctly_2()
 *
 * @see     ordering_correctly_2()
 *
 * @param   eu,this_task,action_mask,ontask,ontask_arg
 *
 * @ingroup DTD_ITERFACE_INTERNAL
 */
static void
iterate_successors_of_dtd_task(parsec_execution_unit_t *eu,
                               const parsec_execution_context_t *this_task,
                               uint32_t action_mask,
                               parsec_ontask_function_t *ontask,
                               void *ontask_arg)
{
    (void)eu; (void)this_task; (void)action_mask; (void)ontask; (void)ontask_arg;
    ordering_correctly_1(eu, this_task, action_mask, ontask, ontask_arg);
}

/* **************************************************************************** */
/**
 * Release dependencies after a task is done
 *
 * Calls iterate successors function that returns a list of tasks that
 * are ready to go. Those ready tasks are scheduled in here.
 *
 * @param[in]   eu
 *                  Execution unit
 * @param[in]   this_task
 *                  Pointer to DTD task we are releasing deps of
 * @param       action_mask,deps
 * @return
 *              0
 *
 * @ingroup     DTD_INTERFACE_INTERNAL
 */
static int
release_deps_of_dtd( parsec_execution_unit_t *eu,
                     parsec_execution_context_t *this_task,
                     uint32_t action_mask,
                     parsec_remote_deps_t *deps )
{
    (void)deps;
    parsec_release_dep_fct_arg_t arg;
    int __vp_id;

    arg.action_mask  = action_mask;
    arg.output_usage = 0;
    arg.output_entry = NULL;
    arg.ready_lists  = (NULL != eu) ? alloca(sizeof(parsec_execution_context_t *) * eu->virtual_process->parsec_context->nb_vp) : NULL;

    if (NULL != eu)
        for (__vp_id = 0; __vp_id < eu->virtual_process->parsec_context->nb_vp; arg.ready_lists[__vp_id++] = NULL);

    iterate_successors_of_dtd_task(eu, (parsec_execution_context_t*)this_task, action_mask, dtd_release_dep_fct, &arg);

    parsec_vp_t **vps = eu->virtual_process->parsec_context->virtual_processes;
    for (__vp_id = 0; __vp_id < eu->virtual_process->parsec_context->nb_vp; __vp_id++) {
        if (NULL == arg.ready_lists[__vp_id]) {
            continue;
        }
        if (__vp_id == eu->virtual_process->vp_id) {
            __parsec_schedule(eu, arg.ready_lists[__vp_id], 0);
        }else {
            __parsec_schedule(vps[__vp_id]->execution_units[0], arg.ready_lists[__vp_id], 0);
        }
        arg.ready_lists[__vp_id] = NULL;
    }

    return 0;
}

/* **************************************************************************** */
/**
 * This function is called internally by PaRSEC once a task is done
 *
 * @param[in]   context
 *                  Execution unit
 * @param[in]   this_task
 *                  Pointer to DTD task we just completed
 * @return
 *              0
 *
 * @ingroup     DTD_INTERFACE_INTERNAL
 */
static int
complete_hook_of_dtd( parsec_execution_unit_t    *context,
                      parsec_execution_context_t *this_task )
{
    parsec_dtd_task_t *task = (parsec_dtd_task_t*) this_task;

    if (dump_traversal_info) {
        static int counter= 0;
        (void)parsec_atomic_add_32b(&counter,1);
        parsec_output(parsec_debug_output, "------------------------------------------------\n"
               "execution done of task: %s \t %" PRIu64 "\n"
               "task done %d \n",
               this_task->function->name,
               task->super.super.key,
               counter);
    }

#if defined(PARSEC_PROF_GRAPHER)
    parsec_prof_grapher_task(this_task, context->th_id, context->virtual_process->vp_id,
                            task->super.super.key);
#endif /* defined(PARSEC_PROF_GRAPHER) */

    PARSEC_TASK_PROF_TRACE(context->eu_profile,
                          this_task->parsec_handle->profiling_array[2 * this_task->function->function_id + 1],
                          this_task);

    release_deps_of_dtd(context, (parsec_execution_context_t*)this_task, 0xFFFF, NULL);
    return 0;
}

/* Prepare_input function */
int
data_lookup_of_dtd_task( parsec_execution_unit_t *context,
                         parsec_execution_context_t *this_task )
{
    (void)context;

    int current_dep, op_type_on_current_flow;
    parsec_dtd_task_t *current_task = (parsec_dtd_task_t *)this_task;

    for( current_dep = 0; current_dep < current_task->super.function->nb_flows; current_dep++ ) {
        op_type_on_current_flow = (current_task->flow[current_dep].op_type & GET_OP_TYPE);

        if( NULL == current_task->super.data[current_dep].data_in ) continue;

        if( INOUT == op_type_on_current_flow ||
            OUTPUT == op_type_on_current_flow ) {
            if( current_task->super.data[current_dep].data_in->readers > 0 ) {
                return PARSEC_HOOK_RETURN_AGAIN;
                /* We create a new data copy to avoid WAR */
                #if 0
                parsec_data_copy_t *new_copy = parsec_data_copy_new(current_task->super.data[current_dep].data_in->original, current_task->super.data[current_dep].data_in->device_index);
                new_copy->older = current_task->super.data[current_dep].data_in;

                /* Copying the actual data */

                //PARSEC_DATA_COPY_RELEASE(current_task->super.data[current_dep].data_out);
                #endif
            }

        }
    }

    return PARSEC_HOOK_RETURN_DONE;
}

/* prepare_output function */
int
output_data_of_dtd_task( parsec_execution_unit_t *context,
                         parsec_execution_context_t *this_task )
{
    (void)context;

    int current_dep, op_type_on_current_flow;
    parsec_dtd_task_t *current_task = (parsec_dtd_task_t *)this_task;

    for( current_dep = 0; current_dep < current_task->super.function->nb_flows; current_dep++ ) {
        op_type_on_current_flow = (current_task->flow[current_dep].op_type & GET_OP_TYPE);
        /* The following check makes sure the behavior is correct when NULL is provided as input to a flow.
           Dependency tracking and building is completely ignored in flow = NULL case, and no expectation should exist.
        */
        if( NULL == current_task->super.data[current_dep].data_in &&
            NULL != current_task->super.data[current_dep].data_out ) {
            parsec_warning( "Please make sure you are not assigning data in data_out when data_in is provided as NULL.\n"
                           "No dependency tracking is performed for a flow when NULL is passed as as INPUT for that flow.\n" );
            exit(0);
        }

        current_task->super.data[current_dep].data_out = current_task->super.data[current_dep].data_in;

        if( INOUT == op_type_on_current_flow ||
            OUTPUT == op_type_on_current_flow ) {
            /* For each Write flow we update the version */
            current_task->super.data[current_dep].data_out->version++;

        }
    }

    return PARSEC_HOOK_RETURN_DONE;
}

int
find_free_in_flow
(const parsec_function_t *function)
{
    int i;
    for (i=0; i<MAX_PARAM_COUNT-1; i++) {
        if ( function->in[i] == NULL ) {
            break;
        }
    }
    return i;
}

int
find_free_out_flow
(const parsec_function_t *function)
{
    int i;
    for (i=0; i<MAX_PARAM_COUNT-1; i++) {
        if ( function->out[i] == NULL ) {
            break;
        }
    }
    return i;
}

int
find_out_flow( const parsec_function_t *function, int flow_index )
{
    int i;
    for (i=0; i<MAX_PARAM_COUNT-1; i++) {
        if ( function->out[i] != NULL ) {
            if ( function->out[i]->flow_index == flow_index ) {
                break;
            }
        }
    }
    return i;
}

int
find_in_flow( const parsec_function_t *function, int flow_index )
{
    int i;
    for (i=0; i<MAX_PARAM_COUNT-1; i++) {
        if ( function->in[i] != NULL ) {
            if ( function->in[i]->flow_index == flow_index ) {
                break;
            }
        }
    }
    return i;
}

/***************************************************************************//**
 *
 * Function to find and return a dep between two tasks
 *
 * @param[in]   parent_task, desc_task, parent_flow_index, desc_flow_index
 * @return
 *              Dep found between two tasks
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 *
 ******************************************************************************/
dep_t *
find_and_return_dep( parsec_dtd_task_t *parent_task, parsec_dtd_task_t *desc_task,
                     int parent_flow_index, int desc_flow_index )
{
    int out_index = find_out_flow( (parsec_function_t *) parent_task->super.function, parent_flow_index );
    parsec_flow_t *flow = (parsec_flow_t*)parent_task->super.function->out[out_index];
    int desc_function_id = desc_task->super.function->function_id, i;
    dep_t *dep = NULL;

    for (i=0; i<MAX_DEP_OUT_COUNT; i++) {
        if ( flow->dep_out[i]->function_id == desc_function_id &&
            flow->dep_out[i]->flow->flow_index == desc_flow_index ) {
            dep = (dep_t *) flow->dep_out[i];
            break;
        }
    }
    return dep;
}

#if defined (WILL_USE_IN_DISTRIBUTED)
/* This function creates relationship between different types of task classes.
 * Arguments:   - parsec handle (parsec_handle_t *)
                - parent master structure (parsec_function_t *)
                - child master structure (parsec_function_t *)
                - flow index of task that belongs to the class of "parent master structure" (int)
                - flow index of task that belongs to the class of "child master structure" (int)
                - the type of data (the structure of the data like square,
                  triangular and etc) this dependency is about (int)
 * Returns:     - void
 */
void
set_dependencies_for_function(parsec_handle_t* parsec_handle,
                              parsec_function_t *parent_function,
                              parsec_function_t *desc_function,
                              uint8_t parent_flow_index,
                              uint8_t desc_flow_index)
{
    uint8_t i, dep_exists = 0, j;

    if (NULL == desc_function) {   /* Data is not going to any other task */
        int out_index = find_out_flow( parent_function, parent_flow_index );
        if(NULL != parent_function->out[out_index]) {
            parsec_flow_t *tmp_d_flow = (parsec_flow_t *)parent_function->out[out_index];
            for (i=0; i<MAX_DEP_IN_COUNT; i++) {
                if (NULL != tmp_d_flow->dep_out[i]) {
                    if (tmp_d_flow->dep_out[i]->function_id == LOCAL_DATA ) {
                        dep_exists = 1;
                        break;
                    }
                }
            }
        }
        if (!dep_exists) {
            dep_t *desc_dep = (dep_t *) malloc(sizeof(dep_t));
            if (dump_function_info) {
                parsec_output(parsec_debug_output, "%s -> LOCAL\n", parent_function->name);
            }

            desc_dep->cond          = NULL;
            desc_dep->ctl_gather_nb = NULL;
            desc_dep->function_id   = LOCAL_DATA; /* 100 is used to indicate data is coming from memory */
            desc_dep->dep_index     = ((parsec_dtd_function_t*)parent_function)->dep_out_index++;
            desc_dep->belongs_to    = parent_function->out[out_index];
            desc_dep->flow          = NULL;
            desc_dep->direct_data   = NULL;
            /* specific for cholesky, will need to change */
            desc_dep->dep_datatype_index    = ((parsec_dtd_function_t*)parent_function)->dep_datatype_index++;

            for (i=0; i<MAX_DEP_IN_COUNT; i++) {
                if (NULL == parent_function->out[out_index]->dep_out[i]) {
                    /* Bypassing constness in function structure */
                    parsec_flow_t **desc_in = (parsec_flow_t**)&(parent_function->out[out_index]);
                    /* Setting dep in the next available dep_in array index */
                    (*desc_in)->dep_out[i] = (dep_t *)desc_dep;
                    break;
                }
            }
        }
        return;
    }

    if (NULL == parent_function) {   /* Data is not coming from any other task */
        int in_index = find_in_flow( desc_function, desc_flow_index );
        if(NULL != desc_function->in[in_index]) {
            parsec_flow_t *tmp_d_flow = (parsec_flow_t *)desc_function->in[in_index];
            for (i = 0; i < MAX_DEP_IN_COUNT; i++) {
                if (NULL != tmp_d_flow->dep_in[i]) {
                    if (tmp_d_flow->dep_in[i]->function_id == LOCAL_DATA ) {
                        dep_exists = 1;
                        break;
                    }
                }
            }
        }
        if (!dep_exists) {
            dep_t *desc_dep = (dep_t *) malloc(sizeof(dep_t));
            if(dump_function_info) {
                parsec_output(parsec_debug_output, "LOCAL -> %s\n", desc_function->name);
            }
            desc_dep->cond          = NULL;
            desc_dep->ctl_gather_nb = NULL;
            desc_dep->function_id   = LOCAL_DATA;
            desc_dep->dep_index     = ((parsec_dtd_function_t*)desc_function)->dep_in_index++;
            desc_dep->belongs_to    = desc_function->in[in_index];
            desc_dep->flow          = NULL;
            desc_dep->direct_data   = NULL;
            desc_dep->dep_datatype_index    = ((parsec_dtd_function_t*)desc_function)->dep_datatype_index;

            for (i=0; i<MAX_DEP_IN_COUNT; i++) {
                if (NULL == desc_function->in[in_index]->dep_in[i]) {
                    /* Bypassing constness in function structure */
                    parsec_flow_t **desc_in = (parsec_flow_t**)&(desc_function->in[in_index]);
                    /* Setting dep in the next available dep_in array index */
                    (*desc_in)->dep_in[i]  = (dep_t *)desc_dep;
                    break;
                }
            }
        }
        return;
    }

    int out_index = find_out_flow( parent_function, parent_flow_index );
    parsec_flow_t *tmp_flow = (parsec_flow_t *) parent_function->out[out_index];

    if (NULL == tmp_flow) {
        int in_index = find_in_flow( parent_function, parent_flow_index );
        dep_t *tmp_dep = NULL;
        parsec_flow_t *tmp_p_flow = NULL;
        tmp_flow = (parsec_flow_t *) parent_function->in[in_index];
        for (i = 0; i < MAX_DEP_IN_COUNT; i++) {
            if(NULL != tmp_flow->dep_in[i]) {
                tmp_dep = (dep_t *) tmp_flow->dep_in[i];
            }
        }
        if(tmp_dep->function_id == LOCAL_DATA) {
            set_dependencies_for_function(parsec_handle,
                                          NULL, desc_function, 0,
                                          desc_flow_index);
            return;
        }
        tmp_p_flow = (parsec_flow_t *)tmp_dep->flow;
        parent_function = (parsec_function_t *)parsec_handle->functions_array[tmp_dep->function_id];

        for(j=0; j<MAX_DEP_OUT_COUNT; j++) {
            if(NULL != tmp_p_flow->dep_out[j]) {
                if((parsec_flow_t *)tmp_p_flow->dep_out[j]->flow == tmp_flow) {
                    parent_flow_index = tmp_p_flow->dep_out[j]->belongs_to->flow_index;
                    set_dependencies_for_function(parsec_handle,
                                                  parent_function,
                                                  desc_function,
                                                  parent_flow_index,
                                                  desc_flow_index);
                    return;
                }
            }
        }
        dep_exists = 1;
    }

    int desc_in_index = find_in_flow( desc_function, desc_flow_index );
    for (i=0; i<MAX_DEP_OUT_COUNT; i++) {
        if (NULL != tmp_flow->dep_out[i]) {
            if( tmp_flow->dep_out[i]->function_id == desc_function->function_id &&
                tmp_flow->dep_out[i]->flow == desc_function->in[desc_in_index] ) {
                dep_exists = 1;
                break;
            }
        }
    }

    if(!dep_exists) {
        dep_t *desc_dep = (dep_t *) malloc(sizeof(dep_t));
        dep_t *parent_dep = (dep_t *) malloc(sizeof(dep_t));

        if (dump_function_info) {
            parsec_output(parsec_debug_output, "%s -> %s\n", parent_function->name, desc_function->name);
        }

        /* setting out-dependency for parent */
        parent_dep->cond            = NULL;
        parent_dep->ctl_gather_nb   = NULL;
        parent_dep->function_id     = desc_function->function_id;
        parent_dep->flow            = desc_function->in[desc_in_index];
        parent_dep->dep_index       = ((parsec_dtd_function_t*)parent_function)->dep_out_index++;
        parent_dep->belongs_to      = parent_function->out[out_index];
        parent_dep->direct_data     = NULL;
        parent_dep->dep_datatype_index = ((parsec_dtd_function_t*)parent_function)->dep_datatype_index++;

        for(i=0; i<MAX_DEP_OUT_COUNT; i++) {
            if(NULL == parent_function->out[out_index]->dep_out[i]) {
                /* to bypass constness in function structure */
                parsec_flow_t **parent_out = (parsec_flow_t **)&(parent_function->out[out_index]);
                (*parent_out)->dep_out[i] = (dep_t *)parent_dep;
                break;
            }
        }

        /* setting in-dependency for descendant */
        desc_dep->cond          = NULL;
        desc_dep->ctl_gather_nb = NULL;
        desc_dep->function_id   = parent_function->function_id;
        desc_dep->flow          = parent_function->out[out_index];
        desc_dep->dep_index     = ((parsec_dtd_function_t*)desc_function)->dep_in_index++;
        desc_dep->belongs_to    = desc_function->in[desc_in_index];
        desc_dep->direct_data   = NULL;
        desc_dep->dep_datatype_index = ((parsec_dtd_function_t*)desc_function)->dep_datatype_index;

        for(i=0; i<MAX_DEP_IN_COUNT; i++) {
            if(NULL == desc_function->in[desc_in_index]->dep_in[i]) {
                /* Bypassing constness in function strucutre */
                parsec_flow_t **desc_in = (parsec_flow_t **)&(desc_function->in[desc_in_index]);
                (*desc_in)->dep_in[i]  = (dep_t *)desc_dep;
                break;
            }
        }
    }
}
#endif


/* **************************************************************************** */
/**
 * This function creates and initializes members of master-structure
 * representing each task-class
 *
 * Most importantly mempool from which we will create DTD task of a
 * task class is created here. The size of each task is calculated
 * from the parameters passed to this function. A master-structure
 * is like a template of a task class. It store the common attributes
 * of all the tasks, belonging to a task class, will have.
 *
 * @param[in,out]   __parsec_handle
 *                      The DTD handle
 * @paramp[in]      fpointer
 *                      Function pointer that uniquely identifies a
 *                      task class, this is the pointer to the body
 *                      of the task
 * @param[in]       name
 *                      The name of the task class the master-structure
 *                      will represent
 * @param[in]       count_of_params
 *                      Total count of parameters each task of this task
 *                      class will have and work on
 * @param[in]       size_of_param
 *                      Total size of all params in bytes
 * @param[in]       flow_count
 *                      Total flow each task of this task class has
 * @return
 *                  The master-structure
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
parsec_function_t*
create_function(parsec_dtd_handle_t *__parsec_handle, parsec_dtd_funcptr_t* fpointer, char* name,
                int count_of_params, long unsigned int size_of_param, int flow_count)
{
    parsec_dtd_function_t *dtd_function = (parsec_dtd_function_t *) calloc(1, sizeof(parsec_dtd_function_t));
    parsec_function_t *function = (parsec_function_t *) dtd_function;

    dtd_function->dep_datatype_index = 0;
    dtd_function->dep_in_index       = 0;
    dtd_function->dep_out_index      = 0;
    dtd_function->count_of_params    = count_of_params;
    dtd_function->size_of_param      = size_of_param;
    dtd_function->fpointer           = fpointer;

    /* Allocating mempool according to the size and param count */
    dtd_function->context_mempool = (parsec_mempool_t*) malloc (sizeof(parsec_mempool_t));
    parsec_dtd_task_t fake_task;

    /*int total_size = sizeof(parsec_dtd_task_t) + count_of_params * sizeof(parsec_dtd_task_param_t)
     + size_of_param + 2; */ /* this is for memory alignment */

    int total_size = sizeof(parsec_dtd_task_t) + count_of_params * sizeof(parsec_dtd_task_param_t) + size_of_param;
    parsec_mempool_construct( dtd_function->context_mempool,
                             OBJ_CLASS(parsec_dtd_task_t), total_size,
                             ((char*)&fake_task.super.super.mempool_owner) - ((char*)&fake_task),
                             1/* no. of threads*/ );

    /*
     To bypass const in function structure.
     Getting address of the const members in local mutable pointers.
     */
    char **name_not_const = (char **)&(function->name);
    symbol_t **params     = (symbol_t **) &function->params;
    symbol_t **locals     = (symbol_t **) &function->locals;
    expr_t **priority     = (expr_t **)&function->priority;
    __parsec_chore_t **incarnations = (__parsec_chore_t **)&(function->incarnations);

    *name_not_const                 = name;
    function->function_id           = __parsec_handle->function_counter++;
    function->nb_flows              = flow_count;
    /* set to one so that prof_grpaher prints the task id properly */
    function->nb_parameters         = 1;
    function->nb_locals             = 1;
    params[0]                       = &symb_dtd_taskid;
    locals[0]                       = &symb_dtd_taskid;
    function->data_affinity         = NULL;
    function->initial_data          = NULL;
    *priority                       = NULL;
    function->flags                 = 0x0 | PARSEC_HAS_IN_IN_DEPENDENCIES | PARSEC_USE_DEPS_MASK;
    function->dependencies_goal     = 0;
    function->key                   = (parsec_functionkey_fn_t *)DTD_identity_hash;
    *incarnations                   = (__parsec_chore_t *)dtd_chore;
    function->iterate_successors    = iterate_successors_of_dtd_task;
    function->iterate_predecessors  = NULL;
    function->release_deps          = release_deps_of_dtd;
    function->prepare_input         = data_lookup_of_dtd_task;
    function->prepare_output        = output_data_of_dtd_task;
    function->complete_execution    = complete_hook_of_dtd;
    function->release_task          = parsec_release_task_to_mempool_update_nbtasks;
    function->fini                  = NULL;

    /* Inserting Function structure in the hash table to keep track for each class of task */
    parsec_dtd_function_insert( __parsec_handle, fpointer, dtd_function );
    __parsec_handle->super.functions_array[function->function_id] = (parsec_function_t *) function;
    __parsec_handle->super.nb_functions++;
    return function;
}

/* **************************************************************************** */
/**
 * This function sets the flows in master-structure as we discover them
 *
 * @param[in,out]   __parsec_handle
 *                      DTD handle
 * @param[in]       this_task
 *                      Task to point to correct master-structure
 * @param[in]       tile_op_type
 *                      The operation type of the task on the flow
 *                      we are setting here
 * @param[in]       flow_index
 *                      The index of the flow we are setting
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
void
set_flow_in_function( parsec_dtd_handle_t *parsec_dtd_handle,
                      parsec_dtd_task_t *this_task, int tile_op_type,
                      int flow_index)
{
    (void)parsec_dtd_handle;
    int tile_type_index = tile_op_type & GET_REGION_INFO;
    parsec_flow_t* flow  = (parsec_flow_t *) calloc(1, sizeof(parsec_flow_t));
    flow->name          = "Random";
    flow->sym_type      = 0;
    flow->flow_index    = flow_index;
    flow->flow_datatype_mask = 1<<tile_type_index;

    int i;
    for (i=0; i<MAX_DEP_IN_COUNT; i++) {
        flow->dep_in[i] = NULL;
    }
    for (i=0; i<MAX_DEP_OUT_COUNT; i++) {
        flow->dep_out[i] = NULL;
    }

    if ((tile_op_type & GET_OP_TYPE) == INPUT) {
        flow->flow_flags = FLOW_ACCESS_READ;
    } else if ((tile_op_type & GET_OP_TYPE) == OUTPUT || (tile_op_type & GET_OP_TYPE) == ATOMIC_WRITE) {
        flow->flow_flags = FLOW_ACCESS_WRITE;
    } else if ((tile_op_type & GET_OP_TYPE) == INOUT) {
        flow->flow_flags = FLOW_ACCESS_RW;
    }

    int in_index = find_free_in_flow (this_task->super.function);
    parsec_flow_t **in = (parsec_flow_t **)&(this_task->super.function->in[in_index]);
    *in = flow;
    int out_index = find_free_out_flow (this_task->super.function);
    parsec_flow_t **out = (parsec_flow_t **)&(this_task->super.function->out[out_index]);
    *out = flow;
}

/* **************************************************************************** */
/**
 * This function sets the parent of a task
 *
 * This function is called by the descendant and the descendant here
 * puts itself as the descendant of the parent.
 *
 * @param[out]  parent_task
 *                  Task we are setting descendant for
 * @param[in]   parent_flow_index
 *                  Flow index of parent for which the
 *                  descendant is being set
 * @param[in]   desc_task
 *                  The descendant
 * @param[in]   desc_flow_index
 *                  Flow index of descendant
 * @param[in]   parent_op_type
 *                  Operation type of parent task on its flow
 * @param[in]   desc_op_type
 *                  Operation type of descendant task on its flow
 *
 * @ingroup     DTD_INTERFACE_INTERNAL
 */
void
set_parent(parsec_dtd_task_t *parent_task, uint8_t parent_flow_index,
           parsec_dtd_task_t *desc_task, uint8_t desc_flow_index,
           int parent_op_type, int desc_op_type)
{
    (void)desc_op_type;
    /* Setting the parent in the descendant for this flow */
    if( (parent_op_type & GET_OP_TYPE)  == INPUT ) {
        parsec_dtd_task_t *tmp_task = parent_task;
        parent_task       = parent_task->parent[parent_flow_index].task;
        parent_op_type    = tmp_task->parent[parent_flow_index].op_type;
        parent_flow_index = tmp_task->parent[parent_flow_index].flow_index;
    }

    desc_task->parent[desc_flow_index].task       = parent_task;
    desc_task->parent[desc_flow_index].op_type    = parent_op_type;
    desc_task->parent[desc_flow_index].flow_index = parent_flow_index;
}

/* **************************************************************************** */
/**
 * This function sets the descendant of a task
 *
 * This function is called by the descendant and the descendant here
 * puts itself as the descendant of the parent.
 *
 * @param[out]  parent_task
 *                  Task we are setting descendant for
 * @param[in]   parent_flow_index
 *                  Flow index of parent for which the
 *                  descendant is being set
 * @param[in]   desc_task
 *                  The descendant
 * @param[in]   desc_flow_index
 *                  Flow index of descendant
 * @param[in]   parent_op_type
 *                  Operation type of parent task on its flow
 * @param[in]   desc_op_type
 *                  Operation type of descendant task on its flow
 *
 * @ingroup     DTD_INTERFACE_INTERNAL
 */
void
set_descendant(parsec_dtd_task_t *parent_task, uint8_t parent_flow_index,
               parsec_dtd_task_t *desc_task, uint8_t desc_flow_index,
               int parent_op_type, int desc_op_type)
{
    (void)parent_op_type;
    parent_task->desc[parent_flow_index].flow_index = desc_flow_index;
    parent_task->desc[parent_flow_index].op_type    = desc_op_type;
    parsec_mfence();
    parent_task->desc[parent_flow_index].task       = desc_task;

    /* Setting the parent in the descendant for this flow */
    set_parent(parent_task, parent_flow_index,
               desc_task, desc_flow_index,
               parent_op_type, desc_op_type);
}

/* **************************************************************************** */
/**
 * Function to push ready tasks in PaRSEC's scheduler
 *
 * We only push when we reach a window size. Window size start from 1
 * and keeps on doubling until a threshold. We also call this function
 * once at the end to schedule remainder tasks, that did not fit in
 * the window.
 *
 * @param[in,out]   __parsec_handle
 *                      DTD handle
 *
 * @ingroup         DTD_INTERFACE_INTERNAL
 */
void
schedule_tasks (parsec_dtd_handle_t *__parsec_handle)
{
    parsec_execution_context_t **startup_list = __parsec_handle->startup_list;
    parsec_list_t temp;

    OBJ_CONSTRUCT( &temp, parsec_list_t );
    for(int p = 0; p < vpmap_get_nb_vp(); p++) {
        if( NULL == startup_list[p] ) continue;

        /* Order the tasks by priority */
        parsec_list_chain_sorted(&temp, (parsec_list_item_t*)startup_list[p],
                                parsec_execution_context_priority_comparator);
        startup_list[p] = (parsec_execution_context_t*)parsec_list_nolock_unchain(&temp);
        /* We should add these tasks on the system queue when there is one */
        __parsec_schedule( __parsec_handle->super.context->virtual_processes[p]->execution_units[0],
                          startup_list[p], 0 );
        startup_list[p] = NULL;
    }
    OBJ_DESTRUCT(&temp);
}

/* **************************************************************************** */
/**
 * Create and initialize a dtd task
 *
 */
parsec_dtd_task_t *
create_and_initialize_dtd_task( parsec_dtd_handle_t *parsec_dtd_handle,
                                parsec_function_t   *function)
{
    int i;
    assert( NULL != parsec_dtd_handle );
    assert( NULL != function );

    parsec_mempool_t *dtd_task_mempool = ((parsec_dtd_function_t*)function)->context_mempool;

    /* Creating Task object */
    parsec_dtd_task_t *this_task = (parsec_dtd_task_t *)parsec_thread_mempool_allocate(dtd_task_mempool->thread_mempools);

    for( i = 0; i < function->nb_flows; i++ ) {
        this_task->flow[i].op_type        = 0;
        this_task->flow[i].tile           = NULL;

        this_task->desc[i].op_type        = 0;
        this_task->desc[i].flow_index     = -1;
        this_task->desc[i].task           = NULL;

        this_task->dont_skip_releasing_data[i] = 0;
    }

    this_task->orig_task = NULL;
    this_task->super.parsec_handle    = (parsec_handle_t*)parsec_dtd_handle;
    this_task->super.super.key       = parsec_atomic_add_32b((int *)&(parsec_dtd_handle->task_id), 1);
    /* this is needed for grapher to work properly */
    this_task->super.locals[0].value = (int)this_task->super.super.key;
    this_task->super.function        = function;
    /**
     * +1 to make sure the task cannot be completed by the potential predecessors,
     * before we are completely done with it here. As we have an atomic operation
     * in all cases, increasing the expected flows by one will have no impact on
     * the performance.
     * */
    this_task->flow_count     = this_task->super.function->nb_flows + 1;
    this_task->super.priority = 0;
    this_task->super.chore_id = 0;
    this_task->super.status   = PARSEC_TASK_STATUS_NONE;

    return this_task;
}

/* **************************************************************************** */
/**
 * Function to set parameters of a dtd task
 *
 */
void
set_params_of_task( parsec_dtd_task_t *this_task, parsec_dtd_tile_t *tile,
                    int tile_op_type, int *flow_index, void **current_val,
                    parsec_dtd_task_param_t *current_param, int *next_arg )
{
    /* We pack the task pointer and flow information together to avoid updating multiple fields
     * atomically. Currently the last 4 bits are available and hence we can not deal with flow exceeding 16
     */
    assert( *flow_index < 16 );

    if( (tile_op_type & GET_OP_TYPE) == INPUT  ||
        (tile_op_type & GET_OP_TYPE) == OUTPUT ||
        (tile_op_type & GET_OP_TYPE) == INOUT  ||
        (tile_op_type & GET_OP_TYPE) == ATOMIC_WRITE)
    {
        current_param->pointer_to_tile = (void *)tile;

        this_task->super.data[*flow_index].data_in   = NULL;
        this_task->super.data[*flow_index].data_out  = NULL;
        this_task->super.data[*flow_index].data_repo = NULL;

        /* Saving tile pointer for each flow in a task */
        this_task->flow[*flow_index].tile    = tile;
        this_task->flow[*flow_index].op_type = tile_op_type;

        *flow_index += 1;
    } else if ((tile_op_type & GET_OP_TYPE) == SCRATCH) {
        if(NULL == tile) {
            current_param->pointer_to_tile = *current_val;
           *current_val = ((char*)*current_val) + *next_arg;
        }else {
            current_param->pointer_to_tile = (void *)tile;
        }
    } else {
        memcpy(*current_val, (void *)tile, *next_arg);
        current_param->pointer_to_tile = *current_val;
       *current_val = ((char*)*current_val) + *next_arg;
    }
}

/* **************************************************************************** */
/**
 * Body of fake task we insert before every INPUT task that reads
 * from memory
 *
 * @param   context, this_task
 *
 * @ingroup DTD_INTERFACE_INTERNAL
 */
int
fake_first_out_body( parsec_execution_unit_t *context, parsec_execution_context_t *this_task)
{
    (void)context; (void)this_task;
    return PARSEC_HOOK_RETURN_DONE;
}

/* **************************************************************************** */
/**
 * Function to insert dtd task in PaRSEC
 *
 * In this function we track all the dependencies and create the DAG
 *
 */
void
parsec_insert_dtd_task( parsec_dtd_task_t *this_task )
{
    const parsec_function_t *function     =  this_task->super.function;
    parsec_dtd_handle_t *parsec_dtd_handle = (parsec_dtd_handle_t *)this_task->super.parsec_handle;

    int flow_index, satisfied_flow = 0, tile_op_type = 0;
    static int vpid = 0;
    parsec_dtd_tile_t *tile = NULL;

    /* In the next segment we resolve the dependencies of each flow */
    for( flow_index = 0, tile = NULL, tile_op_type = 0; flow_index < function->nb_flows; flow_index ++ ) {
        parsec_dtd_tile_user_t last_user;
        tile = this_task->flow[flow_index].tile;
        tile_op_type = this_task->flow[flow_index].op_type;

        if(0 == parsec_dtd_handle->flow_set_flag[function->function_id]) {
            /* Setting flow in function structure */
            set_flow_in_function( parsec_dtd_handle, this_task, tile_op_type, flow_index);
        }

        if( NULL == tile ) {
            satisfied_flow++;
            continue;
        }

        /* Locking the last_user of the tile */
        parsec_dtd_last_user_lock( &(tile->last_user) );

        if( (NULL == tile->last_user.task) && (tile_op_type & GET_OP_TYPE) == INPUT ) {
            /* This tile should be unlocked during the insertion of the fake task */
            parsec_dtd_last_user_unlock( &(tile->last_user) );

            assert(tile == parsec_dtd_tile_find( parsec_dtd_handle, tile->key,
                                                 tile->ddesc ));
            //OBJ_RETAIN(tile); /* Recreating the effect of inserting a real task using the tile */
            /* parentless */
            /* Create Fake output_task */
            parsec_insert_task( (parsec_dtd_handle_t *)this_task->super.parsec_handle,
                                &fake_first_out_body,  "Fake_FIRST_OUT",
                                PASSED_BY_REF, tile, INOUT | REGION_FULL | AFFINITY,
                                0 );

            parsec_dtd_last_user_lock( &(tile->last_user) );
        }
        /* Reading the last_user info */
        last_user.task          = tile->last_user.task;
        last_user.flow_index    = tile->last_user.flow_index;
        last_user.op_type       = tile->last_user.op_type;
        last_user.alive         = tile->last_user.alive;

        /* Setting the last_user info with info of this_task */
        tile->last_user.task        = this_task;
        tile->last_user.flow_index  = flow_index;
        tile->last_user.op_type     = tile_op_type;
        tile->last_user.alive       = TASK_IS_ALIVE;
        /* Unlocking the last_user of the tile */
        parsec_dtd_last_user_unlock( &(tile->last_user) );

        /* TASK_IS_ALIVE indicates we have a parent */
        if(TASK_IS_ALIVE == last_user.alive) {
            set_descendant(last_user.task, last_user.flow_index,
                           this_task, flow_index, last_user.op_type,
                           tile_op_type);

            /* Are we using the same data multiple times for the same task? */
            if(last_user.task == this_task) {
                satisfied_flow += 1;
                this_task->super.data[flow_index].data_in = tile->data_copy;

                /* What if we have the same task using the same data in different flows
                 * with the corresponding  operation type on the data : R then W, we are
                 * doomed and this is to not get doomed
                 */
                if( ((tile_op_type & GET_OP_TYPE) == OUTPUT || (tile_op_type & GET_OP_TYPE) == INOUT)
                    && (last_user.op_type & GET_OP_TYPE) == INPUT ) {
                    (void)parsec_atomic_add_32b( (int *)&(this_task->super.data[flow_index].data_in->readers) , -1 );
                }
            }

#if defined (WILL_USE_IN_DISTRIBUTED)
            set_dependencies_for_function( (parsec_handle_t *)parsec_dtd_handle,
                                           (parsec_function_t *)last_user.task->super.function,
                                           (parsec_function_t *)this_task->super.function,
                                           last_user.flow_index, flow_index );
#endif
        } else {  /* Have parent, but parent is not alive */
            this_task->super.data[flow_index].data_in = tile->data_copy;
            satisfied_flow += 1;

            set_parent( last_user.task, last_user.flow_index,
                        this_task, flow_index, last_user.op_type,
                        tile_op_type );

            if( INPUT == (tile_op_type & GET_OP_TYPE) ) {
                /* Saving the Flow for which a Task is the first one to
                 * use the data and the operation is INPUT or ATOMIC_WRITE
                 */
                this_task->dont_skip_releasing_data[flow_index] = 1;
                (void)parsec_atomic_add_32b( (int *)&(this_task->super.data[flow_index].data_in->readers) , 1 );
            }

#if defined (WILL_USE_IN_DISTRIBUTED)
            if((tile_op_type & GET_OP_TYPE) == INPUT || (tile_op_type & GET_OP_TYPE) == INOUT) {
                set_dependencies_for_function( (parsec_handle_t *)parsec_dtd_handle, NULL,
                                               (parsec_function_t *)this_task->super.function,
                                                0, flow_index );
            }
#endif
        }
    }

    parsec_dtd_handle->flow_set_flag[function->function_id] = 1;

    (void)parsec_atomic_add_32b((int *)&(parsec_dtd_handle->super.nb_tasks), 1);

#if defined(DEBUG_HEAVY)
    parsec_dtd_task_insert( parsec_dtd_handle, this_task );
#endif

    /* Increase the count of satisfied flows to counter-balance the increase in the
     * number of expected flows done during the task creation.  */
    satisfied_flow++;

    if(!parsec_dtd_handle->super.context->active_objects) {
        assert(0);
    }

    /* Building list of initial ready task */
    if ( 0 == parsec_atomic_add_32b((int *)&(this_task->flow_count), -satisfied_flow) ) {
#if defined(DEBUG_HEAVY)
        parsec_dtd_task_release( parsec_dtd_handle, this_task->super.super.key );
#endif
        if(dump_traversal_info) {
            parsec_output(parsec_debug_output, "------\ntask Ready: %s \t %lld\nTotal flow: %d  flow_count:"
                         "%d\n-----\n", this_task->super.function->name, this_task->super.super.key,
                         this_task->super.function->nb_flows, this_task->flow_count);
        }

        PARSEC_LIST_ITEM_SINGLETON(this_task);
        if(NULL != parsec_dtd_handle->startup_list[vpid]) {
            parsec_list_item_ring_merge((parsec_list_item_t *) this_task,
                                       (parsec_list_item_t *) (parsec_dtd_handle->startup_list[vpid]));
        }
        parsec_dtd_handle->startup_list[vpid] = (parsec_execution_context_t *)this_task;
        vpid = (vpid+1)%parsec_dtd_handle->super.context->nb_vp;
    }

    if( OVERLAPPED == parsec_dtd_handle->mode ) {
        if( (this_task->super.super.key % parsec_dtd_handle->task_window_size) == 0 ) {
            schedule_tasks (parsec_dtd_handle);
            if ( parsec_dtd_handle->task_window_size <= dtd_window_size ) {
                 parsec_dtd_handle->task_window_size *= 2;
            } else {
#if defined (OVERLAP)
                parsec_execute_and_come_back (parsec_dtd_handle->super.context, &parsec_dtd_handle->super);
#endif
            }
        }
    } else if( NOT_OVERLAPPED == parsec_dtd_handle->mode ) {
        schedule_tasks (parsec_dtd_handle);
    }
}

/* **************************************************************************** */
/**
 * Function to insert task in PaRSEC
 *
 * Each time the user calls it a task is created with the
 * respective parameters the user has passed. For each task
 * class a structure known as "function" is created as well.
 * (e.g. for Cholesky 4 function structures are created for
 * each task class). The flow of data from each task to others
 * and all other dependencies are tracked from this function.
 *
 * @param[in,out]   __parsec_handle
 *                      DTD handle
 * @param[in]       fpointer
 *                      The pointer to the body of the task
 * @param[in]       name
 *                      The name of the task
 * @param[in]       ...
 *                      Variadic parameters of the task
 *
 * @ingroup         DTD_INTERFACE
 */
void
parsec_insert_task( parsec_dtd_handle_t  *parsec_dtd_handle,
                       parsec_dtd_funcptr_t *fpointer,
                       char *name_of_kernel, ... )
{
    va_list args, args_for_size;
    int next_arg, tile_op_type, flow_index = 0;
    void *tile;

    va_start(args, name_of_kernel);

    /* Creating master function structures */
    /* Hash table lookup to check if the function structure exists or not */
    parsec_function_t *function = (parsec_function_t *) parsec_dtd_function_find
                                                     (parsec_dtd_handle, fpointer);

    if( NULL == function ) {
        /* calculating the size of parameters for each task class*/
        int flow_count_of_template       = 0;
        int count_of_params_sent_by_user = 0;
        long unsigned int size_of_params = 0;

        va_copy(args_for_size, args);
        next_arg = va_arg(args_for_size, int);

        while( next_arg != 0 ) {
            tile         = va_arg(args_for_size, void *);
            tile_op_type = va_arg(args_for_size, int);
            count_of_params_sent_by_user++;

            if( (tile_op_type & GET_OP_TYPE) == VALUE || (tile_op_type & GET_OP_TYPE) == SCRATCH ) {
                size_of_params += next_arg;
            } else {
                flow_count_of_template++;
            }
            next_arg = va_arg(args_for_size, int);
        }

        va_end(args_for_size);

        if (dump_function_info) {
            parsec_output(parsec_debug_output, "Function Created for task Class: %s\n Has %d parameters\n"
                         "Total Size: %lu\n", name_of_kernel, count_of_params_sent_by_user, size_of_params);
        }

        function = create_function(parsec_dtd_handle, fpointer, name_of_kernel, count_of_params_sent_by_user,
                                   size_of_params, flow_count_of_template);

#if defined(PARSEC_PROF_TRACE)
        add_profiling_info(parsec_dtd_handle, function, name_of_kernel, flow_index);
#endif /* defined(PARSEC_PROF_TRACE) */
    }

    parsec_dtd_task_t *this_task = create_and_initialize_dtd_task(parsec_dtd_handle, function);

    /* Iterating through the parameters of the task */
    parsec_dtd_task_param_t *head_of_param_list, *current_param, *tmp_param = NULL;
    void *value_block, *current_val;

    /* Getting the pointer to allocated memory by mempool */
    head_of_param_list = GET_HEAD_OF_PARAM_LIST(this_task);
    current_param      = head_of_param_list;
    value_block        = GET_VALUE_BLOCK(head_of_param_list, ((parsec_dtd_function_t*)function)->count_of_params);
    current_val        = value_block;
    this_task->param_list = head_of_param_list;

    next_arg = va_arg(args, int);

    while(next_arg != 0) {
        tile         = (parsec_dtd_tile_t *)va_arg(args, void *);
        tile_op_type = va_arg(args, int);

        set_params_of_task( this_task, tile, tile_op_type,
                            &flow_index, &current_val,
                            current_param, &next_arg );

        tmp_param = current_param;
        current_param = current_param + 1;
        tmp_param->next = current_param;

        next_arg = va_arg(args, int);
    }

    if( tmp_param != NULL )
        tmp_param->next = NULL;
    va_end(args);

    parsec_insert_dtd_task( this_task );
}
