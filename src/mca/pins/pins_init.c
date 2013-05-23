#include "dague_config.h"
#include "pins.h"
#include "dague/mca/mca_repository.h"
#include "dague/mca/pins/papi_exec/pins_papi_exec.h"
#include "dague/mca/pins/papi_socket/pins_papi_socket.h"
#include "dague/mca/pins/papi_select/pins_papi_select.h"
#include "execution_unit.h"
#include "profiling.h"

static int allowable_modules_in_use;  // allows all modules if not set
static int allowable_modules_defined; // keeps them from being defined more than once
static const char * const default_modules_array[] = {NULL};
char ** allowable_modules;
#define MAX_NAME_SIZE 40 // arbitrary module name limit for 'safety'

extern parsec_pins_callback * pins_array[];

/**
 * pins_init() should be called once and only once per runtime of a PaRSEC execution.
 * It should be called near the beginning of execution, preferably when most
 * other components have been initialized, so as to allow the interfacing of 
 * PINS measurements with working PaRSEC subsystems.
 */
void pins_init(dague_context_t * master_context) {
	int i = 0;
	for (; i < PINS_FLAG_COUNT; i++) {
		if (pins_array[i] == NULL)
			pins_array[i] = &pins_empty_callback;
	}
	DEBUG(("Initialized PaRSEC PINS callbacks to pins_empty_callback()"));

// 	set_allowable_pins_modules(default_modules_array);
	allowable_modules_defined = 1; // block post-init definitions

	mca_base_component_t ** components = NULL;
	dague_pins_module_t * module = NULL;
	int priority = -1;
	i = 0;

	components = mca_components_open_bytype("pins");
	while (components[i] != NULL) {
		if (components[i]->mca_query_component != NULL) {
			components[i]->mca_query_component((mca_base_module_t**)&module, &priority);
			int j = 0;
			while (allowable_modules_in_use && allowable_modules[j] != NULL) {
				if (NULL != module->module.init &&
					0 == strncmp(module->component->base_version.mca_component_name, 
				                 allowable_modules[j], MAX_NAME_SIZE)) {
					module->module.init(master_context);
					DEBUG(("Activated allowable PINS module %s.\n", 
					       module->component->base_version.mca_component_name));
				}
				j++;
			}
			if (!allowable_modules_in_use && NULL != module->module.init) {
				module->module.init(master_context);
				DEBUG(("Activated PINS module %s.\n", 
					   module->component->base_version.mca_component_name));
			}
		}
		i++;
	}
}

/**
 * pins_fini must call fini methods of all modules
 */
void pins_fini(dague_context_t * master_context) {
	mca_base_component_t ** components = NULL;
	dague_pins_module_t * module = NULL;
	int priority = -1;
	int i = 0;

        /*
         * Call all fini methods in reverse order in order to preserve 
         * cleanup semantics.
         */
	components = mca_components_open_bytype("pins");
	while (components[i] != NULL)
            i++; // count
        i--;

        while (i >= 0) {
		if (components[i]->mca_query_component != NULL) {
			components[i]->mca_query_component((mca_base_module_t**)&module, &priority);
			int j = 0;
			while (allowable_modules_in_use && allowable_modules[j] != NULL) {
				if (NULL != module->module.init &&
					0 == strncmp(module->component->base_version.mca_component_name, 
				                 allowable_modules[j], MAX_NAME_SIZE)) {
					module->module.fini(master_context);
					DEBUG(("Finalized allowable PINS module %s.\n", 
					       module->component->base_version.mca_component_name));
				}
				j++;
			}
			if (!allowable_modules_in_use && NULL != module->module.fini) {
				module->module.fini(master_context);
				DEBUG(("Finalized PINS module %s.\n", 
					   module->component->base_version.mca_component_name));
			}
		}
		i--;
	}
}


/**
 * pins_thread_init() should be called once per thread runtime of a PaRSEC execution.
 * It should be called near the beginning of the thread's lifetime, preferably 
 * once most other thread components have been initialized, so as to allow the 
 * interfacing of PINS measurements with working PaRSEC subsystems.
 * It MUST NOT be called BEFORE pins_init().
 */
void pins_thread_init(dague_execution_unit_t * exec_unit) {
	mca_base_component_t ** components = NULL;
	dague_pins_module_t * module = NULL;
	int priority = -1;
	int i = 0;

	components = mca_components_open_bytype("pins");
	while (components[i] != NULL) {
		if (components[i]->mca_query_component != NULL) {
			components[i]->mca_query_component((mca_base_module_t**)&module, &priority);
			int j = 0;
			while (allowable_modules_in_use && allowable_modules[j] != NULL) {
				if (NULL != module->module.thread_init &&
				    0 == strncmp(module->component->base_version.mca_component_name, 
				                 allowable_modules[j], MAX_NAME_SIZE)) {
					module->module.thread_init(exec_unit);
				}
				j++;
			}
			if (!allowable_modules_in_use && NULL != module->module.thread_init) {
				module->module.thread_init(exec_unit);
			}
		}
		i++;
	}

	parsec_instrument(THREAD_INIT, exec_unit, NULL, NULL);
}

/**
 * called in scheduling.c, which is not ideal
 */
void pins_thread_fini(dague_execution_unit_t * exec_unit) {
	mca_base_component_t ** components = NULL;
	dague_pins_module_t * module = NULL;
	int priority = -1;
	int i = 0;

        /*
         * Call all fini methods in reverse order in order to preserve 
         * cleanup semantics.
         */
	components = mca_components_open_bytype("pins");
	while (components[i] != NULL)
            i++; // count
        i--;

        while (i >= 0) {
		if (components[i]->mca_query_component != NULL) {
			components[i]->mca_query_component((mca_base_module_t**)&module, &priority);
			int j = 0;
			while (allowable_modules_in_use && allowable_modules[j] != NULL) {
				if (NULL != module->module.thread_fini &&
				    0 == strncmp(module->component->base_version.mca_component_name, 
				                 allowable_modules[j], MAX_NAME_SIZE)) {
					module->module.thread_fini(exec_unit);
				}
				j++;
			}
			if (!allowable_modules_in_use && NULL != module->module.thread_fini) {
				module->module.thread_fini(exec_unit);
			}
		}
		i--;
	}

	parsec_instrument(THREAD_FINI, exec_unit, NULL, NULL);
}


/**
 * pins_handle_init() should be called once per PaRSEC handle instantiation.
 * It should be called near the beginning of the handle's lifetime, preferably 
 * once most other handle components have been initialized, so as to allow the 
 * interfacing of PINS measurements with working PaRSEC subsystems.
 *
 * It MUST NOT be called BEFORE pins_init().
 */
void pins_handle_init(dague_handle_t * handle) {
	mca_base_component_t ** components = NULL;
	dague_pins_module_t * module = NULL;
	int priority = -1;
	int i = 0;

	components = mca_components_open_bytype("pins");
	while (components[i] != NULL) {
		if (components[i]->mca_query_component != NULL) {
			components[i]->mca_query_component((mca_base_module_t**)&module, &priority);
			int j = 0;
			while (allowable_modules_in_use && allowable_modules[j] != NULL) {
				if (NULL != module->module.handle_init &&
				    0 == strncmp(module->component->base_version.mca_component_name, 
				                 allowable_modules[j], MAX_NAME_SIZE)) {
					module->module.handle_init(handle);
				}
				j++;
			}
			if (!allowable_modules_in_use && NULL != module->module.handle_init) {
				module->module.handle_init(handle);
			}
		}
		i++;
	}

	parsec_instrument(HANDLE_INIT, NULL, NULL, (void *)handle);
}

/**
 * Currently uncalled in the PaRSEC DPLAMSA testing executables
 */
void pins_handle_fini(dague_handle_t * handle) {
	mca_base_component_t ** components = NULL;
	dague_pins_module_t * module = NULL;
	int priority = -1;
	int i = 0;

        /*
         * Call all fini methods in reverse order in order to preserve 
         * cleanup semantics.
         */
	components = mca_components_open_bytype("pins");
	while (components[i] != NULL)
            i++; // count
        i--;

        while (i >= 0) {
		if (components[i]->mca_query_component != NULL) {
			components[i]->mca_query_component((mca_base_module_t**)&module, &priority);
			int j = 0;
			while (allowable_modules_in_use && allowable_modules[j] != NULL) {
				if (NULL != module->module.handle_fini &&
				    0 == strncmp(module->component->base_version.mca_component_name, 
				                 allowable_modules[j], MAX_NAME_SIZE)) {
					module->module.handle_fini(handle);
				}
				j++;
			}
			if (!allowable_modules_in_use && NULL != module->module.handle_fini) {
				module->module.handle_fini(handle);
			}
		}
		i--;
	}

	parsec_instrument(HANDLE_FINI, NULL, NULL, (void *)handle);
}

/** 
 * Addon method to allow for limiting the 'allowable modules.'
 * It is safest to call this method only before pins_init, so as not to introduce
 * new modules without proper initialization, so as not to overwrite 
 * currently-enabled modules, and so as not to cause threading complications 
 * (this method is certainly NOT THREAD-SAFE).
 *
 * The method will only allow itself to be called a single time.
 *
 * The array of modules should be terminated by a NULL pointer.
 */
void set_allowable_pins_modules (const char * const modules[]) {
	if (dague_atomic_cas(&allowable_modules_defined, 0, 1)) {
		int count = 0;
		allowable_modules_in_use = 1; // if this function hasn't been called, 
		while (modules[count] != NULL) 
			count++;
		allowable_modules = calloc(sizeof(char *), count);
		if (allowable_modules != NULL) {
			for (count--; count >= 0; count--) {
				allowable_modules[count] = calloc(sizeof(char), MAX_NAME_SIZE + 1);
				if (NULL != allowable_modules[count]) {
					strncpy(allowable_modules[count], modules[count], MAX_NAME_SIZE);
					DEBUG(("Allowing PINS module %s\n", allowable_modules[count]));
				}
				else {
					DEBUG(("Memory allocation failed in "
					       "'set_allowable_pins_modules.' "
					       "Module %s not enabled\n", modules[count]));
				}
			}
		}
		else {
			DEBUG(("Memory allocation failed in 'set_allowable_pins_modules.'"
			       " All modules disabled\n"));
		}
	}
	else {
		DEBUG3(("PINS modules have already been set and cannot be set again.\n"));
	}
}

int is_pins_module_enabled(char * name) {
	mca_base_component_t ** components = NULL;
	dague_pins_module_t * module = NULL;
	int priority = -1;
	int i = 0;

	components = mca_components_open_bytype("pins");
	while (components[i] != NULL) {
		if (components[i]->mca_query_component != NULL) {
			components[i]->mca_query_component((mca_base_module_t**)&module, &priority);
			int j = 0;
			while (allowable_modules_in_use && allowable_modules[j] != NULL) {
				if ((!allowable_modules_in_use || 
					0 == strncmp(module->component->base_version.mca_component_name, 
				                 allowable_modules[j], MAX_NAME_SIZE)) && 
					0 == strncmp(module->component->base_version.mca_component_name,
								 name, MAX_NAME_SIZE)) {
					return 1; // yes, enabled
				}
				j++;
			}
		}
		i++;
	}
	return 0; // no, not enabled
}