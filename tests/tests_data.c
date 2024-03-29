/*
 * Copyright (c) 2009-2018 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include "parsec/runtime.h"
#include "parsec/data_dist/matrix/two_dim_rectangle_cyclic.h"
#include "tests/tests_data.h"

parsec_tiled_matrix_dc_t *create_and_distribute_data(int rank, int world, int mb, int mt)
{
    two_dim_block_cyclic_t *m = (two_dim_block_cyclic_t*)malloc(sizeof(two_dim_block_cyclic_t));
    two_dim_block_cyclic_init(m, matrix_ComplexDouble, matrix_Tile,
                              rank,
                              mb, 1,      /* Tile size */
                              mt*mb, 1,   /* Global matrix size (what is stored)*/
                              0, 0,       /* Staring point in the global matrix */
                              mt*mb, 1,   /* Submatrix size (the one concerned by the computation */
                              world, 1,
                              1, 1,       /* k-cyclicity */
                              0, 0);

    m->mat = parsec_data_allocate((size_t)m->super.nb_local_tiles *
                                (size_t)m->super.bsiz *
                                (size_t)parsec_datadist_getsizeoftype(m->super.mtype));

    return (parsec_tiled_matrix_dc_t*)m;
}

parsec_tiled_matrix_dc_t *create_and_distribute_empty_data(int rank, int world, int mb, int mt)
{
    two_dim_block_cyclic_t *m = (two_dim_block_cyclic_t*)malloc(sizeof(two_dim_block_cyclic_t));
    two_dim_block_cyclic_init(m, matrix_ComplexDouble, matrix_Tile,
                              rank,
                              mb, 1,      /* Tile size */
                              mt*mb, 1,   /* Global matrix size (what is stored)*/
                              0, 0,       /* Staring point in the global matrix */
                              mt*mb, 1,   /* Submatrix size (the one concerned by the computation */
                              world, 1,
                              1, 1,       /* k-cyclicity */
                              0, 0);

    return (parsec_tiled_matrix_dc_t*)m;
}

void free_data(parsec_tiled_matrix_dc_t *d)
{
    two_dim_block_cyclic_t *m = (two_dim_block_cyclic_t*)d;
    if(NULL != m->mat) {
        parsec_data_free(m->mat);
    }
    parsec_matrix_destroy_data(d);
    parsec_data_collection_destroy(&d->super);
    free(d);
}
