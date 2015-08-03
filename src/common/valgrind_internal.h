/*
 * Copyright (c) 2015, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * valgrind_internal.h -- internal definitions for valgrind macros
 */

#ifdef USE_VALGRIND
#define	USE_VG_PMEMCHECK
#endif

#if defined(USE_VG_PMEMCHECK)
extern int On_valgrind;
#include <valgrind/valgrind.h>
#endif

#ifdef USE_VG_PMEMCHECK

#include <valgrind/pmemcheck.h>

#define	VALGRIND_REGISTER_PMEM_MAPPING(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_REGISTER_PMEM_MAPPING((addr), (len));\
} while (0)

#define	VALGRIND_REGISTER_PMEM_FILE(desc, base_addr, size, offset) do {\
	if (On_valgrind)\
		VALGRIND_PMC_REGISTER_PMEM_FILE((desc), (base_addr), (size), \
		(offset));\
} while (0)

#define	VALGRIND_REMOVE_PMEM_MAPPING(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_REMOVE_PMEM_MAPPING((addr), (len));\
} while (0)

#define	VALGRIND_CHECK_IS_PMEM_MAPPING(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_CHECK_IS_PMEM_MAPPING((addr), (len));\
} while (0)

#define	VALGRIND_PRINT_PMEM_MAPPINGS do {\
	if (On_valgrind)\
		VALGRIND_PMC_PRINT_PMEM_MAPPINGS;\
} while (0)

#define	VALGRIND_DO_FLUSH(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_DO_FLUSH((addr), (len));\
} while (0)

#define	VALGRIND_DO_FENCE do {\
	if (On_valgrind)\
		VALGRIND_PMC_DO_FENCE;\
} while (0)

#define	VALGRIND_DO_COMMIT do {\
	if (On_valgrind)\
		VALGRIND_PMC_DO_COMMIT;\
} while (0)

#define	VALGRIND_DO_PERSIST(addr, len) do {\
	if (On_valgrind) {\
		VALGRIND_PMC_DO_FLUSH((addr), (len));\
		VALGRIND_PMC_DO_FENCE;\
		VALGRIND_PMC_DO_COMMIT;\
		VALGRIND_PMC_DO_FENCE;\
	}\
} while (0)

/* XXX change definition to VALGRIND_PMC_SET_CLEAN */
#define	VALGRIND_SET_CLEAN(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_DO_PERSIST(addr, len);\
} while (0)

#define	VALGRIND_WRITE_STATS do {\
	if (On_valgrind)\
		VALGRIND_PMC_WRITE_STATS;\
} while (0)

#define	VALGRIND_LOG_STORES do {\
	if (On_valgrind)\
		VALGRIND_PMC_LOG_STORES;\
} while (0)

#define	VALGRIND_NO_LOG_STORES do {\
	if (On_valgrind)\
		VALGRIND_PMC_NO_LOG_STORES;\
} while (0)

#define	VALGRIND_ADD_LOG_REGION(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_ADD_LOG_REGION((addr), (len));\
} while (0)

#define	VALGRIND_REMOVE_LOG_REGION(addr, len) do {\
	if (On_valgrind)\ \
		VALGRIND_PMC_REMOVE_LOG_REGION((addr), (len));\
} while (0)

#define	VALGRIND_FULL_REORDER do {\
	if (On_valgrind)\
		VALGRIND_PMC_FULL_REORDER;\
} while (0)

#define	VALGRIND_PARTIAL_REORDER do {\
	if (On_valgrind)\
		VALGRIND_PMC_PARTIAL_REORDER;\
} while (0)

#define	VALGRIND_ONLY_FAULT do {\
	if (On_valgrind)\
		VALGRIND_PMC_ONLY_FAULT;\
} while (0)

#define	VALGRIND_STOP_REORDER_FAULT do {\
	if (On_valgrind)\
		VALGRIND_PMC_STOP_REORDER_FAULT;\
} while (0)

#else

#define	VALGRIND_REGISTER_PMEM_MAPPING(addr, len) do {} while (0)

#define	VALGRIND_REGISTER_PMEM_FILE(desc, base_addr, size, offset)\
	do {} while (0)

#define	VALGRIND_REMOVE_PMEM_MAPPING(addr, len) do {} while (0)

#define	VALGRIND_CHECK_IS_PMEM_MAPPING(addr, len) do {} while (0)

#define	VALGRIND_PRINT_PMEM_MAPPINGS do {} while (0)

#define	VALGRIND_DO_FLUSH(addr, len) do {} while (0)

#define	VALGRIND_DO_FENCE do {} while (0)

#define	VALGRIND_DO_COMMIT do {} while (0)

#define	VALGRIND_DO_PERSIST(addr, len) do {} while (0)

#define	VALGRIND_SET_CLEAN(addr, len) do {} while (0)

#define	VALGRIND_WRITE_STATS do {} while (0)

#define	VALGRIND_LOG_STORES do {} while (0)

#define	VALGRIND_NO_LOG_STORES do {} while (0)

#define	VALGRIND_ADD_LOG_REGION(addr, len) do {} while (0)

#define	VALGRIND_REMOVE_LOG_REGION(addr, len) do {} while (0)

#define	VALGRIND_FULL_REORDER do {} while (0)

#define	VALGRIND_PARTIAL_REORDER do {} while (0)

#define	VALGRIND_ONLY_FAULT do {} while (0)

#define	VALGRIND_STOP_REORDER_FAULT do {} while (0)

#endif