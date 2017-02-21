/*
 * Copyright (c) 2011-2013 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2013      Inria. All rights reserved.
 *
 * @precisions normal z -> c d s
 *
 */

#include "dplasma.h"
#include "dplasma/lib/dplasmatypes.h"

#include "map.h"

struct zplrnt_args_s {
    int                    diagdom;
    unsigned long long int seed;
};
typedef struct zplrnt_args_s zplrnt_args_t;

static int
summa_zplrnt_operator( dague_execution_unit_t *eu,
                       const irregular_tiled_matrix_desc_t *descA,
                       void *_A,
                       PLASMA_enum uplo, int m, int n,
                       void *op_data )
{
    int tempmm, tempnn, ldam;
    zplrnt_args_t     *args = (zplrnt_args_t*)op_data;
    dague_complex64_t *A    = (dague_complex64_t*)_A;
    (void)eu;
    (void)uplo;

    tempmm = ((m)==((descA->mt)-1)) ? ((descA->m)-(m*(descA->mb))) : (descA->mb);
    tempnn = ((n)==((descA->nt)-1)) ? ((descA->n)-(n*(descA->nb))) : (descA->nb);
    ldam   = BLKLDD( *descA, m );

    int i, j;
    for (i = 0; i < descA->mt; ++i) {
	    for (j = 0; j <descA->nt; ++j) {
		    irregular_tile_data_t *t = descA->data_map[(i+descA->i)*desc->lmt+(j+descA->j)];


	    }
    }

    CORE_zplrnt(
        tempmm, tempnn, A, ldam,
        descA->m, m*descA->mb, n*descA->nb, args->seed );

    if (args->diagdom && (m == n))
    {
        dague_complex64_t  alpha;
        int maxmn = summa_imax( descA->m, descA->n );
        int i;

#if defined(PRECISION_z) || defined(PRECISION_c)
        int nvir  = descA->m + descA->n - 1;
        alpha = (double)nvir + I * (double)maxmn;
#else
        alpha = maxmn;
#endif

        for(i=0; i<summa_imin(tempmm, tempnn); i++) {
            (*A) += alpha;
            A += (ldam+1);
        }
    }

    return 0;
}

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 * summa_zplrnt_New - Generates the handle that generates a random general
 * matrix by tiles.
 *
 * See dplasma_map_New() for further information.
 *
 *  WARNINGS: The computations are not done by this call.
 *
 *******************************************************************************
 *
 * @param[in] diagdom
 *          Specify if the diagonal is increased by max(M,N) or not to get a
 *          diagonal dominance.
 *
 * @param[in,out] A
 *          Descriptor of the distributed matrix A to generate. Any tiled matrix
 *          descriptor can be used.
 *          On exit, the symmetric matrix A generated.
 *
 * @param[in] seed
 *          The seed used in the random generation.
 *
 *******************************************************************************
 *
 * @return
 *          \retval NULL if incorrect parameters are given.
 *          \retval The dague handle describing the operation that can be
 *          enqueued in the runtime with dague_enqueue(). It, then, needs to be
 *          destroy with dplasma_zplrnt_Destruct();
 *
 *******************************************************************************
 *
 * @sa dplasma_zplrnt
 * @sa dplasma_zplrnt_Destruct
 * @sa dplasma_cplrnt_New
 * @sa dplasma_dplrnt_New
 * @sa dplasma_splrnt_New
 *
 ******************************************************************************/
dague_handle_t*
summa_zplrnt_New( int diagdom,
                  tiled_matrix_desc_t *A,
                  unsigned long long int seed)
{
    zplrnt_args_t *params = (zplrnt_args_t*)malloc(sizeof(zplrnt_args_t));

    params->diagdom = diagdom;
    params->seed    = seed;

    return summa_map_New( PlasmaUpperLower, A, summa_zplrnt_operator, params );
}

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 *  dplasma_zplrnt_Destruct - Free the data structure associated to an handle
 *  created with dplasma_zplrnt_New().
 *
 *******************************************************************************
 *
 * @param[in,out] handle
 *          On entry, the handle to destroy.
 *          On exit, the handle cannot be used anymore.
 *
 *******************************************************************************
 *
 * @sa dplasma_zplrnt_New
 * @sa dplasma_zplrnt
 *
 ******************************************************************************/
void
summa_zplrnt_Destruct( dague_handle_t *handle )
{
    summa_map_Destruct(handle);
}

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 * dplasma_zplrnt - Generates a random general matrix by tiles.
 *
 * See dplasma_map() for further information.
 *
 *******************************************************************************
 *
 * @param[in,out] dague
 *          The dague context of the application that will run the operation.
 *
 * @param[in] diagdom
 *          Specify if the diagonal is increased by max(M,N) or not to get a
 *          diagonal dominance.
 *
 * @param[in,out] A
 *          Descriptor of the distributed matrix A to generate. Any tiled matrix
 *          descriptor can be used.
 *          On exit, the symmetric matrix A generated.
 *
 * @param[in] seed
 *          The seed used in the random generation.
 *
 *******************************************************************************
 *
 * @return
 *          \retval -i if the ith parameters is incorrect.
 *          \retval 0 on success.
 *
 *******************************************************************************
 *
 * @sa dplasma_zplrnt_New
 * @sa dplasma_zplrnt_Destruct
 * @sa dplasma_cplrnt
 * @sa dplasma_dplrnt
 * @sa dplasma_splrnt
 *
 ******************************************************************************/
int
summa_zplrnt( dague_context_t *dague,
                int diagdom,
                tiled_matrix_desc_t *A,
                unsigned long long int seed)
{
    dague_handle_t *dague_zplrnt = NULL;

    dague_zplrnt = summa_zplrnt_New(diagdom, A, seed);

    dague_enqueue(dague, (dague_handle_t*)dague_zplrnt);
    dplasma_progress(dague);

    summa_zplrnt_Destruct( dague_zplrnt );
    return 0;
}
