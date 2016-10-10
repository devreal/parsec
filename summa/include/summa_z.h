/*
 * Copyright (c) 2010-2016 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 */
#ifndef _SUMMA_Z_H_
#define _SUMMA_Z_H_

#include "irregular_tiled_matrix.h"

/* Level 3 Blas */
int summa_zsumma( dague_context_t *dague,
                  PLASMA_enum transA, PLASMA_enum transB,
                  dague_complex64_t alpha, const irregular_tiled_matrix_desc_t *A,
                  const irregular_tiled_matrix_desc_t *B,
                  irregular_tiled_matrix_desc_t *C);

/***********************************************************
 *             Non-Blocking interface
 */
/* Level 3 Blas */
dague_handle_t*
summa_zsumma_New( PLASMA_enum transA, PLASMA_enum transB,
                  dague_complex64_t alpha, const irregular_tiled_matrix_desc_t* A,
                  const irregular_tiled_matrix_desc_t* B,
                  irregular_tiled_matrix_desc_t* C);

/***********************************************************
 *               Destruct functions
 */
/* Level 3 Blas */
void summa_zsumma_Destruct( dague_handle_t *o );

/**********************************************************
 * Check routines
 */
/* int check_zsumma(  dague_context_t *dague, int loud, PLASMA_enum uplo, irregular_tiled_matrix_desc_t *A, irregular_tiled_matrix_desc_t *b, irregular_tiled_matrix_desc_t *x ); */

#endif /* _SUMMA_Z_H_ */
