#ifndef __LINUX_BYTEORDER_H
#define __LINUX_BYTEORDER_H

#include <arch/byteorder.h>

static inline void le16_add_cpu(__le16 *var, u16 val)
{
	*var = cpu_to_le16(le16_to_cpup(var) + val);
}

static inline void le32_add_cpu(__le32 *var, u32 val)
{
	*var = cpu_to_le32(le32_to_cpup(var) + val);
}

static inline void le64_add_cpu(__le64 *var, u64 val)
{
	*var = cpu_to_le64(le64_to_cpup(var) + val);
}

static inline void be16_add_cpu(__be16 *var, u16 val)
{
	*var = cpu_to_be16(be16_to_cpup(var) + val);
}

static inline void be32_add_cpu(__be32 *var, u32 val)
{
	*var = cpu_to_be32(be32_to_cpup(var) + val);
}

static inline void be64_add_cpu(__be64 *var, u64 val)
{
	*var = cpu_to_be64(be64_to_cpup(var) + val);
}

#endif
