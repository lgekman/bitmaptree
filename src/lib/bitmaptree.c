/*
  SPDX-License-Identifier: Apache-2.0
  Copyright (c) 2022 Lars Ekman
*/

#include "bmt.h"
#include <string.h>
#include <assert.h>


static void freeTree(struct bmtitem* n)
{
	if (n == NULL || n == FULL)
		return;
	if (n->level > 0) {
		freeTree(n->zero);
		freeTree(n->one);
	}
	free(n);
}

struct BitmapTree* bmtCreate(uint64_t size)
{
	struct BitmapTree* bmt = CALLOC(sizeof(struct BitmapTree));
	if (size > 0x8000000000000000ULL)
		size = 0;
	// levels are really log2(size) but the last 6 bits are a 64-bit
	// bitmask so the used levels are log2(size) - 6.
	if (size == 0) {
		bmt->levels = 64 - BM_BITS;
	} else{
		unsigned levels = ulog2(size);
		bmt->size = 1ULL << levels;
		bmt->levels = levels - BM_BITS;
	}

	D(printf("sizeof(struct BitmapTree)=%lu\n", sizeof(struct BitmapTree)));
	D(printf("sizeof(struct bmtitem)=%lu\n", sizeof(struct bmtitem)));
	D(printf("bmtCreate: size=%lu, level=%u\n", bmt->size, bmt->levels));
	return bmt;
}

static struct bmtitem* treeClone(struct bmtitem* n) {
	if (n == NULL || n == FULL)
		return n;
	struct bmtitem* b = CALLOC(sizeof(struct bmtitem));
	b->level = n->level;
	if (b->level > 0) {
		b->zero = treeClone(n->zero);
		b->one = treeClone(n->one);
	} else {
		b->bits = n->bits;
	}
	return b;
}

struct BitmapTree* bmtClone(struct BitmapTree* b)
{
	struct BitmapTree* bmt = CALLOC(sizeof(struct BitmapTree));
	bmt->size = b->size;
	bmt->levels = b->levels;
	bmt->top = treeClone(b->top);
	return bmt;
}

void bmtDelete(struct BitmapTree* bmt)
{
	if (bmt == NULL)
		return;
	freeTree(bmt->top);
	free(bmt);
}

static struct bmtitem* setbit(
	struct bmtitem* n, uint64_t offset, unsigned level, void* value)
{
	if (n == value)
		return n;
	if (n == NULL || n == FULL)
		n = expandItem(level, n);

	uint64_t bitmask;
	if (level > 0) {
		bitmask = 1ULL << (level + 5);
		if (offset & bitmask) {
			n->one = setbit(n->one, offset, level - 1, value);
		} else {
			n->zero = setbit(n->zero, offset, level - 1, value);
		}
		if (n->zero == value && n->one == value) {
			free(n);
			return value;
		}
	} else {
		bitmask = 1ULL << (offset & 0x3f);
		if (value == FULL) {
			n->bits |= bitmask;
			if (n->bits == UINT64_MAX) {
				free(n);
				return FULL;
			}
		} else {
			n->bits &= ~bitmask;
			if (n->bits == 0) {
				free(n);
				return NULL;
			}
		}
		D(printf("setbit: bits=0x%016lx, bitmask=0x%lx\n", n->bits,bitmask));
	}
	return n;
}

void bmtSetBit(struct BitmapTree* bmt, uint64_t offset)
{
	if (bmt->size > 0 && offset >= bmt->size)
		return;
	bmt->top = setbit(bmt->top, offset, bmt->levels, FULL);
}

void bmtClearBit(struct BitmapTree* bmt, uint64_t offset)
{
	if (bmt->size > 0 && offset >= bmt->size)
		return;
	bmt->top = setbit(bmt->top, offset, bmt->levels, NULL);
}

struct bmtitem* reserveBit(
	struct bmtitem* n, unsigned level, uint64_t* offset, int* rc)
{
	if (n == FULL)
		return n;
	if (n == NULL) {
		n = CALLOC(sizeof(struct bmtitem));
		n->level = level;
	}
	if (level > 0) {
		if (n->zero != FULL) {
			n->zero = reserveBit(n->zero, level - 1, offset, rc);
		} else {
			*offset += 1ULL << (level + 5);
			n->one = reserveBit(n->one, level - 1, offset, rc);
		}
		if (n->zero == FULL && n->one == FULL) {
			free(n);
			return FULL;
		}
	} else {
		unsigned o = 0;
		uint64_t bitmask = 1;
		for (o = 0; o < 64; o++) {
			bitmask = 1ULL << o;
			if ((n->bits & bitmask) == 0)
				break;
		}
		assert(o < 64);	/* Full bitmasks should have been replaced with FULL */
		*offset += o;
		n->bits |= bitmask;
		*rc = 0;
		if (n->bits == UINT64_MAX) {
			free(n);
			return FULL;
		}
	}
	return n;
}

int bmtReserveBit(struct BitmapTree* bmt, uint64_t* offset)
{
	int rc = -1;
	*offset = 0;
	bmt->top = reserveBit(bmt->top, bmt->levels, offset, &rc);
	return rc;
}

static int getbit(struct bmtitem* n, uint64_t offset)
{
	if (n == NULL)
		return 0;
	if (n == FULL)
		return 1;

	uint64_t bitmask;
	if (n->level > 0) {
		bitmask = 1ULL << (n->level + 5);
		if (offset & bitmask) {
			return getbit(n->one, offset);
		} else {
			return getbit(n->zero, offset);
		}
	}

	bitmask = 1ULL << (offset & 0x3f);
	return (n->bits & bitmask) != 0;
}

int bmtBit(struct BitmapTree* bmt, uint64_t offset)
{
	if (bmt->size > 0 && offset >= bmt->size)
		return 0;
	return getbit(bmt->top, offset);
}

static struct bmtitem* setbranch(
	struct bmtitem* n, uint64_t offset, unsigned level,
	unsigned wantedLevel, void* value)
{
	if (n == value) {
		return n;
	}
	if (n == NULL || n == FULL)
		n = expandItem(level, n);

	if (level > 0 && (level + 6) > wantedLevel) {
		uint64_t bitmask = 1ULL << (level + 5);
		if (offset & bitmask) {
			n->one = setbranch(n->one, offset, level - 1, wantedLevel, value);
		} else {
			n->zero = setbranch(n->zero, offset, level - 1, wantedLevel, value);
		}
		if (n->zero == value && n->one == value) {
			free(n);
			return value;
		}
		return n;
	}
	// We have found the wanted level
	if (wantedLevel >= 6) {
		freeTree(n);
		return value;
	}
	assert(n->level == 0);
	D(printf("setbranch: level=%u, wantedLevel=%u\n", n->level, wantedLevel));
	// We must set/clear sections in the bitmap
	// Create a mask that has 2^wantedLevel bits and shift it to position
	unsigned shift = 1 << wantedLevel;
	uint64_t m = (1ULL << shift) - 1;
	shift = offset & 0x3f;
	m = m << shift;
	if (value == FULL) {
		n->bits |= m;
		if (n->bits == UINT64_MAX) {
			free(n);
			return FULL;
		}
	} else {
		n->bits &= ~m;
		if (n->bits == 0) {
			free(n);
			return NULL;
		}
	}
	D(printf("setbranch: m=0x%016lx, bits=0x%016lx\n", m, n->bits));
	return n;
}

int bmtsetbranch(
	struct BitmapTree* bmt, uint64_t offset, uint64_t size, void* value)
{
	if (size == 0) {
		size = bmt->size;
		if (offset == 0 && size == 0) {
			// Handle full set
			freeTree(bmt->top);
			bmt->top = value;
			return 0;
		}
	}

	// Is size a power of 2?
	if (size == 0)
		return -1;
	if (bmt->size > 0 && size > bmt->size)
		return -1;
	unsigned level = 0;			/* level = log2(size) */
	uint64_t m = 1;
	while (size != m) {
		if (size < m)
			return -1;
		m = m << 1;
		level++;
	}
	D(printf("bmtsetbranch: level=%u\n", level));
	// Is offset a multiple of size?
	if (offset % size)
		return -1;
	// Is offset + size out of range?
	if (bmt->size == 0) {
		if (offset > (UINT64_MAX - size + 1))
			return -1;
	} else if ((offset + size) > bmt->size)
		return -1;

	bmt->top = setbranch(bmt->top, offset, bmt->levels, level, value);
	return 0;
}

int bmtSetBranch(struct BitmapTree* bmt, uint64_t offset, uint64_t size)
{
	return bmtsetbranch(bmt, offset, size, FULL);
}
int bmtClearBranch(struct BitmapTree* bmt, uint64_t offset, uint64_t size)
{
	return bmtsetbranch(bmt, offset, size, NULL);
}

// ----------------------------------------------------------------------
// Serialize;

bmtRead_t bmtRead = NULL;
bmtWrite_t bmtWrite = NULL;

#define MAX_METHODS 4
static unsigned nMethods = 0;
struct {
	char const* name;
	bmtRead_t readFn;
	bmtWrite_t writeFn;
} methods[MAX_METHODS];

int bmtSerializeMethodRegister(
	char const* name, bmtRead_t readFn, bmtWrite_t writeFn, int set)
{
	if (nMethods >= MAX_METHODS)
		return -1;
	methods[nMethods].name = name;
	methods[nMethods].readFn = readFn;
	methods[nMethods].writeFn = writeFn;
	if (set) {
		bmtRead = readFn;
		bmtWrite = writeFn;
	}
	nMethods++;
	return 0;
}

int bmtSerializeMethod(char const* method)
{
	for (unsigned int i = 0; i < nMethods; i++) {
		if (strcmp(method, methods[i].name) == 0) {
			bmtRead = methods[i].readFn;
			bmtWrite = methods[i].writeFn;
			return 0;
		}
	}
	return -1;
}


// ----------------------------------------------------------------------
// Stats;

uint64_t bmtSize(struct BitmapTree* bmt)
{
	return bmt->size;
}

static int itemCmp(struct bmtitem* n1, struct bmtitem* n2)
{
	if (n1 == NULL && n2 == NULL)
		return 0;
	if (n1 == FULL && n2 == FULL)
		return 0;
	D(printf("itemCmp: level=%u,%u\n", n1->level, n2->level));
	if (n1->level != n2->level)
		return 1;
	if (n1->level > 0) {
		if (itemCmp(n1->zero, n2->zero) != 0)
			return 1;
		if (itemCmp(n1->one, n2->one) != 0)
			return 1;
		return 0;
	}
	if (n1->bits != n2->bits)
		return 1;
	return 0;
}
int bmtCompare(struct BitmapTree* bmt1, struct BitmapTree* bmt2)
{
	if (bmt1->size != bmt2->size)
		return 1;
	if (bmt1->levels != bmt2->levels)
		return 1;
	return itemCmp(bmt1->top, bmt2->top);
}


static uint64_t countOnesInWord(uint64_t w)
{
	uint64_t cnt = 0;
	uint64_t m;
	for (m = 0x8000000000000000; m != 0; m = m >> 1) {
		if (m & w)
			cnt++;
	}
	D(printf("countOnesInWord: %lu\n", cnt));
	return cnt;
}
static uint64_t cntOnes(struct bmtitem* n, unsigned level)
{
	if (n == NULL)
		return 0;
	if (n == FULL) {
		if ((level + 6) == 64)
			return UINT64_MAX;		/* This is one too few */

		return 1ULL << (level + 6);
	}
	if (level == 0)
		return countOnesInWord(n->bits);

	return cntOnes(n->zero, level - 1) + cntOnes(n->one, level - 1);
}
uint64_t bmtOnes(struct BitmapTree* bmt)
{
	return cntOnes(bmt->top, bmt->levels);
}

static uint64_t cntNodes(struct bmtitem* n)
{
	if (n == NULL || n == FULL)
		return 0;
	if (n->level == 0)
		return 1;
	return 1 + cntNodes(n->zero) + cntNodes(n->one);
}
uint64_t bmtNodes(struct BitmapTree* bmt)
{
	return cntNodes(bmt->top);
}

uint64_t bmtAllocated(struct BitmapTree* bmt)
{
	return sizeof(struct BitmapTree) + bmtNodes(bmt) * sizeof(struct bmtitem);
}


static void nodePrint(struct bmtitem* n, unsigned maxlevel, unsigned level)
{
	if (n == NULL || n == FULL) {
		for (int i = 0; i < (maxlevel - level) * 2; i++)
			putchar(' ');
		printf("(%u) %s\n", level, n == NULL ? "NULL":"FULL");
		return;
	}
	if (level == 0) {
		for (int i = 0; i < (maxlevel - level) * 2; i++)
			putchar(' ');
		printf("(%u) 0x%016lx\n", level, n->bits);
		return;
	}
	nodePrint(n->zero, maxlevel, level - 1);
	for (int i = 0; i < (maxlevel - level) * 2; i++)
		putchar(' ');
	printf("(%u)\n", level);
	nodePrint(n->one, maxlevel, level - 1);
}
void bmtPrint(struct BitmapTree* bmt)
{
	nodePrint(bmt->top, bmt->levels, bmt->levels);
}
