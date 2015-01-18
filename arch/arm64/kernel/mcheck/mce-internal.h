#include <arch/mce.h>

#define MCE_DBG( fmt, args... )\
   printk("%d:%s():%d: " fmt, this_cpu,__func__,__LINE__, ## args ) 

typedef struct cpumask cpumask_var_t[1];

#define FW_BUG "[Firmware Bug]: " 
#define HW_ERR "[Hardware Error]: " 

#define __this_cpu_ptr( xxx ) xxx[this_cpu]

enum severity_level {
	MCE_NO_SEVERITY,
	MCE_KEEP_SEVERITY,
	MCE_SOME_SEVERITY,
	MCE_AO_SEVERITY,
	MCE_UC_SEVERITY,
	MCE_AR_SEVERITY,
	MCE_PANIC_SEVERITY,
};

#define ATTR_LEN		16

/* One object for each MCE bank, shared by all CPUs */
struct mce_bank {
	u64			ctl;			/* subevents to enable */
	unsigned char init;				/* initialise bank? */
	char			attrname[ATTR_LEN];	/* attribute name */
};

int mce_severity(struct mce *a, int tolerant, char **msg);
struct dentry *mce_get_debugfs_dir(void);

extern int mce_ser;

extern struct mce_bank *mce_banks;

#ifdef CONFIG_ACPI_APEI
int apei_write_mce(struct mce *m);
ssize_t apei_read_mce(struct mce *m, u64 *record_id);
int apei_check_mce(void);
int apei_clear_mce(u64 record_id);
#else
static inline int apei_write_mce(struct mce *m)
{
	return -EINVAL;
}
static inline ssize_t apei_read_mce(struct mce *m, u64 *record_id)
{
	return 0;
}
static inline int apei_check_mce(void)
{
	return 0;
}
static inline int apei_clear_mce(u64 record_id)
{
	return -EINVAL;
}
#endif
