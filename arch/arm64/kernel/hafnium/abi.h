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

#include "ffa.h"
#include "types.h"

/* Keep macro alignment */
/* clang-format off */

/* TODO: Define constants below according to spec. */
#define HF_VM_GET_COUNT                0xff01
#define HF_VCPU_GET_COUNT              0xff02
#define HF_MAILBOX_WRITABLE_GET        0xff03
#define HF_MAILBOX_WAITER_GET          0xff04
#define HF_INTERRUPT_ENABLE            0xff05
#define HF_INTERRUPT_GET               0xff06
#define HF_INTERRUPT_INJECT            0xff07

/* Custom FF-A-like calls returned from FFA_RUN. */
#define HF_FFA_RUN_WAIT_FOR_INTERRUPT  0xff09
#define HF_FFA_RUN_WAKE_UP             0xff0a

/* This matches what Trusty and its ATF module currently use. */
#define HF_DEBUG_LOG                   0xbd000000

/* clang-format on */
