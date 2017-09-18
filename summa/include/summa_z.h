/*
 * Copyright (c) 2010-2016 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 */
#ifndef _zsumma_h_has_been_included_
#define _zsumma_h_has_been_included_

#include "irregular_tiled_matrix.h"

#if defined(PARSEC_HAVE_RECURSIVE)
#include "parsec/recursive.h"
#endif

BEGIN_C_DECLS

#define SUMMA_NN 1
#define SUMMA_NT 2
#define SUMMA_TN 3
#define SUMMA_TT 4
#define GEMM_BCAST_NN 5

/***********************************************************
 *               Blocking interface
 */
/* Level 3 Blas */
int summa_zsumma( parsec_context_t *parsec,
                  PLASMA_enum transA, PLASMA_enum transB,
                  parsec_complex64_t alpha,
				  const irregular_tiled_matrix_desc_t *A,
                  const irregular_tiled_matrix_desc_t *B,
                  irregular_tiled_matrix_desc_t *C);

/* Recursive kernel */
int
summa_zsumma_rec( parsec_context_t *parsec,
				  PLASMA_enum transA, PLASMA_enum transB,
				  parsec_complex64_t alpha,
				  const irregular_tiled_matrix_desc_t *A,
				  const irregular_tiled_matrix_desc_t *B,
				  irregular_tiled_matrix_desc_t *C, int bigtile, int opttile);

/***********************************************************
 *             Non-Blocking interface
 */
/* Level 3 Blas */
parsec_taskpool_t*
summa_zsumma_New( PLASMA_enum transA, PLASMA_enum transB,
                  parsec_complex64_t alpha, const irregular_tiled_matrix_desc_t* A,
                  const irregular_tiled_matrix_desc_t* B,
                  irregular_tiled_matrix_desc_t* C);

/***********************************************************
 *               Destruct functions
 */
/* Level 3 Blas */
void summa_zsumma_Destruct( parsec_taskpool_t *o );

void summa_zsumma_recursive_Destruct(parsec_taskpool_t *tp);

/**********************************************************
 * Check routines
 */
/* int check_zsumma(  parsec_context_t *parsec, int loud, PLASMA_enum uplo, irregular_tiled_matrix_desc_t *A, irregular_tiled_matrix_desc_t *b, irregular_tiled_matrix_desc_t *x ); */

#if defined(PARSEC_HAVE_RECURSIVE)
void summa_zsumma_setrecursive( parsec_taskpool_t *o, int bigtile, int opttile );

static inline int summa_recursivecall_callback(parsec_taskpool_t* tp, void* cb_data)
{
    int i, rc = 0;
    cb_data_t* data = (cb_data_t*)cb_data;

    rc = __parsec_complete_execution(data->eu, data->context);

    for(i=0; i<data->nbdesc; i++){
        irregular_tiled_matrix_desc_destroy( (irregular_tiled_matrix_desc_t*)(data->desc[i]) );
        free( data->desc[i] );
    }

    data->destruct( tp );
    free(data);

    return rc;
}
#endif

/* Helper structures and functions */

/* This will define a GEMM execution plan for the bcast_gemm interface */
typedef struct gemm_plan_s gemm_plan_t;

/*
 * Returns k such that gemm_plan_red_index(plan, m, n, k) == i
 */
int gemm_plan_k_of_red_index(gemm_plan_t *plan, int m, int n, int i);
/*
 * Returns the position in the pipeline reduction of the
 * different node contributions to C(m, n), such that
 * k is the last local contribution to C(m, n) for the calling
 * node.
 */
int gemm_plan_red_index(gemm_plan_t *plan, int m, int n, int k);
/*
 * Returns how many nodes contribute to C(m ,n) 
 */
int gemm_plan_max_red_index(gemm_plan_t *plan, int m, int n);
/*
 * Returns k' such that gemm_plan_next(plan, m, n, k') = k
 * Return -1 if there is no such k'
 */
int gemm_plan_prev(gemm_plan_t *plan, int m, int n, int k);
/*
 * GEMM(m, n, k) was a previous local contribution to C(m, n)
 * This function returns k' such that GEMM(m, n, k') is the next
 * local GEMM to execute
 * Returns -1 if there is no such k'
 */
int gemm_plan_next(gemm_plan_t *plan, int m, int n, int k);

END_C_DECLS

#endif /* _zsumma_h_has_been_included_ */