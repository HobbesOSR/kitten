
#ifndef _LWK_ORTE_RML_MSG_H 
#define _LWK_ORTE_RML_MSG_H 

#include "orte_config.h"
#include "orte/orte_constants.h"
#include "orte/mca/rml/base/base.h"

struct LwkOrteRmlMsg {
    enum { OOB, BARRIER } type;
    orte_process_name_t myname;
    union {
        struct {
            orte_process_name_t peer;
            orte_rml_tag_t      tag;
            orte_std_cntr_t     nBytes;
        } oob;
    } u;
};

#endif
