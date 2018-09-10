/*
 * Copyright (c) 2018      The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef _parsec_term_detect_h
#define _parsec_term_detect_h

#include "parsec.h"
#include "parsec/parsec_internal.h"
#include "parsec/mca/mca.h"

#include <stdio.h>
#include <mpi.h>

BEGIN_C_DECLS

/**
 * @ingroup parsec_internal_runtime
 * @{
 */

struct parsec_termdet_base_component_2_0_0 {
    mca_base_component_2_0_0_t      base_version;
    mca_base_component_data_2_0_0_t base_data;
};

typedef struct parsec_termdet_base_component_2_0_0 parsec_termdet_base_component_2_0_0_t;
typedef struct parsec_termdet_base_component_2_0_0 parsec_termdet_base_component_t;

/**
 * @brief possible values for a taskpool state
 *
 * @details
 *   PARSEC_TERM_TP_NOT_MONITORED: the taskpool is not monitored
 *   PARSEC_TERM_TP_NOT_READY: the taskpool is not ready, the user may still be
 *       registering work with it
 *   PARSEC_TERM_TP_BUSY: the taskpool is busy, there is still local work to
 *       process on it
 *   PARSEC_TERM_TP_IDLE: the taskpool is idle, termination has not been
 *       detected, but there is no known local work to process
 *   PARSEC_TERM_TP_TERMINATED: the global termination has been detected on
 *       that taskpool.
 *
 *  parsec_termination_monitor_taskpool:
 *    Transitions from PARSEC_TERM_TP_NOT_MONITORED to PARSEC_TERM_TP_NOT_READY
 *             or from PARSEC_TERM_TP_TERMINATED to PARSEC_TERM_TP_NOT_READY
 *  parsec_termination_taskpool_ready:
 *    Transitions from PARSEC_TERM_TP_NOT_READY to PARSEC_TERM_TP_BUSY
 *  parsec_termination_taskpool_idle:
 *    Transitions from PARSEC_TERM_TP_BUSY to PARSEC_TERM_TP_IDLE
 *  parsec_termination_incoming_message:
 *    Transitions from PARSEC_TERM_TP_IDLE to PARSEC_TERM_TP_BUSY
 *  When global termination is detected:
 *    Transitions from PARSEC_TERM_TP_IDLE to PARSEC_TERM_TP_TERMINATED
 */
typedef enum {
    PARSEC_TERM_TP_NOT_MONITORED = 0,
    PARSEC_TERM_TP_NOT_READY,
    PARSEC_TERM_TP_BUSY,
    PARSEC_TERM_TP_IDLE,
    PARSEC_TERM_TP_TERMINATED
} parsec_termdet_taskpool_state_t ;

/**
 * For some termination detectors, we provide two versions of the concept of IDLE:
 *  - if nb_tasks==0, then the runtime actions are for pending messages to be sent.
 *    Once the operation of the termination detector has noted that some work is
 *    potentially located in these outgoing messages (e.g. for the Credit Distribution
 *    Algorithm, once part of the credit has been assigned to each message), these
 *    actions have no impact on the idleness of the process. So we can declare
 *    idleness just after nb_tasks==0 and all outgoing messages have been taken
 *    into account.
 *  - if nb_tasks==0 && nb_pending_actions == 0, the process is clearly idle for
 *    the taskpool, so this is a more generic approach.
 *
 *  Which version we use needs to be known globally, because the generated code is
 *  not the same: in one case (undef TERMDET_XP_IDLE_ON_NBTASKS), we count a fake task 
 *  to prevent the processes to detect termination before all the startup tasks are
 *  discovered; in the other case (define TERMDET_XP_IDLE_ON_NBTASKS), we count
 *  a fake pending action to prevent the processes to detect termination before
 *  all the startup tasks are discovered.
 *
 *  The credit distribution algorithm is slightly better with 
 *  define TERMDET_XP_IDLE_ON_NBTASKS, but this is still experimental and being
 *  evaluated.
 */
#define TERMDET_XP_IDLE_ON_NBTASKS

/**
 * @brief Type of callbacks when termination is detected
 *
 * @details This is the prototype of functions called when the
 *   termination on a given taskpool is detected.
 *
 *   @param[INOUT] tp the taskpool on which termination is detected
 */
typedef void (*parsec_termdet_termination_detected_function_t)(parsec_taskpool_t *tp);

/**
 * @brief start monitoring the termination of a taskpool
 *
 * @details this function assumes that the taskpool starts busy (i.e.
 *   termination will not be detected at least before the user calls
 *   parsec_term_detect_idling). Moreover, the function assumes that
 *   no message has been sent or received on the taskpool yet, and
 *   that the user may still push tasks (add more local work).  All
 *   termination detectors maintain a load of number of tasks and of
 *   pending actions. If a taskpool that has been READY reaches a load
 *   of 0 for both numbers, it becomes IDLE. If the corresponding
 *   taskpool on all ranks will reach 0 without possibility to give
 *   more work to the local taskpool, the taskpool will become
 *   TERMINATED. A new taskpool starts with 0 for both numbers, but
 *   those can be changed before the taskpool is READY.
 *
 *    @param[INOUT] tp the taskpool on which the termination is monitored
 *    @param[IN]    cb the callback that should be called when the termination
 *                     is detected.
 */
typedef void (*parsec_termdet_monitor_taskpool_fn_t)(parsec_taskpool_t *tp,
                                                     parsec_termdet_termination_detected_function_t cb);

/**
 * @brief gives the current state of a monitored taskpool
 *
 * @brief as an alternative to callback mechanism, the user may poll
 *   the current state of a taskpool to detect termination.
 *   @param[IN] tp the taskpool to poll
 *   @return the current state of the taskpool (see parsec_termdet_taskpool_state_t)
 */
typedef parsec_termdet_taskpool_state_t (*parsec_termdet_taskpool_state_fn_t)(parsec_taskpool_t *tp);

/**
 * @brief Signals that no more local work will be added by the user onto
 *   that taskpool
 *
 * @details This function informs the monitor that the user is done
 *   pushing work onto that taskpool, and that remaining work is only
 *   a consequence of the communications.
 *  @param[INOUT] tp the monitored taskpool
 *  @return PARSEC_SUCCESS if used correctly; an error if called on
 *   a taskpool that is not monitored.
 */
typedef int (*parsec_termdet_taskpool_ready_fn_t)(parsec_taskpool_t *tp);

/**
 * @brief Change the number of tasks or pending actions related to
 *    this taskpool.
 *
 * @details This function informs the monitor that tasks or pending
 *    actions have been discovered (if v>0) or completed (if v<0).
 *    NB: the taskpool must be ready (i.e. no more local work
 *    can be added to the taskpool by the user before termination
 *    is detected).
 *  @param[INOUT] tp the taskpool that becomes idle
 *  @param[IN] v the increment or absolute value (depending on the
 *    function name) to change / set
 *  @return The new value of the parameter set
 */
typedef int (*parsec_termdet_taskpool_load_fn_t)(parsec_taskpool_t *tp, int v);

/**
 * @brief Signals that an application message is being sent, in relation to a 
 *   taskpool, for piggybacking
 *
 * @details This function signals the termination detection that a message
 *   is being sent in relation to a monitored taskpool. The monitor can pack 
 *   a few bytes in the message, at the given position. This call happens 
 *   when the first bytes of message are being sent (header). The process
 *   might not be busy at the time of the call, as the message is already
 *   considered to be in the network when this call happens.
 *  @param[INOUT] tp a taskpool
 *  @param[IN] dst_rank the destination rank
 *  @param[OUT] packed_buffer the packed buffer of the message
 *  @param[INOUT] position an offset in packed_buffer in which the function can
 *                pack informaiton; position is updated by the call to reflect
 *                the next available byte in packed_buffer
 *  @param[IN] buffer_size the amount of bytes available from packed_buffer
 *  @param[INOUT] comm the object used to determine the current set of processes
 *  @return PARSEC_SUCCESS except if a fatal error occurs.
 */
typedef int (*parsec_termdet_outgoing_message_pack_fn_t)(parsec_taskpool_t *tp,
                                                         int dst_rank,
                                                         char *packed_buffer,
                                                         int  *position,
                                                         int   buffer_size,
                                                         MPI_Comm comm);

/**
 * @brief Signals that an application message is being sent, in relation to a 
 *   taskpool
 *
 * @details This function signals the termination detection that a message
 *   is being sent in relation to a monitored taskpool. This call happens 
 *   before the message is sent while the process is still busy because
 *   of the task that generates that message. The function can signal
 *   the runtime engine that the message must be delayed, it is then
 *   the responsibility of the termination detection algorithm to
 *   keep a pointer to remote_deps and call remote_dep_send(rank, remote_deps)
 *   on the message when it can go.
 *  @param[INOUT] tp a taskpool
 *  @param[IN] rank the destination rank
 *  @param[IN] remote_deps a descriptor of the message to send
 *  @return 1 if the message can go now, 0 otherwise (see details).
 */
typedef int (*parsec_termdet_outgoing_message_start_fn_t)(parsec_taskpool_t *tp,
                                                          int dst_rank,
                                                          parsec_remote_deps_t *remote_deps);

/**
 * @brief Signals that an application message is being received, in
 *    relation to a monitored taskpool
 *
 * @details This functions signals the termination detection algorithm
 *    that an application message is being received in relation to a
 *    monitored taskpool. This happens the first time the
 *    communication engine notices this message (when the header is
 *    received). For simplicity of usage, it is not incorrect to call
 *    on a non-monitored taskpool, the call is then ignored. The
 *    monitor may unpack a few bytes in the message, at the given
 *    position. If the taskpool was idle, it becomes busy, and the
 *    user shall call idle again to notify that all local actions to
 *    manage this message have been completed.
 *       @param[INOUT] tp a taskpool 
 *       @param[IN] src_rank the rank of the source of the message 
 *       @param[IN] packed_buffer the packed buffer of the message
 *       @param[INOUT] position an offset in packed_buffer in which the
 *                     function may unpack information; position is
 *                     updated by the call to reflect the next available 
 *                     byte in packed_buffer
 *       @param[IN] buffer_size the amount of bytes available after packed_buffer
 *       @param[IN] msg the message descriptor
 *       @param[INOUT] comm the communication object used to define the group
 *                     of processes
 *                  
 *       @return PARSEC_SUCCESS except if a fatal error occurs.
 */
typedef int (*parsec_termdet_incoming_message_start_fn_t)(parsec_taskpool_t *tp,
                                                          int src_rank,
                                                          char *packed_buffer,
                                                          int  *position,
                                                          int   buffer_size,
                                                          const parsec_remote_deps_t *msg,
                                                          MPI_Comm comm);

/**
 * @brief Signals that an application message has been received, in
 *    relation to a monitored taskpool
 *
 * @details This functions signals the termination detection algorithm
 *    that an application message that it started received is now completely
 *    received. For simplicity of usage, it is not incorrect to call
 *    on a non-monitored taskpool, the call is then ignored. 
 *       @param[INOUT] tp a taskpool 
 *       @param[IN] msg the message descriptor
 *  @return PARSEC_SUCCESS except if a fatal error occurs.
 */
typedef int (*parsec_termdet_incoming_message_end_fn_t)(parsec_taskpool_t *tp,
                                                        const parsec_remote_deps_t *msg);

/**
 * @brief convenience function to select a dynamic termination detection module
 *
 * @details this functions uses the MCA mechanism to select the user-preferred
 *          termination detection module.
 *   @param[INOUT] tp the taskpool in which to load the module
 *   @return PARSEC_SUCCESS except if a fatal error occurs. If only the
 *           local termination detection module can be loaded, a warning
 *           will be issued, as it is assumed that a global termination
 *           detection is expected by the user.
 */
int parsec_termdet_open_dyn_module(parsec_taskpool_t *tp);

/**
 * @brief convenience function to close the termination detection component
 *        that was selected by open_dyn_module.
 *
 * @details this functions uses the MCA mechanism to close the component that
 *          was opened by open_dyn_module; it unregisters the callback messages
 *          and makes the module opened by open_dyn_module un-operable. It needs
 *          only to be called at cleaning time once.
 *   @return PARSEC_SUCCESS except if a fatal error occurs.
 */
int parsec_termdet_close_dyn_module(void);

/**
 * @brief prints out statistics on the run
 *
 * @details this function prints on the passed FILE how many messages
 *          and transitions this process executed to reach the current
 *          state (typically called when termination is detected)
 *
 *    @param[IN] tp the taskpool that was monitored
 *    @param[INOUT] fp the FILE * in which to print the statistics
 *    @return PARSEC_SUCCESS unless a fatal error occured
 */
typedef int (*parsec_termdet_write_stats_fn_t)(parsec_taskpool_t *tp, FILE *fp);

struct parsec_termdet_base_module_1_0_0_t {
    parsec_termdet_monitor_taskpool_fn_t       monitor_taskpool;
    parsec_termdet_taskpool_state_fn_t         taskpool_state;
    parsec_termdet_taskpool_ready_fn_t         taskpool_ready;
    parsec_termdet_taskpool_load_fn_t          taskpool_addto_nb_tasks;
    parsec_termdet_taskpool_load_fn_t          taskpool_addto_nb_pa;
    parsec_termdet_taskpool_load_fn_t          taskpool_set_nb_tasks;
    parsec_termdet_taskpool_load_fn_t          taskpool_set_nb_pa;
    size_t                                     outgoing_message_piggyback_size;
    parsec_termdet_outgoing_message_start_fn_t outgoing_message_start;
    parsec_termdet_outgoing_message_pack_fn_t  outgoing_message_pack;
    parsec_termdet_incoming_message_start_fn_t incoming_message_start;
    parsec_termdet_incoming_message_end_fn_t   incoming_message_end;
    parsec_termdet_write_stats_fn_t            write_stats;
};

typedef struct parsec_termdet_base_module_1_0_0_t parsec_termdet_base_module_1_0_0_t;
typedef struct parsec_termdet_base_module_1_0_0_t parsec_termdet_base_module_t;

typedef struct parsec_termdet_module_s {
    const parsec_termdet_base_component_t *component;
    parsec_termdet_base_module_t           module;
} parsec_termdet_module_t;

typedef union {
    struct {
        volatile uint32_t nb_tasks;            /**< A placeholder for the upper level to count (if necessary) the tasks
                                                *   in the taskpool. This value is checked upon each task completion by
                                                *   the runtime, to see if the taskpool is completed (a nb_tasks equal
                                                *   to zero signal a completed taskpool). However, in order to prevent
                                                *   multiple completions of the taskpool due to multiple tasks completing
                                                *   simultaneously, the runtime reuse this value (once set to zero), for
                                                *   internal purposes (in which case it is atomically set to
                                                *   PARSEC_RUNTIME_RESERVED_NB_TASKS).
                                                */
        volatile uint32_t nb_pending_actions;  /**< Internal counter of pending actions tracking all runtime
                                                *   activities (such as communications, data movement, and
                                                *   so on). Also, its value is increase by one for all the tasks
                                                *   in the taskpool. This extra reference will be removed upon
                                                *   completion of all tasks.
                                                */
    };
    volatile int64_t atomic;
} parsec_task_counter_t;

typedef struct parsec_termdet_monitor_s {
    const parsec_termdet_base_module_t *module;              /**< The module used for this taskpool */
    parsec_termdet_termination_detected_function_t callback; /**< All modules need to remember the callback */
    parsec_task_counter_t               counters;
    void                               *monitor;             /**< The taskpool-specific data for this module */
} parsec_termdet_monitor_t;

/**
 * Macro for use in components that are of type termdet
 */
#define PARSEC_TERMDET_BASE_VERSION_2_0_0 \
    MCA_BASE_VERSION_2_0_0, \
    "termdet", 2, 0, 0

#define PARSEC_TASK_COUNTER_SET_NB_TASKS(_tc, _v) ({                    \
            parsec_task_counter_t ov, nv;                               \
            do {                                                        \
                ov = (_tc);                                             \
                nv = ov;                                                \
                nv.nb_tasks = _v;                                       \
            } while( !parsec_atomic_cas_int64( &( (_tc).atomic ), ov.atomic, nv.atomic ) ); \
            PARSEC_DEBUG_VERBOSE(30, parsec_debug_output, "SET_NB_TASKS %d (%08x.%08x) %s:%d\n", (_v), nv.nb_pending_actions, nv.nb_tasks, __FILE__, __LINE__); \
            nv; })
#define PARSEC_TASK_COUNTER_SET_NB_PA(_tc, _v) ({                       \
            parsec_task_counter_t ov, nv;                               \
            do {                                                        \
                ov = (_tc);                                             \
                nv = ov;                                                \
                nv.nb_pending_actions = _v;                             \
            } while( !parsec_atomic_cas_int64( &( (_tc).atomic ), ov.atomic, nv.atomic ) ); \
            PARSEC_DEBUG_VERBOSE(30, parsec_debug_output, "SET_NB_PA %d (%08x.%08x) %s:%d\n", (_v), nv.nb_pending_actions, nv.nb_tasks, __FILE__, __LINE__); \
            nv; })
#define PARSEC_TASK_COUNTER_ADDTO_NB_TASKS(_tc, _v) ({                  \
            parsec_task_counter_t ov, nv;                               \
            do {                                                        \
                ov = (_tc);                                             \
                nv = ov;                                                \
                nv.nb_tasks += _v;                                      \
            } while( !parsec_atomic_cas_int64( &( (_tc).atomic ), ov.atomic, nv.atomic ) ); \
            PARSEC_DEBUG_VERBOSE(30, parsec_debug_output, "ADDTO_NB_TASKS %d -> %d (%08x.%08x) %s:%d\n", ov.nb_tasks, nv.nb_tasks, nv.nb_pending_actions, nv.nb_tasks, __FILE__, __LINE__); \
            nv; })
#define PARSEC_TASK_COUNTER_ADDTO_NB_PA(_tc, _v) ({                     \
            parsec_task_counter_t ov, nv;                               \
            do {                                                        \
                ov = (_tc);                                             \
                nv = ov;                                                \
                nv.nb_pending_actions += _v;                            \
            } while( !parsec_atomic_cas_int64( &( (_tc).atomic ), ov.atomic, nv.atomic ) ); \
            PARSEC_DEBUG_VERBOSE(30, parsec_debug_output, "ADDTO_NB_PA %d -> %d (%08x.%08x) %s:%d\n", ov.nb_pending_actions, nv.nb_pending_actions, nv.nb_pending_actions, nv.nb_tasks, __FILE__, __LINE__); \
            nv; })



/** @} */

END_C_DECLS

#endif /* _parsec_term_detect_h */
