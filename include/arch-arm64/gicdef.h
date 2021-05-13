#ifndef __ARM64_GICDEF_H
#define __ARM64_GICDEF_H




struct icc_sre {
    union {
	uint64_t val; 
	struct {
	    uint64_t sre  : 1;
	    uint64_t dfb  : 1;
	    uint64_t dib  : 1;
	    uint64_t res0 : 61;
	};
    };
} __attribute__((packed));

struct gicd_ctlr {
    union {
	u64 val;
	struct {
	    u64 enable_grp_0   : 1;
	    u64 enable_grp_1ns : 1;
	    u64 enable_grp_1s  : 1;
	    u64 res0_0         : 1;
	    u64 are_s          : 1;
	    u64 are_ns         : 1;
	    u64 ds             : 1;
	    u64 e1_nwf         : 1;
	    u64 res0_1	       : 55;
	    u64 rwp            : 1;
	};
    };
} __attribute__((packed));




#endif
