subs = {
  'all' : [ ## Special key: Changes are applied to all applicable conversions automatically
    [None,None]
  ],
  'mixed' : [
    ['zc','ds'],
    ('ZC','DS'),
    ('zc','ds'),
    ('PLASMA_Complex64_t','double'),
    ('PLASMA_Complex32_t','float'),
    ('PlasmaComplexDouble','PlasmaRealDouble'),
    ('PlasmaComplexFloat','PlasmaRealFloat'),
    ('parsec_complex64_t',   'double'           ),
    ('parsec_complex32_t',   'float'            ),
    ('matrix_ComplexDouble','matrix_RealDouble'),
    ('matrix_ComplexFloat', 'matrix_RealFloat' ),
    ('zlange','dlange'),
    ('zlag2c','dlag2s'),
    ('clag2z','slag2d'),
    ('zlacpy','dlacpy'),
      ('zgemm','dgemm'),
      ('zsumma','dsumma'),
    ('zherk','dsyrk'),
    ('zlansy','dlansy'),
    ('zaxpy','daxpy'),
    ('pzgetrf','pdgetrf'),
    ('pcgetrf','psgetrf'),
    ('ztrsm','dtrsm'),
    ('ctrsm','strsm'),
    ('CBLAS_SADDR',''),
    ('zlarnv','dlarnv'),
    ('zgesv','dgesv'),
    ('zhemm','dsymm'),
    ('zlanhe','dlansy'),
    ('zlaghe','dlagsy'),
    ('ztrmm','dtrmm'),
    ('ctrmm','strmm'),
    ('zherbt','dsyrbt'),
    ('cherbt','ssyrbt'),
    ('zhbrdt','dsbrdt'),
    ('chbrdt','ssbrdt'),
    ('Conj',''),
    ('zpotrf','dpotrf'),
    ('cpotrf','spotrf'),
    ('PLASMA_Alloc_Workspace_zgels','PLASMA_Alloc_Workspace_dgels'),
    ('pcgeqrf','psgeqrf'),
    ('pcunmqr','psormqr'),
    ('pcgelqf','psgelqf'),
    ('pcunmlq','psormlq'),
    ('pzgeqrf','pdgeqrf'),
    ('pzunmqr','pdormqr'),
    ('pzgelqf','pdgelqf'),
    ('pzherbt','pdsyrbt'),
    ('pcherbt','pssyrbt'),
    ('pzhbrdt','pdsbrdt'),
    ('pchbrdt','pssbrdt'),
    ('pzunmlq','pdormlq'),
    ('plasma_pclapack','plasma_pslapack'),
    ('plasma_pzlapack','plasma_pdlapack'),
    ('plasma_pctile','plasma_pstile'),
    ('plasma_pztile','plasma_pdtile'),
  ],
  'normal' : [ ## Dictionary is keyed on substitution type
    ['s','d','c','z'], ## Special Line Indicating type columns

    ('#define PRECISION_s', '#define PRECISION_d', '#define PRECISION_c', '#define PRECISION_z' ),
    ('#define REAL',        '#define REAL',        '#define COMPLEX',     '#define COMPLEX'     ),
    ('#undef COMPLEX',      '#undef COMPLEX',      '#undef REAL',         '#undef REAL'         ),
    ('#define SINGLE',      '#define DOUBLE',      '#define SINGLE',      '#define DOUBLE'      ),
    ('#undef DOUBLE',       '#undef SINGLE',       '#undef DOUBLE',       '#undef SINGLE'       ),
    ('float',               'double',              'parsec_complex32_t',   'parsec_complex64_t'   ),
    ('matrix_RealFloat',    'matrix_RealDouble',   'matrix_ComplexFloat', 'matrix_ComplexDouble'),
      ('tile_coll_RealFloat',    'tile_coll_RealDouble',   'tile_coll_ComplexFloat', 'tile_coll_ComplexDouble'),
    ('float_t'  ,           'double_t',            'complex_t',           'double_complex_t'    ),
    ('float',               'double',              'float',               'double'              ),
    ('matrix_RealFloat',    'matrix_RealDouble',   'matrix_RealFloat',    'matrix_RealDouble'   ),
    ('MPI_FLOAT',           'MPI_DOUBLE',          'MPI_COMPLEX',         'MPI_DOUBLE_COMPLEX'  ),
    ('MPI_FLOAT',           'MPI_DOUBLE',          'MPI_FLOAT',           'MPI_DOUBLE'          ),
    ('smatrix',             'dmatrix',             'cmatrix',             'zmatrix'             ),
    ('stwoDBC',             'dtwoDBC',             'ctwoDBC',             'ztwoDBC'             ),
    ('float',               'double',              'cuFloatComplex',      'cuDoubleComplex'     ),
    ('',                    '',                    'crealf',              'creal'               ),
    ('',                    '',                    'cimagf',              'cimag'               ),
    ('',                    '',                    'conjf',               'conj'                ),
    ('',                    '',                    'cuCfmaf',             'cuCfma'              ),

    ('cblas_snrm2','cblas_dnrm2','cblas_scnrm2','cblas_dznrm2'),
    ('cblas_sasum','cblas_dasum','cblas_scasum','cblas_dzasum'),
    ('CORE_sasum','CORE_dasum','CORE_scasum','CORE_dzasum'),
    ('core_sasum','core_dasum','core_scasum','core_dzasum'),
    ('','','CORE_slag2c','CORE_dlag2z'),
    ('strdv', 'dtrdv', 'ctrdv', 'ztrdv'),
    ('strdsm', 'dtrdsm', 'ctrdsm', 'ztrdsm'),
    ('ssytr', 'dsytr', 'chetr', 'zhetr'),
    ('ssygst','dsygst','chegst','zhegst'),
    ('SSYGST','DSYGST','CHEGST','ZHEGST'),
    ('ssterf','dsterf','ssterf','dsterf'),
    ('ssytrd','dsytrd','chetrd','zhetrd'),
    ('SSYTRD','DSYTRD','CHETRD','ZHETRD'),
    ('sgebrd','dgebrd','cgebrd','zgebrd'),
    ('SGEBRD','DGEBRD','CGEBRD','ZGEBRD'),
    ('STILE','DTILE','CTILE','ZTILE'),
    ('stile','dtile','ctile','ztile'),
    ('slag2d','dlag2s','clag2z','zlag2c'),
    ('ssyrfb','dsyrfb','cherfb','zherfb'),
    ('ssyrf','dsyrf','cherf','zherf'),
    ('saxpy','daxpy','caxpy','zaxpy'),
    ('sgeadd','dgeadd','cgeadd','zgeadd'),
    ('ssymm','dsymm','chemm','zhemm'),
    ('SSYMM','DSYMM','CHEMM','ZHEMM'),
    ('ssyr','dsyr','cher','zher'),
    ('SSYR','DSYR','CHER','ZHER'),
    ('SYRK','SYRK','HERK','HERK'),
    ('SYR2K','SYR2K','HER2K','HER2K'),
    ('sgesv','dgesv','cgesv','zgesv'),
    ('SUNGESV','SUNGESV','CUNGESV','CUNGESV'),
    ('SGESV','DGESV','CGESV','ZGESV'),
    ('SGESV','SGESV','CGESV','CGESV'),
    ('SPOSV','SPOSV','CPOSV','CPOSV'),
    ('sgels','dgels','cgels','zgels'),
    ('SGELS','DGELS','CGELS','ZGELS'),
      ('sgemm','dgemm','cgemm','zgemm'),
      ('SGEMM','DGEMM','CGEMM','ZGEMM'),
      ('ssumma','dsumma','csumma','zsumma'),
      ('SSUMMA','DSUMMA','CSUMMA','ZSUMMA'),
    ('sposv','dposv','cposv','zposv'),
    ('SPOSV','DPOSV','CPOSV','ZPOSV'),
    ('ssymm','dsymm','csymm','zsymm'),
    ('SSYMM','DSYMM','CSYMM','ZSYMM'),
    ('ssyrk','dsyrk','csyrk','zsyrk'),
    ('SSYRK','DSYRK','CSYRK','ZSYRK'),
    ('ssyr2k','dsyr2k','csyr2k','zsyr2k'),
    ('SSYR2K','DSYR2K','CSYR2K','ZSYR2K'),
    ('strmm','dtrmm','ctrmm','ztrmm'),
    ('strmdm','dtrmdm','ctrmdm','ztrmdm'),
    ('STRMM','DTRMM','CTRMM','ZTRMM'),
    ('ssyrbt','dsyrbt','cherbt','zherbt'),
    ('SSYRBT','DSYRBT','CHERBT','ZHERBT'),
    ('ssbrdt','dsbrdt','chbrdt','zhbrdt'),
    ('SSBRDT','DSBRDT','CHBRDT','ZHBRDT'),
    ('strsm','dtrsm','ctrsm','ztrsm'),
    ('STRSM','DTRSM','CTRSM','ZTRSM'),
    ('sgelq2','dgelq2','cgelq2','zgelq2'),
    ('sgelqf','dgelqf','cgelqf','zgelqf'),
    ('SGELQF','DGELQF','CGELQF','ZGELQF'),
    ('sgelqs','dgelqs','cgelqs','zgelqs'),
    ('SGELQS','DGELQS','CGELQS','ZGELQS'),
    ('sgeqr','dgeqr','cgeqr','zgeqr'),
    ('SGEQR','DGEQR','CGEQR','ZGEQR'),
    ('sgetf2','dgetf2','cgetf2','zgetf2'),
    ('sgetrf','dgetrf','cgetrf','zgetrf'),
    ('SGETRF','DGETRF','CGETRF','ZGETRF'),
    ('sgetrs','dgetrs','cgetrs','zgetrs'),
    ('SGETRS','DGETRS','CGETRS','ZGETRS'),
    ('sgerbb','dgerbb','cgerbb','zgerbb'),
    ('SGERBB','DGERBB','CGERBB','ZGERBB'),
    ('splrnt','dplrnt','cplrnt','zplrnt'),
    ('splgsy','dplgsy','cplgsy','zplgsy'),
    ('splgsy','dplgsy','cplghe','zplghe'),
    ('spltmg','dpltmg','cpltmg','zpltmg'),
    ('slasc', 'dlasc', 'clasc', 'zlasc'),
    ('slaset','dlaset','claset','zlaset'),
    ('slatms', 'dlatms', 'clatms', 'zlatms'),
    ('sprint','dprint','cprint','zprint'),
    ('slacgv','dlacgv','clacgv','zlacgv'),
    ('slacpy','dlacpy','clacpy','zlacpy'),
    ('slagsy','dlagsy','claghe','zlaghe'),
    ('slagsy','dlagsy','clagsy','zlagsy'),
    ('SLANGE','DLANGE','CLANGE','ZLANGE'),
    ('SLANM2','DLANM2','CLANM2','ZLANM2'),
    ('SLANSY','DLANSY','CLANHE','ZLANHE'),
    ('SLANSY','DLANSY','CLANSY','ZLANSY'),
    ('SLANTR','DLANTR','CLANTR','ZLANTR'),
    ('slange','dlange','clange','zlange'),
    ('slanm2','dlanm2','clanm2','zlanm2'),
    ('slansy','dlansy','clanhe','zlanhe'),
    ('slansy','dlansy','clansy','zlansy'),
    ('slantr','dlantr','clantr','zlantr'),
    ('slarfb','dlarfb','clarfb','zlarfb'),
    ('slarfg','dlarfg','clarfg','zlarfg'),
    ('slarft','dlarft','clarft','zlarft'),
    ('slarnv','dlarnv','clarnv','zlarnv'),
    ('slaswp','dlaswp','claswp','zlaswp'),
    ('smap','dmap','cmap','zmap'),
    ('spotrf','dpotrf','cpotrf','zpotrf'),
    ('sgebmm','dgebmm','cgebmm','zgebmm'),
    ('sgebut','dgebut','cgebut','zgebut'),
    ('ssybut','dsybut','chebut','zhebut'),
    ('SPOTRF','DPOTRF','CPOTRF','ZPOTRF'),
    ('spotrs','dpotrs','cpotrs','zpotrs'),
    ('SPOTRS','DPOTRS','CPOTRS','ZPOTRS'),
    ('sorglq','dorglq','cunglq','zunglq'),
    ('SORGLQ','DORGLQ','CUNGLQ','ZUNGLQ'),
    ('sorgqr','dorgqr','cungqr','zungqr'),
    ('SORGQR','DORGQR','CUNGQR','ZUNGQR'),
    ('sormlq','dormlq','cunmlq','zunmlq'),
    ('SORMLQ','DORMLQ','CUNMLQ','ZUNMLQ'),
    ('sormqr','dormqr','cunmqr','zunmqr'),
    ('SORMQR','DORMQR','CUNMQR','ZUNMQR'),
    ('ORMQR','ORMQR','UNMQR','UNMQR'),
    ('slamch','dlamch','slamch','dlamch'),
    ('slarnv','dlarnv','slarnv','dlarnv'),
    ('slauum','dlauum','clauum','zlauum'),
    ('spoinv','dpoinv','cpoinv','zpoinv'),
    ('spotri','dpotri','cpotri','zpotri'),
    ('strtri','dtrtri','ctrtri','ztrtri'),
    ('strsmpl','dtrsmpl','ctrsmpl','ztrsmpl'),
    ('STRSMPL','DTRSMPL','CTRSMPL','ZTRSMPL'),
    ('stsqrt','dtsqrt','ctsqrt','ztsqrt'),
    ('STSQRT','DTSQRT','CTSQRT','ZTSQRT'),
    ('stsmqr','dtsmqr','ctsmqr','ztsmqr'),
    ('STSMQR','DTSMQR','CTSMQR','ZTSMQR'),
    ('sparfb','dparfb','cparfb','zparfb'),
    ('SPARFB','DPARFB','CPARFB','ZPARFB'),
    ('slacpy','dlacpy','slacpy','dlacpy'),
    ('slaset','dlaset','slaset','dlaset'),
    ('sscal','dscal','csscal','zdscal'),
    ('sger','dger','cger','zger'),
    ('ger','ger','gerc','gerc'),
    ('ger','ger','geru','geru'),
    ('symm','symm','hemm','hemm'),
    ('syrk','syrk','herk','herk'),
    ('syr2k','syr2k','her2k','her2k'),
    ('sytr', 'sytr', 'hetr', 'hetr'),
    ('syrbt','syrbt','herbt','herbt'),
    ('ssyev','dsyev','cheev','zheev'),
    ('syrfb1','syrfb1','herfb1','herfb1'),
    ('lansy','lansy','lanhe','lanhe'),
    ('syssq','syssq','hessq','hessq'),
    ('\*\*T','\*\*T','\*\*H','\*\*H'),
    ('BLAS_s','BLAS_d','BLAS_s','BLAS_d'),
    ('BLAS_s','BLAS_d','BLAS_c','BLAS_z'),
    ('','','CBLAS_SADDR','CBLAS_SADDR'),
    ('CblasTrans','CblasTrans','CblasConjTrans','CblasConjTrans'),
    ('REAL','DOUBLE_PRECISION','COMPLEX','COMPLEX_16'),
    ('powf', 'pow', 'cpowf','cpow'),
    ('fabsf','fabs','fabsf','fabs'),
    ('fabsf','fabs','cabsf','cabs'),
    ('fmaxf','fmax','fmaxf','fmax'),
    ('fmaxf','fmax','cmaxf','cmax'),
    ('fminf','fmin','fminf','fmin'),
    ('fminf','fmin','cminf','cmin'),
    ('float','double','float _Complex','double _Complex'),
    ('float','double','float','double'),
    ('lapack_slamch','lapack_dlamch','lapack_slamch','lapack_dlamch'),
    ('float','double','PLASMA_Complex32_t','PLASMA_Complex64_t'),
    ('float','double','PLASMA_voidComplex32_t','PLASMA_voidComplex64_t'),
    ('PLASMA_sor','PLASMA_dor','PLASMA_cun','PLASMA_zun'),
    ('PlasmaRealFloat','PlasmaRealDouble','PlasmaComplexFloat','PlasmaComplexDouble'),
    ('PlasmaTrans','PlasmaTrans','PlasmaConjTrans','PlasmaConjTrans'),
    ('BFT_s','BFT_d','BFT_c','BFT_z'),
    ('RBMM_s','RBMM_d','RBMM_c','RBMM_z'),
    ('stesting','dtesting','ctesting','ztesting'),
    ('SAUXILIARY','DAUXILIARY','CAUXILIARY','ZAUXILIARY'),
    ('sauxiliary','dauxiliary','cauxiliary','zauxiliary'),
    ('scheck','dcheck','ccheck','zcheck'),
    ('stile','dtile','ctile','ztile'),
    ('sdot','ddot','sdot','ddot'),
    ('dplasma_ssqrt','dplasma_dsqrt','dplasma_ssqrt','dplasma_dsqrt'),

    ('cblas_is',    'cblas_id',    'cblas_ic',    'cblas_iz'   ),
    ('cblas_s',     'cblas_d',     'cblas_c',     'cblas_z'    ),
    ('cblas_s',     'cblas_d',     'cblas_s',     'cblas_d'    ),
    ('check_s',     'check_d',     'check_c',     'check_z'    ),
    ('control_s',   'control_d',   'control_c',   'control_z'  ),
    ('compute_s',   'compute_d',   'compute_c',   'compute_z'  ),
    ('CORE_S',      'CORE_D',      'CORE_C',      'CORE_Z'     ),
    ('CORE_s',      'CORE_d',      'CORE_c',      'CORE_z'     ),
    ('CORE_s',      'CORE_d',      'CORE_s',      'CORE_d'     ),
    ('core_ssy',    'core_dsy',    'core_che',    'core_zhe'   ),
    ('core_s',      'core_d',      'core_c',      'core_z'     ),
    ('coreblas_s',  'coreblas_d',  'coreblas_c',  'coreblas_z' ),
    ('cuda_s',      'cuda_d',      'cuda_s',      'cuda_d'     ),
    ('cuda_s',      'cuda_d',      'cuda_c',      'cuda_z'     ),
    ('cublasS',     'cublasD',     'cublasS',     'cublasD'    ),
    ('cublasS',     'cublasD',     'cublasC',     'cublasZ'    ),
    ('example_s',   'example_d',   'example_c',   'example_z'  ),
    ('FLOPS_SSY',   'FLOPS_DSY',   'FLOPS_CHE',   'FLOPS_ZHE'  ),
    ('FLOPS_S',     'FLOPS_D',     'FLOPS_C',     'FLOPS_Z'    ),
    ('lapack_s',    'lapack_d',    'lapack_c',    'lapack_z'   ),
    ('LAPACKE_s',   'LAPACKE_d',   'LAPACKE_c',   'LAPACKE_z'   ),
    ('PLASMA_s',    'PLASMA_d',    'PLASMA_c',    'PLASMA_z'   ),
    ('PLASMA_S',    'PLASMA_D',    'PLASMA_C',    'PLASMA_Z'   ),
    ('plasma_s',    'plasma_d',    'plasma_c',    'plasma_z'   ),
    ('plasma_ps',   'plasma_pd',   'plasma_pc',   'plasma_pz'  ),
      ('dplasma_s',   'dplasma_d',   'dplasma_c',   'dplasma_z'  ),
      ('summa_s',   'summa_d',   'summa_c',   'summa_z'  ),
    ('matrix_s',    'matrix_d',    'matrix_c',    'matrix_z'   ),
    ('testing_ds',  'testing_ds',  'testing_zc',  'testing_zc' ),
    ('testing_s',   'testing_d',   'testing_c',   'testing_z'  ),
    ('TESTING_S',   'TESTING_D',   'TESTING_C',   'TESTING_Z'  ),
    ('twoDBC_s',    'twoDBC_d',    'twoDBC_c',    'twoDBC_z'   ),
    ('workspace_s', 'workspace_d', 'workspace_c', 'workspace_z'),
    ('dplasmacore_s',   'dplasmacore_d',   'dplasmacore_c',   'dplasmacore_z'  ),

# Be carefull: don't put something matching the following line,
# it matches dplasma_datatype (should be changed)
#    ('plasma_s',    'plasma_d',    'plasma_s',    'plasma_d'   ),

  ],
  'tracing' : [
    ['plain','tau'],
    ('(\w+\*?)\s+(\w+)\s*\(([a-z* ,A-Z_0-9]*)\)\s*{\s+(.*)\s*#pragma tracing_start\s+(.*)\s+#pragma tracing_end\s+(.*)\s+}',r'\1 \2(\3){\n\4tau("\2");\5tau();\6}'),
    ('\.c','.c.tau'),
  ],
};
