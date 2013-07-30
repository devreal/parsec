#!/usr/bin/env python

# this is PYTHON-ONLY interface to
# pure Python objects generated by py_dbpreader.

# It is especially suitable for use when a separate
# Python program has done the reading of the profile
# and stored the profile in this format in some sort
# of cross-process format, such as a pickle.

# Note that even though all of this is pure Python,
# py_dbpreader should be recompiled any time the
# class attributes it uses from these classes are changed.

# pure Python

import copy

########################################################
############## CUSTOM EVENT INFO SECTION ###############
######### -- add a Python type to this section #########
######### to allow for new 'info' types        #########

class dbp_Exec_EventInfo:
    class_version = 1.0 # added versioning
    __max_length__ = 0
    def __init__(self, kernel_type, kernel_name, vp_id, th_id, values):
        self.__version__ = self.__class__.class_version
        self.kernel_type = kernel_type
        self.kernel_name = kernel_name
        self.vp_id = vp_id
        self.th_id = th_id
        self.values = values

        # set global max length
        for attr, val in vars(self).items():
            if attr[:2] == '__':
                continue # skip special vars
            # values that we don't want printed generically
            elif attr == 'values':
                for value in val:
                    if len(str(value)) > dbp_Exec_EventInfo.__max_length__:
                        dbp_Exec_EventInfo.__max_length__ = len(str(value))
            elif len(str(val)) > dbp_Exec_EventInfo.__max_length__:
                dbp_Exec_EventInfo.__max_length__ = len(str(val))
            elif len(attr) > dbp_Exec_EventInfo.__max_length__:
                dbp_Exec_EventInfo.__max_length__ = len(attr)

    def row_header(self):
        # first, establish max length
        header = ''
        length = str(dbp_Exec_EventInfo.__max_length__)
        header += ('{:>' + length + '}  ').format('kernel_type')
        header += ('{:>' + length + '}  ').format('kernel_name')
        header += ('{:>' + length + '}  ').format('vp_id')
        header += ('{:>' + length + '}  ').format('th_id')
        header += ('{:>' + length + '}  ').format('values')
        return header
    def row(self):
        row = ''
        length = str(dbp_Exec_EventInfo.__max_length__)
        row += ('{:>' + length + '}  ').format(self.kernel_type)
        row += ('{:>' + length + '}  ').format(self.kernel_name)
        row += ('{:>' + length + '}  ').format(self.vp_id)
        row += ('{:>' + length + '}  ').format(self.th_id)
        for value in self.values:
            row += ('{:>' + length + '}  ').format(value)
        return row
    def __repr__(self):
        return self.row()

class dbp_Select_EventInfo:
    class_version = 1.0
    __max_length__ = 0
    def __init__(self, kernel_type, kernel_name, vp_id, th_id,
                 victim_vp_id, victim_th_id, starvation, exec_context, values):
        self.__version__ = self.__class__.class_version
        self.kernel_type = kernel_type
        self.kernel_name = kernel_name
        self.vp_id = vp_id
        self.th_id = th_id
        self.victim_vp_id = victim_vp_id
        self.victim_th_id = victim_th_id
        self.starvation = starvation
        self.exec_context = exec_context
        self.values = values

        # set global max length
        for attr, val in vars(self).items():
            if attr[:2] == '__':
                continue # skip special vars
            # values that we don't want printed generically
            elif attr == 'values':
                for value in val:
                    if len(str(value)) > dbp_Select_EventInfo.__max_length__:
                        dbp_Select_EventInfo.__max_length__ = len(str(value))
            elif len(str(val)) > dbp_Select_EventInfo.__max_length__:
                dbp_Select_EventInfo.__max_length__ = len(str(val))
            # base case
            elif len(attr) > dbp_Select_EventInfo.__max_length__:
                dbp_Select_EventInfo.__max_length__ = len(attr)
    def isStarvation(self):
        return self.exec_context == 0
    def isSystemQueueSteal(self):
        return self.victim_vp_id == SYSTEM_QUEUE_VP
    def row(self):
        row = ''
        length = str(dbp_Select_EventInfo.__max_length__)
        row += ('{:>' + length + '}  ').format(self.kernel_type)
        row += ('{:>' + length + '}  ').format(self.kernel_name)
        row += ('{:>' + length + '}  ').format(self.vp_id)
        row += ('{:>' + length + '}  ').format(self.th_id)
        row += ('{:>' + length + '}  ').format(self.victim_vp_id)
        row += ('{:>' + length + '}  ').format(self.victim_th_id)
        row += ('{:>' + length + '}  ').format(self.starvation)
        row += ('{:>' + length + '}  ').format(self.exec_context)
        for value in self.values:
            row += ('{:>' + length + '}  ').format(value)
        return row
    def row_header(self):
        # first, establish max length
        header = ''
        length = str(dbp_Select_EventInfo.__max_length__)
        header += ('{:>' + length + '}  ').format('kernel_type')
        header += ('{:>' + length + '}  ').format('kernel_name')
        header += ('{:>' + length + '}  ').format('vp_id')
        header += ('{:>' + length + '}  ').format('th_id')
        header += ('{:>' + length + '}  ').format('vict_vp_id')
        header += ('{:>' + length + '}  ').format('vict_th_id')
        header += ('{:>' + length + '}  ').format('exec_context')
        header += ('{:>' + length + '}  ').format('starvation')
        header += ('{:>' + length + '}  ').format('values')
        return header
    def __repr__(self):
        return self.row()

class dbp_Socket_EventInfo:
    class_version = 1.0
    __max_length__ = 0
    def __init__(self, vp_id, th_id, values):
        self.__version__ = self.__class__.class_version
        self.vp_id = vp_id
        self.th_id = th_id
        self.values = values

        # set global max length
        for attr, val in vars(self).items():
            if attr[:2] == '__':
                continue # skip special vars
            # values that we don't want printed generically
            elif attr == 'values':
                for value in val:
                    if len(str(value)) > dbp_Socket_EventInfo.__max_length__:
                        dbp_Socket_EventInfo.__max_length__ = len(str(value))
            elif len(str(val)) > dbp_Socket_EventInfo.__max_length__:
                dbp_Socket_EventInfo.__max_length__ = len(str(val))
            # base case
            elif len(attr) > dbp_Socket_EventInfo.__max_length__:
                dbp_Socket_EventInfo.__max_length__ = len(attr)
    def row(self):
        row = ''
        length = str(dbp_Socket_EventInfo.__max_length__)
        row += ('{:>' + length + '}  ').format(self.vp_id)
        row += ('{:>' + length + '}  ').format(self.th_id)
        for value in self.values:
            row += ('{:>' + length + '}  ').format(value)
        return row
    def row_header(self):
        # first, establish max length
        header = ''
        length = str(dbp_Socket_EventInfo.__max_length__)
        header += ('{:>' + length + '}  ').format('vp_id')
        header += ('{:>' + length + '}  ').format('th_id')
        header += ('{:>' + length + '}  ').format('values')
        return header
    def __repr__(self):
        return self.row()

class dbp_L123_EventInfo:
    class_version = 1.0
    __max_length__ = 0
    def __init__(self, vp_id, th_id, values):
        self.__version__ = self.__class__.class_version
        self.vp_id = vp_id
        self.th_id = th_id
        self.values = values

        # set global max length
        for attr, val in vars(self).items():
            if attr[:2] == '__':
                continue # skip special vars
            # values that we don't want printed generically
            elif attr == 'values':
                for value in val:
                    if len(str(value)) > dbp_Socket_EventInfo.__max_length__:
                        dbp_Socket_EventInfo.__max_length__ = len(str(value))
            elif len(str(val)) > dbp_Socket_EventInfo.__max_length__:
                dbp_Socket_EventInfo.__max_length__ = len(str(val))
            # base case
            elif len(attr) > dbp_Socket_EventInfo.__max_length__:
                dbp_Socket_EventInfo.__max_length__ = len(attr)
    def row(self):
        row = ''
        length = str(dbp_Socket_EventInfo.__max_length__)
        row += ('{:>' + length + '}  ').format(self.vp_id)
        row += ('{:>' + length + '}  ').format(self.th_id)
        for value in self.values:
            row += ('{:>' + length + '}  ').format(value)
        return row
    def row_header(self):
        # first, establish max length
        header = ''
        length = str(dbp_Socket_EventInfo.__max_length__)
        header += ('{:>' + length + '}  ').format('vp_id')
        header += ('{:>' + length + '}  ').format('th_id')
        header += ('{:>' + length + '}  ').format('values')
        return header
    def __repr__(self):
        return self.row()

########################################################
############## CUSTOM INFO STATS SECTION ###############
######### -- add a Python type to this section #########
######### to allow for new 'info stats' types  #########
        
class PapiStats(object):
    class_version = 1.1 # moved name from 'name' of parent
    @classmethod
    def class_row_header(cls):
        return ('{:>15} {:>12}').format('COUNT', 'DURATION/CT')
    ############
    def __init__(self, name):
        self.__version__ = self.__class__.class_version
        self.name = name
        self.total_duration = 0
        self.count = 0
    def row(self):
        return '{:15d} {:12.0f}'.format(self.count, self.total_duration/float(self.count))
    def row_header(self):
        return self.class_row_header()
    def __repr__(self):
        return self.row()
    def __add__(self, x):
        sum_stats = copy.deepcopy(self)
        for attrname in self.__dict__:
            setattr(sum_stats, attrname,
                    getattr(self, attrname) + getattr(x, attrname))
        if self.name.replace('ALL_', '') == x.name.replace('ALL_', ''):
            sum_stats.name = 'ALL_' + self.name.replace('ALL_', '')
        else:
            sum_stats.name = '<<SUM>>'
        return sum_stats

class ExecSelectStats(PapiStats):
    class_version = 1.0 # added versioning
    class_version = 1.1 # moved kernel_name to 'name' of parent
    @classmethod
    def class_row_header(cls):
        hdr = '{:>16} '.format('KERNEL') + str(PapiStats.class_row_header())
        hdr += ' {:>12} {:>12} {:>12} {:>12}'.format(
            'L1M/CT', 'L2H/CT', 'L2M/CT', 'L2A/CT')
        return hdr
    #############
    def __init__(self, kernel_name):
        super(ExecSelectStats, self).__init__(kernel_name)
        self.__version__ = self.__class__.class_version
        self.res_stalls = 0
        self.l2_hits = 0
        self.l2_misses = 0
        self.l2_accesses = 0
        self.l1_misses = 0
    def row(self):
        if self.count == 0:
            self.count = 1
        row = '{:>16} '.format(self.name) + super(ExecSelectStats, self).row()
        row += ' {:12.0f} {:12.0f} {:12.0f} {:12.0f}'.format(
            float(self.l1_misses)/self.count,
            float(self.l2_hits)/self.count,
            float(self.l2_misses)/self.count,
            float(self.l2_accesses)/self.count,
        )
        return row
    def row_header(self):
        return self.class_row_header()
    def __repr__(self):
        return self.row()
    def __setstate__(self, dict):
        self.__dict__.update(dict)
        if not hasattr(self, '__version__'):
            self.__version__ = 1.0
        if self.__version__ < 1.1:
            if hasattr(self, 'kernel_name'):
                self.name = self.kernel_name # update var name change
            self.__version__ = 1.1

class SocketStats(PapiStats):
    class_name = 'L3'
    class_version = 1.0 # added versioning
    class_version = 1.1 # moved name to 'name' of parent
    @classmethod
    def class_row_header(cls):
        hdr = '{:>16} '.format('SOCKET') + str(PapiStats.class_row_header())
        hdr += (' {:>12} {:>12} {:>12} {:>12}').format(
            'L3_SH_M/CT', 'L3_MOD_M/CT', 'L3_EX_M/CT', 'L3_TOT_M/CT')
        return hdr
    ###############
    def __init__(self):
        super(SocketStats, self).__init__(self.__class__.class_name)
        self.__version__ = self.__class__.class_version
        self.l3_exc_misses = 0
        self.l3_shr_misses = 0
        self.l3_mod_misses = 0
        self.l1_misses = 0
        self.l2_misses = 0
    def row(self):
        if self.count == 0:
            self.count = 1
        row = '{:>16} '.format(self.name) + super(SocketStats, self).row()
        row += (' {:12.0f} {:12.0f} {:12.0f} {:12.0f}').format(
            float(self.l3_shr_misses)/self.count,
            float(self.l3_mod_misses)/self.count,
            float(self.l3_exc_misses)/self.count,
            float(self.l3_exc_misses + self.l3_shr_misses
                  + self.l3_mod_misses)/self.count
        )
        return row
    def row_header(self):
        return self.__class__.class_row_header()
    def __repr__(self):
        return self.row()
    def __setstate__(self, dict):
        self.__dict__.update(dict)
        if not hasattr(self, '__version__'):
            self.__version__ = 1.0
        if self.__version__ < 1.1:
            self.name = self.__class__.class_name
            self.__version__ = 1.1
        
        