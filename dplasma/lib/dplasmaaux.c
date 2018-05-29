/*
 * Copyright (c) 2011-2018 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2013      Inria. All rights reserved.
 * $COPYRIGHT
 *
 */

#include "dplasma.h"
#include "parsec/vpmap.h"
#include <math.h>

#if defined(PARSEC_HAVE_STRING_H)
#include <string.h>
#endif  /* defined(PARSEC_HAVE_STRING_H) */

#include "dplasmaaux.h"

int
dplasma_aux_get_priority_limit( char* function, const parsec_tiled_matrix_dc_t* dc )
{
    char *v;
    char *keyword;

    if( NULL == function || NULL == dc )
        return 0;

    keyword = alloca( strlen(function)+2 );
    
    switch( dc->mtype ) {
    case matrix_RealFloat:
        sprintf(keyword, "S%s", function);
        break;
    case matrix_RealDouble:
        sprintf(keyword, "D%s", function);
        break;
    case matrix_ComplexFloat:
        sprintf(keyword, "C%s", function);
        break;
    case matrix_ComplexDouble:
        sprintf(keyword, "Z%s", function);
        break;
    default:
        return 0;
    }

    if( (v = getenv(keyword)) != NULL ) {
        return atoi(v);
    }
    return 0;
}

int
dplasma_aux_getGEMMLookahead( parsec_tiled_matrix_dc_t *A )
{
    /**
     * Assume that the number of threads per node is constant, and compute the
     * look ahead based on the global information to get the same one on all
     * nodes.
     */
    int nbunits = vpmap_get_nb_total_threads() * A->super.nodes;
    double alpha =  3. * (double)nbunits / ( A->mt * A->nt );

    if ( A->super.nodes == 1 ) {
        /* No look ahaead */
        return dplasma_imax( A->mt, A->nt );
    }
    else {
        /* Look ahaed of at least 2, and that provides 3 tiles per computational units */
        return dplasma_imax( ceil( alpha ), 2 );
    }
}

int
dplasma_irr_tiled_getGEMMLookahead( irregular_tiled_matrix_desc_t *A )
{
    /**
     * Assume that the number of threads per node is constant, and compute the
     * look ahead based on the global information to get the same one on all
     * nodes.
     */
    int nbunits = vpmap_get_nb_total_threads() * A->super.nodes;
    double alpha =  3. * (double)nbunits / ( A->mt * A->nt );

    if ( A->super.nodes == 1 ) {
        /* No look ahaead */
        return dplasma_imax( A->mt, A->nt );
    }
    else {
        /* Look ahaed of at least 2, and that provides 3 tiles per computational units */
        return dplasma_imax( ceil( alpha ), 2 );
    }
}
