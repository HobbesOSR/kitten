/*
 * Copyright 2018 The Hafnium Authors.
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

/* Define the standard types for the platform. */
#if defined(__KERNEL__)

#include <lwk/types.h>

#define INT32_C(c) c

typedef phys_addr_t hf_ipaddr_t;

#else

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uintptr_t hf_ipaddr_t;

#endif

#define HF_HYPERVISOR_VM_ID (0)

/**
 * An offset to use when assigning VM IDs.
 * The offset is needed because VM ID 0 is reserved.
 */
#define HF_VM_ID_OFFSET     (1)

/**
 * The index and ID of the primary VM, which is responsible for scheduling.
 *
 * These are not equal because ID 0 is reserved for the hypervisor itself.
 * Primary VM therefore gets ID 1 and all other VMs come after that.
 */
#define HF_PRIMARY_VM_INDEX (0)
#define HF_PRIMARY_VM_ID    (HF_VM_ID_OFFSET + HF_PRIMARY_VM_INDEX)

/**
 * The special VM ID reserved for the OS running in the trusted execution
 * environment, e.g. secure EL1 on AArch64.
 */
#define HF_TEE_VM_ID              (0x8000)

/** Sleep value for an indefinite period of time. */
#define HF_SLEEP_INDEFINITE       (0xffffffffffffffff)

/** The amount of data that can be sent to a mailbox. */
#define HF_MAILBOX_SIZE           (4096)


#define HF_NUM_INTIDS             (64)         /** The number of virtual interrupt IDs which are supported.  */

#define HF_INVALID_INTID          0xffffffff /** Interrupt ID returned when there is no interrupt pending. */
#define HF_MAILBOX_READABLE_INTID 1          /** Interrupt ID indicating the mailbox is readable.          */
#define HF_MAILBOX_WRITABLE_INTID 2          /** Interrupt ID indicating a mailbox is writable.            */
#define HF_VIRTUAL_TIMER_INTID    3          /** The virtual interrupt ID used for the virtual timer.      */
