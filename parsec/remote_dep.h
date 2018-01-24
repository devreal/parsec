/*
 * Copyright (c) 2009-2018 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */
#ifndef __USE_REMOTE_DEP_H__
#define __USE_REMOTE_DEP_H__

/** @addtogroup parsec_internal_communication
 *  @{
 */

#include "parsec/class/lifo.h"
#include "parsec/parsec_description_structures.h"

typedef unsigned long remote_dep_datakey_t;

#define PARSEC_ACTION_DEPS_MASK                  0x00FFFFFF
#define PARSEC_ACTION_RELEASE_LOCAL_DEPS         0x01000000
#define PARSEC_ACTION_RELEASE_LOCAL_REFS         0x02000000
#define PARSEC_ACTION_GET_REPO_ENTRY             0x04000000
#define PARSEC_ACTION_SEND_INIT_REMOTE_DEPS      0x10000000
#define PARSEC_ACTION_SEND_REMOTE_DEPS           0x20000000
#define PARSEC_ACTION_RECV_INIT_REMOTE_DEPS      0x40000000
#define PARSEC_ACTION_RELEASE_REMOTE_DEPS        (PARSEC_ACTION_SEND_INIT_REMOTE_DEPS | PARSEC_ACTION_SEND_REMOTE_DEPS)

typedef struct remote_dep_wire_activate_s
{
    remote_dep_datakey_t deps;         /**< a pointer to the dep structure on the source */
    remote_dep_datakey_t output_mask;  /**< the mask of the output dependencies satisfied by this activation message */
    remote_dep_datakey_t tag;
    uint32_t             taskpool_id;
    uint32_t             task_class_id;
    uint32_t             length;
    assignment_t         locals[MAX_LOCAL_COUNT];
} remote_dep_wire_activate_t;

typedef struct remote_dep_wire_get_s
{
    remote_dep_datakey_t deps;
    remote_dep_datakey_t output_mask;
    remote_dep_datakey_t tag;
} remote_dep_wire_get_t;

/**
 * This structure holds the key information for any data mouvement. It contains the arena
 * where the data is allocated from, or will be allocated from. It also contains the
 * pointer to the buffer involved in the communication (or NULL if the data will be
 * allocated before the reception). Finally, it contains the triplet allowing a correct send
 * or receive operation: the memory layout, the number fo repetitions and the displacement
 * from the data pointer where the operation will start. If the memory layout is NULL the
 * one attached to the arena must be used instead.
 */
struct parsec_dep_data_description_s {
    struct parsec_arena_s     *arena;
    struct parsec_data_copy_s *data;
    parsec_datatype_t          layout;
    uint64_t                  count;
    int64_t                   displ;
};

struct remote_dep_output_param_s {
    /** Never change this structure without understanding the
     *   "subtle" relation with remote_deps_allocation_init in
     *  remote_dep.c
     */
    parsec_list_item_t                    super;
    parsec_remote_deps_t                 *parent;
    struct parsec_dep_data_description_s  data;       /**< The data propagated by this message. */
    uint32_t                             deps_mask;   /**< A bitmask of all the output dependencies
                                                       *   propagated by this message. The bitmask uses
                                                       *   depedencies indexes not flow indexes. */
    int32_t                              priority;    /**< the priority of the message */
    uint32_t                             count_bits;  /**< The number of participants */
    uint32_t*                            rank_bits;   /**< The array of bits representing the propagation path */
};

struct parsec_remote_deps_s {
    parsec_list_item_t               super;
    parsec_lifo_t                   *origin;        /**< The memory arena where the data pointer is comming from */
    struct parsec_taskpool_s        *taskpool;      /**< parsec taskpool generating this data transfer */
    int32_t                          pending_ack;   /**< Number of releases before completion */
    int32_t                          from;          /**< From whom we received the control */
    int32_t                          root;          /**< The root of the control message */
    uint32_t                         incoming_mask; /**< track all incoming actions (receives) */
    uint32_t                         outgoing_mask; /**< track all outgoing actions (send) */
    remote_dep_wire_activate_t       msg;           /**< A copy of the message control */
    int32_t                          max_priority;
    int32_t                          priority;
    uint32_t                        *remote_dep_fw_mask;  /**< list of peers already notified about
                                                           * the control sequence (only used for control messages) */
    struct data_repo_entry_s        *repo_entry;
    struct remote_dep_output_param_s output[1];
};
/* { item .. remote_dep_fw_mask (points to fw_mask_bitfield),
 *   output[0] .. output[max_deps < MAX_PARAM_COUNT],
 *   (max_dep_count x (np+31)/32 uint32_t) rank_bits
 *   ((np+31)/32 x uint32_t) fw_mask_bitfield } */

/* This int can take the following values:
 * - negative: no communication engine has been enabled
 * - 0: the communication engine is not turned on
 * - positive: the meaning is defined by the communication engine.
 */
extern int parsec_communication_engine_up;

#if defined(DISTRIBUTED)

typedef struct {
    parsec_lifo_t freelist;
    uint32_t     max_dep_count;
    uint32_t     max_nodes_number;
    uint32_t     elem_size;
} parsec_remote_dep_context_t;

extern parsec_remote_dep_context_t parsec_remote_dep_context;

void remote_deps_allocation_init(int np, int max_deps);
void remote_deps_allocation_fini(void);

parsec_remote_deps_t* remote_deps_allocate( parsec_lifo_t* lifo );

#define PARSEC_ALLOCATE_REMOTE_DEPS_IF_NULL(REMOTE_DEPS, TASK, COUNT) \
    if( NULL == (REMOTE_DEPS) ) { /* only once per function */                 \
        (REMOTE_DEPS) = (parsec_remote_deps_t*)remote_deps_allocate(&parsec_remote_dep_context.freelist); \
    }

/* This returns the deps to the freelist, no use counter */
void remote_deps_free(parsec_remote_deps_t* deps);

int parsec_remote_dep_init(parsec_context_t* context);
int parsec_remote_dep_fini(parsec_context_t* context);
int parsec_remote_dep_on(parsec_context_t* context);
int parsec_remote_dep_off(parsec_context_t* context);

/* Poll for remote completion of tasks that would enable some work locally */
int parsec_remote_dep_progress(parsec_execution_stream_t* es);

/* Inform the communication engine from the creation of new taskpools */
int parsec_remote_dep_new_taskpool(parsec_taskpool_t* tp);

/* Send remote dependencies to target processes */
int parsec_remote_dep_activate(parsec_execution_stream_t* es,
                               const parsec_task_t* origin,
                               parsec_remote_deps_t* remote_deps,
                               uint32_t propagation_mask);

/* Memcopy a particular data using datatype specification */
void parsec_remote_dep_memcpy(parsec_taskpool_t* tp,
                             parsec_data_copy_t *dst,
                             parsec_data_copy_t *src,
                             parsec_dep_data_description_t* data);

/* This function adds a command in the commnad queue to activate
 * release_deps of dep we had to delay in DTD runs.
 */
int
remote_dep_dequeue_delayed_dep_release(parsec_remote_deps_t *deps);

/* This function creates a fake eu for comm thread for profiling DTD runs */
void
remote_dep_mpi_initialize_execution_stream(parsec_context_t *context);

#ifdef PARSEC_DIST_COLLECTIVES
/* Propagate an activation order from the current node down the original tree */
int parsec_remote_dep_propagate(parsec_execution_stream_t* es,
                               const parsec_task_t* task,
                               parsec_remote_deps_t* deps);
#endif

/* This function sends a delayed deps */
int parsec_remote_dep_send(int rank, parsec_remote_deps_t *deps);

#else
#define parsec_remote_dep_init(ctx)            1
#define parsec_remote_dep_fini(ctx)            0
#define parsec_remote_dep_on(ctx)              0
#define parsec_remote_dep_off(ctx)             0
#define parsec_remote_dep_progress(ctx)        0
#define parsec_remote_dep_activate(ctx, o, r) -1
#define parsec_remote_dep_new_taskpool(ctx)    0
#define remote_dep_mpi_initialize_execution_stream(ctx) 0
#define parsec_remote_dep_send(r, deps)        0
#endif /* DISTRIBUTED */

/**
 * @brief Sends a protocol-specific message to another process
 *
 * @details this function schedules the emission
 *   of the message passed as parameter towards the destination rank.
 *  @param[IN] dst_rank the destination process' rank
 *  @param[IN] tag the tag of the message (same tag as the one returned by
 *             parsec_comm_register_callback)
 *  @param[IN] tp the taskpool related to the message (NULL if that
 *        type of message is not linked to a taskpool -- see parsec_comm_register_callback)
 *  @param[IN] msg the message to send
 *  @param[IN] size the number of bytes to send
 *  @return PARSEC_SUCCESS if the message is scheduled to be sent, a
 *    (fatal) error notification otherwise
 */
int parsec_comm_send_message(int dst_rank, int tag, const parsec_taskpool_t *tp, const void *msg, size_t size);

/**
 * @brief Sends a protocol-specific message to another process with delay
 *
 * @details this function schedules the emission
 *   of the message passed as parameter towards the destination rank, but not before
 *   some delay has passed
 *  @param[IN] dst_rank the destination process' rank
 *  @param[IN] tag the tag of the message (same tag as the one returned by
 *             parsec_comm_register_callback)
 *  @param[IN] tp the taskpool related to the message (NULL if that
 *        type of message is not linked to a taskpool -- see parsec_comm_register_callback)
 *  @param[IN] msg the message to send
 *  @param[IN] size the number of bytes to send
 *  @param[IN] ms the number of milliseconds to delay the message
 *  @return PARSEC_SUCCESS if the message is scheduled to be sent, a
 *    (fatal) error notification otherwise
 */
int parsec_comm_send_message_with_delay(int dst_rank, int tag, const parsec_taskpool_t *tp, const void *msg, size_t size, int ms);

/**
 * @brief Receives a protocol-specific message from another process
 *
 * @details the communication subsystem calls this function when a
 *          protocol-specific message is received.
 *  @param[IN] src_rank the source process' rank
 *  @param[IN] msg the message that was received
 *  @param[IN] tp the taskpool related to the message (NULL if that type
 *        of message is not linked to a taskpool -- see parsec_comm_register_callback(
 *  @param[IN] size the number of bytes received
 *  @return PARSEC_SUCCESS, or a (fatal) error notification
 */
typedef int (*parsec_comm_recv_message_t)(int src_rank, parsec_taskpool_t *tp, const void *msg, size_t size);

/**
 * @brief Register what function should be called when a protocol-specific 
 *   message of a given tag is received.
 *
 * @details This function must be called in the same order on all ranks
 *   so that it produces matching tags.
 *  @param[OUT] tag a unique protocol identifier (to pass to comm_send_message)
 *  @param[IN]  max_msg_size the number of bytes (max) of messages that can be
 *              received.
 *  @param[IN]  tp_flag does this type of message require a taskpool? If not 0,
 *              messages will take / provide a taskpool argument and will be
 *              delayed until the taskpool exists.
 *  @param[IN]  cb the function to call when a messag with this tag is received.
 *  @return PARSEC_SUCCESS, or a (fatal) error notification
 */
int parsec_comm_register_callback(int *tag, size_t max_msg_size, int tp_flag, parsec_comm_recv_message_t cb);

/**
 * @brief Unregister a callback previously registered with parsec_comm_register_callback
 *
 * @details This function must be called before parsec_fini.
 *  @param[IN] tag the unique protocol identifier returned by parsec_comm_register_callback
 *  @return PARSEC_SUCCESS, or a (fatal) error notification
 */
int parsec_comm_unregister_callback(int tag);

/** @} */

#endif /* __USE_REMOTE_DEP_H__ */
