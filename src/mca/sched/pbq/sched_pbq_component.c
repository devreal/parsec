/*
 * Copyright (c) 2013      The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 * These symbols are in a file by themselves to provide nice linker
 * semantics.  Since linkers generally pull in symbols by object
 * files, keeping these symbols as the only symbols in this file
 * prevents utility programs such as "ompi_info" from having to import
 * entire components just to query their version and parameters.
 */

#include "dague_config.h"
#include "dague.h"

#include "dague/mca/sched/sched.h"
#include "dague/mca/sched/pbq/sched_pbq.h"

/*
 * Local function
 */
static int sched_pbq_component_query(mca_base_module_t **module, int *priority);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */
const dague_sched_base_component_t dague_sched_pbq_component = {

    /* First, the mca_component_t struct containing meta information
       about the component itself */

    {
        DAGUE_SCHED_BASE_VERSION_2_0_0,

        /* Component name and version */
        "pbq",
        DAGUE_VERSION_MAJOR,
        DAGUE_VERSION_MINOR,

        /* Component open and close functions */
        NULL, /*< No open: sched_pbq is always available, no need to check at runtime */
        NULL, /*< No close: open did not allocate any resource, no need to release them */
        sched_pbq_component_query, 
        /*< specific query to return the module and add it to the list of available modules */
        NULL, /*< No register: no parameters to the priority based queue component */
    },
    {
        /* The component has no metada */
        MCA_BASE_METADATA_PARAM_NONE
    }
};
mca_base_component_t *sched_pbq_static_component(void)
{
    return (mca_base_component_t *)&dague_sched_pbq_component;
}

static int sched_pbq_component_query(mca_base_module_t **module, int *priority)
{
    *priority = 18;
    *module = (mca_base_module_t *)&dague_sched_pbq_module;
    return MCA_SUCCESS;
}

