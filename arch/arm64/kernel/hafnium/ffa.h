/*
 * Copyright 2019 The Hafnium Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "types.h"

#define FFA_VERSION_MAJOR 0x1
#define FFA_VERSION_MINOR 0x0

#define FFA_VERSION_MAJOR_OFFSET 16

/* clang-format off */

#define FFA_LOW_32_ID  0x84000060
#define FFA_HIGH_32_ID 0x8400007F
#define FFA_LOW_64_ID  0xC4000060
#define FFA_HIGH_32_ID 0x8400007F

/* FF-A function identifiers. */
#define FFA_ERROR_32                 0x84000060
#define FFA_SUCCESS_32               0x84000061
#define FFA_INTERRUPT_32             0x84000062
#define FFA_VERSION_32               0x84000063
#define FFA_FEATURES_32              0x84000064
#define FFA_RX_RELEASE_32            0x84000065
#define FFA_RXTX_MAP_32              0x84000066
#define FFA_RXTX_MAP_64              0xC4000066
#define FFA_RXTX_UNMAP_32            0x84000067
#define FFA_PARTITION_INFO_GET_32    0x84000068
#define FFA_ID_GET_32                0x84000069
#define FFA_MSG_POLL_32              0x8400006A
#define FFA_MSG_WAIT_32              0x8400006B
#define FFA_YIELD_32                 0x8400006C
#define FFA_RUN_32                   0x8400006D
#define FFA_MSG_SEND_32              0x8400006E
#define FFA_MSG_SEND_DIRECT_REQ_32   0x8400006F
#define FFA_MSG_SEND_DIRECT_RESP_32  0x84000070
#define FFA_MEM_DONATE_32            0x84000071
#define FFA_MEM_LEND_32              0x84000072
#define FFA_MEM_SHARE_32             0x84000073
#define FFA_MEM_RETRIEVE_REQ_32      0x84000074
#define FFA_MEM_RETRIEVE_RESP_32     0x84000075
#define FFA_MEM_RELINQUISH_32        0x84000076
#define FFA_MEM_RECLAIM_32           0x84000077
#define FFA_MEM_FRAG_RX_32           0x8400007A
#define FFA_MEM_FRAG_TX_32           0x8400007B

/* FF-A error codes. */
#define FFA_NOT_SUPPORTED      INT32_C(-1)
#define FFA_INVALID_PARAMETERS INT32_C(-2)
#define FFA_NO_MEMORY          INT32_C(-3)
#define FFA_BUSY               INT32_C(-4)
#define FFA_INTERRUPTED        INT32_C(-5)
#define FFA_DENIED             INT32_C(-6)
#define FFA_RETRY              INT32_C(-7)
#define FFA_ABORTED            INT32_C(-8)

/* clang-format on */

/* FF-A function specific constants. */
#define FFA_MSG_RECV_BLOCK 0x1
#define FFA_MSG_RECV_BLOCK_MASK 0x1

#define FFA_MSG_SEND_NOTIFY 0x1
#define FFA_MSG_SEND_NOTIFY_MASK 0x1

#define FFA_MEM_RECLAIM_CLEAR 0x1

#define FFA_SLEEP_INDEFINITE 0

/**
 * For use where the FF-A specification refers explicitly to '4K pages'. Not to
 * be confused with PAGE_SIZE, which is the translation granule Hafnium is
 * configured to use.
 */
#define FFA_PAGE_SIZE 4096

/* The maximum length possible for a single message. */
#define FFA_MSG_PAYLOAD_MAX HF_MAILBOX_SIZE

enum ffa_data_access {
	FFA_DATA_ACCESS_NOT_SPECIFIED,
	FFA_DATA_ACCESS_RO,
	FFA_DATA_ACCESS_RW,
	FFA_DATA_ACCESS_RESERVED,
};

enum ffa_instruction_access {
	FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED,
	FFA_INSTRUCTION_ACCESS_NX,
	FFA_INSTRUCTION_ACCESS_X,
	FFA_INSTRUCTION_ACCESS_RESERVED,
};

enum ffa_memory_type {
	FFA_MEMORY_NOT_SPECIFIED_MEM,
	FFA_MEMORY_DEVICE_MEM,
	FFA_MEMORY_NORMAL_MEM,
};

enum ffa_memory_cacheability {
	FFA_MEMORY_CACHE_RESERVED = 0x0,
	FFA_MEMORY_CACHE_NON_CACHEABLE = 0x1,
	FFA_MEMORY_CACHE_RESERVED_1 = 0x2,
	FFA_MEMORY_CACHE_WRITE_BACK = 0x3,
	FFA_MEMORY_DEV_NGNRNE = 0x0,
	FFA_MEMORY_DEV_NGNRE = 0x1,
	FFA_MEMORY_DEV_NGRE = 0x2,
	FFA_MEMORY_DEV_GRE = 0x3,
};

enum ffa_memory_shareability {
	FFA_MEMORY_SHARE_NON_SHAREABLE,
	FFA_MEMORY_SHARE_RESERVED,
	FFA_MEMORY_OUTER_SHAREABLE,
	FFA_MEMORY_INNER_SHAREABLE,
};

typedef uint8_t ffa_memory_access_permissions_t;

/**
 * This corresponds to table 44 of the FF-A 1.0 EAC specification, "Memory
 * region attributes descriptor".
 */
typedef uint8_t ffa_memory_attributes_t;

#define FFA_DATA_ACCESS_OFFSET          (0x0U)
#define FFA_DATA_ACCESS_MASK            ((0x3U) << FFA_DATA_ACCESS_OFFSET)

#define FFA_INSTRUCTION_ACCESS_OFFSET   (0x2U)
#define FFA_INSTRUCTION_ACCESS_MASK     ((0x3U) << FFA_INSTRUCTION_ACCESS_OFFSET)

#define FFA_MEMORY_TYPE_OFFSET          (0x4U)
#define FFA_MEMORY_TYPE_MASK            ((0x3U) << FFA_MEMORY_TYPE_OFFSET)

#define FFA_MEMORY_CACHEABILITY_OFFSET  (0x2U)
#define FFA_MEMORY_CACHEABILITY_MASK    ((0x3U) << FFA_MEMORY_CACHEABILITY_OFFSET)

#define FFA_MEMORY_SHAREABILITY_OFFSET  (0x0U)
#define FFA_MEMORY_SHAREABILITY_MASK    ((0x3U) << FFA_MEMORY_SHAREABILITY_OFFSET)

#define ATTR_FUNCTION_SET(name, container_type, offset, mask)                \
	static inline void ffa_set_##name##_attr(container_type *attr,       \
						 const enum ffa_##name perm) \
	{                                                                    \
		*attr = (*attr & ~(mask)) | ((perm << offset) & mask);       \
	}

#define ATTR_FUNCTION_GET(name, container_type, offset, mask)      \
	static inline enum ffa_##name ffa_get_##name##_attr(       \
		container_type attr)                               \
	{                                                          \
		return (enum ffa_##name)((attr & mask) >> offset); \
	}

ATTR_FUNCTION_SET(data_access, ffa_memory_access_permissions_t,
		  FFA_DATA_ACCESS_OFFSET, FFA_DATA_ACCESS_MASK)
ATTR_FUNCTION_GET(data_access, ffa_memory_access_permissions_t,
		  FFA_DATA_ACCESS_OFFSET, FFA_DATA_ACCESS_MASK)

ATTR_FUNCTION_SET(instruction_access, ffa_memory_access_permissions_t,
		  FFA_INSTRUCTION_ACCESS_OFFSET, FFA_INSTRUCTION_ACCESS_MASK)
ATTR_FUNCTION_GET(instruction_access, ffa_memory_access_permissions_t,
		  FFA_INSTRUCTION_ACCESS_OFFSET, FFA_INSTRUCTION_ACCESS_MASK)

ATTR_FUNCTION_SET(memory_type, ffa_memory_attributes_t, FFA_MEMORY_TYPE_OFFSET,
		  FFA_MEMORY_TYPE_MASK)
ATTR_FUNCTION_GET(memory_type, ffa_memory_attributes_t, FFA_MEMORY_TYPE_OFFSET,
		  FFA_MEMORY_TYPE_MASK)

ATTR_FUNCTION_SET(memory_cacheability, ffa_memory_attributes_t,
		  FFA_MEMORY_CACHEABILITY_OFFSET, FFA_MEMORY_CACHEABILITY_MASK)
ATTR_FUNCTION_GET(memory_cacheability, ffa_memory_attributes_t,
		  FFA_MEMORY_CACHEABILITY_OFFSET, FFA_MEMORY_CACHEABILITY_MASK)

ATTR_FUNCTION_SET(memory_shareability, ffa_memory_attributes_t,
		  FFA_MEMORY_SHAREABILITY_OFFSET, FFA_MEMORY_SHAREABILITY_MASK)
ATTR_FUNCTION_GET(memory_shareability, ffa_memory_attributes_t,
		  FFA_MEMORY_SHAREABILITY_OFFSET, FFA_MEMORY_SHAREABILITY_MASK)

#define FFA_MEMORY_HANDLE_ALLOCATOR_MASK \
	((ffa_memory_handle_t)(UINT64_C(1) << 63))
#define FFA_MEMORY_HANDLE_ALLOCATOR_HYPERVISOR \
	((ffa_memory_handle_t)(UINT64_C(1) << 63))
#define FFA_MEMORY_HANDLE_INVALID (~UINT64_C(0))

/** The ID of a VM. These are assigned sequentially starting with an offset. */
typedef uint16_t ffa_vm_id_t;

/**
 * A globally-unique ID assigned by the hypervisor for a region of memory being
 * sent between VMs.
 */
typedef uint64_t ffa_memory_handle_t;

/**
 * A count of VMs. This has the same range as the VM IDs but we give it a
 * different name to make the different semantics clear.
 */
typedef ffa_vm_id_t ffa_vm_count_t;

/** The index of a vCPU within a particular VM. */
typedef uint16_t ffa_vcpu_index_t;

/**
 * A count of vCPUs. This has the same range as the vCPU indices but we give it
 * a different name to make the different semantics clear.
 */
typedef ffa_vcpu_index_t ffa_vcpu_count_t;

/** Parameter and return type of FF-A functions. */
struct ffa_value {
	uint64_t func;
	uint64_t arg1;
	uint64_t arg2;
	uint64_t arg3;
	uint64_t arg4;
	uint64_t arg5;
	uint64_t arg6;
	uint64_t arg7;
};

static inline ffa_vm_id_t 
ffa_msg_send_sender(struct ffa_value args)
{
	return (args.arg1 >> 16) & 0xffff;
}

static inline ffa_vm_id_t 
ffa_msg_send_receiver(struct ffa_value args)
{
	return args.arg1 & 0xffff;
}

static inline uint32_t 
ffa_msg_send_size(struct ffa_value args)
{
	return args.arg3;
}

static inline uint32_t 
ffa_msg_send_attributes(struct ffa_value args)
{
	return args.arg4;
}

static inline ffa_memory_handle_t 
ffa_assemble_handle(uint32_t a1, 
                    uint32_t a2)
{
	return (uint64_t)a1 | (uint64_t)a2 << 32;
}

static inline ffa_memory_handle_t 
ffa_mem_success_handle(struct ffa_value args)
{
	return ffa_assemble_handle(args.arg2, args.arg3);
}

static inline ffa_memory_handle_t 
ffa_frag_handle(struct ffa_value args)
{
	return ffa_assemble_handle(args.arg1, args.arg2);
}

static inline struct ffa_value 
ffa_mem_success(ffa_memory_handle_t handle)
{
	return (struct ffa_value) {
			.func = FFA_SUCCESS_32,
			.arg2 = (uint32_t)handle,
			.arg3 = (uint32_t)(handle >> 32)
		};
}

static inline ffa_vm_id_t 
ffa_vm_id(struct ffa_value args)
{
	return (args.arg1 >> 16) & 0xffff;
}

static inline ffa_vcpu_index_t
ffa_vcpu_index(struct ffa_value args)
{
	return args.arg1 & 0xffff;
}

static inline uint64_t 
ffa_vm_vcpu(ffa_vm_id_t      vm_id,
            ffa_vcpu_index_t vcpu_index)
{
	return ((uint32_t)vm_id << 16) | vcpu_index;
}

static inline ffa_vm_id_t 
ffa_frag_sender(struct ffa_value args)
{
	return (args.arg4 >> 16) & 0xffff;
}

/**
 * A set of contiguous pages which is part of a memory region. This corresponds
 * to table 40 of the FF-A 1.0 EAC specification, "Constituent memory region
 * descriptor".
 */
struct ffa_memory_region_constituent {
	/**
	 * The base IPA of the constituent memory region, aligned to 4 kiB page
	 * size granularity.
	 */
	uint64_t address;

	/** The number of 4 kiB pages in the constituent memory region. */
	uint32_t page_count;

	/** Reserved field, must be 0. */
	uint32_t reserved;
};

/**
 * A set of pages comprising a memory region. This corresponds to table 39 of
 * the FF-A 1.0 EAC specification, "Composite memory region descriptor".
 */
struct ffa_composite_memory_region {
	/**
	 * The total number of 4 kiB pages included in this memory region. This
	 * must be equal to the sum of page counts specified in each
	 * `ffa_memory_region_constituent`.
	 */
	uint32_t page_count;

	/**
	 * The number of constituents (`ffa_memory_region_constituent`)
	 * included in this memory region range.
	 */
	uint32_t constituent_count;

	/** Reserved field, must be 0. */
	uint64_t reserved_0;

	/** An array of `constituent_count` memory region constituents. */
	struct ffa_memory_region_constituent constituents[];
};

/** Flags to indicate properties of receivers during memory region retrieval. */
typedef uint8_t ffa_memory_receiver_flags_t;

/**
 * This corresponds to table 41 of the FF-A 1.0 EAC specification, "Memory
 * access permissions descriptor".
 */
struct ffa_memory_region_attributes {
	/** The ID of the VM to which the memory is being given or shared. */
	ffa_vm_id_t receiver;

	/**
	 * The permissions with which the memory region should be mapped in the
	 * receiver's page table.
	 */
	ffa_memory_access_permissions_t permissions;

	/**
	 * Flags used during FFA_MEM_RETRIEVE_REQ and FFA_MEM_RETRIEVE_RESP
	 * for memory regions with multiple borrowers.
	 */
	ffa_memory_receiver_flags_t flags;
};

/** Flags to control the behaviour of a memory sharing transaction. */
typedef uint32_t ffa_memory_region_flags_t;

/**
 * Clear memory region contents after unmapping it from the sender and before
 * mapping it for any receiver.
 */
#define FFA_MEMORY_REGION_FLAG_CLEAR 0x1

/**
 * Whether the hypervisor may time slice the memory sharing or retrieval
 * operation.
 */
#define FFA_MEMORY_REGION_FLAG_TIME_SLICE 0x2

/**
 * Whether the hypervisor should clear the memory region after the receiver
 * relinquishes it or is aborted.
 */
#define FFA_MEMORY_REGION_FLAG_CLEAR_RELINQUISH 0x4

#define FFA_MEMORY_REGION_TRANSACTION_TYPE_MASK         ((0x3U) << 3)
#define FFA_MEMORY_REGION_TRANSACTION_TYPE_UNSPECIFIED  ((0x0U) << 3)
#define FFA_MEMORY_REGION_TRANSACTION_TYPE_SHARE        ((0x1U) << 3)
#define FFA_MEMORY_REGION_TRANSACTION_TYPE_LEND         ((0x2U) << 3)
#define FFA_MEMORY_REGION_TRANSACTION_TYPE_DONATE       ((0x3U) << 3)

/**
 * This corresponds to table 42 of the FF-A 1.0 EAC specification, "Endpoint
 * memory access descriptor".
 */
struct ffa_memory_access {
	struct ffa_memory_region_attributes receiver_permissions;

	/**
	 * Offset in bytes from the start of the outer `ffa_memory_region` to
	 * an `ffa_composite_memory_region` struct.
	 */
	uint32_t composite_memory_region_offset;
	uint64_t reserved_0;
};

/**
 * Information about a set of pages which are being shared. This corresponds to
 * table 45 of the FF-A 1.0 EAC specification, "Lend, donate or share memory
 * transaction descriptor". Note that it is also used for retrieve requests and
 * responses.
 */
struct ffa_memory_region {
	/**
	 * The ID of the VM which originally sent the memory region, i.e. the
	 * owner.
	 */
	ffa_vm_id_t               sender;
	ffa_memory_attributes_t   attributes;

	/** Reserved field, must be 0. */
	uint8_t                   reserved_0;

	/** Flags to control behaviour of the transaction. */
	ffa_memory_region_flags_t flags;
	ffa_memory_handle_t       handle;

	/**
	 * An implementation defined value associated with the receiver and the
	 * memory region.
	 */
	uint64_t                  tag;

	uint32_t                  reserved_1; 	/** Reserved field, must be 0. */

	/**
	 * The number of `ffa_memory_access` entries included in this
	 * transaction.
	 */
	uint32_t                  receiver_count;

	/**
	 * An array of `attribute_count` endpoint memory access descriptors.
	 * Each one specifies a memory region offset, an endpoint and the
	 * attributes with which this memory region should be mapped in that
	 * endpoint's page table.
	 */
	struct ffa_memory_access  receivers[];
};

/**
 * Descriptor used for FFA_MEM_RELINQUISH requests. This corresponds to table
 * 150 of the FF-A 1.0 EAC specification, "Descriptor to relinquish a memory
 * region".
 */
struct ffa_mem_relinquish {
	ffa_memory_handle_t       handle;
	ffa_memory_region_flags_t flags;
	uint32_t                  endpoint_count;
	ffa_vm_id_t               endpoints[];
};

/**
 * Gets the `ffa_composite_memory_region` for the given receiver from an
 * `ffa_memory_region`, or NULL if it is not valid.
 */
static inline struct ffa_composite_memory_region *
ffa_memory_region_get_composite(struct ffa_memory_region * memory_region,
				uint32_t                   receiver_index)
{
	uint32_t offset = memory_region->receivers[receiver_index].composite_memory_region_offset;

	if (offset == 0) {
		return NULL;
	}

	return (struct ffa_composite_memory_region *)((uint8_t *)memory_region + offset);
}

static inline uint32_t 
ffa_mem_relinquish_init(struct ffa_mem_relinquish * relinquish_request,
                        ffa_memory_handle_t         handle, 
                        ffa_memory_region_flags_t   flags,
                        ffa_vm_id_t                 sender)
{
	relinquish_request->handle         = handle;
	relinquish_request->flags          = flags;
	relinquish_request->endpoint_count = 1;
	relinquish_request->endpoints[0]   = sender;

	return sizeof(struct ffa_mem_relinquish) + sizeof(ffa_vm_id_t);
}

uint32_t 
ffa_memory_region_init(struct ffa_memory_region                   * memory_region, 
                       size_t                                       memory_region_max_size,
                       ffa_vm_id_t                                  sender, 
                       ffa_vm_id_t                                  receiver,
                       const struct ffa_memory_region_constituent   constituents[],
                       uint32_t                                     constituent_count, 
                       uint32_t                                     tag,
                       ffa_memory_region_flags_t                    flags, 
                       enum ffa_data_access                         data_access,
                       enum ffa_instruction_access                  instruction_access,
                       enum ffa_memory_type                         type, 
                       enum ffa_memory_cacheability                 cacheability,
                       enum ffa_memory_shareability                 shareability, 
                       uint32_t                                   * fragment_length,
                       uint32_t                                   * total_length);


uint32_t 
ffa_memory_retrieve_request_init(struct ffa_memory_region            * memory_region, 
                                        ffa_memory_handle_t            handle,
                                        ffa_vm_id_t                    sender, 
                                        ffa_vm_id_t                    receiver, 
                                        uint32_t                       tag,
                                        ffa_memory_region_flags_t      flags, 
                                        enum ffa_data_access           data_access,
                                        enum ffa_instruction_access    instruction_access,
                                        enum ffa_memory_type           type, 
                                        enum ffa_memory_cacheability   cacheability,
                                        enum ffa_memory_shareability   shareability);


uint32_t 
ffa_memory_lender_retrieve_request_init(struct ffa_memory_region * memory_region, 
                                        ffa_memory_handle_t        handle,
                                        ffa_vm_id_t                sender);


bool 
ffa_retrieved_memory_region_init(struct ffa_memory_region                   * response, 
                                 size_t                                       response_max_size,
                                 ffa_vm_id_t                                  sender, 
                                 ffa_memory_attributes_t                      attributes,
                                 ffa_memory_region_flags_t                    flags, 
                                 ffa_memory_handle_t                          handle,
                                 ffa_vm_id_t                                  receiver, 
                                 ffa_memory_access_permissions_t              permissions,
                                 uint32_t                                     page_count, 
                                 uint32_t                                     total_constituent_count,
                                 const struct ffa_memory_region_constituent   constituents[],
                                 uint32_t                                     fragment_constituent_count,
                                 uint32_t                                   * total_length,
                                 uint32_t                                   * fragment_length);


uint32_t 
ffa_memory_fragment_init(struct ffa_memory_region_constituent       * fragment,
                         size_t                                       fragment_max_size,
                         const struct ffa_memory_region_constituent   constituents[],
                         uint32_t                                     constituent_count,
                         uint32_t                                   * fragment_length);
