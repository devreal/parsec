####################################
# DBPreader Python interface
# run 'python setup.py build_ext --inplace' to compile
# import py_dbpreader to use
#
# This is verified to work with Python 2.4.3 and above, compiled with Cython 0.16rc0
# However, it is recommended that Python 2.7 or greater is used to build and run.
#
# Be SURE to build this against the same version of Python as you have built Cython itself.
# Contrasting versions will likely lead to odd errors about Unicode functions.
import sys
from operator import attrgetter
from libc.stdlib cimport malloc, free
from profiling      import * # the pure Python classes
from profiling_info import * # the pure Python classes representing custom INFO structs

MAX_BELIEVABLE_HANDLE_COUNT = 100

# this is the public Python interface function. call it.
cpdef readProfile(filenames):
    cdef char ** c_filenames = stringListToCStrings(filenames)
    cdef dbp_multifile_reader_t * dbp = dbp_reader_open_files(len(filenames), c_filenames)
    
    nb_files = dbp_reader_nb_files(dbp)
    nb_dict_entries = dbp_reader_nb_dictionary_entries(dbp)
    cdef dbp_file_t * cfile
    cdef dbp_dictionary_t * cdict

    profile = Profile()
    profile.worldsize = dbp_reader_worldsize(dbp) # what does this even do?

    # create dictionary first, for later use while making Events
    for key in range(nb_dict_entries):
        cdict = dbp_reader_get_dictionary(dbp, key)
        event_name = dbp_dictionary_name(cdict)
        profile.dict_key_to_name[key] = event_name
        profile.dictionary[event_name] = dbpDictEntry(profile, key,
                                                      dbp_dictionary_attributes(cdict))
    # convert c to py
    for ifd in range(nb_files):
        cfile = dbp_reader_get_file(dbp, ifd)
        rank = dbp_file_get_rank(cfile)
        pfile = dbpFile(profile, dbp_file_hr_id(cfile),
                        dbp_file_get_name(cfile), rank)
        for index in range(dbp_file_nb_infos(cfile)):
            cinfo = dbp_file_get_info(cfile, index)
            key = dbp_info_get_key(cinfo)
            value = dbp_info_get_value(cinfo)
            pfile.infos.append(dbpInfo(key, value))
        for thread_id in range(dbp_file_nb_threads(cfile)):
            new_thr = makeDbpThread(profile, dbp, cfile, thread_id, pfile)
            pfile.threads.append(new_thr)
        profile.files[rank] = pfile

    dbp_reader_close_files(dbp) # does nothing as of 2013-04-21
#   dbp_reader_dispose_reader(dbp)
    free(c_filenames)

    return profile

# helper function for readProfile
cdef char** stringListToCStrings(strings):
    cdef char ** c_argv
    bytes_strings = [bytes(x) for x in strings]
    c_argv = <char**>malloc(sizeof(char*) * len(bytes_strings)) 
    if c_argv is NULL:
        raise MemoryError()
    try:
        for idx, s in enumerate(bytes_strings):
            c_argv[idx] = s
    except:
        print("exception caught while converting to c strings")
        free(c_argv)
    return c_argv

# you can't call this. it will be called for you. call readProfile()
cdef makeDbpThread(profile, dbp_multifile_reader_t * dbp, dbp_file_t * cfile, int index, pfile):
    cdef dbp_thread_t * cthread = dbp_file_get_thread(cfile, index)
    cdef dbp_event_iterator_t * it_s = dbp_iterator_new_from_thread(cthread)
    cdef dbp_event_iterator_t * it_e = NULL
    cdef dbp_event_t * event_s = dbp_iterator_current(it_s)
    cdef dbp_event_t * event_e = NULL
    cdef dague_time_t reader_begin = dbp_reader_min_date(dbp)
    cdef unsigned long long begin = 0
    cdef unsigned long long end = 0
    cdef void * cinfo = NULL
    cdef papi_exec_info_t * cast_exec_info = NULL
    cdef select_info_t * cast_select_info = NULL

    thread = dbpThread(pfile, index)

    while event_s != NULL:
        if KEY_IS_START( dbp_event_get_key(event_s) ):
            it_e = dbp_iterator_find_matching_event_all_threads(it_s)
            if it_e != NULL:
                event_e = dbp_iterator_current(it_e)
                if event_e != NULL:
                    begin = diff_time(reader_begin, dbp_event_get_timestamp(event_s))
                    end = diff_time(reader_begin, dbp_event_get_timestamp(event_e))
                    event_key = int(dbp_event_get_key(event_s)) / 2 # to match dictionary
                    event_flags = dbp_event_get_flags(event_s)
                    event_handle_id = int(dbp_event_get_handle_id(event_s))
                    event_id = int(dbp_event_get_event_id(event_s))
                    event = dbpEvent(thread,
                                     event_key,
                                     event_flags,
                                     event_handle_id,
                                     event_id,
                                     begin, end)
                    event_name = profile.dict_key_to_name[event_key]
                    
                    if event_handle_id not in profile.handles:
                        profile.handles[event_handle_id] = dbpHandle(profile, event_handle_id)
                    
                    stats = profile.dictionary[event_name].stats
                    stats.count += 1
                    stats.total_duration += event.duration
                    
                    #####################################
                    # not all events have info
                    # also, not all events have the same info.
                    # so this is where users must add code to translate
                    # their own info objects
                    cinfo = dbp_event_get_info(event_e)
                    if cinfo != NULL:
                        if ('PINS_EXEC' in profile.dictionary and
                            event_key == profile.dictionary['PINS_EXEC'].key):
                            cast_exec_info = <papi_exec_info_t *>cinfo
                            kernel_name = str(cast_exec_info.kernel_name)
                            event.info = dbp_Exec_EventInfo(
                                cast_exec_info.kernel_type,
                                kernel_name,
                                cast_exec_info.vp_id,
                                cast_exec_info.th_id,
                                [cast_exec_info.values[x] for x
                                 in range(cast_exec_info.values_len)])
                            if kernel_name not in stats.exec_stats:
                                stats.exec_stats[kernel_name] = ExecSelectStats(kernel_name)
                            pstats = stats.exec_stats[kernel_name]
                            pstats.count += 1
                            pstats.total_duration += event.duration
                            pstats.l1_misses += event.info.values[0]
                            pstats.l2_hits += event.info.values[1]
                            pstats.l2_misses += event.info.values[2]
                            pstats.l2_accesses += event.info.values[3]
                            # if kernel_name == 'POTRF':
                            #     print('PYDBPR found POTRF ' + str(pstats.count) + ' ' + str(event_key))
                        elif ('PINS_SELECT' in profile.dictionary and
                              event_key == profile.dictionary['PINS_SELECT'].key):
                            cast_select_info = <select_info_t *>cinfo
                            kernel_name = str(cast_exec_info.kernel_name)
                            event.info = dbp_Select_EventInfo(
                                cast_select_info.kernel_type,
                                kernel_name,
                                cast_select_info.vp_id,
                                cast_select_info.th_id,
                                cast_select_info.victim_vp_id,
                                cast_select_info.victim_th_id,
                                cast_select_info.exec_context,
                                [cast_select_info.values[x] for x
                                 in range(cast_select_info.values_len)])
                            if kernel_name not in stats.select_stats:
                                stats.select_stats[kernel_name] = ExecSelectStats(kernel_name)
                            pstats = stats.select_stats[kernel_name]
                            pstats.count += 1
                            pstats.total_duration += event.duration
                            pstats.l1_misses += event.info.values[0]
                            pstats.l2_hits += event.info.values[1]
                            pstats.l2_misses += event.info.values[2]
                            pstats.l2_accesses += event.info.values[3]
                        elif ('PINS_SOCKET' in profile.dictionary and
                              event_key == profile.dictionary['PINS_SOCKET'].key):
                            cast_socket_info = <papi_socket_info_t *>cinfo
                            event.info = dbp_Socket_EventInfo(
                                cast_socket_info.vp_id,
                                cast_socket_info.th_id,
                                [cast_socket_info.values[x] for x
                                 in range(cast_socket_info.values_len)])
                            if SocketStats.class_name not in stats.socket_stats:
                                stats.socket_stats[SocketStats.class_name] = SocketStats()
                            pstats = stats.socket_stats[SocketStats.class_name]
                            pstats.count += 1
                            pstats.total_duration += event.duration
                            pstats.l3_exc_misses  += event.info.values[0]
                            pstats.l3_shr_misses += event.info.values[1]
                            pstats.l3_mod_misses += event.info.values[2]
                        elif ('PINS_L12_EXEC' in profile.dictionary and
                              event_key == profile.dictionary['PINS_L12_EXEC'].key):
                            cast_L12_info = <papi_L12_exec_info_t *>cinfo
                            
                            something = None
                        elif ('PINS_L12_SELECT' in profile.dictionary and
                              event_key == profile.dictionary['PINS_L12_SELECT'].key):
                            something_else = None
                        # elif event.key == profile.dictionary['<SOME OTHER TYPE WITH INFO>'].key:
                            # event.info = <write a function and a Python type to translate>
                        else:
                            dont_print = True
                            # print('missed an info for event key ' + event_name )
                    # event constructed. add it everywhere it belongs.
                    thread.append(event)
                    profile.handles[event_handle_id].append(event)
                    profile.dictionary[event_name].append(event)
                    profile.append(event)
                    
                dbp_iterator_delete(it_e)
                it_e = NULL
        dbp_iterator_next(it_s)
        event_s = dbp_iterator_current(it_s)
        
    dbp_iterator_delete(it_s)
    it_s = NULL
    thread.sort(key = attrgetter('begin'))
    # now that they're sorted, compute the wasted time
    thread.begin = thread[0].begin
    thread.end = thread[-1].end
    thread.duration = thread.end - thread.begin
    return thread