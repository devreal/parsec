/*
 * Copyright (c) 2016      The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * $COPYRIGHT
 *
 * @precisions normal z -> s d c
 *
 */

#include <string.h>

#include "dplasma.h"
#include "dplasma_bcast.h"
#include "dplasmatypes.h"
#include "parsec/data_dist/matrix/two_dim_rectangle_cyclic.h"
#include "parsec/data_dist/matrix/irregular_tiled_matrix.h"
#include "dplasma_z.h"
#include "zsumma_NN.h"
#include "zsumma_NT.h"
#include "zsumma_TN.h"
#include "zsumma_TT.h"
#include "zgemm_bcast_NN.h"

typedef struct parsec_function_vampire_s {
    parsec_task_class_t super;
    parsec_hook_t    *saved_prepare_input;
    void *         (*resolve_future_function)(void*, void*, void*);
} parsec_function_vampire_t;

static int future_input_for_read_a_task(parsec_execution_stream_t * es, __parsec_zsumma_NN_READ_A_task_t * this_task)
{
    const parsec_zsumma_NN_taskpool_t *__parsec_tp = (parsec_zsumma_NN_taskpool_t *) this_task->taskpool;
    parsec_function_vampire_t *vf = (parsec_function_vampire_t*)this_task->task_class;
    parsec_data_copy_t *copy = NULL;
    void *f = NULL, *tile = NULL;
    const int m = this_task->locals.m.value;
    const int k = this_task->locals.k.value;
    /** Lookup the input data, and store them in the es if any */
    assert(NULL == this_task->data._f_A.data_in);
    copy = parsec_data_get_copy(((parsec_data_collection_t*)__parsec_tp->_g_descA)->data_of(((parsec_data_collection_t*)__parsec_tp->_g_descA), m, k), 0);
    f = PARSEC_DATA_COPY_GET_PTR(copy);
    tile = vf->resolve_future_function(f, es, this_task);
    if( NULL != tile ) {
        copy->device_private = tile;
        return vf->saved_prepare_input(es, (parsec_task_t *)this_task);
    } else {
        return PARSEC_HOOK_RETURN_ASYNC;
    }
}

static int future_input_for_read_b_task(parsec_execution_stream_t * es, __parsec_zsumma_NN_READ_B_task_t * this_task)
{
    const parsec_zsumma_NN_taskpool_t *__parsec_tp = (parsec_zsumma_NN_taskpool_t *) this_task->taskpool;
    parsec_function_vampire_t *vf = (parsec_function_vampire_t*)this_task->task_class;
    parsec_data_copy_t *copy = NULL;
    void *f = NULL, *tile = NULL;
    const int k = this_task->locals.k.value;
    const int n = this_task->locals.n.value;
    /** Lookup the input data, and store them in the es if any */
    assert(NULL == this_task->data._f_B.data_in);
    copy = parsec_data_get_copy(((parsec_data_collection_t*)__parsec_tp->_g_descB)->data_of(((parsec_data_collection_t*)__parsec_tp->_g_descB), k, n), 0);
    f = PARSEC_DATA_COPY_GET_PTR(copy);
    tile = vf->resolve_future_function(f, es, this_task);
    if( NULL != tile ) {
        copy->device_private = tile;
        return vf->saved_prepare_input(es, (parsec_task_t *)this_task);
    } else {
        return PARSEC_HOOK_RETURN_ASYNC;
    }
}

static int future_input_for_summa_task(parsec_execution_stream_t * es, __parsec_zsumma_NN_SUMMA_task_t * this_task)
{
    const parsec_zsumma_NN_taskpool_t *__parsec_tp = (parsec_zsumma_NN_taskpool_t *) this_task->taskpool;
    parsec_function_vampire_t *vf = (parsec_function_vampire_t*)this_task->task_class;
    parsec_data_copy_t *copy = NULL;
    void *f = NULL, *tile = NULL;
    const int m = this_task->locals.m.value;
    const int n = this_task->locals.n.value;
    const int k = this_task->locals.k.value;
    if(k == 0 ) {
        /** Lookup the input data, and store them in the es if any */
        assert(NULL == this_task->data._f_C.data_in);
        copy = parsec_data_get_copy(((parsec_data_collection_t*)__parsec_tp->_g_descC)->data_of(((parsec_data_collection_t*)__parsec_tp->_g_descC), m, n), 0);
        f = PARSEC_DATA_COPY_GET_PTR(copy);
        tile = vf->resolve_future_function(f, es, this_task);
        if( NULL != tile ) {
            copy->device_private = tile;
        } else {
            return PARSEC_HOOK_RETURN_ASYNC;
        }
    }
    return vf->saved_prepare_input(es, (parsec_task_t *)this_task);
}

static int future_input_for_accumulate_c_task(parsec_execution_stream_t * es, __parsec_zgemm_bcast_NN_ACCUMULATE_C_task_t * this_task)
{
    const parsec_zgemm_bcast_NN_taskpool_t *__parsec_tp = (parsec_zgemm_bcast_NN_taskpool_t *) this_task->taskpool;
    parsec_function_vampire_t *vf = (parsec_function_vampire_t*)this_task->task_class;
    parsec_data_copy_t *copy = NULL;
    void *f = NULL, *tile = NULL;
    const int m = this_task->locals.m.value;
    const int n = this_task->locals.n.value;
    const int i = this_task->locals.i.value;
    if( i == 0 ) {
        /** Lookup the input data, and store them in the es if any */
        assert(NULL == this_task->data._f_C.data_in);
        copy = parsec_data_get_copy(((parsec_data_collection_t*)__parsec_tp->_g_descC)->data_of(((parsec_data_collection_t*)__parsec_tp->_g_descC), m, n), 0);
        f = PARSEC_DATA_COPY_GET_PTR(copy);
        tile = vf->resolve_future_function(f, es, this_task);
        if( NULL != tile ) {
            copy->device_private = tile;
            return vf->saved_prepare_input(es, (parsec_task_t *)this_task);
        } else {
            return PARSEC_HOOK_RETURN_ASYNC;
        }
    }
    return vf->saved_prepare_input(es, (parsec_task_t *)this_task);
}

static void attach_futures_prepare_input(parsec_taskpool_t *tp, const char *task_name, void*(*resolve_future_function)(void*, void*, void*))
{
    int fid;
    parsec_function_vampire_t *vf;
    for(fid = 0; NULL != tp->task_classes_array[fid]; fid++) {
        if( strcmp(tp->task_classes_array[fid]->name, task_name) == 0 ) {
            break;
        }
    }
    if( NULL == tp->task_classes_array[fid] ) {
        fprintf(stderr, "%s:%d -- Internal Error: could not find a task class with name '%s' in the taskpool\n", __FILE__, __LINE__, task_name);
        assert(0);
        return;
    }
    assert(NULL != resolve_future_function);
    vf = (parsec_function_vampire_t*)malloc(sizeof(parsec_function_vampire_t));
    memcpy(&vf->super, tp->task_classes_array[fid], sizeof(parsec_task_class_t));
    asprintf((char **)&vf->super.name, "%s(vampirized)", tp->task_classes_array[fid]->name);
    vf->saved_prepare_input = vf->super.prepare_input;
    vf->resolve_future_function = resolve_future_function;
    if( strcmp(task_name, "READ_A") == 0 )
        vf->super.prepare_input = (parsec_hook_t*)future_input_for_read_a_task;
    else if( strcmp(task_name, "READ_B") == 0 )
        vf->super.prepare_input = (parsec_hook_t*)future_input_for_read_b_task;
    else if( strcmp(task_name, "SUMMA") == 0 )
        vf->super.prepare_input = (parsec_hook_t*)future_input_for_summa_task;
    else if( strcmp(task_name, "ACCUMULATE_C") == 0 )
        vf->super.prepare_input = (parsec_hook_t*)future_input_for_accumulate_c_task;
    else exit(3);
    tp->task_classes_array[fid] = (parsec_task_class_t*)vf;
}

static int
zsumma_check_operation_valid(PLASMA_enum transA, PLASMA_enum transB,
                             parsec_complex64_t alpha,
                             const irregular_tiled_matrix_desc_t *A,
                             const irregular_tiled_matrix_desc_t *B,
                             irregular_tiled_matrix_desc_t *C)
{
    (void)alpha;
    int b = 1, i;
    unsigned int *mAtiling = A->Mtiling;
    unsigned int *nAtiling = A->Ntiling;
    unsigned int *mBtiling = B->Mtiling;
    unsigned int *nBtiling = B->Ntiling;
    unsigned int *mCtiling = C->Mtiling;
    unsigned int *nCtiling = C->Ntiling;

    int Am, An, Ai, Aj, Amt, Ant;
    int Bm, Bn, Bi, Bj, Bmt, Bnt;

    /* Check input arguments */
    if ((transA != PlasmaNoTrans) && (transA != PlasmaTrans) && (transA != PlasmaConjTrans)) {
        dplasma_error("summa_zsumma", "illegal value of transA");
        return -1;
    }
    if ((transB != PlasmaNoTrans) && (transB != PlasmaTrans) && (transB != PlasmaConjTrans)) {
        dplasma_error("summa_zsumma", "illegal value of transB");
        return -2;
    }

    if ( transA == PlasmaNoTrans ) {
        Am  = A->m;
        An  = A->n;
        Ai  = A->i;
        Aj  = A->j;
        Amt = A->mt;
        Ant = A->nt;
    } else {
        Am  = A->n;
        An  = A->m;
        mAtiling = A->Ntiling;
        nAtiling = A->Mtiling;
        Ai  = A->j;
        Aj  = A->i;
        Amt = A->nt;
        Ant = A->mt;
    }

    if ( transB == PlasmaNoTrans ) {
        Bm  = B->m;
        Bn  = B->n;
        Bi  = B->i;
        Bj  = B->j;
        Bmt = B->mt;
        Bnt = B->nt;
    } else {
        Bm  = B->n;
        Bn  = B->m;
        mBtiling = B->Ntiling;
        nBtiling = B->Mtiling;
        Bi  = B->j;
        Bj  = B->i;
        Bmt = B->nt;
        Bnt = B->mt;
    }

    unsigned int *mAsubtiling = mAtiling+Ai;
    unsigned int *nAsubtiling = nAtiling+Aj;
    unsigned int *mBsubtiling = mBtiling+Bi;
    unsigned int *nBsubtiling = nBtiling+Bj;
    unsigned int *mCsubtiling = mCtiling+C->i;
    unsigned int *nCsubtiling = nCtiling+C->j;

    if (Amt != C->mt || Ant != Bmt || Bnt != C->nt) {
        dplasma_error("summa_zsumma","Symbolic tilings differ");
        return -101;
    }

    for (i = 0; i < Amt; ++i)
        if (mAsubtiling[i] != mCsubtiling[i])
            b = -102;

    for (i = 0; i < Ant; ++i)
        if (nAsubtiling[i] != mBsubtiling[i])
            b = -103;

    for (i = 0; i < Bnt; ++i)
        if (nBsubtiling[i] != nCsubtiling[i])
            b = -104;

    if (b < -100) {
        dplasma_error("summa_zsumma", "Tile sizes differ");
        return b;
    }

    if ( (Am != C->m) || (An != Bm) || (Bn != C->n) ) {
        dplasma_error("summa_zsumma", "sizes of submatrices have to match");
        return -101;
    }

    if ( (Ai != C->i) || (Aj != Bi) || (Bj != C->j) ) {
        dplasma_error("summa_zsumma", "start indexes have to match");
        return -101;
    }

    if ( !(C->dtype & irregular_tiled_matrix_desc_type) ) {
        dplasma_error("summa_zsumma", "illegal type of descriptor for C");
        return -3.;
    }

    return b;
}

parsec_taskpool_t*
dplasma_zgemm_bcast_New( PLASMA_enum transA, PLASMA_enum transB,
                         parsec_complex64_t alpha, const irregular_tiled_matrix_desc_t* A,
                         const irregular_tiled_matrix_desc_t* B,
                         irregular_tiled_matrix_desc_t* C)
{
    parsec_taskpool_t* zgemm_handle;
    parsec_arena_t* arena;
    int P, Q, m, n, i, j, k, rank;
    gemm_plan_t *plan;

    /* Check input arguments */
    if ((transA != PlasmaNoTrans)) {
        dplasma_error("summa_zgemm_bcast_New", "illegal value of transA");
        return NULL /*-1*/;
    }
    if ((transB != PlasmaNoTrans)) {
        dplasma_error("summa_zgemm_bcast_New", "illegal value of transB");
        return NULL /*-2*/;
    }
    if ( !(C->dtype & irregular_tiled_matrix_desc_type) ) {
        dplasma_error("summa_zgemm_bcast_New", "illegal type of descriptor for C (must be irregular_tiled_matrix_desc_t)");
        return NULL;
    }

    P = ((irregular_tiled_matrix_desc_t*)C)->grid.rows;
    Q = ((irregular_tiled_matrix_desc_t*)C)->grid.cols;
    plan = (gemm_plan_t*)malloc(sizeof(gemm_plan_t));
    plan->P = P;
    plan->mt = C->mt;
    plan->nt = C->nt;
    plan->kt = B->mt;
    plan->ip   = malloc(C->mt * C->nt * plan->P * sizeof(int));
    plan->prev = malloc(C->mt * C->nt * B->mt * sizeof(int));
    plan->next = malloc(C->mt * C->nt * B->mt * sizeof(int));
    int *lastk = malloc(plan->P * sizeof(int));
    for(m = 0; m < C->mt; m++) {
        for(n = 0; n < C->nt; n++) {
            for(i = 0; i < plan->P; i++)
                lastk[i] = -1;
            for(k = 0; k < B->mt; k++) {
                /* Cubic loop to determine, for each C(m, n),
                 * what are the local GEMM segments */
                rank = B->super.rank_of((parsec_data_collection_t*)B, k, n)/Q;
                plan->prev[(m*plan->nt+n)*plan->kt + k] = lastk[rank];
                if( -1 != lastk[rank] )
                    plan->next[(m*plan->nt+n)*plan->kt + lastk[rank]] = k;
                lastk[rank] = k;
            }
            /* Mark the last ones as finals */
            for(i = 0; i < plan->P; i++)
                if( -1 != lastk[i] )
                    plan->next[(m*plan->nt+n)*plan->kt + lastk[i]] = -1;
            /* Now, compute the reduction indexes:
             *  - Start with rank next to the host of C(m, n), so we can end on C(m, n)
             *    This is used in an attempt to distribute the order of reductions
             *  - Remember the last k used by each rank in the index/process array
             */
            rank = C->super.rank_of((parsec_data_collection_t*)C, m, n)/Q;
            j = 0;
            i = rank;
            do {
                i = (i+1)%P;
                if( lastk[i] != -1 ) {
                    plan->ip[(m*plan->nt+n)*plan->P + j] = lastk[i];
                    j++;
                }
            } while(i != rank);
            assert(j != 0);
            for(; j < P; j++)
                plan->ip[(m*plan->nt+n)*plan->P + j] = -1;
        }
    }
    free(lastk);

    if( PlasmaNoTrans == transA ) {
        if( PlasmaNoTrans == transB ) {
            parsec_zgemm_bcast_NN_taskpool_t* handle;
            handle = parsec_zgemm_bcast_NN_new(GEMM_BCAST_NN, transA, transB, alpha,
                                               (const irregular_tiled_matrix_desc_t *)A,
                                               (const irregular_tiled_matrix_desc_t *)B,
                                               (irregular_tiled_matrix_desc_t *)C,
                                               (parsec_data_collection_t*)B,
                                               plan);
            arena = handle->arenas[PARSEC_zgemm_bcast_NN_DEFAULT_ARENA];
            zgemm_handle = (parsec_taskpool_t*)handle;
        } 
    }

    if( A->future_resolve_fct != NULL ) {
        attach_futures_prepare_input(zgemm_handle, "READ_A", A->future_resolve_fct);
    }
    if( B->future_resolve_fct != NULL ) {
        attach_futures_prepare_input(zgemm_handle, "READ_B", B->future_resolve_fct);
    }
    if( C->future_resolve_fct != NULL ) {
        attach_futures_prepare_input(zgemm_handle, "ACCUMULATE_C", C->future_resolve_fct);
    }

    parsec_datatype_t mtype;
    parsec_type_create_contiguous(1, parsec_datatype_double_complex_t, &mtype);

    parsec_arena_construct(arena, sizeof(parsec_complex64_t),
                           PARSEC_ARENA_ALIGNMENT_SSE,
                           mtype);

    return zgemm_handle;
}


/**
 *******************************************************************************
 *
 * @ingroup summa_zsumma
 *
 *  summa_zsumma_New - Generates the taskpool that performs one of the following
 *  matrix-matrix operations. WARNING: The computations are not done by this call.
 *
 *    \f[ C = \alpha [op( A )\times op( B )],
 *
 *  where op( X ) is one of
 *
 *    op( X ) = X  or op( X ) = X' or op( X ) = conjg( X' )
 *
 *  alpha is scalar, and A, B and C are matrices, with op( A )
 *  an m by k matrix, op( B ) a k by n matrix and C an m by n matrix.
 *
 *******************************************************************************
 *
 * @param[in] transA
 *          Specifies whether the matrix A is transposed, not transposed or conjugate transposed:
 *          = PlasmaNoTrans:   A is not transposed;
 *          = PlasmaTrans:     A is transposed;
 *          = PlasmaConjTrans: A is conjugate transposed.
 *
 * @param[in] transB
 *          Specifies whether the matrix B is transposed, not transposed or conjugate transposed:
 *          = PlasmaNoTrans:   B is not transposed;
 *          = PlasmaTrans:     B is transposed;
 *          = PlasmaConjTrans: B is conjugate transposed.
 *
 * @param[in] alpha
 *          alpha specifies the scalar alpha
 *
 * @param[in] A
 *          Descriptor of the distributed matrix A.
 *
 * @param[in] B
 *          Descriptor of the distributed matrix B.
 *
 *
 * @param[out] C
 *          Descriptor of the distributed matrix C.
 *          On exit, the data described by C contain the matrix (
 *          alpha*op( A )*op( B ) )
 *
 *******************************************************************************
 *
 * @return
 *          \retval NULL if incorrect parameters are given.
 *          \retval The parsec taskpool describing the operation that can be
 *          enqueued in the runtime with parsec_enqueue(). It, then, needs to be
 *          destroy with summa_zsumma_Destruct();
 *
 *******************************************************************************
 *
 * @sa dplasma_zsumma
 * @sa dplasma_zsumma_Destruct
 * @sa dplasma_csumma_New
 * @sa dplasma_dsumma_New
 * @sa dplasma_ssumma_New
 *
 ******************************************************************************/
parsec_taskpool_t*
dplasma_zsumma_New( PLASMA_enum transA, PLASMA_enum transB,
                    parsec_complex64_t alpha, const irregular_tiled_matrix_desc_t* A,
                    const irregular_tiled_matrix_desc_t* B,
                    irregular_tiled_matrix_desc_t* C)
{
    two_dim_block_cyclic_t *Cdist;
    parsec_taskpool_t* zsumma_taskpool;
    parsec_arena_t* arena;
    int P, Q, m, n;

    int Asize, Bsize, Csize;

    /* Check input arguments */
    if ((transA != PlasmaNoTrans) && (transA != PlasmaTrans) && (transA != PlasmaConjTrans)) {
        dplasma_error("summa_zsumma_New", "illegal value of transA");
        return NULL /*-1*/;
    }
    if ((transB != PlasmaNoTrans) && (transB != PlasmaTrans) && (transB != PlasmaConjTrans)) {
        dplasma_error("summa_zsumma_New", "illegal value of transB");
        return NULL /*-2*/;
    }
    if ( !(C->dtype & irregular_tiled_matrix_desc_type) ) {
        dplasma_error("summa_zsumma_New", "illegal type of descriptor for C (must be irregular_tiled_matrix_desc_t)");
        return NULL;
    }

    Asize = A->m * A->n;
    Bsize = B->m * B->n;
    Csize = C->m * C->n;

    if( ((transA == PlasmaNoTrans) && (transB == PlasmaNoTrans))
        || ((10 * (Asize + Csize) < Bsize)) ) {
        fprintf(stdout, "calling zgemm_bcast\n");
        return dplasma_zgemm_bcast_New(transA, transB, alpha, A, B, C);
    }

    P = ((irregular_tiled_matrix_desc_t*)C)->grid.rows;
    Q = ((irregular_tiled_matrix_desc_t*)C)->grid.cols;

    m = (C->mt > P) ? C->mt : P;
    n = (C->nt > Q) ? C->nt : Q;

    Cdist = (two_dim_block_cyclic_t*)malloc(sizeof(two_dim_block_cyclic_t));

        two_dim_block_cyclic_init(
            Cdist, matrix_RealDouble, matrix_Tile,
            C->super.nodes, C->super.myrank,
            1, 1, /* Dimensions of the tiles              */
            m, n, /* Dimensions of the matrix             */
            0, 0, /* Starting points (not important here) */
            m, n, /* Dimensions of the submatrix          */
            1, 1, P);
        Cdist->super.super.data_of = NULL;
        Cdist->super.super.data_of_key = NULL;

    if( PlasmaNoTrans == transA ) {
        if( PlasmaNoTrans == transB ) {
            parsec_zsumma_NN_taskpool_t* tp;
            tp = parsec_zsumma_NN_new(SUMMA_NN, transA, transB, alpha,
                                          (const irregular_tiled_matrix_desc_t *)A,
                                          (const irregular_tiled_matrix_desc_t *)B,
                                          (irregular_tiled_matrix_desc_t *)C,
                                          Cdist,
                                          0/*createC*/);
            arena = tp->arenas[PARSEC_zsumma_NN_DEFAULT_ARENA];
            zsumma_taskpool = (parsec_taskpool_t*)tp;
        } else {
            parsec_zsumma_NT_taskpool_t* tp;
            tp = parsec_zsumma_NT_new(SUMMA_NT, transA, transB, alpha,
                                          (const irregular_tiled_matrix_desc_t *)A,
                                          (const irregular_tiled_matrix_desc_t *)B,
                                          (irregular_tiled_matrix_desc_t *)C,
                                          Cdist,
                                          0);
            arena = tp->arenas[PARSEC_zsumma_NT_DEFAULT_ARENA];
            zsumma_taskpool = (parsec_taskpool_t*)tp;
        }
    } else {
        if( PlasmaNoTrans == transB ) {
            parsec_zsumma_TN_taskpool_t* tp;
            tp = parsec_zsumma_TN_new(SUMMA_TN, transA, transB, alpha,
                                          (const irregular_tiled_matrix_desc_t *)A,
                                          (const irregular_tiled_matrix_desc_t *)B,
                                          (irregular_tiled_matrix_desc_t *)C,
                                          Cdist,
                                          0);
            arena = tp->arenas[PARSEC_zsumma_TN_DEFAULT_ARENA];
            zsumma_taskpool = (parsec_taskpool_t*)tp;
        }
        else {
            parsec_zsumma_TT_taskpool_t* tp;
            tp = parsec_zsumma_TT_new(SUMMA_TT, transA, transB, alpha,
                                          (const irregular_tiled_matrix_desc_t *)A,
                                          (const irregular_tiled_matrix_desc_t *)B,
                                          (irregular_tiled_matrix_desc_t *)C,
                                          Cdist,
                                          0);
            arena = tp->arenas[PARSEC_zsumma_TT_DEFAULT_ARENA];
            zsumma_taskpool = (parsec_taskpool_t*)tp;
        }
    }


    if( A->future_resolve_fct != NULL ) {
        attach_futures_prepare_input(zsumma_taskpool, "READ_A", A->future_resolve_fct);
    }
    if( B->future_resolve_fct != NULL ) {
        attach_futures_prepare_input(zsumma_taskpool, "READ_B", B->future_resolve_fct);
    }
    if( C->future_resolve_fct != NULL ) {
        attach_futures_prepare_input(zsumma_taskpool, "SUMMA", C->future_resolve_fct);
    }

    parsec_datatype_t mtype;
    parsec_type_create_contiguous(1, parsec_datatype_double_complex_t, &mtype);

    parsec_arena_construct(arena, sizeof(parsec_complex64_t),
                           PARSEC_ARENA_ALIGNMENT_SSE,
                           mtype);

    return zsumma_taskpool;
}

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 *  summa_zsumma_Destruct - Free the data structure associated to an taskpool
 *  created with dplasma_zsumma_New().
 *
 *******************************************************************************
 *
 * @param[in,out] tp
 *          On entry, the taskpool to destroy.
 *          On exit, the taskpool cannot be used anymore.
 *
 *******************************************************************************
 *
 * @sa dplasma_zsumma_New
 * @sa dplasma_zsumma
 *
 ******************************************************************************/
void
dplasma_zsumma_Destruct( parsec_taskpool_t *tp )
{
    parsec_zsumma_NN_taskpool_t *zsumma_taskpool = (parsec_zsumma_NN_taskpool_t *)tp;
    if( zsumma_taskpool->_g_summa_type == SUMMA_NN ||
        zsumma_taskpool->_g_summa_type == SUMMA_NT ||
        zsumma_taskpool->_g_summa_type == SUMMA_TN ||
        zsumma_taskpool->_g_summa_type == SUMMA_TT ) {
        if ( zsumma_taskpool->_g_Cdist != NULL ) {
            parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)(zsumma_taskpool->_g_Cdist) );
            free( (parsec_tiled_matrix_dc_t*)zsumma_taskpool->_g_Cdist );
            zsumma_taskpool->_g_Cdist = NULL;
        }
        parsec_arena_t *arena = ((parsec_zsumma_NN_taskpool_t *)tp)->arenas[PARSEC_zsumma_NN_DEFAULT_ARENA];
        if (arena)
            parsec_matrix_del2arena( ((parsec_zsumma_NN_taskpool_t *)tp)->arenas[PARSEC_zsumma_NN_DEFAULT_ARENA] );
    }
    if( zsumma_taskpool->_g_summa_type == GEMM_BCAST_NN ) {
        parsec_arena_t *arena = ((parsec_zgemm_bcast_NN_taskpool_t *)tp)->arenas[PARSEC_zgemm_bcast_NN_DEFAULT_ARENA];
        if (arena)
            parsec_matrix_del2arena( ((parsec_zgemm_bcast_NN_taskpool_t *)tp)->arenas[PARSEC_zgemm_bcast_NN_DEFAULT_ARENA] );
    }
    parsec_taskpool_free(tp);
}

void
dplasma_zsumma_recursive_Destruct(parsec_taskpool_t *tp)
{
    parsec_zsumma_NN_taskpool_t *zsumma_taskpool = (parsec_zsumma_NN_taskpool_t *)tp;
    if ( zsumma_taskpool->_g_Cdist != NULL ) {
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)(zsumma_taskpool->_g_Cdist) );
        free( (parsec_tiled_matrix_dc_t*)zsumma_taskpool->_g_Cdist );
    }
    parsec_taskpool_free(tp);
}

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 *  summa_zsumma - Performs one of the following matrix-matrix operations
 *
 *    \f[ C = \alpha [op( A )\times op( B )],
 *
 *  where op( X ) is one of
 *
 *    op( X ) = X  or op( X ) = X' or op( X ) = conjg( X' )
 *
 *  alpha is scalar, and A, B and C  are matrices, with op( A )
 *  an m by k matrix, op( B ) a k by n matrix and C an m by n matrix.
 *
 *******************************************************************************
 *
 * @param[in,out] parsec
 *          The parsec context of the application that will run the operation.
 *
 * @param[in] transA
 *          Specifies whether the matrix A is transposed, not transposed or conjugate transposed:
 *          = PlasmaNoTrans:   A is not transposed;
 *          = PlasmaTrans:     A is transposed;
 *          = PlasmaConjTrans: A is conjugate transposed.
 *
 * @param[in] transB
 *          Specifies whether the matrix B is transposed, not transposed or conjugate transposed:
 *          = PlasmaNoTrans:   B is not transposed;
 *          = PlasmaTrans:     B is transposed;
 *          = PlasmaConjTrans: B is conjugate transposed.
 *
 * @param[in] alpha
 *          alpha specifies the scalar alpha
 *
 * @param[in] A
 *          Descriptor of the distributed matrix A.
 *
 * @param[in] B
 *          Descriptor of the distributed matrix B.
 *
 *
 * @param[out] C
 *          Descriptor of the distributed matrix C.
 *          On exit, the data described by C are overwritten by the matrix (
 *          alpha*op( A )*op( B ))
 *
 *******************************************************************************
 *
 * @return
 *          \retval -i if the ith parameters is incorrect.
 *          \retval 0 on success.
 *
 *******************************************************************************
 *
 * @sa dplasma_zsumma_New
 * @sa dplasma_zsumma_Destruct
 * @sa dplasma_csumma
 * @sa dplasma_dsumma
 * @sa dplasma_ssumma
 *
 ******************************************************************************/
int
dplasma_zsumma(parsec_context_t *parsec,
               PLASMA_enum transA, PLASMA_enum transB,
               parsec_complex64_t alpha, const irregular_tiled_matrix_desc_t *A,
               const irregular_tiled_matrix_desc_t *B,
               irregular_tiled_matrix_desc_t *C)
{
    parsec_taskpool_t *parsec_zsumma = NULL;
    int M, N, K;

    zsumma_check_operation_valid(transA, transB, alpha, A, B, C);

    M = C->m;
    N = C->n;
    K = (transA == PlasmaNoTrans) ? A->n : A->m;

    /* Quick return */
    if (M == 0 || N == 0 || ((alpha == (PLASMA_Complex64_t)0.0 || K == 0)))
        return 0;

    parsec_zsumma = dplasma_zsumma_New(transA, transB,
                                     alpha, A,
                                     B,
                                     C);

    if ( parsec_zsumma != NULL ) {
        parsec_enqueue( parsec, (parsec_taskpool_t*)parsec_zsumma);
        dplasma_wait_until_completion(parsec);
        dplasma_zsumma_Destruct( parsec_zsumma );
        return 0;
    }
    else {
        return -101;
    }
}

#if defined(PARSEC_HAVE_RECURSIVE)
void
dplasma_zsumma_setrecursive(parsec_taskpool_t *tp, int bigtile, int opttile)
{
    parsec_zsumma_NN_taskpool_t *parsec_zsumma = (parsec_zsumma_NN_taskpool_t*)tp;
    if (bigtile > 0 && opttile > 0) {
        parsec_zsumma->_g_bigtile = bigtile;
        parsec_zsumma->_g_opttile = opttile;
    }
}


int
dplasma_zsumma_rec(parsec_context_t *parsec,
                 PLASMA_enum transA, PLASMA_enum transB,
                 parsec_complex64_t alpha,
                 const irregular_tiled_matrix_desc_t *A,
                 const irregular_tiled_matrix_desc_t *B,
                 irregular_tiled_matrix_desc_t *C, int bigtile, int opttile)
{
    parsec_taskpool_t *parsec_zsumma = NULL;

    zsumma_check_operation_valid(transA, transB, alpha, A, B, C);

    parsec_zsumma = dplasma_zsumma_New(transA, transB, alpha, A, B, C);

    if (parsec_zsumma) {
        parsec_enqueue(parsec, parsec_zsumma);
        dplasma_zsumma_setrecursive(parsec_zsumma, bigtile, opttile);
        dplasma_progress(parsec);
        dplasma_zsumma_recursive_Destruct(parsec_zsumma);
        parsec_taskpool_sync_ids();
    }

    return 0;
}
#endif
