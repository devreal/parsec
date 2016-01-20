/*
 * Copyright (c) 2011-2013 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2013      Inria. All rights reserved.
 *
 * @precisions normal z -> z c
 *
 */

#include "dplasma.h"
#include "dplasma/lib/dplasmatypes.h"
#include "data_dist/matrix/two_dim_rectangle_cyclic.h"
#include "data_dist/matrix/sym_two_dim_rectangle_cyclic.h"

#include "zlansy.h"

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 *  dplasma_zlanhe_New - Generates the handle that computes the value
 *
 *     zlanhe = ( max(abs(A(i,j))), NORM = PlasmaMaxNorm
 *              (
 *              ( norm1(A),         NORM = PlasmaOneNorm
 *              (
 *              ( normI(A),         NORM = PlasmaInfNorm
 *              (
 *              ( normF(A),         NORM = PlasmaFrobeniusNorm
 *
 *  where norm1 denotes the one norm of a matrix (maximum column sum),
 *  normI denotes the infinity norm of a matrix (maximum row sum) and
 *  normF denotes the Frobenius norm of a matrix (square root of sum
 *  of squares). Note that max(abs(A(i,j))) is not a consistent matrix
 *  norm.
 *
 *  WARNING: The computations are not done by this call
 *
 *******************************************************************************
 *
 * @param[in] norm
 *          = PlasmaMaxNorm: Max norm
 *          = PlasmaOneNorm: One norm
 *          = PlasmaInfNorm: Infinity norm
 *          = PlasmaFrobeniusNorm: Frobenius norm
 *
 * @param[in] uplo
 *          = PlasmaUpper: Upper triangle of A is stored;
 *          = PlasmaLower: Lower triangle of A is stored.
 *
 * @param[in] A
 *          The descriptor of the hermitian matrix A.
 *          Must be a two_dim_rectangle_cyclic or sym_two_dim_rectangle_cyclic
 *          matrix
 *
 * @param[out] result
 *          The norm described above. Might not be set when the function returns.
 *
 *******************************************************************************
 *
 * @return
 *          \retval NULL if incorrect parameters are given.
 *          \retval The dague handle describing the operation that can be
 *          enqueued in the runtime with dague_enqueue(). It, then, needs to be
 *          destroy with dplasma_zlanhe_Destruct();
 *
 *******************************************************************************
 *
 * @sa dplasma_zlanhe
 * @sa dplasma_zlanhe_Destruct
 * @sa dplasma_clanhe_New
 *
 ******************************************************************************/
dague_handle_t*
dplasma_zlanhe_New( PLASMA_enum norm,
                    PLASMA_enum uplo,
                    const tiled_matrix_desc_t *A,
                    double *result )
{
    int P, Q, mb, nb, elt, m;
    two_dim_block_cyclic_t *Tdist;
    dague_handle_t *dague_zlanhe = NULL;

    if ( (norm != PlasmaMaxNorm) && (norm != PlasmaOneNorm)
        && (norm != PlasmaInfNorm) && (norm != PlasmaFrobeniusNorm) ) {
        dplasma_error("dplasma_zlanhe", "illegal value of norm");
        return NULL;
    }
    if ( (uplo != PlasmaUpper) && (uplo != PlasmaLower) ) {
        dplasma_error("dplasma_zlanhe", "illegal value of uplo");
        return NULL;
    }
    if ( !(A->dtype & ( two_dim_block_cyclic_type | sym_two_dim_block_cyclic_type)) ) {
        dplasma_error("dplasma_zlanhe", "illegal type of descriptor for A");
        return NULL;
    }

    P = ((sym_two_dim_block_cyclic_t*)A)->grid.rows;
    Q = ((sym_two_dim_block_cyclic_t*)A)->grid.cols;

    /* Warning: Pb with smb/snb when mt/nt lower than P/Q */
    switch( norm ) {
    case PlasmaFrobeniusNorm:
        mb = 2;
        nb = 1;
        elt = 2;
        break;
    case PlasmaInfNorm:
    case PlasmaOneNorm:
        mb = A->mb;
        nb = 1;
        elt = 1;
        break;
    case PlasmaMaxNorm:
    default:
        mb = 1;
        nb = 1;
        elt = 1;
    }
    m = dplasma_imax(A->mt, P);

    /* Create a copy of the A matrix to be used as a data distribution metric.
     * As it is used as a NULL value we must have a data_copy and a data associated
     * with it, so we can create them here.
     * Create the task distribution */
    Tdist = (two_dim_block_cyclic_t*)malloc(sizeof(two_dim_block_cyclic_t));

    two_dim_block_cyclic_init(
        Tdist, matrix_RealDouble, matrix_Tile,
        A->super.nodes, A->super.myrank,
        1, 1,   /* Dimensions of the tiles              */
        m, P*Q, /* Dimensions of the matrix             */
        0, 0,   /* Starting points (not important here) */
        m, P*Q, /* Dimensions of the submatrix          */
        1, 1, P);
    Tdist->super.super.data_of = fake_data_of;

    /* Create the DAG */
    dague_zlanhe = (dague_handle_t*)dague_zlansy_new(
        P, Q, norm, uplo, PlasmaConjTrans,
        (dague_ddesc_t*)A,
        (dague_ddesc_t*)Tdist,
        result);

    /* Set the datatypes */
    dplasma_add2arena_tile(((dague_zlansy_handle_t*)dague_zlanhe)->arenas[DAGUE_zlansy_DEFAULT_ARENA],
                           A->mb*A->nb*sizeof(dague_complex64_t),
                           DAGUE_ARENA_ALIGNMENT_SSE,
                           dague_datatype_double_complex_t, A->mb);
    dplasma_add2arena_rectangle(((dague_zlansy_handle_t*)dague_zlanhe)->arenas[DAGUE_zlansy_COL_ARENA],
                                mb * nb * sizeof(double), DAGUE_ARENA_ALIGNMENT_SSE,
                                dague_datatype_double_t, mb, nb, -1);
    dplasma_add2arena_rectangle(((dague_zlansy_handle_t*)dague_zlanhe)->arenas[DAGUE_zlansy_ELT_ARENA],
                                elt * sizeof(double), DAGUE_ARENA_ALIGNMENT_SSE,
                                dague_datatype_double_t, elt, 1, -1);

    return (dague_handle_t*)dague_zlanhe;
}

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 *  dplasma_zlanhe_Destruct - Free the data structure associated to an handle
 *  created with dplasma_zlanhe_New().
 *
 *******************************************************************************
 *
 * @param[in,out] handle
 *          On entry, the handle to destroy.
 *          On exit, the handle cannot be used anymore.
 *
 *******************************************************************************
 *
 * @sa dplasma_zlanhe_New
 * @sa dplasma_zlanhe
 *
 ******************************************************************************/
void
dplasma_zlanhe_Destruct( dague_handle_t *handle )
{
    dague_zlansy_handle_t *dague_zlanhe = (dague_zlansy_handle_t *)handle;

    tiled_matrix_desc_destroy( (tiled_matrix_desc_t*)(dague_zlanhe->Tdist) );
    free( dague_zlanhe->Tdist );

    dague_matrix_del2arena( dague_zlanhe->arenas[DAGUE_zlansy_DEFAULT_ARENA] );
    dague_matrix_del2arena( dague_zlanhe->arenas[DAGUE_zlansy_COL_ARENA] );
    dague_matrix_del2arena( dague_zlanhe->arenas[DAGUE_zlansy_ELT_ARENA] );

    handle->destructor(handle);
}

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 *  dplasma_zlanhe - Computes the value
 *
 *     zlanhe = ( max(abs(A(i,j))), NORM = PlasmaMaxNorm
 *              (
 *              ( norm1(A),         NORM = PlasmaOneNorm
 *              (
 *              ( normI(A),         NORM = PlasmaInfNorm
 *              (
 *              ( normF(A),         NORM = PlasmaFrobeniusNorm
 *
 *  where norm1 denotes the one norm of a matrix (maximum column sum),
 *  normI denotes the infinity norm of a matrix (maximum row sum) and
 *  normF denotes the Frobenius norm of a matrix (square root of sum
 *  of squares). Note that max(abs(A(i,j))) is not a consistent matrix
 *  norm.
 *
 *******************************************************************************
 *
 * @param[in,out] dague
 *          The dague context of the application that will run the operation.
 *
 * @param[in] norm
 *          = PlasmaMaxNorm: Max norm
 *          = PlasmaOneNorm: One norm
 *          = PlasmaInfNorm: Infinity norm
 *          = PlasmaFrobeniusNorm: Frobenius norm
 *
 * @param[in] uplo
 *          = PlasmaUpper: Upper triangle of A is stored;
 *          = PlasmaLower: Lower triangle of A is stored.
 *
 * @param[in] A
 *          The descriptor of the hermitian matrix A.
 *          Must be a two_dim_rectangle_cyclic or sym_two_dim_rectangle_cyclic
 *          matrix
 *
*******************************************************************************
 *
 * @return
 *          \retval the computed norm described above.
 *
 *******************************************************************************
 *
 * @sa dplasma_zlanhe_New
 * @sa dplasma_zlanhe_Destruct
 * @sa dplasma_clanhe
 *
 ******************************************************************************/
double
dplasma_zlanhe( dague_context_t *dague,
                PLASMA_enum norm,
                PLASMA_enum uplo,
                const tiled_matrix_desc_t *A)
{
    double result = 0.;
    dague_handle_t *dague_zlanhe = NULL;

    if ( (norm != PlasmaMaxNorm) && (norm != PlasmaOneNorm)
        && (norm != PlasmaInfNorm) && (norm != PlasmaFrobeniusNorm) ) {
        dplasma_error("dplasma_zlanhe", "illegal value of norm");
        return -2.;
    }
    if ( (uplo != PlasmaUpper) && (uplo != PlasmaLower) ) {
        dplasma_error("dplasma_zlanhe", "illegal value of uplo");
        return -3.;
    }
    if ( !(A->dtype & ( two_dim_block_cyclic_type | sym_two_dim_block_cyclic_type)) ) {
        dplasma_error("dplasma_zlanhe", "illegal type of descriptor for A");
        return -4.;
    }
    if ( A->m != A->n ) {
        dplasma_error("dplasma_zlanhe", "illegal matrix A (not square)");
        return -5.;
    }

    dague_zlanhe = dplasma_zlanhe_New(norm, uplo, A, &result);

    if ( dague_zlanhe != NULL )
    {
        dague_enqueue( dague, (dague_handle_t*)dague_zlanhe);
        dplasma_progress(dague);
        dplasma_zlanhe_Destruct( dague_zlanhe );
    }

    return result;
}

