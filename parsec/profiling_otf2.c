/*
 * Copyright (c) 2018-2019 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include "parsec/parsec_config.h"

#include "parsec/profiling.h"
#include "parsec/data_distribution.h"
#include "parsec/utils/debug.h"
#include "parsec/parsec_hwloc.h"
#include "parsec/os-spec-timing.h"
#include "parsec/sys/atomic.h"
#include "parsec/utils/mca_param.h"
#include "parsec/sys/tls.h"

#include <sys/stat.h>
#include <otf2/otf2.h>
#if MPI_VERSION < 3
#define OTF2_MPI_UINT64_T MPI_UNSIGNED_LONG
#define OTF2_MPI_INT64_T  MPI_LONG
#endif
#include <otf2/OTF2_MPI_Collectives.h>

#include <mpi.h>
static MPI_Comm parsec_otf2_profiling_comm;
static int parsec_profiling_mpi_on = 0;
void *parsec_profiling_pcomm = NULL;

struct parsec_profiling_stream_s {
    parsec_list_item_t super;
    int                id;
    uint64_t           nb_evt;
    parsec_list_t      informations;
    OTF2_EvtWriter    *evt_writer;
};

typedef struct {
    int    otf2_region_id;
    char  *name;
    char  *alternative_name;
    char  *description;
    size_t info_length;
    int    attr_index;
    int   *otf2_attribute_types;
    int    otf2_nb_attributes;
} parsec_profiling_region_t;

typedef struct {
    parsec_list_item_t super;
    char *key;
    char *value;
} parsec_profiling_info_t;

PARSEC_DECLSPEC PARSEC_OBJ_CLASS_DECLARATION(parsec_profiling_info_t);
PARSEC_OBJ_CLASS_INSTANCE(parsec_profiling_info_t, parsec_list_item_t,
                   NULL, NULL);

int parsec_profile_enabled = 0;

PARSEC_TLS_DECLARE(tls_profiling);
static parsec_list_t threads;
static int __profile_initialized = 0;  /* not initialized */
static int __already_called = 0;
static parsec_time_t parsec_start_time;
static int          start_called = 0;
#define MAX_PROFILING_ERROR_STRING_LEN 1024
static char  parsec_profiling_last_error[MAX_PROFILING_ERROR_STRING_LEN+1] = { '\0', };
static int   parsec_profiling_raise_error = 0;
static parsec_list_t global_informations;

static parsec_profiling_region_t *regions = NULL;
static int nbregions                      = 0;
static int next_region                    = 0;

static int  thread_profiling_id = 0;
static int *threads_per_rank = NULL;
static int  threads_before_me = 0;

typedef struct {
    char *type_name;
    OTF2_Type type_desc;
} otf2_convertor_t;
static otf2_convertor_t otf2_convertor[] = {
    { "uint8_t", OTF2_TYPE_UINT8 },
    { "uint16_t", OTF2_TYPE_UINT16 },
    { "uint32_t", OTF2_TYPE_UINT32 },
    { "uint64_t", OTF2_TYPE_UINT64 },
    { "int8_t", OTF2_TYPE_INT8 },
    { "int16_t", OTF2_TYPE_INT16 },
    { "int32_t", OTF2_TYPE_INT32 },
    { "int64_t", OTF2_TYPE_INT64 },
    { "float", OTF2_TYPE_FLOAT },
    { "double", OTF2_TYPE_DOUBLE },
};
static int nb_native_otf2_types = (int)sizeof(otf2_convertor)/sizeof(otf2_convertor_t);

static void set_last_error(const char *format, ...)
{
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = vsnprintf(parsec_profiling_last_error, MAX_PROFILING_ERROR_STRING_LEN, format, ap);
    va_end(ap);
    parsec_warning("ParSEC profiling OTF2 -- Last error set to %s", parsec_profiling_last_error);
    parsec_profiling_raise_error = 1;
    (void)rc;
}

static OTF2_FlushType pre_flush(void*            userData,
                                OTF2_FileType    fileType,
                                OTF2_LocationRef location,
                                void*            callerData,
                                bool             final )
{
    (void)userData;
    (void)fileType;
    (void)location;
    (void)callerData;
    (void)final;
    return OTF2_FLUSH;
}

static OTF2_TimeStamp post_flush(void*            userData,
                                 OTF2_FileType    fileType,
                                 OTF2_LocationRef location )
{
    (void)userData;
    (void)fileType;
    (void)location;
    return (OTF2_TimeStamp)parsec_profiling_get_time();
}

static OTF2_FlushCallbacks flush_callbacks = {
    .otf2_pre_flush  = pre_flush,
    .otf2_post_flush = post_flush
};


static OTF2_Archive* otf2_archive = NULL;
static OTF2_GlobalDefWriter* global_def_writer = NULL;
static int32_t next_strid = 1;
static int emptystrid = 0;

static int next_otf2_global_strid(void)
{
    return parsec_atomic_fetch_inc_int32(&next_strid);
}

char *parsec_profiling_strerror(void)
{
    return parsec_profiling_last_error;
}

void parsec_profiling_add_information( const char *key, const char *value )
{
    parsec_profiling_info_t *new_info;
    char *c;

    if( !__profile_initialized ) return;

    new_info = PARSEC_OBJ_NEW(parsec_profiling_info_t);
    /* OTF2 format: keys must be in a namespace separated by ::,
     *              keys are in [a-zA-Z0-9_]+ */
    asprintf(&new_info->key, "PARSEC::%s", key);
    for(c = new_info->key; *c!='\0'; c++) {
        if( ! ( (*c>='a' && *c <= 'z') ||
                (*c>='A' && *c <= 'Z') ||
                (*c>='0' && *c <= '9') ||
                (*c == ':') ||
                (*c == '_') ) ) {
            *c = '_';
        }
    }
    new_info->value = strdup(value);
    parsec_list_push_back(&global_informations, &new_info->super);
}

void parsec_profiling_stream_add_information(parsec_profiling_stream_t* stream,
                                             const char *key, const char *value )
{
    char *info;
    asprintf(&info, "%s [Thread %d]", key, stream->id);
    parsec_profiling_add_information(info, value);
    free(info);
}

void parsec_profiling_otf2_set_comm(void *_pcomm)
{
    MPI_Comm *pcomm = (MPI_Comm*)_pcomm;
    (void)MPI_Initialized( &parsec_profiling_mpi_on );
    if( parsec_profiling_mpi_on ) {
        MPI_Comm_dup(*pcomm, &parsec_otf2_profiling_comm);
    }
}

int parsec_profiling_init( int process_id )
{
    if( __profile_initialized ) return PARSEC_ERR_NOT_SUPPORTED;

    (void)process_id; /* OTF2 renames the processes according to their rank */

    PARSEC_TLS_KEY_CREATE(tls_profiling);

    PARSEC_OBJ_CONSTRUCT( &threads, parsec_list_t );
    PARSEC_OBJ_CONSTRUCT(&global_informations, parsec_list_t);

    /* As we called the _start function automatically, the timing will be
     * based on this moment. By forcing back the __already_called to 0, we
     * allow the caller to decide when to rebase the timing in case there
     * is a need.
     */
    __already_called = 0;
    parsec_profile_enabled = 1;  /* turn on the profiling */

    /* add the hostname, for the sake of explicit profiling */
    char buf[HOST_NAME_MAX];
    if (0 == gethostname(buf, HOST_NAME_MAX))
        parsec_profiling_add_information("hostname", buf);
    else
        parsec_profiling_add_information("hostname", "");

    /* the current working directory may also be helpful */
    char * newcwd = NULL;
    int bufsize = HOST_NAME_MAX;
    errno = 0;
    char * cwd = getcwd(buf, bufsize);
    while (cwd == NULL && errno == ERANGE) {
        bufsize *= 2;
        cwd = realloc(cwd, bufsize);
        if (cwd == NULL)            /* failed  - just give up */
            break;
        errno = 0;
        newcwd = getcwd(cwd, bufsize);
        if (newcwd == NULL) {
            free(cwd);
            cwd = NULL;
        }
    }
    if (cwd != NULL) {
        parsec_profiling_add_information("cwd", cwd);
        if (cwd != buf)
            free(cwd);
    } else
        parsec_profiling_add_information("cwd", "");

    nbregions = 128;
    regions = (parsec_profiling_region_t*)malloc(sizeof(parsec_profiling_region_t) * nbregions);
    next_region = 0;

    if( !parsec_profiling_mpi_on ) {
        /* Nobody has called parsec_profiling_otf2_set_comm yet,
         * so we use MPI_COMM_WORLD by default */
        MPI_Comm comm = MPI_COMM_WORLD;
        parsec_profiling_otf2_set_comm( &comm );
    }

    __profile_initialized = 1; //* confirmed */
    return 0;
}

parsec_profiling_stream_t* parsec_profiling_stream_init( size_t length, const char *format, ...)
{
    parsec_profiling_stream_t* res;

    (void)length;

    if( !__profile_initialized ) return NULL;

    res = (parsec_profiling_stream_t*)calloc(sizeof(parsec_profiling_stream_t), 1);
    PARSEC_OBJ_CONSTRUCT(res, parsec_list_item_t);
    PARSEC_OBJ_CONSTRUCT(&res->informations, parsec_list_t);

    res->id = parsec_atomic_fetch_inc_int32(&thread_profiling_id);
    res->nb_evt = 0;
    res->evt_writer = NULL;

    (void)format; /* All strings must be written by the rank 0 in OTF2.
                   * For now, forget about the human-readable data */

    parsec_list_push_back( &threads, (parsec_list_item_t*)res );

    return res;
}

parsec_profiling_stream_t *parsec_profiling_set_default_thread( parsec_profiling_stream_t *new )
{
    parsec_profiling_stream_t *old;
    old = PARSEC_TLS_GET_SPECIFIC(tls_profiling);
    PARSEC_TLS_SET_SPECIFIC(tls_profiling, new);
    return old;
}

int parsec_profiling_dbp_start( const char *_basefile, const char *hr_info )
{
    char *archive_path, *archive_name, *c, *basefile;
    struct stat sb;
    OTF2_ErrorCode rc;
    char hostname[256];
    int rank = 0;
    int size = 1;
    char *xmlbuffer;
    int buflen;
    if(parsec_profiling_mpi_on) {
        MPI_Comm_rank(parsec_otf2_profiling_comm, &rank);
        MPI_Comm_size(parsec_otf2_profiling_comm, &size);
    }

    if( !__profile_initialized ) return -1;

    basefile = strdup(_basefile);
    archive_name = NULL;
    for(c = basefile; *c != '\0'; c++) {
        if( *c == '/' ) {
            archive_name = c+1;
        }
    }
    if( NULL == archive_name ) {
        /* No '/' in basefile */
        archive_path = strdup(".");
        archive_name = basefile;
    } else {
        archive_path = basefile;
        *(archive_name-1) = '\0'; /* Cut basefile at last '/' */
        archive_name = strdup(archive_name); /* Get an independent copy of the archive_name */
    }
    basefile = NULL; /* It's either in archive_name or archive_path, so it's going to be freed later anyway */

    if (stat(archive_path, &sb) != 0 || !S_ISDIR(sb.st_mode)) {
        set_last_error("PaRSEC Profiling System: error -- '%s': directory not found", archive_path);
        free(archive_path);
        free(archive_name);
        return PARSEC_ERR_NOT_FOUND;
    }

    /* Reset the error system */
    snprintf(parsec_profiling_last_error, MAX_PROFILING_ERROR_STRING_LEN, "PaRSEC Profiling System: success");
    parsec_profiling_raise_error = 0;

    /* It's fine to re-reset the event date: we're back with a zero-length event set */
    start_called = 0;

    otf2_archive = OTF2_Archive_Open( archive_path,
                                      archive_name,
                                      OTF2_FILEMODE_WRITE,
                                      1024 * 1024 /* event chunk size */,
                                      4 * 1024 * 1024 /* def chunk size */,
                                      OTF2_SUBSTRATE_POSIX,
                                      OTF2_COMPRESSION_NONE );
    free(archive_path);
    free(archive_name);

    if( NULL == otf2_archive ) {
        set_last_error("PaRSEC Profiling System: OTF2 Error while creating archive");
        /* archive was not created, do not close it */
        return PARSEC_ERROR;
    }

    rc = OTF2_Archive_SetFlushCallbacks( otf2_archive, &flush_callbacks, NULL );
    if( OTF2_SUCCESS != rc ) {
        set_last_error("PaRSEC Profiling System: OTF2 error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        /* OTF2 seg faults if closing the archive at this time */
        otf2_archive = NULL;
        return PARSEC_ERROR;
    }
    rc = OTF2_MPI_Archive_SetCollectiveCallbacks( otf2_archive,
                                                  parsec_otf2_profiling_comm,
                                                  MPI_COMM_NULL );
    if( OTF2_SUCCESS != rc ) {
        set_last_error("PaRSEC Profiling System: OTF2 error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        /* OTF2 seg faults if closing the archive at this time */
        otf2_archive = NULL;
        return PARSEC_ERROR;
    }
    rc = OTF2_Archive_OpenEvtFiles( otf2_archive );
    if( OTF2_SUCCESS != rc ) {
        set_last_error("PaRSEC Profiling System: OTF2 error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        OTF2_Archive_Close(otf2_archive);
        return PARSEC_ERROR;
    }

    if( rank == 0 ) {
        global_def_writer = OTF2_Archive_GetGlobalDefWriter( otf2_archive );
        OTF2_GlobalDefWriter_WriteString(global_def_writer, emptystrid, "");
    }

    gethostname(hostname, 256);
    if( (rc = OTF2_Archive_SetMachineName(otf2_archive, hostname)) != OTF2_SUCCESS ) {
        set_last_error("PaRSEC Profiling System: error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        return PARSEC_ERROR;
    }

    if( (rc = OTF2_Archive_SetDescription(otf2_archive, hr_info)) != OTF2_SUCCESS ) {
        set_last_error("PaRSEC Profiling System: error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        return PARSEC_ERROR;
    }

    if( (rc = OTF2_Archive_SetCreator(otf2_archive, "PaRSEC Profiling System")) != OTF2_SUCCESS ) {
        set_last_error("PaRSEC Profiling System: error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        return PARSEC_ERROR;
    }

    if( parsec_hwloc_export_topology(&buflen, &xmlbuffer) != -1 &&
        buflen > 0 ) {
        parsec_profiling_add_information("HWLOC-XML", xmlbuffer);
        parsec_hwloc_free_xml_buffer(xmlbuffer);
    }

    return 0;
}

void parsec_profiling_start(void)
{
    parsec_list_item_t *r;
    parsec_profiling_stream_t *tp;
    int size = 1, rank = 0;

    if(start_called)
        return;

    if( NULL == otf2_archive )
        return;

    if(parsec_profiling_mpi_on) {
        MPI_Comm_size(parsec_otf2_profiling_comm, &size);
        MPI_Comm_rank(parsec_otf2_profiling_comm, &rank);
        threads_per_rank = (int*)malloc(sizeof(int) * size);
        MPI_Allgather(&thread_profiling_id, 1, MPI_INT,
                      threads_per_rank, 1, MPI_INT,
                      parsec_otf2_profiling_comm);
        threads_before_me = 0;
        for(int r = 0; r < rank; r++)
            threads_before_me += threads_per_rank[r];
    } else {
        threads_per_rank = (int*)malloc(sizeof(int) );
        threads_per_rank[0] = thread_profiling_id;
        threads_before_me = 0;
    }

    parsec_list_lock( &threads );
    for( r = PARSEC_LIST_ITERATOR_FIRST(&threads);
         r != PARSEC_LIST_ITERATOR_END(&threads);
         r = PARSEC_LIST_ITERATOR_NEXT(r) ) {
        tp = (parsec_profiling_stream_t*)r;
        tp->id += threads_before_me;
        tp->evt_writer = OTF2_Archive_GetEvtWriter( otf2_archive, tp->id );
        if( NULL == tp->evt_writer ) {
            parsec_warning("PaRSEC Profiling -- OTF2: could not allocate event writer for location %d\n", tp->id);
        }
    }
    parsec_list_unlock( &threads );

    if( parsec_profiling_mpi_on ) {
        MPI_Barrier(parsec_otf2_profiling_comm);
    }

    start_called = 1;
    parsec_start_time = take_time();
}


int parsec_profiling_reset( void )
{
    return 0;
}

int parsec_profiling_add_dictionary_keyword( const char* key_name, const char* attributes,
                                             size_t info_length,
                                             const char* orig_convertor_code,
                                             int* key_start, int* key_end )
{
    int region;
    int rank = 0, rc;
    char *c;
    char *convertor_code;
    char *name, *type;
    int t;
    int strid;
    if( !__profile_initialized ) return 0;
    if(parsec_profiling_mpi_on) {
        MPI_Comm_rank(parsec_otf2_profiling_comm, &rank);
    }
    (void)attributes;

    if( next_region + 1 >= nbregions ) {
        nbregions += 128;
        regions = realloc(regions, sizeof(parsec_profiling_region_t) * nbregions);
    }
    region = next_region;
    next_region++;

    regions[region].otf2_region_id = region;
    regions[region].name = strdup(key_name);
    regions[region].alternative_name = strdup(key_name);
    regions[region].description = strdup("");
    regions[region].info_length = info_length;
    regions[region].otf2_attribute_types = NULL;
    regions[region].otf2_nb_attributes = 0;

    if( region > 0 ) {
        regions[region].attr_index = regions[region-1].attr_index + regions[region-1].otf2_nb_attributes;
    } else {
        regions[region].attr_index = 0;
    }

    if( NULL == orig_convertor_code )
        return 0; /* Nothing else to do */

    convertor_code = strdup(orig_convertor_code);
    c = convertor_code;
    regions[region].otf2_nb_attributes = 1;
    for(c = convertor_code; *c != '\0'; c++)
        if( *c == ';' )
            regions[region].otf2_nb_attributes++;

    while( *c != '\0') {
        while( *c != '{' ) {
            c++;
        }
        c++;
        for(t = 0; t < nb_native_otf2_types; t++) {
            if( strcmp(c, otf2_convertor[t].type_name) == 0 ) {
                regions[region].otf2_nb_attributes++;
                break;
            }
        }
        if(t == nb_native_otf2_types) {
            parsec_warning("parsec_profiling: in description of informations for dictionary entry '%s', unspecified type '%s' used in convertor code\n"
                           "  All informations for this key are going to be ignored.\n",
                           key_name, c, convertor_code);
            regions[region].otf2_nb_attributes = 0;
            goto malformed_convertor_code;
        }
        while( *c != ';' ) {
            if( *c == '\0' )
                break;
            c++;
        }
    }

    regions[region].otf2_attribute_types = (int*)malloc(sizeof(int) * regions[region].otf2_nb_attributes);
    c = convertor_code;
    name = c;
    regions[region].otf2_nb_attributes = 0;
    while( *c != '\0') {
        while( *c != '{' ) {
            if( *c == ';' || *c == '\0' ) {
                parsec_warning("parsec_profiling: in description of informations for dictionary entry '%s', an invalid convertor code is used (at character %d of '%s')\n"
                               "  All informations for this key are going to be ignored.\n",
                               key_name, (int)((uintptr_t)c - (uintptr_t)convertor_code), convertor_code);
                regions[region].otf2_nb_attributes = 0;
                goto malformed_convertor_code;
            }
            c++;
        }
        *c++ = '\0'; /* Overwrite '{' into a '\0' so name is terminated */
        type = c;
        while( *c != '}' ) {
            if( *c == ';' || *c == '\0' ) {
                parsec_warning("parsec_profiling: in description of informations for dictionary entry '%s', an invalid convertor code is used (at character %d of '%s')\n"
                               "  All informations for this key are going to be ignored.\n",
                               key_name, (int)((uintptr_t)c - (uintptr_t)convertor_code), orig_convertor_code);
                regions[region].otf2_nb_attributes = 0;
                goto malformed_convertor_code;
            }
            c++;
        }
        *c++ = '\0'; /* Overwrite '}' into a '\0' so type is terminated */

        for(t = 0; t < nb_native_otf2_types; t++) {
            if( strcmp(type, otf2_convertor[t].type_name) == 0 ) {
                regions[region].otf2_attribute_types[regions[region].otf2_nb_attributes] = otf2_convertor[t].type_desc;
                if( NULL != global_def_writer ) {
                    strid = next_otf2_global_strid();
                    rc = OTF2_GlobalDefWriter_WriteString(global_def_writer,
                                                          strid,
                                                          name);
                    if(rc != OTF2_SUCCESS) {
                        parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
                    }
                    rc = OTF2_GlobalDefWriter_WriteAttribute(global_def_writer,
                                                             regions[region].attr_index + regions[region].otf2_nb_attributes,
                                                             strid,
                                                             emptystrid,
                                                             otf2_convertor[t].type_desc);
                    if(rc != OTF2_SUCCESS) {
                        parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
                    }
                }
                break;
            }
        }
        if(t == nb_native_otf2_types ) {
            if( strncmp(type, "char[", 5) == 0 ) {
                /* We don't support fixed-size strings yet, so we just remember to skip the bytes */
                int nb = atoi(&type[5]);
                regions[region].otf2_attribute_types[regions[region].otf2_nb_attributes] = -nb;
            } else {
                parsec_warning("PaRSEC Profiling System: OTF2 Error -- Unrecognized type '%s' -- type size must be specified e.g. int32_t", type);
                regions[region].otf2_attribute_types[regions[region].otf2_nb_attributes] = 0;
            }
        }
        regions[region].otf2_nb_attributes++;
        if(*c == '\0')
            break;
        if(*c == ';') {
            c++;
            name=c;
            continue;
        }
        parsec_warning("parsec_profiling: in description of informations for dictionary entry '%s', an invalid convertor code is used (at character %d of '%s')\n"
                               "  All informations for this key are going to be ignored.\n",
                               key_name, (int)((uintptr_t)c - (uintptr_t)convertor_code), orig_convertor_code);
                regions[region].otf2_nb_attributes = 0;
                goto malformed_convertor_code;
    }

    malformed_convertor_code:
    free(convertor_code);
    *key_start = region;
    *key_end   = -region;

    return 0;
}


int parsec_profiling_dictionary_flush( void )
{
    return 0;
}


int parsec_profiling_ts_trace_flags(int key, uint64_t event_id, uint32_t taskpool_id,
                                    void *info, uint16_t flags )
{
    parsec_profiling_stream_t* ctx;

    if( !start_called ) {
        return PARSEC_ERR_NOT_SUPPORTED;
    }

    ctx = PARSEC_TLS_GET_SPECIFIC(tls_profiling);
    if( NULL != ctx )
        return parsec_profiling_trace_flags(ctx, key, event_id, taskpool_id, info, flags);

    set_last_error("Profiling system: error: called parsec_profiling_ts_trace_flags"
                   " from a thread that did not call parsec_profiling_stream_init\n");
    return PARSEC_ERR_NOT_SUPPORTED;
}

int
parsec_profiling_trace_flags(parsec_profiling_stream_t* context, int key,
                             uint64_t event_id, uint32_t taskpool_id,
                             void *info, uint16_t flags)
{
    parsec_time_t now;
    int region;
    char *ptr;
    int rc = OTF2_SUCCESS;
    OTF2_AttributeList *attribute_list = NULL;
    uint64_t timestamp;

    (void)taskpool_id;
    (void)event_id;
    (void)flags;

    if( !start_called ) {
        return PARSEC_ERR_NOT_SUPPORTED;
    }

    if( NULL == context->evt_writer )
        return PARSEC_ERR_BAD_PARAM;

    now = take_time();
    timestamp = diff_time(parsec_start_time, now);

    region = key < 0 ? -key : key;

    if( NULL != info ) {
        attribute_list = OTF2_AttributeList_New();
        ptr = info;
        for(int t = 0; t < regions[region].otf2_nb_attributes; t++) {
            if( regions[region].otf2_attribute_types[t] > 0 ) {
                switch( regions[region].otf2_attribute_types[t] ) {
                case  OTF2_TYPE_UINT8:
                    rc = OTF2_AttributeList_AddUint8(attribute_list, regions[region].attr_index + t, *(uint8_t*)ptr);
                    ptr += sizeof(uint8_t);
                    break;
                case OTF2_TYPE_UINT16:
                    rc = OTF2_AttributeList_AddUint16(attribute_list, regions[region].attr_index + t, *(uint16_t*)ptr);
                    ptr += sizeof(uint16_t);
                    break;
                case OTF2_TYPE_UINT32:
                    rc = OTF2_AttributeList_AddUint32(attribute_list, regions[region].attr_index + t, *(uint32_t*)ptr);
                    ptr += sizeof(uint32_t);
                    break;
                case OTF2_TYPE_UINT64:
                    rc = OTF2_AttributeList_AddUint64(attribute_list, regions[region].attr_index + t, *(uint64_t*)ptr);
                    ptr += sizeof(uint64_t);
                    break;
                case OTF2_TYPE_INT8:
                    rc = OTF2_AttributeList_AddInt8(attribute_list, regions[region].attr_index + t, *(int8_t*)ptr);
                    ptr += sizeof(int8_t);
                    break;
                case OTF2_TYPE_INT16:
                    rc = OTF2_AttributeList_AddInt16(attribute_list, regions[region].attr_index + t, *(int16_t*)ptr);
                    ptr += sizeof(int16_t);
                    break;
                case OTF2_TYPE_INT32:
                    rc = OTF2_AttributeList_AddInt32(attribute_list, regions[region].attr_index + t, *(int32_t*)ptr);
                    ptr += sizeof(int32_t);
                    break;
                case OTF2_TYPE_INT64:
                    rc = OTF2_AttributeList_AddInt64(attribute_list, regions[region].attr_index + t, *(int64_t*)ptr);
                    ptr += sizeof(int64_t);
                    break;
                case OTF2_TYPE_FLOAT:
                    rc = OTF2_AttributeList_AddFloat(attribute_list, regions[region].attr_index + t, *(float*)ptr);
                    ptr += sizeof(float);
                    break;
                case OTF2_TYPE_DOUBLE:
                    rc = OTF2_AttributeList_AddDouble(attribute_list, regions[region].attr_index + t, *(double*)ptr);
                    ptr += sizeof(double);
                    break;
                default:
                    parsec_warning("PaRSEC Profiling System: internal error, type %d unkown", regions[region].otf2_attribute_types[t]);
                    break;
                }
                if(rc != OTF2_SUCCESS) {
                    parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
                }
            } else {
                ptr += -regions[region].otf2_attribute_types[t]; /* Skip negative types: they are used to say how many bytes to skip */
            }
        }
    }

    if( key > 0 )
        rc = OTF2_EvtWriter_Enter( context->evt_writer,
                                   attribute_list,
                                   timestamp,
                                   region );
    else
        rc = OTF2_EvtWriter_Leave( context->evt_writer,
                                   attribute_list,
                                   timestamp,
                                  region );
    if(rc != OTF2_SUCCESS) {
        parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
    } else {
        context->nb_evt++;
    }
    if( NULL != attribute_list ) {
        rc = OTF2_AttributeList_Delete(attribute_list);
         if(rc != OTF2_SUCCESS) {
             parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
         }
    }

    return 0;
}

int parsec_profiling_dbp_dump( void )
{
    int rank = 0;
    int size = 1;
    uint64_t epoch, gepoch;
    uint64_t *perlocation, *levts;
    int nb_threads_total;
    int nb_local_threads = 0;
    parsec_list_item_t *r;
    int rc;
    int strid;
    char string[64];
    int *displs;

    if( NULL == otf2_archive )
        return PARSEC_ERR_NOT_SUPPORTED;

    if(parsec_profiling_mpi_on) {
        MPI_Comm_rank(parsec_otf2_profiling_comm, &rank);
        MPI_Comm_size(parsec_otf2_profiling_comm, &size);
    }
    levts = (uint64_t*)malloc( sizeof(uint64_t) * thread_profiling_id );

    if( !__profile_initialized ) return 0;

    epoch = parsec_profiling_get_time();

    parsec_list_lock( &threads );
    for( r = PARSEC_LIST_ITERATOR_FIRST(&threads);
         r != PARSEC_LIST_ITERATOR_END(&threads);
         r = PARSEC_LIST_ITERATOR_NEXT(r) ) {
        parsec_profiling_stream_t *tp = (parsec_profiling_stream_t*)r;
        rc = OTF2_Archive_CloseEvtWriter( otf2_archive, tp->evt_writer );
        if(rc != OTF2_SUCCESS) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        }
        levts[nb_local_threads++] = tp->nb_evt;

        OTF2_DefWriter *def_writer = OTF2_Archive_GetDefWriter( otf2_archive, tp->id );
        if(NULL == def_writer ) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- could not open def_writer for location %d", tp->id);
        }
        rc = OTF2_Archive_CloseDefWriter( otf2_archive, def_writer );
        if(rc != OTF2_SUCCESS) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        }
    }
    parsec_list_unlock( &threads );

    if( parsec_profiling_mpi_on ) {
        MPI_Reduce( &epoch,
                    &gepoch,
                    1, OTF2_MPI_UINT64_T, MPI_MAX,
                    0, parsec_otf2_profiling_comm );
    } else {
        gepoch = epoch;
    }

    nb_threads_total = 0;
    for(int i = 0; i < size; i++)
        nb_threads_total += threads_per_rank[i];
    if( rank == 0 )
        perlocation = (uint64_t*)malloc( sizeof(uint64_t) * nb_threads_total);
    else
        perlocation = NULL;
    if(parsec_profiling_mpi_on) {
        int acc = 0;
        int rank;
        MPI_Comm_rank(parsec_otf2_profiling_comm, &rank);
        displs = (int*)malloc(sizeof(int)*size);
        for(int i = 0; i < size; i++) {
            displs[i] = acc;
            acc += threads_per_rank[i];
        }
        MPI_Gatherv(levts, threads_per_rank[rank], OTF2_MPI_UINT64_T,
                    perlocation, threads_per_rank, displs, OTF2_MPI_UINT64_T,
                    0, parsec_otf2_profiling_comm);
        free(displs);
    } else {
        memcpy(perlocation, levts, nb_threads_total * sizeof(uint64_t));
    }
    free(levts);

    if ( 0 == rank ) {
        r = PARSEC_LIST_ITERATOR_FIRST(&global_informations);
        while( r != PARSEC_LIST_ITERATOR_END(&global_informations) ) {
            parsec_profiling_info_t *pi = (parsec_profiling_info_t*)r;
            if((rc = OTF2_Archive_SetProperty(otf2_archive, pi->key, pi->value, 1)) != OTF2_SUCCESS) {
                set_last_error("PaRSEC Profiling System: error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
            }
            free(pi->key);
            free(pi->value);
            r = PARSEC_LIST_ITERATOR_NEXT(r);
            PARSEC_OBJ_RELEASE(pi);
        }

        rc = OTF2_GlobalDefWriter_WriteClockProperties( global_def_writer,
                                                        1000000000,
                                                        0, gepoch + 1);
        if(rc != OTF2_SUCCESS) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        }

        /* Define the dictionary */
        for(int r = 0; r < next_region; r++) {
            int nameid = next_otf2_global_strid();
            rc = OTF2_GlobalDefWriter_WriteString( global_def_writer, nameid, regions[r].name );
            if(rc != OTF2_SUCCESS) {
                parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
            }
            int altnameid = next_otf2_global_strid();
            rc = OTF2_GlobalDefWriter_WriteString( global_def_writer, altnameid, regions[r].alternative_name );
            if(rc != OTF2_SUCCESS) {
                parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
            }
            int descid = next_otf2_global_strid();
            rc = OTF2_GlobalDefWriter_WriteString( global_def_writer, descid, regions[r].description );
            if(rc != OTF2_SUCCESS) {
                parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
            }

            rc = OTF2_GlobalDefWriter_WriteRegion( global_def_writer,
                                                   regions[r].otf2_region_id /* id */,
                                                   nameid /* region name  */,
                                                   altnameid /* alternative name */,
                                                   descid /* description */,
                                                   OTF2_REGION_ROLE_FUNCTION,
                                                   OTF2_PARADIGM_NONE,
                                                   OTF2_REGION_FLAG_NONE,
                                                   emptystrid /* source file */,
                                                   0 /* begin lno */,
                                                   0 /* end lno */ );
            if(rc != OTF2_SUCCESS) {
                parsec_warning("PaRSEC Profiling System: OTF2 Error in write region -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
            }
        }

        /* Write the system tree into the global definitions */
        strid = next_otf2_global_strid();
        char hostname[256];
        gethostname(hostname, 256);
        rc = OTF2_GlobalDefWriter_WriteString( global_def_writer, strid, hostname );
        if(rc != OTF2_SUCCESS) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        }

        int strid2 = next_otf2_global_strid();
        rc = OTF2_GlobalDefWriter_WriteString( global_def_writer, strid2, "Node" );
        if(rc != OTF2_SUCCESS) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        }
        rc = OTF2_GlobalDefWriter_WriteSystemTreeNode( global_def_writer,
                                                       0 /* Node id */,
                                                       strid /* name */,
                                                       strid2 /* class */,
                                                       OTF2_UNDEFINED_SYSTEM_TREE_NODE /* parent */ );

        int id = 0;
        for(int r = 0; r < size; r++) {
            char pname[64];
            strid = next_otf2_global_strid();
            snprintf(pname, 64, "MPI Rank %d", r);
            rc = OTF2_GlobalDefWriter_WriteString( global_def_writer, strid, pname );
            if(rc != OTF2_SUCCESS) {
                parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
            }
            rc = OTF2_GlobalDefWriter_WriteLocationGroup( global_def_writer,
                                                          id,
                                                          strid,
                                                          OTF2_LOCATION_GROUP_TYPE_PROCESS,
                                                          0 /* system tree */ );
            if(rc != OTF2_SUCCESS) {
                parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
            }

            for(int t = 0; t < threads_per_rank[r]; t++) {
                strid = next_otf2_global_strid();
                snprintf(string, 64, "Thread %d, MPI Rank %d", t, r);
                rc = OTF2_GlobalDefWriter_WriteString( global_def_writer, strid, string );
                if(rc != OTF2_SUCCESS) {
                    parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
                }
                rc = OTF2_GlobalDefWriter_WriteLocation( global_def_writer,
                                                         id,
                                                         strid,
                                                         OTF2_LOCATION_TYPE_CPU_THREAD,
                                                         perlocation[id] /* # events */,
                                                         r /* location group */ );
                id++;
            }
        }

        /* Now we need to define the MPI communicator
         *  - First, what is the universe */
        id = 0;
        for(int r = 0; r < size; r++) {
            for(int t = 0; t < threads_per_rank[r]; t++) {
                perlocation[id++] = r;
            }
        }
        strid = next_otf2_global_strid();
        snprintf(string, 64, "MPI");
        rc = OTF2_GlobalDefWriter_WriteString( global_def_writer, strid, string );
        if(rc != OTF2_SUCCESS) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        }
        rc = OTF2_GlobalDefWriter_WriteGroup( global_def_writer,
                                              0 /* Group id */,
                                              strid /* name */,
                                              OTF2_GROUP_TYPE_COMM_LOCATIONS,
                                              OTF2_PARADIGM_MPI,
                                              OTF2_GROUP_FLAG_NONE,
                                              size,
                                              perlocation );
        if(rc != OTF2_SUCCESS) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        }
        /*  - Second, what is the group that contains parsec_otf2_profiling_comm */
        rc = OTF2_GlobalDefWriter_WriteGroup( global_def_writer,
                                              1 /* Group id */,
                                              emptystrid /* name */,
                                              OTF2_GROUP_TYPE_COMM_GROUP,
                                              OTF2_PARADIGM_MPI,
                                              OTF2_GROUP_FLAG_NONE,
                                              size,
                                              perlocation );
        if(rc != OTF2_SUCCESS) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        }
        /*  - And third, define the communicator above that group */
        strid = next_otf2_global_strid();
        snprintf(string, 64, "MPI_COMM_WORLD");
        rc = OTF2_GlobalDefWriter_WriteString( global_def_writer, strid, string );
        if(rc != OTF2_SUCCESS) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        }
        rc = OTF2_GlobalDefWriter_WriteComm( global_def_writer,
                                             0 /* Communicator id */,
                                             strid /* name */,
                                             1 /* group */,
                                             OTF2_UNDEFINED_COMM /* parent */ );
        if(rc != OTF2_SUCCESS) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        }

        /* Finally done: close the global definition writer */
        rc = OTF2_Archive_CloseGlobalDefWriter( otf2_archive, global_def_writer );
        if(rc != OTF2_SUCCESS) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
        }

        free(perlocation);
    }

    MPI_Barrier(parsec_otf2_profiling_comm); /* All the ranks must wait here that the rank 0 has written everything */
    rc = OTF2_Archive_Close( otf2_archive );
    if(rc != OTF2_SUCCESS) {
            parsec_warning("PaRSEC Profiling System: OTF2 Error -- %s (%s)", OTF2_Error_GetName(rc), OTF2_Error_GetDescription(rc));
    }
    otf2_archive = NULL;

    if( parsec_profiling_raise_error )
        return PARSEC_ERROR;

    return 0;
}

uint64_t parsec_profiling_get_time(void) {
    return diff_time(parsec_start_time, take_time());
}

void parsec_profiling_enable(void)
{
    parsec_profile_enabled = 1;
}

void parsec_profiling_disable(void)
{
    parsec_profile_enabled = 0;
}

void profiling_save_dinfo(const char *key, double value)
{
    char *svalue;
    int rv=asprintf(&svalue, "%g", value);
    (void)rv;
    parsec_profiling_add_information(key, svalue);
    free(svalue);
}

void profiling_save_iinfo(const char *key, int value)
{
    char *svalue;
    int rv=asprintf(&svalue, "%d", value);
    (void)rv;
    parsec_profiling_add_information(key, svalue);
    free(svalue);
}

void profiling_save_uint64info(const char *key, unsigned long long int value)
{
    char *svalue;
    int rv=asprintf(&svalue, "%llu", value);
    (void)rv;
    parsec_profiling_add_information(key, svalue);
    free(svalue);
}

void profiling_save_sinfo(const char *key, char* svalue)
{
    parsec_profiling_add_information(key, svalue);
}

void profiling_stream_save_dinfo(parsec_profiling_stream_t* stream,
                                 const char *key, double value)
{
    char *svalue;
    int rv = asprintf(&svalue, "%g", value);
    (void)rv;
    parsec_profiling_stream_add_information(stream, key, svalue);
    free(svalue);
}

void profiling_stream_save_iinfo(parsec_profiling_stream_t* stream,
                                 const char *key, int value)
{
    char *svalue;
    int rv = asprintf(&svalue, "%d", value);
    (void)rv;
    parsec_profiling_stream_add_information(stream, key, svalue);
    free(svalue);
}

void profiling_stream_save_uint64info(parsec_profiling_stream_t* stream,
                                      const char *key, unsigned long long int value)
{
    char *svalue;
    int rv = asprintf(&svalue, "%llu", value);
    (void)rv;
    parsec_profiling_stream_add_information(stream, key, svalue);
    free(svalue);
}

void profiling_stream_save_sinfo(parsec_profiling_stream_t* stream,
                                 const char *key, char* svalue)
{
    parsec_profiling_stream_add_information(stream, key, svalue);
}

int parsec_profiling_fini( void )
{
    parsec_profiling_stream_t *t;

    if( !__profile_initialized ) return PARSEC_ERR_NOT_SUPPORTED;

    if( 0 != parsec_profiling_dbp_dump() ) {
        return PARSEC_ERROR;
    }

    while( (t = (parsec_profiling_stream_t*)parsec_list_nolock_pop_front(&threads)) ) {
        free(t);
    }
    PARSEC_OBJ_DESTRUCT(&threads);

    parsec_profiling_dictionary_flush();
    start_called = 0;  /* Allow the profiling to be reinitialized */
    parsec_profile_enabled = 0;  /* turn off the profiling */
    __profile_initialized = 0;  /* not initialized */

    MPI_Comm_free(&parsec_otf2_profiling_comm);
    parsec_profiling_mpi_on = 0;

    return 0;
}
