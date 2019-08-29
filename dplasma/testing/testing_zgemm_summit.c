/*
 * Copyright (c) 2009-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 *
 */

#include "common.h"
#include "parsec/data_dist/matrix/two_dim_rectangle_cyclic.h"

#include <signal.h>

static int check_solution( parsec_context_t *parsec, int loud,
                           PLASMA_enum transA, PLASMA_enum transB,
                           parsec_complex64_t alpha, int Am, int An, int Aseed,
                                                    int Bm, int Bn, int Bseed,
                           parsec_complex64_t beta,  int M,  int N,  int Cseed,
                           two_dim_block_cyclic_t *dcCfinal );

static char *gdb_message = NULL;

void catch_error(int s)
{
    volatile int loop = 1;
    write(2, gdb_message, strlen(gdb_message));
    while(loop) {
        sleep(1);
    }
}

int main(int argc, char ** argv)
{
    parsec_context_t* parsec;
    int iparam[IPARAM_SIZEOF];
    int info_solution = 0;
    int Aseed = 3872;
    int Bseed = 4674;
    int Cseed = 2873;
    int tA = PlasmaNoTrans;
    int tB = PlasmaNoTrans;
    parsec_complex64_t alpha =  0.51;
    parsec_complex64_t beta  = -0.42;
    struct timeval tv1, tv2, tv3;
    int watchdog;

    gettimeofday(&tv1, NULL);
    
#if defined(PRECISION_z) || defined(PRECISION_c)
    alpha -= I * 0.32;
    beta  += I * 0.21;
#endif

    if(0) {
        char hostname[512];
        gethostname(hostname, 512);
        asprintf(&gdb_message, "ssh -t %s gdb -p %d -x ~/gdb\n", hostname, getpid());
        signal(SIGSEGV, catch_error);
        signal(SIGABRT, catch_error);
    }
    if(0) {
        char hostname[512];
        gethostname(hostname, 512);
        asprintf(&gdb_message, "ssh -t %s gdb -p %d -x ~/gdb\n", hostname, getpid());
        fprintf(stderr, "%s", gdb_message);
    }
    
    /* Set defaults for non argv iparams */
    iparam_default_gemm(iparam);
    iparam_default_ibnbmb(iparam, 0, 200, 200);
#if defined(PARSEC_HAVE_CUDA) && 1
    iparam[IPARAM_NGPUS] = 0;
#endif
    /* Initialize PaRSEC */
    parsec = setup_parsec(argc, argv, iparam);
    PASTE_CODE_IPARAM_LOCALS(iparam);

    if(rank==0) {
        gettimeofday(&tv2, NULL);
        timersub(&tv2, &tv1, &tv3);
        fprintf(stderr, "INIT 1 %d.%06d (s)\n", (int)tv3.tv_sec, (int)tv3.tv_usec);
    }

    PASTE_CODE_FLOPS(FLOPS_ZGEMM, ((DagDouble_t)M,(DagDouble_t)N,(DagDouble_t)K));

    LDA = max(LDA, max(M, K));
    LDB = max(LDB, max(K, N));
    LDC = max(LDC, M);

    PASTE_CODE_ALLOCATE_MATRIX(dcC, 1,
        two_dim_block_cyclic, (&dcC, matrix_ComplexDouble, matrix_Tile,
                               nodes, rank, MB, NB, LDC, N, 0, 0,
                               M, N, SMB, SNB, P));
    if(1) {
        watchdog = (int)(5.0 * flops / 7e12 / ((double)P * (double)Q * (double)gpus)) + 60;
        if( rank == 0 ) {
            fprintf(stderr, "Watchdog of %d seconds between two runs (flops are %g, P*Q*G = %d)\n", watchdog, flops, P*Q*gpus);
        }
    } else {
        watchdog = -1;
    }
    
    /* initializing matrix structure */
    if(!check)
    {
        PASTE_CODE_ALLOCATE_MATRIX(dcA, 1,
            two_dim_block_cyclic, (&dcA, matrix_ComplexDouble, matrix_Tile,
                                   nodes, rank, MB, NB, LDA, K, 0, 0,
                                   M, K, SMB, SNB, P));
        PASTE_CODE_ALLOCATE_MATRIX(dcB, 1,
            two_dim_block_cyclic, (&dcB, matrix_ComplexDouble, matrix_Tile,
                                   nodes, rank, MB, NB, LDB, N, 0, 0,
                                   K, N, SMB, SNB, P));

        if(rank==0) {
            gettimeofday(&tv2, NULL);
            timersub(&tv2, &tv1, &tv3);
            fprintf(stderr, "INIT 2 %d.%06d (s)\n", (int)tv3.tv_sec, (int)tv3.tv_usec);
        }

        /* matrix generation */
        if(loud > 2) printf("+++ Generate matrices ... ");
        dplasma_zplrnt( parsec, 0, (parsec_tiled_matrix_dc_t *)&dcA, Aseed);
        dplasma_zplrnt( parsec, 0, (parsec_tiled_matrix_dc_t *)&dcB, Bseed);
        dplasma_zplrnt( parsec, 0, (parsec_tiled_matrix_dc_t *)&dcC, Cseed);
        if(loud > 2) printf("Done\n");

        if(rank==0) {
            gettimeofday(&tv2, NULL);
            timersub(&tv2, &tv1, &tv3);
            fprintf(stderr, "INIT 3 %d.%06d (s)\n", (int)tv3.tv_sec, (int)tv3.tv_usec);
        }

        int t;
        for ( t = 0; t < 5; ++t) {
            if(watchdog > 0 ) alarm(watchdog);

            /* Create PaRSEC */
            PASTE_CODE_ENQUEUE_KERNEL(parsec, zgemm_summit,
                                      (tA, tB, alpha,
                                       (parsec_tiled_matrix_dc_t *)&dcA,
                                       (parsec_tiled_matrix_dc_t *)&dcB,
                                       beta,
                                       (parsec_tiled_matrix_dc_t *)&dcC,
                                       sB, sC, sD, P, Q, look_ahead));

            /* lets rock! */
            PASTE_CODE_PROGRESS_KERNEL(parsec, zgemm);
            fflush(stdout);
            fflush(stderr);
            dplasma_zgemm_summit_Destruct( PARSEC_zgemm_summit );
            parsec_devices_reset_load(parsec);
            parsec_devices_release_memory();
        }

        if(rank==0) {
            gettimeofday(&tv2, NULL);
            timersub(&tv2, &tv1, &tv3);
            fprintf(stderr, "INIT 4 %d.%06d (s)\n", (int)tv3.tv_sec, (int)tv3.tv_usec);
        }

        parsec_data_free(dcA.mat);
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcA);
        parsec_data_free(dcB.mat);
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcB);

        if(rank==0) {
            gettimeofday(&tv2, NULL);
            timersub(&tv2, &tv1, &tv3);
            fprintf(stderr, "INIT 5 %d.%06d (s)\n", (int)tv3.tv_sec, (int)tv3.tv_usec);
        }
    } else {
        if( (rank ==0) && (nodes > 1) ) {
            fprintf(stderr, "Current check only supports single node runs");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        PASTE_CODE_ALLOCATE_MATRIX(dcC2, check,
            two_dim_block_cyclic, (&dcC2, matrix_ComplexDouble, matrix_Tile,
                                   nodes, rank, MB, NB, LDC, N, 0, 0,
                                   M, N, SMB, SNB, P));

        dplasma_zplrnt( parsec, 0, (parsec_tiled_matrix_dc_t *)&dcC2, Cseed);

        PASTE_CODE_ALLOCATE_MATRIX(dcA, 1,
                                   two_dim_block_cyclic, (&dcA, matrix_ComplexDouble, matrix_Tile,
                                                          nodes, rank, MB, NB, LDA, LDA, 0, 0,
                                                          M, K, SMB, SNB, P));
        PASTE_CODE_ALLOCATE_MATRIX(dcB, 1,
                                   two_dim_block_cyclic, (&dcB, matrix_ComplexDouble, matrix_Tile,
                                                          nodes, rank, MB, NB, LDB, LDB, 0, 0,
                                                          K, N, SMB, SNB, P));

        dplasma_zplrnt( parsec, 0, (parsec_tiled_matrix_dc_t *)&dcA, Aseed);
        dplasma_zplrnt( parsec, 0, (parsec_tiled_matrix_dc_t *)&dcB, Bseed);

        if ( rank == 0 ) {
            printf("***************************************************\n");
            printf(" ----- TESTING ZGEMM (NoTrans, NoTrans) -------- \n");
        }

        /* matrix generation */
        if(loud) printf("Generate matrices ... ");
        dplasma_zlacpy( parsec, PlasmaUpperLower,
                        (parsec_tiled_matrix_dc_t *)&dcC2, (parsec_tiled_matrix_dc_t *)&dcC );
        if(loud) printf("Done\n");

        /* Create GEMM PaRSEC */
        if(loud) { printf("Compute ... ... "); fflush(stdout); }
        PASTE_CODE_ENQUEUE_KERNEL(parsec, zgemm_summit,
                                      (tA, tB, alpha,
                                       (parsec_tiled_matrix_dc_t *)&dcA,
                                       (parsec_tiled_matrix_dc_t *)&dcB,
                                       beta,
                                       (parsec_tiled_matrix_dc_t *)&dcC,
                                       sB, sC, sD, P, Q, look_ahead));
        /* lets rock! */
        PASTE_CODE_PROGRESS_KERNEL(parsec, zgemm);
        if(loud) printf("Done\n");

        parsec_data_free(dcA.mat);
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcA);
        parsec_data_free(dcB.mat);
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcB);

        /* Check the solution */
        info_solution = check_solution( parsec, (rank == 0) ? loud : 0,
                                        tA, tB,
                                        alpha, M, K, Aseed,
                                        K, N, Bseed,
                                        beta,  M,  N,  Cseed,
                                        &dcC);
        if ( rank == 0 ) {
            if (info_solution == 0) {
                printf(" ---- TESTING ZGEMM (NoTrans, NoTrans) ...... PASSED !\n");
            } else {
                printf(" ---- TESTING ZGEMM (NoTrans, NoTrans) ... FAILED !\n");
            }
            printf("***************************************************\n");
        }
        parsec_data_free(dcC2.mat);
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcC2);
    }

    parsec_data_free(dcC.mat);
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcC);

    cleanup_parsec(parsec, iparam);

    if(rank==0) {
        gettimeofday(&tv2, NULL);
        timersub(&tv2, &tv1, &tv3);
        fprintf(stderr, "INIT 6 %d.%06d (s)\n", (int)tv3.tv_sec, (int)tv3.tv_usec);
    }

    return info_solution;
}

/**********************************
 * static functions
 **********************************/

/*------------------------------------------------------------------------
 *  Check the accuracy of the solution
 */
static int check_solution( parsec_context_t *parsec, int loud,
                           PLASMA_enum transA, PLASMA_enum transB,
                           parsec_complex64_t alpha, int Am, int An, int Aseed,
                                                    int Bm, int Bn, int Bseed,
                           parsec_complex64_t beta,  int M,  int N,  int Cseed,
                           two_dim_block_cyclic_t *dcCfinal )
{
    int info_solution = 1;
    double Anorm, Bnorm, Cinitnorm, Cdplasmanorm, Clapacknorm, Rnorm;
    double eps, result;
    int K  = ( transA == PlasmaNoTrans ) ? An : Am ;
    int MB = dcCfinal->super.mb;
    int NB = dcCfinal->super.nb;
    int LDA = Am;
    int LDB = Bm;
    int LDC = M;
    int rank  = dcCfinal->super.super.myrank;

    eps = LAPACKE_dlamch_work('e');

    PASTE_CODE_ALLOCATE_MATRIX(dcA, 1,
        two_dim_block_cyclic, (&dcA, matrix_ComplexDouble, matrix_Lapack,
                               1, rank, MB, NB, LDA, An, 0, 0,
                               Am, An, 1, 1, 1));
    PASTE_CODE_ALLOCATE_MATRIX(dcB, 1,
        two_dim_block_cyclic, (&dcB, matrix_ComplexDouble, matrix_Lapack,
                               1, rank, MB, NB, LDB, Bn, 0, 0,
                               Bm, Bn, 1, 1, 1));
    PASTE_CODE_ALLOCATE_MATRIX(dcC, 1,
        two_dim_block_cyclic, (&dcC, matrix_ComplexDouble, matrix_Lapack,
                               1, rank, MB, NB, LDC, N, 0, 0,
                               M, N, 1, 1, 1));

    dplasma_zplrnt( parsec, 0, (parsec_tiled_matrix_dc_t *)&dcA, Aseed );
    dplasma_zplrnt( parsec, 0, (parsec_tiled_matrix_dc_t *)&dcB, Bseed );
    dplasma_zplrnt( parsec, 0, (parsec_tiled_matrix_dc_t *)&dcC, Cseed );

    Anorm        = dplasma_zlange( parsec, PlasmaInfNorm, (parsec_tiled_matrix_dc_t*)&dcA );
    Bnorm        = dplasma_zlange( parsec, PlasmaInfNorm, (parsec_tiled_matrix_dc_t*)&dcB );
    Cinitnorm    = dplasma_zlange( parsec, PlasmaInfNorm, (parsec_tiled_matrix_dc_t*)&dcC );
    Cdplasmanorm = dplasma_zlange( parsec, PlasmaInfNorm, (parsec_tiled_matrix_dc_t*)dcCfinal );

    if ( rank == 0 ) {
        cblas_zgemm(CblasColMajor,
                    (CBLAS_TRANSPOSE)transA, (CBLAS_TRANSPOSE)transB,
                    M, N, K,
                    CBLAS_SADDR(alpha), dcA.mat, LDA,
                                        dcB.mat, LDB,
                    CBLAS_SADDR(beta),  dcC.mat, LDC );
    }

    Clapacknorm = dplasma_zlange( parsec, PlasmaInfNorm, (parsec_tiled_matrix_dc_t*)&dcC );

    dplasma_zgeadd( parsec, PlasmaNoTrans, -1.0, (parsec_tiled_matrix_dc_t*)dcCfinal,
                                           1.0, (parsec_tiled_matrix_dc_t*)&dcC );

    Rnorm = dplasma_zlange( parsec, PlasmaMaxNorm, (parsec_tiled_matrix_dc_t*)&dcC);

    if ( rank == 0 ) {
        if ( loud > 2 ) {
            printf("  ||A||_inf = %e, ||B||_inf = %e, ||C||_inf = %e\n"
                   "  ||lapack(a*A*B+b*C)||_inf = %e, ||dplasma(a*A*B+b*C)||_inf = %e, ||R||_m = %e\n",
                   Anorm, Bnorm, Cinitnorm, Clapacknorm, Cdplasmanorm, Rnorm);
        }

        result = Rnorm / ((Anorm + Bnorm + Cinitnorm) * max(M,N) * eps);
        if (  isinf(Clapacknorm) || isinf(Cdplasmanorm) ||
              isnan(result) || isinf(result) || (result > 10.0) ) {
            info_solution = 1;
        }
        else {
            info_solution = 0;
        }
    }

#if defined(PARSEC_HAVE_MPI)
    MPI_Bcast(&info_solution, 1, MPI_INT, 0, MPI_COMM_WORLD);
#endif

    parsec_data_free(dcA.mat);
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcA);
    parsec_data_free(dcB.mat);
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcB);
    parsec_data_free(dcC.mat);
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcC);

    return info_solution;
}
