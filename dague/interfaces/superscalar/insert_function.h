#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>
#include "dague/data_distribution.h"
#include "dague.h"

/* 
    **  Details of Flags **
    
 *  INPUT:          Data is used in read-only mode, no modification is done.
 *  OUTPUT:         Data is used in write-only, written only, not read.
 *  INOUT:          Data is read and written both.
 *  ATOMIC_WRITE:   Data is used like OUTPUT but the ordering of the tasks having this flag is not maintained by the scheduler.
                    It is the responsibility of the user to make sure data is written atomically. Treated like INPUT by the scheduler.
 *  SCRATCH:        Will be used by the task as scratch pad, does not effect the DAG, tells the runtime to allocate memory specified 
                    by the user.
 *  VALUE:          Tells the runtime to copy the value as a parameter of the task.

 */

#define GET_OP_TYPE 0xf00
typedef enum {  INPUT=0x100,
                OUTPUT=0x200,
                INOUT=0x300,
                ATOMIC_WRITE=0x400,
                SCRATCH=0x500,
                VALUE=0x600
             } dtd_op_type;

#define GET_REGION_INFO 0xff
typedef enum {  REGION_FULL=1<<0,/* 0x1 is reserved for default(FULL tile) */
                REGION_L=1<<1,   
                REGION_D=1<<2,
                REGION_U=1<<3
             } dtd_regions;

#define DAGUE_dtd_NB_FUNCTIONS  25
#define DTD_TASK_COUNT          10000
#define PASSED_BY_REF           1
#define UNPACK_VALUE            1
#define UNPACK_DATA             2
#define UNPACK_SCRATCH          3
#define MAX_DESC                25

#define TILE_OF(DAGUE, DDESC, I, J) \
    tile_manage(DAGUE, &(__ddesc##DDESC->super.super), I, J)


typedef struct task_param_s task_param_t;
typedef struct dtd_task_s dtd_task_t;
typedef struct dtd_tile_s dtd_tile_t;
typedef struct dague_dtd_handle_s dague_dtd_handle_t;
typedef struct dague_dtd_function_s dague_dtd_function_t;
typedef struct __dague_dtd_internal_handle_s __dague_dtd_internal_handle_t;

typedef int (task_func)(dague_execution_unit_t *, dague_execution_context_t *); /* Function pointer typeof  kernel pointer pased as parameter to insert_function() */

dtd_tile_t* tile_manage(dague_dtd_handle_t *dague_dtd_handle,
                        dague_ddesc_t *ddesc, int i, int j);

dague_dtd_handle_t* dague_dtd_new(dague_context_t *, int, int, int* );

void insert_task_generic_fptr(dague_dtd_handle_t *,
                              task_func *, char *, ...);

void dague_dtd_unpack_args(dague_execution_context_t *this_task, ...);
void dtd_destructor(__dague_dtd_internal_handle_t * handle);

void increment_task_counter(dague_dtd_handle_t *);
