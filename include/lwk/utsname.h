#ifndef _LWK_UTSNAME_H
#define _LWK_UTSNAME_H

#define __UTS_LEN 64

struct utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};

extern struct utsname linux_utsname;

static inline struct utsname *
utsname(void)
{
	return &linux_utsname;
}

#endif /* _LWK_UTSNAME_H */
