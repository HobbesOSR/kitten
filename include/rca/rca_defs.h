/*
 * Copyright (c) 2003 Cray, Inc.
 *
 * The contents of this file are proprietary information of Cray Inc. 
 * and may not be disclosed without prior written consent.
 */
/*
 * This code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */



#ifndef __RCA_DEFS_H__
#define __RCA_DEFS_H__


// Definitions moved from rs_event_name.h
/* Console log */
#define RS_CONSOLE_LOG                          (28)
/* Debug */
#define RS_CONSOLE_INPUT                        (51)
#define RS_KGDB_INPUT                           (52)
#define RS_KGDB_OUTPUT                          (53)

#define RS_DBG_CLASS		0x00010000
#define RS_LOG_CLASS		0x00001000

/* Console log */
#define ec_console_log		(RS_LOG_CLASS | RS_CONSOLE_LOG)

/* Debug */
#define ec_console_input	(RS_DBG_CLASS | RS_CONSOLE_INPUT)
#define ec_kgdb_input		(RS_DBG_CLASS | RS_KGDB_INPUT)
#define ec_kgdb_output		(RS_DBG_CLASS | RS_KGDB_OUTPUT)

#define RS_RCA_SVC_CLASS	7	/* RCA service class */
/* service type class bits */
#define RS_CLASS_BITS		8
#define RS_CLASS_MASK		((1 << RS_CLASS_BITS) - 1)

#define RS_SUBCLASS_BITS	24
#define RS_SUBCLASS_MASK	((1 << RS_SUBCLASS_BITS) -1)

/* generate service type */
#define RCA_MAKE_SERVICE_INDEX(class, subclass)		\
     ( (((class)&RS_CLASS_MASK) << RS_SUBCLASS_BITS) |	\
              ((subclass) & RS_SUBCLASS_MASK) )


/* macro for setting up service id */
#define RS_MKSVC(i, t, n)	(rs_service_id_t){(i), (t), (n)}


/* need to set RS_MSG_LEN before including rs_event.h */
#define RS_MSG_LEN 256	

#define RCA_SVC_CLASS     RS_RCA_SVC_CLASS	/* 7 */

#define RCA_CLASS_BITS		RS_CLASS_BITS
#define RCA_CLASS_MASK		RS_CLASS_MASK

/* number of bits client may use in subclass */
#define RCA_SUBCLASS_BITS	RS_SUBCLASS_BITS
#define RCA_SUBCLASS_MASK	RS_SUBCLASS_MASK

#define RCA_INST_ANY   0xffffffffUL

/* system console log */
#define RCA_SVCTYPE_CONS	RCA_MAKE_SERVICE_INDEX(RCA_SVC_CLASS, 6)
#define RCA_SVCTYPE_TEST0	RCA_MAKE_SERVICE_INDEX(RCA_SVC_CLASS, 10)

/* rs_service_id_t constants and helpers */
#define RCA_MKSVC(i, t, n)	RS_MKSVC((i), (t), (n))

#define RCA_LOG_DEBUG	7

#endif /* !_RCA_TYPES_H_ */
