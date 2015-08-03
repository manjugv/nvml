/*
 * Copyright (c) 2014-2015, Intel Corporation
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
 * common.c -- definitions of common functions
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <err.h>
#include <sys/param.h>
#include <ctype.h>
#include <assert.h>
#include <getopt.h>
#include "common.h"
#include "output.h"
#include "libpmemblk.h"
#include "libpmemlog.h"
#include "libpmemobj.h"

#define	__USE_UNIX98
#include <unistd.h>

#define	REQ_BUFF_SIZE	2048

typedef const char *(*enum_to_str_fn)(int);

/*
 * pmem_pool_type_parse -- return pool type based on pool header data
 */
pmem_pool_type_t
pmem_pool_type_parse_hdr(const struct pool_hdr *hdrp)
{
	if (strncmp(hdrp->signature, LOG_HDR_SIG, POOL_HDR_SIG_LEN) == 0)
		return PMEM_POOL_TYPE_LOG;
	else if (strncmp(hdrp->signature, BLK_HDR_SIG, POOL_HDR_SIG_LEN) == 0)
		return PMEM_POOL_TYPE_BLK;
	else if (strncmp(hdrp->signature, OBJ_HDR_SIG, POOL_HDR_SIG_LEN) == 0)
		return PMEM_POOL_TYPE_OBJ;
	else
		return PMEM_POOL_TYPE_UNKNOWN;
}

/*
 * pmempool_info_parse_type -- returns pool type from command line arg
 */
pmem_pool_type_t
pmem_pool_type_parse_str(const char *str)
{
	if (strcmp(str, "blk") == 0) {
		return PMEM_POOL_TYPE_BLK;
	} else if (strcmp(str, "log") == 0) {
		return PMEM_POOL_TYPE_LOG;
	} else if (strcmp(str, "obj") == 0) {
		return PMEM_POOL_TYPE_OBJ;
	} else {
		return PMEM_POOL_TYPE_UNKNOWN;
	}
}

/*
 * util_validate_checksum -- validate checksum and return valid one
 */
int
util_validate_checksum(void *addr, size_t len, uint64_t *csum)
{
	/* validate checksum */
	int csum_valid = util_checksum(addr, len, csum, 0);
	/* get valid one */
	if (!csum_valid)
		util_checksum(addr, len, csum, 1);
	return csum_valid;
}

/*
 * util_parse_size -- parse size from string
 */
int
util_parse_size(const char *str, uint64_t *sizep)
{
	uint64_t size = 0;
	int shift = 0;
	char unit[3] = {0};
	int ret = sscanf(str, "%lu%3s", &size, unit);
	if (ret <= 0)
		return -1;
	if (ret == 2) {
		if ((unit[1] != '\0' && unit[1] != 'B') ||
			unit[2] != '\0')
			return -1;
		switch (unit[0]) {
		case 'K':
			shift = 10;
			break;
		case 'M':
			shift = 20;
			break;
		case 'G':
			shift = 30;
			break;
		case 'T':
			shift = 40;
			break;
		case 'P':
			shift = 50;
			break;
		default:
			return -1;
		}
	}

	if (sizep)
		*sizep = size << shift;

	return 0;
}

/*
 * util_parse_mode -- parse file mode from octal string
 */
int
util_parse_mode(const char *str, mode_t *mode)
{
	mode_t m = 0;
	int digits = 0;

	/* skip leading zeros */
	while (*str == '0')
		str++;

	/* parse at most 3 octal digits */
	while (digits < 3 && *str != '\0') {
		if (*str < '0' || *str > '7')
			return -1;
		m = (m << 3) | (*str - '0');
		digits++;
		str++;
	}

	/* more than 3 octal digits */
	if (digits == 3 && *str != '\0')
		return -1;

	if (mode)
		*mode = m;

	return 0;
}

static void
util_range_limit(struct range *rangep, struct range limit)
{
	if (rangep->first < limit.first)
		rangep->first = limit.first;
	if (rangep->last > limit.last)
		rangep->last = limit.last;
}

/*
 * util_parse_range_from_to -- parse range string as interval
 */
static int
util_parse_range_from_to(char *str, struct range *rangep, struct range entire)
{
	char *str1 = NULL;
	char sep;
	char *str2 = NULL;

	int ret = 0;
	if (sscanf(str, "%m[^-]%c%m[^-]", &str1, &sep, &str2) == 3 &&
			sep == '-' &&
			strlen(str) == (strlen(str1) + 1 + strlen(str2))) {
		if (util_parse_size(str1, &rangep->first) != 0)
			ret = -1;
		else if (util_parse_size(str2, &rangep->last) != 0)
			ret = -1;

		if (rangep->first > rangep->last) {
			uint64_t tmp = rangep->first;
			rangep->first = rangep->last;
			rangep->last = tmp;
		}

		util_range_limit(rangep, entire);
	} else {
		ret = -1;
	}

	if (str1)
		free(str1);
	if (str2)
		free(str2);

	return ret;
}

/*
 * util_parse_range_from -- parse range string as interval from specified number
 */
static int
util_parse_range_from(char *str, struct range *rangep, struct range entire)
{
	char *str1 = NULL;
	char sep;

	int ret = 0;
	if (sscanf(str, "%m[^-]%c", &str1, &sep) == 2 &&
			sep == '-' &&
			strlen(str) == (strlen(str1) + 1)) {
		if (util_parse_size(str1, &rangep->first) == 0) {
			rangep->last = entire.last;
			util_range_limit(rangep, entire);
		} else {
			ret = -1;
		}
	} else {
		ret = -1;
	}

	if (str1)
		free(str1);

	return ret;
}

/*
 * util_parse_range_to -- parse range string as interval to specified number
 */
static int
util_parse_range_to(char *str, struct range *rangep, struct range entire)
{
	char *str1 = NULL;
	char sep;

	int ret = 0;
	if (sscanf(str, "%c%m[^-]", &sep, &str1) == 2 &&
			sep == '-' &&
			strlen(str) == (1 + strlen(str1))) {
		if (util_parse_size(str1, &rangep->last) == 0) {
			rangep->first = entire.first;
			util_range_limit(rangep, entire);
		} else {
			ret = -1;
		}
	} else {
		ret = -1;
	}

	if (str1)
		free(str1);

	return ret;
}

/*
 * util_parse_range_number -- parse range string as a single number
 */
static int
util_parse_range_number(char *str, struct range *rangep, struct range entire)
{
	if (util_parse_size(str, &rangep->first) != 0)
		return -1;
	rangep->last = rangep->first;
	if (rangep->first > entire.last ||
	    rangep->last < entire.first)
		return -1;
	util_range_limit(rangep, entire);
	return 0;
}

/*
 * util_parse_range -- parse single range string
 */
static int
util_parse_range(char *str, struct range *rangep, struct range entire)
{
	if (util_parse_range_from_to(str, rangep, entire) == 0)
		return 0;
	if (util_parse_range_from(str, rangep, entire) == 0)
		return 0;
	if (util_parse_range_to(str, rangep, entire) == 0)
		return 0;
	if (util_parse_range_number(str, rangep, entire) == 0)
		return 0;
	return -1;
}

/*
 * util_ranges_overlap -- return 1 if two ranges are overlapped
 */
static int
util_ranges_overlap(struct range *rangep1, struct range *rangep2)
{
	if (rangep1->last + 1 < rangep2->first ||
	    rangep2->last + 1 < rangep1->first)
		return 0;
	else
		return 1;
}

/*
 * util_ranges_add -- create and add range
 */
int
util_ranges_add(struct ranges *rangesp, struct range range)
{
	struct range *rangep = malloc(sizeof (struct range));
	if (!rangep)
		err(1, "Cannot allocate memory for range\n");
	memcpy(rangep, &range, sizeof (*rangep));

	struct range *curp, *next;
	uint64_t first = rangep->first;
	uint64_t last = rangep->last;

	curp = LIST_FIRST(&rangesp->head);
	while (curp) {
		next = LIST_NEXT(curp, next);
		if (util_ranges_overlap(curp, rangep)) {
			LIST_REMOVE(curp, next);
			if (curp->first < first)
				first = curp->first;
			if (curp->last > last)
				last = curp->last;
			free(curp);
		}
		curp = next;
	}

	rangep->first = first;
	rangep->last = last;

	LIST_FOREACH(curp, &rangesp->head, next) {
		if (curp->first < rangep->first) {
			LIST_INSERT_AFTER(curp, rangep, next);
			return 0;
		}
	}

	LIST_INSERT_HEAD(&rangesp->head, rangep, next);

	return 0;
}

/*
 * util_ranges_contain -- return 1 if ranges contain the number n
 */
int
util_ranges_contain(const struct ranges *rangesp, uint64_t n)
{
	struct range *curp  = NULL;
	LIST_FOREACH(curp, &rangesp->head, next) {
		if (curp->first <= n && n <= curp->last)
			return 1;
	}

	return 0;
}

/*
 * util_ranges_empty -- return 1 if ranges are empty
 */
int
util_ranges_empty(const struct ranges *rangesp)
{
	return LIST_EMPTY(&rangesp->head);
}

/*
 * util_ranges_clear -- clear list of ranges
 */
void
util_ranges_clear(struct ranges *rangesp)
{
	while (!LIST_EMPTY(&rangesp->head)) {
		struct range *rangep = LIST_FIRST(&rangesp->head);
		LIST_REMOVE(rangep, next);
		free(rangep);
	}
}

/*
 * util_parse_ranges -- parser ranges from string
 *
 * The valid formats of range are:
 * - 'n-m' -- from n to m
 * - '-m'  -- from minimum passed in entirep->first to m
 * - 'n-'  -- from n to maximum passed in entirep->last
 * - 'n'   -- n'th byte/block
 * Multiple ranges may be separated by comma:
 * 'n1-m1,n2-,-m3,n4'
 */
int
util_parse_ranges(const char *ptr, struct ranges *rangesp, struct range entire)
{
	if (ptr == NULL)
		return util_ranges_add(rangesp, entire);

	char *dup = strdup(ptr);
	if (!dup)
		err(1, "Cannot allocate memory for ranges");
	char *str = dup;
	int ret = 0;
	char *next = str;
	do {
		str = next;
		next = strchr(str, ',');
		if (next != NULL) {
			*next = '\0';
			next++;
		}
		struct range range;
		if (util_parse_range(str, &range, entire)) {
			ret = -1;
			goto out;
		} else if (util_ranges_add(rangesp, range)) {
			ret = -1;
			goto out;
		}
	} while (next != NULL);
out:
	free(dup);
	return ret;
}

/*
 * pmem_pool_get_min_size -- return minimum size of pool for specified type
 */
uint64_t
pmem_pool_get_min_size(pmem_pool_type_t type)
{
	switch (type) {
	case PMEM_POOL_TYPE_LOG:
		return PMEMLOG_MIN_POOL;
	case PMEM_POOL_TYPE_BLK:
		return PMEMBLK_MIN_POOL;
	case PMEM_POOL_TYPE_OBJ:
		return PMEMOBJ_MIN_POOL;
	default:
		break;
	}

	return 0;
}

/*
 * pmem_pool_parse_params -- parse pool type, file size and block size
 */
int
pmem_pool_parse_params(const char *fname, struct pmem_pool_params *paramsp)
{
	struct stat stat_buf;
	struct pool_hdr hdr;

	paramsp->type = PMEM_POOL_TYPE_NONE;

	int fd = open(fname, O_RDONLY);
	if (fd < 0)
		return -1;

	int ret = 0;
	paramsp->type = PMEM_POOL_TYPE_UNKNOWN;

	/* read pool_hdr */
	if (pread(fd, &hdr, sizeof (hdr), 0) ==
			sizeof (hdr)) {
		paramsp->type = pmem_pool_type_parse_hdr(&hdr);
	} else {
		ret = -1;
		goto out;
	}


	/* get file size and mode */
	if (fstat(fd, &stat_buf) == 0) {
		paramsp->size = stat_buf.st_size;
		paramsp->mode = stat_buf.st_mode;
	} else {
		ret = -1;
		goto out;
	}

	if (paramsp->type == PMEM_POOL_TYPE_BLK) {
		struct pmemblk pbp;
		if (pread(fd, &pbp, sizeof (pbp), 0) == sizeof (pbp)) {
			paramsp->blk.bsize = le32toh(pbp.bsize);
		} else {
			ret = -1;
			goto out;
		}
	} else if (paramsp->type == PMEM_POOL_TYPE_OBJ) {
		struct pmemobjpool pop;
		if (pread(fd, &pop, sizeof (pop), 0) == sizeof (pop)) {
			memcpy(paramsp->obj.layout, pop.layout,
					PMEMOBJ_MAX_LAYOUT);
		} else {
			ret = -1;
			goto out;
		}
	}
out:
	close(fd);

	return ret;
}

/*
 * util_pool_hdr_convert -- convert pool header to host byte order
 */
void
util_convert2h_pool_hdr(struct pool_hdr *hdrp)
{
	hdrp->compat_features = le32toh(hdrp->compat_features);
	hdrp->incompat_features = le32toh(hdrp->incompat_features);
	hdrp->ro_compat_features = le32toh(hdrp->ro_compat_features);
	hdrp->crtime = le64toh(hdrp->crtime);
	hdrp->checksum = le64toh(hdrp->checksum);
}

/*
 * util_pool_hdr_convert -- convert pool header to LE byte order
 */
void
util_convert2le_pool_hdr(struct pool_hdr *hdrp)
{
	hdrp->compat_features = htole32(hdrp->compat_features);
	hdrp->incompat_features = htole32(hdrp->incompat_features);
	hdrp->ro_compat_features = htole32(hdrp->ro_compat_features);
	hdrp->crtime = htole64(hdrp->crtime);
	hdrp->checksum = htole64(hdrp->checksum);
}

/*
 * util_convert_btt_info -- convert btt_info header to host byte order
 */
void
util_convert2h_btt_info(struct btt_info *infop)
{
	infop->flags = le32toh(infop->flags);
	infop->minor = le16toh(infop->minor);
	infop->external_lbasize = le32toh(infop->external_lbasize);
	infop->external_nlba = le32toh(infop->external_nlba);
	infop->internal_lbasize = le32toh(infop->internal_lbasize);
	infop->internal_nlba = le32toh(infop->internal_nlba);
	infop->nfree = le32toh(infop->nfree);
	infop->infosize = le32toh(infop->infosize);
	infop->nextoff = le64toh(infop->nextoff);
	infop->dataoff = le64toh(infop->dataoff);
	infop->mapoff = le64toh(infop->mapoff);
	infop->flogoff = le64toh(infop->flogoff);
	infop->infooff = le64toh(infop->infooff);
	infop->checksum = le64toh(infop->checksum);
}

/*
 * util_convert_btt_info -- convert btt_info header to LE byte order
 */
void
util_convert2le_btt_info(struct btt_info *infop)
{
	infop->flags = htole64(infop->flags);
	infop->minor = htole16(infop->minor);
	infop->external_lbasize = htole32(infop->external_lbasize);
	infop->external_nlba = htole32(infop->external_nlba);
	infop->internal_lbasize = htole32(infop->internal_lbasize);
	infop->internal_nlba = htole32(infop->internal_nlba);
	infop->nfree = htole32(infop->nfree);
	infop->infosize = htole32(infop->infosize);
	infop->nextoff = htole64(infop->nextoff);
	infop->dataoff = htole64(infop->dataoff);
	infop->mapoff = htole64(infop->mapoff);
	infop->flogoff = htole64(infop->flogoff);
	infop->infooff = htole64(infop->infooff);
	infop->checksum = htole64(infop->checksum);
}

/*
 * util_convert2h_btt_flog -- convert btt_flog to host byte order
 */
void
util_convert2h_btt_flog(struct btt_flog *flogp)
{
	flogp->lba = le32toh(flogp->lba);
	flogp->old_map = le32toh(flogp->old_map);
	flogp->new_map = le32toh(flogp->new_map);
	flogp->seq = le32toh(flogp->seq);
}

/*
 * util_convert2le_btt_flog -- convert btt_flog to LE byte order
 */
void
util_convert2le_btt_flog(struct btt_flog *flogp)
{
	flogp->lba = htole32(flogp->lba);
	flogp->old_map = htole32(flogp->old_map);
	flogp->new_map = htole32(flogp->new_map);
	flogp->seq = htole32(flogp->seq);
}

/*
 * util_convert2h_pmemlog -- convert pmemlog structure to host byte order
 */
void
util_convert2h_pmemlog(struct pmemlog *plp)
{
	plp->start_offset = le64toh(plp->start_offset);
	plp->end_offset = le64toh(plp->end_offset);
	plp->write_offset = le64toh(plp->write_offset);
}

/*
 * util_convert2le_pmemlog -- convert pmemlog structure to LE byte order
 */
void
util_convert2le_pmemlog(struct pmemlog *plp)
{
	plp->start_offset = htole64(plp->start_offset);
	plp->end_offset = htole64(plp->end_offset);
	plp->write_offset = htole64(plp->write_offset);
}

/*
 * util_check_memory -- check if memory contains single value
 */
int
util_check_memory(const uint8_t *buff, size_t len, uint8_t val)
{
	size_t i;
	for (i = 0; i < len; i++) {
		if (buff[i] != val)
			return -1;
	}

	return 0;
}

/*
 * util_get_max_bsize -- return maximum size of block for given file size
 */
uint32_t
util_get_max_bsize(uint64_t fsize)
{
	if (fsize == 0)
		return 0;

	/* default nfree */
	uint32_t nfree = BTT_DEFAULT_NFREE;

	/* number of blocks must be at least 2 * nfree */
	uint32_t internal_nlba = 2 * nfree;

	/* compute flog size */
	int flog_size = nfree *
		roundup(2 * sizeof (struct btt_flog), BTT_FLOG_PAIR_ALIGN);
	flog_size = roundup(flog_size, BTT_ALIGNMENT);

	/* compute arena size from file size */
	uint64_t arena_size = fsize;
	/* without pmemblk structure */
	arena_size -= sizeof (struct pmemblk);
	if (arena_size > BTT_MAX_ARENA) {
		arena_size = BTT_MAX_ARENA;
	}
	/* without BTT Info header and backup */
	arena_size -= 2 * sizeof (struct btt_info);
	/* without BTT FLOG size */
	arena_size -= flog_size;

	/* compute maximum internal LBA size */
	uint32_t internal_lbasize = (arena_size - BTT_ALIGNMENT) /
			internal_nlba - BTT_MAP_ENTRY_SIZE;

	if (internal_lbasize < BTT_MIN_LBA_SIZE)
		internal_lbasize = BTT_MIN_LBA_SIZE;

	internal_lbasize =
		roundup(internal_lbasize, BTT_INTERNAL_LBA_ALIGNMENT)
			- BTT_INTERNAL_LBA_ALIGNMENT;

	return internal_lbasize;
}

/*
 * util_check_bsize -- check if block size is valid for given file size
 */
int
util_check_bsize(uint32_t bsize, uint64_t fsize)
{
	uint32_t max_bsize = util_get_max_bsize(fsize);
	return !(bsize < max_bsize);
}

char
ask(char op, char *answers, char def_ans, const char *fmt, va_list ap)
{
	char ans = '\0';
	if (op != '?')
		return op;
	do {
		vprintf(fmt, ap);
		size_t len = strlen(answers);
		size_t i;
		char def_ansl = tolower(def_ans);
		printf(" [");
		for (i = 0; i < len; i++) {
			char ans = tolower(answers[i]);
			printf("%c", ans == def_ansl ? toupper(ans) : ans);
			if (i != len -1)
				printf("/");
		}
		printf("] ");
		ans = tolower(getchar());
		if (ans != '\n')
			getchar();
	} while (ans != '\n' && strchr(answers, ans) == NULL);
	return ans == '\n' ? def_ans : ans;
}

char
ask_yn(char op, char def_ans, const char *fmt, va_list ap)
{
	char ret = ask(op, "yn", def_ans, fmt, ap);
	return ret;
}

char
ask_Yn(char op, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char ret = ask_yn(op, 'y', fmt, ap);
	va_end(ap);
	return ret;
}

char
ask_yN(char op, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char ret = ask_yn(op, 'n', fmt, ap);
	va_end(ap);
	return ret;
}

/*
 * util_parse_enum -- parse single enum and store to bitmap
 */
static int
util_parse_enum(const char *str, int first, int max, uint64_t *bitmap,
		enum_to_str_fn enum_to_str)
{
	for (int i = first; i < max; i++) {
		if (strcmp(str, enum_to_str(i)) == 0) {
			*bitmap |= (1<<i);
			return 0;
		}
	}

	return -1;
}

/*
 * util_parse_enums -- parse enums and store to bitmap
 */
static int
util_parse_enums(const char *str, int first, int max, uint64_t *bitmap,
		enum_to_str_fn enum_to_str)
{
	char *dup = strdup(str);
	if (!dup)
		err(1, "Cannot allocate memory for enum str");

	char *ptr = dup;
	int ret = 0;
	char *comma;
	do {
		comma = strchr(ptr, ',');
		if (comma) {
			*comma = '\0';
			comma++;
		}

		if ((ret = util_parse_enum(ptr, first, max,
				bitmap, enum_to_str))) {
			goto out;
		}

		ptr = comma;
	} while (ptr);
out:
	free(dup);
	return ret;
}

/*
 * util_parse_chunk_types -- parse chunk types strings
 */
int
util_parse_chunk_types(const char *str, uint64_t *types)
{
	assert(MAX_CHUNK_TYPE < 8 * sizeof (*types));
	return util_parse_enums(str, 0, MAX_CHUNK_TYPE, types,
			(enum_to_str_fn)out_get_chunk_type_str);
}

/*
 * util_parse_lane_section -- parse lane section strings
 */
int
util_parse_lane_sections(const char *str, uint64_t *types)
{
	assert(MAX_LANE_SECTION < 8 * sizeof (*types));
	return util_parse_enums(str, 0, MAX_LANE_SECTION, types,
			(enum_to_str_fn)out_get_lane_section_str);
}

/*
 * util_options_alloc -- allocate and initialize options structure
 */
struct options *
util_options_alloc(const struct option *options,
		size_t nopts, const struct option_requirement *req)
{
	struct options *opts = calloc(1, sizeof (*opts));
	if (!opts)
		err(1, "Cannot allocate memory for options structure");

	opts->options = options;
	opts->noptions = nopts;
	opts->req = req;
	size_t bitmap_size = howmany(nopts, 8);
	opts->bitmap = calloc(bitmap_size, 1);
	if (!opts->bitmap)
		err(1, "Cannot allocate memory for options bitmap");

	return opts;
}

/*
 * util_options_free -- free options structure
 */
void
util_options_free(struct options *opts)
{
	free(opts->bitmap);
	free(opts);
}

/*
 * util_opt_get_index -- return index of specified option in global
 * array of options
 */
static int
util_opt_get_index(const struct options *opts, int opt)
{
	const struct option *lopt = &opts->options[0];
	int ret = 0;
	while (lopt->name) {
		if ((lopt->val & ~OPT_MASK) == opt)
			return ret;
		lopt++;
		ret++;
	}
	return -1;
}

/*
 * util_opt_get_req -- get required option for specified option
 */
static struct option_requirement *
util_opt_get_req(const struct options *opts, int opt, pmem_pool_type_t type)
{
	size_t n = 0;
	struct option_requirement *ret = NULL;
	const struct option_requirement *req = &opts->req[0];
	while (req->opt) {
		if (req->opt == opt && (req->type & type)) {
			n++;
			ret = realloc(ret, n * sizeof (*ret));
			if (!ret)
				err(1, "Cannot allocate memory for"
					" option requirements");
			ret[n - 1] = *req;
		}
		req++;
	}

	if (ret) {
		ret = realloc(ret, (n + 1) * sizeof (*ret));
		if (!ret)
			err(1, "Cannot allocate memory for"
				" option requirements");
		memset(&ret[n], 0, sizeof (*ret));
	}

	return ret;
}

/*
 * util_opt_check_requirements -- check if requirements has been fulfilled
 */
static int
util_opt_check_requirements(const struct options *opts,
		const struct option_requirement *req)
{
	int count = 0;
	int set = 0;
	uint64_t tmp;
	while ((tmp = req->req) != 0) {
		while (tmp) {
			int req_idx =
				util_opt_get_index(opts, tmp & OPT_REQ_MASK);

			if (isset(opts->bitmap, req_idx)) {
				set++;
				break;
			}

			tmp >>= OPT_REQ_SHIFT;
		}
		req++;
		count++;
	}

	return count != set;
}

/*
 * util_opt_print_requirements -- print requirements for specified option
 */
static void
util_opt_print_requirements(const struct options *opts,
		const struct option_requirement *req)
{
	char buff[REQ_BUFF_SIZE];
	int n = 0;
	uint64_t tmp;
	const struct option *opt =
		&opts->options[util_opt_get_index(opts, req->opt)];
	n += snprintf(&buff[n], REQ_BUFF_SIZE - n,
			"option [-%c|--%s] requires: ", opt->val, opt->name);
	size_t rc = 0;
	while ((tmp = req->req) != 0) {
		if (rc != 0)
			n += snprintf(&buff[n], REQ_BUFF_SIZE - n, " and ");

		size_t c = 0;
		while (tmp) {
			if (c == 0)
				n += snprintf(&buff[n], REQ_BUFF_SIZE - n, "[");
			else
				n += snprintf(&buff[n], REQ_BUFF_SIZE - n, "|");

			int req_opt_ind =
				util_opt_get_index(opts, tmp & OPT_REQ_MASK);
			const struct option *req_option =
				&opts->options[req_opt_ind];

			n += snprintf(&buff[n], REQ_BUFF_SIZE - n,
				"-%c|--%s", req_option->val, req_option->name);

			tmp >>= OPT_REQ_SHIFT;
			c++;
		}
		n += snprintf(&buff[n], REQ_BUFF_SIZE - n, "]");

		req++;
		rc++;
	}

	out_err("%s\n", buff);
}

/*
 * util_opt_verify_requirements -- verify specified requirements for options
 */
static int
util_opt_verify_requirements(const struct options *opts, size_t index,
		pmem_pool_type_t type)
{
	const struct option *opt = &opts->options[index];
	int val = opt->val & ~OPT_MASK;
	struct option_requirement *req;

	if ((req = util_opt_get_req(opts, val, type)) == NULL)
		return 0;

	int ret = 0;

	if (util_opt_check_requirements(opts, req)) {
		ret = -1;
		util_opt_print_requirements(opts, req);
	}

	free(req);
	return ret;
}

/*
 * util_opt_verify_type -- check if used option matches pool type
 */
static int
util_opt_verify_type(const struct options *opts, pmem_pool_type_t type,
		size_t index)
{
	const struct option *opt = &opts->options[index];
	int val = opt->val & ~OPT_MASK;
	int opt_type = opt->val;
	opt_type >>= OPT_SHIFT;
	if (!(opt_type & (1<<type))) {
		out_err("'--%s|-%c' -- invalid option specified"
			" for pool type '%s'\n",
			opt->name, val,
			out_get_pool_type_str(type));
		return -1;
	}

	return 0;
}

/*
 * util_options_getopt -- wrapper for getopt_long which sets bitmap
 */
int
util_options_getopt(int argc, char *argv[], const char *optstr,
		const struct options *opts)
{
	int opt = getopt_long(argc, argv, optstr, opts->options, NULL);
	if (opt == -1 || opt == '?')
		return opt;

	opt &= ~OPT_MASK;
	int option_index = util_opt_get_index(opts, opt);
	assert(option_index >= 0);

	setbit(opts->bitmap, option_index);

	return opt;
}

/*
 * util_options_verify -- verify options
 */
int
util_options_verify(const struct options *opts, pmem_pool_type_t type)
{
	for (size_t i = 0; i < opts->noptions; i++) {
		if (isset(opts->bitmap, i)) {
			if (util_opt_verify_type(opts, type, i))
				return -1;

			if (opts->req)
				if (util_opt_verify_requirements(opts, i, type))
					return -1;
		}
	}

	return 0;
}

/*
 * util_heap_max_zone -- get number of zones
 */
int
util_heap_max_zone(size_t size)
{
	int max_zone = 0;
	size -= sizeof (struct heap_header);

	while (size >= ZONE_MIN_SIZE) {
		max_zone++;
		size -= size <= ZONE_MAX_SIZE ? size : ZONE_MAX_SIZE;
	}

	return max_zone;
}

/*
 * util_plist_nelements -- count number of elements on a list
 */
size_t
util_plist_nelements(struct pmemobjpool *pop, struct list_head *headp)
{
	size_t i = 0;
	struct list_entry *entryp;
	PLIST_FOREACH(entryp, pop, headp)
		i++;
	return i;
}

/*
 * util_plist_get_entry -- return nth element from list
 */
struct list_entry *
util_plist_get_entry(struct pmemobjpool *pop,
	struct list_head *headp, size_t n)
{
	struct list_entry *entryp;
	PLIST_FOREACH(entryp, pop, headp) {
		if (n == 0)
			return entryp;
		n--;
	}

	return NULL;
}