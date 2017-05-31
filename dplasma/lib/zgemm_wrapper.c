/*
 * Copyright (c) 2010-2013 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2013      Inria. All rights reserved.
 * $COPYRIGHT
 *
 * @precisions normal z -> s d c
 *
 */

#include "dplasma.h"
#include "dplasma/lib/dplasmatypes.h"
#include "data_dist/matrix/two_dim_rectangle_cyclic.h"

#include "zgemm_NN.h"
#include "zgemm_NT.h"
#include "zgemm_TN.h"
#include "zgemm_TT.h"

#include "zgemm_NN_summa.h"
#include "zgemm_NT_summa.h"
#include "zgemm_TN_summa.h"
#include "zgemm_TT_summa.h"

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 *  dplasma_zgemm_New - Generates the handle that performs one of the following
 *  matrix-matrix operations. WARNING: The computations are not done by this call.
 *
 *    \f[ C = \alpha [op( A )\times op( B )] + \beta C \f],
 *
 *  where op( X ) is one of
 *
 *    op( X ) = X  or op( X ) = X' or op( X ) = conjg( X' )
 *
 *  alpha and beta are scalars, and A, B and C  are matrices, with op( A )
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
 * @param[in] beta
 *          beta specifies the scalar beta
 *
 * @param[in,out] C
 *          Descriptor of the distributed matrix C.
 *          On exit, the data described by C are overwritten by the matrix (
 *          alpha*op( A )*op( B ) + beta*C )
 *
 *******************************************************************************
 *
 * @return
 *          \retval NULL if incorrect parameters are given.
 *          \retval The parsec handle describing the operation that can be
 *          enqueued in the runtime with parsec_enqueue(). It, then, needs to be
 *          destroy with dplasma_zgemm_Destruct();
 *
 *******************************************************************************
 *
 * @sa dplasma_zgemm
 * @sa dplasma_zgemm_Destruct
 * @sa dplasma_cgemm_New
 * @sa dplasma_dgemm_New
 * @sa dplasma_sgemm_New
 *
 ******************************************************************************/
parsec_handle_t*
dplasma_zgemm_New( PLASMA_enum transA, PLASMA_enum transB,
                   parsec_complex64_t alpha, const tiled_matrix_desc_t* A, const tiled_matrix_desc_t* B,
                   parsec_complex64_t beta,  tiled_matrix_desc_t* C)
{
    two_dim_block_cyclic_t *Cdist;
    parsec_handle_t* zgemm_handle;
    parsec_arena_t* arena;
    int P, Q, m, n;

    /* Check input arguments */
    if ((transA != PlasmaNoTrans) && (transA != PlasmaTrans) && (transA != PlasmaConjTrans)) {
        dplasma_error("dplasma_zgemm_New", "illegal value of transA");
        return NULL /*-1*/;
    }
    if ((transB != PlasmaNoTrans) && (transB != PlasmaTrans) && (transB != PlasmaConjTrans)) {
        dplasma_error("dplasma_zgemm_New", "illegal value of transB");
        return NULL /*-2*/;
    }
    if ( !(C->dtype & two_dim_block_cyclic_type) ) {
        dplasma_error("dplasma_zgemm_New", "illegal type of descriptor for C (must be two_dim_block_cyclic_t)");
        return NULL;
    }

    P = ((two_dim_block_cyclic_t*)C)->grid.rows;
    Q = ((two_dim_block_cyclic_t*)C)->grid.cols;

    m = dplasma_imax(C->mt, P);
    n = dplasma_imax(C->nt, Q);

    /* Create a copy of the A matrix to be used as a data distribution metric.
     * As it is used as a NULL value we must have a data_copy and a data associated
     * with it, so we can create them here.
     * Create the task distribution */
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

    if ( C->dtype & two_dim_block_cyclic_type ) {
        P = ((two_dim_block_cyclic_t*)C)->grid.rows;
        Q = ((two_dim_block_cyclic_t*)C)->grid.cols;

        m = dplasma_imax(C->mt, P);
        n = dplasma_imax(C->nt, Q);

        /* Create a copy of the A matrix to be used as a data distribution metric.
         * As it is used as a NULL value we must have a data_copy and a data associated
         * with it, so we can create them here.
         * Create the task distribution */
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
                parsec_zgemm_NN_summa_handle_t* handle;
                handle = parsec_zgemm_NN_summa_new(transA, transB, alpha, beta,
                                                   A, B, C,
                                                   (parsec_ddesc_t*)Cdist);
                arena = handle->arenas[PARSEC_zgemm_NN_DEFAULT_ARENA];
                zgemm_handle = (parsec_handle_t*)handle;
            } else {
                parsec_zgemm_NT_summa_handle_t* handle;
                handle = parsec_zgemm_NT_summa_new(transA, transB, alpha, beta,
                                                   A, B, C,
                                                   (parsec_ddesc_t*)Cdist);
                arena = handle->arenas[PARSEC_zgemm_NT_DEFAULT_ARENA];
                zgemm_handle = (parsec_handle_t*)handle;
            }
        } else {
            if( PlasmaNoTrans == transB ) {
                parsec_zgemm_TN_summa_handle_t* handle;
                handle = parsec_zgemm_TN_summa_new(transA, transB, alpha, beta,
                                                   A, B, C,
                                                   (parsec_ddesc_t*)Cdist);
                arena = handle->arenas[PARSEC_zgemm_TN_DEFAULT_ARENA];
                zgemm_handle = (parsec_handle_t*)handle;
            }
            else {
                parsec_zgemm_TT_summa_handle_t* handle;
                handle = parsec_zgemm_TT_summa_new(transA, transB, alpha, beta,
                                                   A, B, C,
                                                   (parsec_ddesc_t*)Cdist);
                arena = handle->arenas[PARSEC_zgemm_TT_DEFAULT_ARENA];
                zgemm_handle = (parsec_handle_t*)handle;
            }
        }
    }
    /* C is NOT 2D block-cyclic distributed */
    else {
        if( PlasmaNoTrans == transA ) {
            if( PlasmaNoTrans == transB ) {
                parsec_zgemm_NN_handle_t* handle;
                handle = parsec_zgemm_NN_new(transA, transB, alpha, beta,
                                             A, B, C);
                arena = handle->arenas[PARSEC_zgemm_NN_DEFAULT_ARENA];
                zgemm_handle = (parsec_handle_t*)handle;
            } else {
                parsec_zgemm_NT_handle_t* handle;
                handle = parsec_zgemm_NT_new(transA, transB, alpha, beta,
                                             A, B, C);
                arena = handle->arenas[PARSEC_zgemm_NT_DEFAULT_ARENA];
                zgemm_handle = (parsec_handle_t*)handle;
            }
        } else {
            if( PlasmaNoTrans == transB ) {
                parsec_zgemm_TN_handle_t* handle;
                handle = parsec_zgemm_TN_new(transA, transB, alpha, beta,
                                             A, B, C);
                arena = handle->arenas[PARSEC_zgemm_TN_DEFAULT_ARENA];
                zgemm_handle = (parsec_handle_t*)handle;
            }
            else {
                parsec_zgemm_TT_handle_t* handle;
                handle = parsec_zgemm_TT_new(transA, transB, alpha, beta,
                                             A, B, C);
                arena = handle->arenas[PARSEC_zgemm_TT_DEFAULT_ARENA];
                zgemm_handle = (parsec_handle_t*)handle;
            }
        }
    }

    dplasma_add2arena_tile(arena,
                           A->mb*A->nb*sizeof(parsec_complex64_t),
                           PARSEC_ARENA_ALIGNMENT_SSE,
                           parsec_datatype_double_complex_t, A->mb);
    return zgemm_handle;
}

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 *  dplasma_zgemm_Destruct - Free the data structure associated to an handle
 *  created with dplasma_zgemm_New().
 *
 *******************************************************************************
 *
 * @param[in,out] handle
 *          On entry, the handle to destroy.
 *          On exit, the handle cannot be used anymore.
 *
 *******************************************************************************
 *
 * @sa dplasma_zgemm_New
 * @sa dplasma_zgemm
 *
 ******************************************************************************/
void
dplasma_zgemm_Destruct( parsec_handle_t *handle )
{
    parsec_zgemm_NN_handle_t *zgemm_handle = (parsec_zgemm_NN_handle_t *)handle;

    if ( zgemm_handle->_g_Cdist != NULL ) {
        tiled_matrix_desc_destroy( (tiled_matrix_desc_t*)(zgemm_handle->_g_Cdist) );
        free( zgemm_handle->_g_Cdist );
    }

    parsec_matrix_del2arena( ((parsec_zgemm_NN_handle_t *)handle)->arenas[PARSEC_zgemm_NN_DEFAULT_ARENA] );
    parsec_handle_free(handle);
}

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 *  dplasma_zgemm - Performs one of the following matrix-matrix operations
 *
 *    \f[ C = \alpha [op( A )\times op( B )] + \beta C \f],
 *
 *  where op( X ) is one of
 *
 *    op( X ) = X  or op( X ) = X' or op( X ) = conjg( X' )
 *
 *  alpha and beta are scalars, and A, B and C  are matrices, with op( A )
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
 * @param[in] beta
 *          beta specifies the scalar beta
 *
 * @param[in,out] C
 *          Descriptor of the distributed matrix C.
 *          On exit, the data described by C are overwritten by the matrix (
 *          alpha*op( A )*op( B ) + beta*C )
 *
 *******************************************************************************
 *
 * @return
 *          \retval -i if the ith parameters is incorrect.
 *          \retval 0 on success.
 *
 *******************************************************************************
 *
 * @sa dplasma_zgemm_New
 * @sa dplasma_zgemm_Destruct
 * @sa dplasma_cgemm
 * @sa dplasma_dgemm
 * @sa dplasma_sgemm
 *
 ******************************************************************************/
int
dplasma_zgemm( parsec_context_t *parsec,
               PLASMA_enum transA, PLASMA_enum transB,
               parsec_complex64_t alpha, const tiled_matrix_desc_t *A,
                                        const tiled_matrix_desc_t *B,
               parsec_complex64_t beta,        tiled_matrix_desc_t *C)
{
    parsec_handle_t *parsec_zgemm = NULL;
    int M, N, K;
    int Am, An, Ai, Aj, Amb, Anb;
    int Bm, Bn, Bi, Bj, Bmb, Bnb;

    /* Check input arguments */
    if ((transA != PlasmaNoTrans) && (transA != PlasmaTrans) && (transA != PlasmaConjTrans)) {
        dplasma_error("dplasma_zgemm", "illegal value of transA");
        return -1;
    }
    if ((transB != PlasmaNoTrans) && (transB != PlasmaTrans) && (transB != PlasmaConjTrans)) {
        dplasma_error("dplasma_zgemm", "illegal value of transB");
        return -2;
    }

    if ( transA == PlasmaNoTrans ) {
        Am  = A->m;
        An  = A->n;
        Amb = A->mb;
        Anb = A->nb;
        Ai  = A->i;
        Aj  = A->j;
    } else {
        Am  = A->n;
        An  = A->m;
        Amb = A->nb;
        Anb = A->mb;
        Ai  = A->j;
        Aj  = A->i;
    }

    if ( transB == PlasmaNoTrans ) {
        Bm  = B->m;
        Bn  = B->n;
        Bmb = B->mb;
        Bnb = B->nb;
        Bi  = B->i;
        Bj  = B->j;
    } else {
        Bm  = B->n;
        Bn  = B->m;
        Bmb = B->nb;
        Bnb = B->mb;
        Bi  = B->j;
        Bj  = B->i;
    }

    if ( (Amb != C->mb) || (Anb != Bmb) || (Bnb != C->nb) ) {
        dplasma_error("dplasma_zgemm", "tile sizes have to match");
        return -101;
    }
    if ( (Am != C->m) || (An != Bm) || (Bn != C->n) ) {
        dplasma_error("dplasma_zgemm", "sizes of matrices have to match");
        return -101;
    }
    if ( (Ai != C->i) || (Aj != Bi) || (Bj != C->j) ) {
        dplasma_error("dplasma_zgemm", "start indexes have to match");
        return -101;
    }

    if ( !(C->dtype & two_dim_block_cyclic_type) ) {
        dplasma_error("dplasma_zgemm", "illegal type of descriptor for C");
        return -3.;
    }

    M = C->m;
    N = C->n;
    K = An;

    /* Quick return */
    if (M == 0 || N == 0 ||
        ((alpha == (PLASMA_Complex64_t)0.0 || K == 0) && beta == (PLASMA_Complex64_t)1.0))
        return 0;

    parsec_zgemm = dplasma_zgemm_New(transA, transB,
                                    alpha, A, B,
                                    beta, C);

    if ( parsec_zgemm != NULL )
    {
        parsec_enqueue( parsec, (parsec_handle_t*)parsec_zgemm);
        dplasma_progress(parsec);
        dplasma_zgemm_Destruct( parsec_zgemm );
        return 0;
    }
    return -101;
}
