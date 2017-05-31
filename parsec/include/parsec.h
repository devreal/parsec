/*
 * Copyright (c) 2009-2017 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#ifndef PARSEC_H_HAS_BEEN_INCLUDED
#define PARSEC_H_HAS_BEEN_INCLUDED

#ifndef PARSEC_CONFIG_H_HAS_BEEN_INCLUDED
#include "parsec_config.h"
#endif /* #ifndef PARSEC_CONFIG_H */

#if defined(PARSEC_HAVE_STDDEF_H)
#include <stddef.h>
#endif

#include "parsec/debug.h"

BEGIN_C_DECLS

/**
 * @defgroup parsec_public_runtime Runtime System
 * @ingroup parsec_public
 *   PaRSEC Core routines belonging to the PaRSEC Runtime System.
 *
 *   This is the public API of the PaRSEC runtime system. Functions of
 *   this module are used to manipulate PaRSEC highest level objects.
 *
 *  @{
 */

/** @brief Define the PaRSEC major version number */
#define PARSEC_VERSION    2
/** @brief Define the PaRSEC minor version number */
#define PARSEC_SUBVERSION 0
/** @brief For backward compatibility, major version number */
#define PARSEC_VERSION_MAJOR 2
/** @brief For backward compatibility, minor version number */
#define PARSEC_VERSION_MINOR 0

/** $brief To check if any parsec function returned error.
  *        Should be used by users to check correctness.
  */
#define PARSEC_CHECK_ERROR(rc, MSG) \
        if( rc < 0 ) {            \
            parsec_warning( "**** Error occurred in file: %s"   \
                            ":%d : "                            \
                            "%s", __FILE__, __LINE__, MSG );    \
            exit(-1);                                           \
        }                                                       \

/**
 * @brief Defines a DAG of tasks
 */
typedef struct parsec_handle_s            parsec_handle_t;

/**
 * @brief Defines a Task
 */
typedef struct parsec_execution_context_s parsec_execution_context_t;

/**
 * @brief Represents a computing element (usually a system thread)
*/
typedef struct parsec_execution_unit_s    parsec_execution_unit_t;

/**
 * @brief Holds all the resources used by PaRSEC for this process (threads, memory, ...)
 */
typedef struct parsec_context_s           parsec_context_t;

/**
 * @brief Defines shape, allocator and deallocator of temporary data transferred on the network
 */
typedef struct parsec_arena_s             parsec_arena_t;

/**
 * @brief Prototype of the allocator function
 */
typedef void* (*parsec_data_allocate_t)(size_t matrix_size);

/**
 * @brief Prototype of the deallocator function
 */
typedef void (*parsec_data_free_t)(void *data);

/**
 * @brief Global allocator function that PaRSEC uses (defaults to libc malloc)
 */
extern parsec_data_allocate_t parsec_data_allocate;

/**
 * @brief Global deallocator function that PaRSEC uses (defaults to libc free)
 */
extern parsec_data_free_t     parsec_data_free;

/**
 * @brief Create a new PaRSEC execution context
 *
 * @details
 * Create a new execution context, using the number of resources passed
 * with the arguments. Every execution happend in the context of such an
 * execution context. Several contextes can cohexist on disjoint resources
 * in same time.
 *
 * @param[in]    nb_cores the number of cores to use
 * @param[inout] pargc a pointer to the number of arguments passed in pargv
 * @param[inout] pargv an argv-like NULL terminated array of arguments to pass to
 *        the PaRSEC engine.
 * @return the newly created PaRSEC context
 */
parsec_context_t* parsec_init( int nb_cores, int* pargc, char** pargv[]);

/**
 * @brief Change the communicator to use with the context
 *
 * @details
 * Reset the remote_dep comm engine associated with the PaRSEC context, and use
 * the communication context opaque_comm_ctx in the future (typically an MPI
 * communicator).
 *
 * parsec_context_wait becomes collective accross nodes spanning
 * on this communication context.
 *
 * @param[inout] context the PaRSEC context
 * @param[in] opaque_comm_ctx the new communicator object to use
 * @return PARSEC_SUCCESS on success
 */
int parsec_remote_dep_set_ctx( parsec_context_t* context, void* opaque_comm_ctx );


/**
 * @brief Abort PaRSEC context
 *
 * @details
 * Aborts the PaRSEC context. The execution stops at resources on which the
 * context spans. The call does not return.
 *
 * @param[in] pcontext a pointer to the PaRSEC context to abort
 * @param[in] status an integer value transmitted to the OS specific abort
 * method (an exit code)
 * @return this call does not return.
 */
void parsec_abort( parsec_context_t* pcontext, int status);


/**
 * @brief Finalize a PaRSEC context
 *
 * @details
 * Complete all pending operations on the execution context, and release
 * all associated resources. Threads and acclerators attached to this
 * context will be released.
 *
 * @param[inout] pcontext a pointer to the PaRSEC context to finalize
 * @return PARSEC_SUCCESS on success
 */
int parsec_fini( parsec_context_t** pcontext );

/**
 * @brief Enqueue a PaRSEC handle into the PaRSEC context
 *
 * @details
 * Attach an execution handle on a context, in other words on the set of
 * resources associated to this particular context. A matching between
 * the capabilitis of the context and the support from the handle will be
 * done during this step, which will basically define if accelerators can
 * be used for the execution.
 *
 * @param[inout] context The parsec context where the tasks generated by the parsec_handle_t
 *                are to be executed.
 * @param[inout] dag The parsec object with pending tasks.
 *
 * @return 0 If the enqueue operation succeeded.
 */
int parsec_enqueue( parsec_context_t* context , parsec_handle_t* dag);

/**
 * @brief Start handles that were enqueued into the PaRSEC context
 *
 * @details
 * Start the runtime by allowing all the other threads to start executing.
 * This call should be paired with one of the completion calls, test or wait.
 *
 * @param[inout] context the PaRSEC context
 * @returns: 0 if the other threads in this context have been started, -1 if the
 * context was already active, -2 if there was nothing to do and no threads hav
 * been activated.
 */
int parsec_context_start(parsec_context_t* context);

/**
 * @brief Check the status of an ongoing execution, started with parsec_start
 *
 * @details
 * Check the status of an ongoing execution, started with parsec_start
 *
 * @param[inout] context The parsec context where the execution is taking place.
 *
 * @return 0 If the execution is still ongoing.
 * @return 1 If the execution is completed, and the parsec_context has no
 *           more pending tasks. All subsequent calls on the same context
 *           will automatically succeed.
 */
int parsec_context_test( parsec_context_t* context );

/**
 * @brief Complete the execution of all PaRSEC handles enqueued into this
 *        PaRSEC context
 *
 * @details
 * Progress the execution context until no further operations are available.
 * Upon return from this function, all resources (threads and accelerators)
 * associated with the corresponding context are put in a mode where they are
 * not active. New handles enqueued during the progress stage are automatically
 * taken into account, and the caller of this function will not return to the
 * user until all pending handles are completed and all other threads are in a
 * sleeping mode.
 *
 * @param[inout] context The parsec context where the execution is taking place.
 *
 * @return 0 If the execution is completed.
 * @return * Any other error raised by the tasks themselves.
 */
int parsec_context_wait(parsec_context_t* context);

/**
 * @brief Handle-callback type definition
 *
 * @details
 * The completion callback of a parsec_handle. Once the handle has been
 * completed, i.e. all the local tasks associated with the handle have
 * been executed, and before the handle is marked as done, this callback
 * will be triggered. Inside the callback the handle should not be
 * modified.
 */
typedef int (*parsec_event_cb_t)(parsec_handle_t* parsec_handle, void*);

/**
 * @brief Hook to update runtime task for each handle type
 *
 * @details
 * Each parsec handle has a counter nb_pending_action and to update
 * that counter we need a function type that will act as a hook. Each
 * handle type can attach it's own callback to get desired way to
 * update the nb_pending_action.
 */
typedef int (*parsec_update_ref_t)(parsec_handle_t *parsec_handle, int32_t);

/**
 * @brief Setter for the completion callback and data
 *
 * @details
 * Sets the complete callback of a PaRSEC handle
 *
 * @param[inout] parsec_handle the handle on which the callback should
 *               be attached
 * @param[in] complete_cb the function to call when parsec_handle is completed
 * @param[inout] complete_data the user-defined data to passe to complete_cb
 *               when it is called
 * @return PARSEC_SUCCESS on success, an error otherwise
 */

int parsec_set_complete_callback(parsec_handle_t* parsec_handle,
                                parsec_event_cb_t complete_cb, void* complete_data);

/**
 * @brief Get the current completion callback associated with a PaRSEC handle
 *
 * @details
 * Returns the current completion callback associated with a parsec_handle.
 * Typically used to chain callbacks together, when inserting a new completion
 * callback.
 *
 * @param[in] parsec_handle the PaRSEC handle on which a callback is set
 * @param[out] complete_cb a function pointer to the corresponding callback
 * @param[out] complete_data a pointer to the data called with the callback
 * @return PARSEC_SUCCESS on success
 */
int parsec_get_complete_callback(const parsec_handle_t* parsec_handle,
                                parsec_event_cb_t* complete_cb, void** complete_data);


/**
 * @brief Setter for the enqueuing callback and data
 *
 * @details
 * Sets the enqueuing callback of a PaRSEC handle
 *
 * @param[inout] parsec_handle the handle on which the callback should
 *               be attached
 * @param[in] enqueue_cb the function to call when parsec_handle is enqueued
 * @param[inout] enqueue_data the user-defined data to passe to enqueue_cb
 *               when it is called
 * @return PARSEC_SUCCESS on success, an error otherwise
 */
int parsec_set_enqueue_callback(parsec_handle_t* parsec_handle,
                               parsec_event_cb_t enqueue_cb, void* enqueue_data);

/**
 * @brief Get the current enqueuing callback associated with a PaRSEC handle
 *
 * @details
 * Returns the current completion callback associated with a parsec_handle.
 * Typically used to chain callbacks together, when inserting a new enqueuing
 * callback.
 *
 * @param[in] parsec_handle the PaRSEC handle on which a callback is set
 * @param[out] enqueue_cb a function pointer to the corresponding callback
 * @param[out] enqueue_data a pointer to the data called with the callback
 * @return PARSEC_SUCCESS on success
 */
int parsec_get_enqueue_callback(const parsec_handle_t* parsec_handle,
                               parsec_event_cb_t* enqueue_cb, void** enqueue_data);

/**
 * @brief Retrieve the local object attached to a unique object id
 *
 * @details
 * Converts a handle id into the corresponding handle.
 *
 * @param[in] handle_id the handle id to lookup
 * @return NULL if no handle is associated with this handle_id,
 *         the PaRSEC handle pointer otherwise
 */
parsec_handle_t* parsec_handle_lookup(uint32_t handle_id);

/**
 * @brief Reserve a unique ID of a handle.
 *
 * @details
 * Reverse an unique ID for the handle. Beware that on a distributed environment the
 * connected objects must have the same ID.
 *
 * @param[in] handle the PaRSEC handle for which an ID should be reserved
 * @return the handle ID of handle (allocates one if handle has no handle_id)
 */
int parsec_handle_reserve_id(parsec_handle_t* handle);

/**
 * @brief Register the object with the engine.
 *
 * @details
 * Register the object with the engine. The handle must have a unique handle, especially
 * in a distributed environment.
 *
 * @param[in] handle the handle to register
 * @return PARSEC_SUCCESS on success, an error otherwise
 */
int parsec_handle_register(parsec_handle_t* handle);

/**
 * @brief Unregister the object with the engine.
 *
 * @details
 * Unregister the object with the engine. This make the handle ID available for
 * future handles. Beware that in a distributed environment the connected objects
 * must have the same ID.
 *
 * @param[in] handle the handle to unregister
 * @return PARSEC_SUCCESS on success, an error otherwise
 */
void parsec_handle_unregister(parsec_handle_t* handle);

/**
 * @brief Globally synchronize handle IDs.
 *
 * @details
 *  Globally synchronize handle IDs so that next register generates the same
 *  id at all ranks. This is a collective over the communication object
 *  associated with PaRSEC, and can be used to resolve discrepancies introduced by
 *  handles not registered over all ranks.
*/
void parsec_handle_sync_ids(void);

/**
 * @cond FALSE
 * Sequentially compose two handles, triggering the start of next upon
 * completion of start. If start is already a composed object, then next will be
 * appended to the already existing list. These handles will execute one after
 * another as if there were sequential.  The resulting compound parsec_handle is
 * returned.
 */
parsec_handle_t* parsec_compose(parsec_handle_t* start, parsec_handle_t* next);
/** @endcond */

/**
 * @brief Free the resource allocated in the parsec handle.
 *
 * @details
 * Free the resource allocated in the parsec handle. The handle should be unregistered first.
 * @param[inout] handle the handle to free
 */
void parsec_handle_free(parsec_handle_t *handle);

/**
 * @private
 * @brief The final step of a handle activation.
 *
 * @details
 * The final step of a handle activation. At this point we assume that all the local
 * initializations have been successfully completed for all components, and that the
 * handle is ready to be registered with the system, and any potential pending tasks
 * ready to go. If distributed is non 0, then the runtime assumes that the handle has
 * a distributed scope and should be registered with the communication engine.
 *
 * The local_task allows for concurrent management of the startup_queue, and provide a way
 * to prevent a task from being added to the scheduler. As the different tasks classes are
 * initialized concurrently, we need a way to prevent the beginning of the tasks generation until
 * all the tasks classes associated with a DAG are completed. Thus, until the synchronization
 * is complete, the task generators are put on hold in the startup_queue. Once the handle is
 * ready to advance, and this is the same moment as when the handle is ready to be enabled,
 * we reactivate all pending tasks, starting the tasks generation step for all type classes.
 *
 * @param[inout] handle the handle to enable
 * @param[in] startup_queue a list of tasks that should be fed to the
 *            ready tasks of eu
 * @param[in] local_task the task that is calling parsec_handle_enable, and
 *            that might be included in the startup_queue
 * @param[inout] eu the execution unit on which the tasks should be enqueued
 * @param[in] distributed 0 if that handle is local, non zero if it exists on all ranks.
 */
int parsec_handle_enable(parsec_handle_t* handle,
                        parsec_execution_context_t** startup_queue,
                        parsec_execution_context_t* local_task,
                        parsec_execution_unit_t * eu,
                        int distributed);


/**
 * This function will block until all the tasks belonging to this handle
 * complete, and the handle completion function is called.
 * @todo Move into the PTG interface level.
 */
void parsec_ptg_handle_wait( parsec_context_t *parsec,
                             parsec_handle_t  *parsec_handle );

/**
 * @brief Print PaRSEC usage message.
 *
 * @details
 * Print PaRSEC Modular Component Architecture help message.
 */
void parsec_usage(void);

/**
 * @brief Change the priority of an entire handle
 *
 * @details
 * Allow to change the default priority of a handle. It returns the
 * old priority (the default priority of a handle is 0). This function
 * can be used during the lifetime of a handle, however, only tasks
 * generated after this call will be impacted.
 *
 * @param[inout] handle the handle to bump in priority
 * @param[in] new_priority the new priority to set to that handle
 * @return The priority of the handle before being assigned to new_priority
 */
int32_t parsec_set_priority( parsec_handle_t* handle, int32_t new_priority );

/**
 * @brief Human-readable print function for tasks
 *
 * @details
 * Prints up to size bytes into str, to provide a human-readable description
 * of task.
 * @param[out] str the string that should be initialized
 * @param[in] size the maximum bytes in str
 * @param[in] task the task to represent with str
 * @return str
 */
char* parsec_snprintf_execution_context( char* str, size_t size,
                                        const parsec_execution_context_t* task);

/**
 * @brief Opaque structure representing a Task Class
 */
struct parsec_function_s;

/**
 * @brief Opaque structure representing the parameters of a task
 */
struct assignment_s;

/**
 * @brief Human-readable print function for task parameters
 *
 * @details
 * Prints up to size bytes into str, to provide a human-readable description
 * of the task parameters locals, assuming they belong to the task class
 * function.
 *
 * @param[out] str a buffer of size bytes in which to set the assignment representation
 * @param[in] size the number of bytes of str
 * @param[in] function the task class to which locals belong
 * @param[in] locals the set of parameters of the task
 * @return str
 */
char* parsec_snprintf_assignments( char* str, size_t size,
                                  const struct parsec_function_s* function,
                                  const struct assignment_s* locals);

/**  @} */

END_C_DECLS

#endif  /* PARSEC_H_HAS_BEEN_INCLUDED */
