/*
  SPDX-License-Identifier: Apache-2.0
  Copyright (c) 2022 Lars Ekman
*/

#include "bmt.h"
#include <assert.h>

static void setbits(
	struct BitmapTree* bmt, uint64_t offset, unsigned cnt, int value)
{
	if (value == 0) {
		for (unsigned i = 0; i < cnt; i++)
			bmtClearBit(bmt, offset + i);
	} else {
		for (unsigned i = 0; i < cnt; i++)
			bmtSetBit(bmt, offset + i);
	}
}

int main(int argc, char* argv[])
{
	struct BitmapTree* bmt;
	uint64_t offset;
	uint64_t x;

	// Useful debug printouts;
	D(bmtPrint(bmt);printf("---\n"));
	D(printf("offset=%lu\n", offset));
	D(printf("bmtNodes()=%lu\n", bmtNodes(bmt)));
	D(printf("bmtOnes()=%lu\n", bmtOnes(bmt)));
	D(printf("bmtAllocated()=%lu\n", bmtAllocated(bmt)));

	// log2
	D(printf("sizeof(unsigned long) = %lu\n", sizeof(unsigned long)));
	D(printf("ulog2(%lu) = %u\n", 1UL, ulog2(1UL)));
	assert(ulog2(0) == 0);		/* (invalid but works) */
	assert(ulog2(1) == 0);
	assert(ulog2(2) == 1);
	assert(ulog2(3) == 2);
	assert(ulog2(7) == 3);
	assert(ulog2(UINT64_MAX) == 64);
	assert(ulog2(0x8000000000000000ULL) == 63);
	assert(ulog2(0x8000000000000001ULL) == 64);
	//assert(ulog2() == );

	// Basics;
	bmt = bmtCreate(256);
	assert(bmt != NULL);
	assert(bmtOnes(bmt) == 0);
	assert(bmtNodes(bmt) == 0);
	bmtSetBit(bmt, 256);		/* Out of range = no-op */
	assert(bmtBit(bmt, 256) == 0);
	bmtSetBit(bmt, 255);
	assert(bmtBit(bmt, 255) == 1);
	bmtSetBit(bmt, 0);
	assert(bmtBit(bmt, 0) == 1);
	assert(bmtNodes(bmt) == 5);
	assert(bmtOnes(bmt) == 2);
	bmtClearBit(bmt, 255);
	bmtClearBit(bmt, 0);
	assert(bmtNodes(bmt) == 0);
	bmtDelete(bmt);

	// Full size;
	bmt = bmtCreate(0);
	assert(bmtBit(bmt, UINT64_MAX) == 0);
	bmtSetBit(bmt, UINT64_MAX);
	D(bmtPrint(bmt);printf("---\n"));
	assert(bmtBit(bmt, UINT64_MAX) == 1);
	assert(bmtNodes(bmt) == 59);
	D(printf("bmtAllocated()=%lu\n", bmtAllocated(bmt)));
	bmtClearBit(bmt, UINT64_MAX);
	assert(bmtNodes(bmt) == 0);
	bmtSetBit(bmt, 1024*8);
	D(bmtPrint(bmt);printf("---\n"));
	bmtDelete(bmt);

	// Single-word;
	bmt = bmtCreate(64);
	bmtSetBit(bmt, 64);
	assert(bmtOnes(bmt) == 0);
	bmtSetBit(bmt, 63);
	assert(bmtOnes(bmt) == 1);
	assert(bmtNodes(bmt) == 1);
	bmtClearBit(bmt, 63);
	assert(bmtOnes(bmt) == 0);
	assert(bmtNodes(bmt) == 0);	
	bmtDelete(bmt);

	// Node create/delete;
	bmt = bmtCreate(128);
	assert(bmtBit(bmt, 64) == 0);
	bmtSetBit(bmt, 64);
	assert(bmtOnes(bmt) == 1);
	assert(bmtNodes(bmt) == 2);
	assert(bmtBit(bmt, 64) == 1);
	setbits(bmt, 64, 64, 1);
	assert(bmtNodes(bmt) == 1);
	assert(bmtOnes(bmt) == 64);
	setbits(bmt, 0, 64, 1);
	assert(bmtNodes(bmt) == 0);
	assert(bmtOnes(bmt) == 128);
	setbits(bmt, 0, 64, 0);
	assert(bmtNodes(bmt) == 1);
	assert(bmtOnes(bmt) == 64);
	setbits(bmt, 64, 64, 0);
	assert(bmtOnes(bmt) == 0);
	assert(bmtNodes(bmt) == 0);
	bmtDelete(bmt);

	// Reserve bit;
	bmt = bmtCreate(256);
	setbits(bmt, 0, 64, 1);
	assert(bmtOnes(bmt) == 64);
	assert(bmtNodes(bmt) == 2);
	assert(bmtReserveBit(bmt, &offset) == 0);
	assert(offset == 64);
	assert(bmtOnes(bmt) == 65);
	assert(bmtNodes(bmt) == 3);
	bmtClearBit(bmt, 1);
	assert(bmtOnes(bmt) == 64);
	assert(bmtReserveBit(bmt, &offset) == 0);
	assert(offset == 1);
	bmtDelete(bmt);

	// Branch set/clear basic;
	bmt = bmtCreate(256);
	assert(bmtSetBranch(bmt, 0, 3) != 0);
	assert(bmtSetBranch(bmt, 17, 8) != 0);
	assert(bmtSetBranch(bmt, 256, 8) != 0);
	assert(bmtSetBranch(bmt, 0, 256) == 0); /* Fill the array */
	assert(bmtOnes(bmt) == 256);
	assert(bmtNodes(bmt) == 0);
	assert(bmtClearBranch(bmt, 128, 128) == 0); /* Clear the upper half */
	assert(bmtOnes(bmt) == 128);
	assert(bmtNodes(bmt) == 1);
	assert(bmtReserveBit(bmt, &offset) == 0);
	assert(offset == 128);
	bmtClearBit(bmt, offset);
	assert(bmtClearBranch(bmt, 0, 64) == 0);
	assert(bmtSetBranch(bmt, 32, 32) == 0);
	assert(bmtOnes(bmt) == 32+64);
	assert(bmtNodes(bmt) == 3);
	assert(bmtClearBranch(bmt, 32, 32) == 0);
	assert(bmtOnes(bmt) == 64);
	assert(bmtNodes(bmt) == 2);
	bmtDelete(bmt);

	// Branch set/clear in full array;
	bmt = bmtCreate(0);
	assert(bmtSetBranch(bmt, 0, 0) == 0);
	assert(bmtOnes(bmt) == UINT64_MAX);
	assert(bmtClearBranch(bmt, 1024, 64) == 0);
	assert(bmtOnes(bmt) == (UINT64_MAX - 63));
	assert(bmtReserveBit(bmt, &offset) == 0);
	assert(offset == 1024);
	assert(bmtSetBranch(bmt, 1024, 64) == 0);
	assert(bmtNodes(bmt) == 0);
	offset = 0x8000000000000000ul;
	assert(bmtClearBranch(bmt, offset, offset) == 0); /* Half empty */
	assert(bmtOnes(bmt) == offset);
	bmtDelete(bmt);

	// Branch set/clear with size < 64;
	bmt = bmtCreate(256);
	assert(bmtSetBranch(bmt, 0, 0) == 0);
	assert(bmtOnes(bmt) == 256);
	assert(bmtClearBranch(bmt, 200, 8) == 0);
	assert(bmtClearBranch(bmt, 248, 4) == 0);
	for (x = 200; x < 208; x++) {
		assert(bmtReserveBit(bmt, &offset) == 0);
		assert(offset == x);
	}
	for (x = 248; x < 252; x++) {
		assert(bmtReserveBit(bmt, &offset) == 0);
		assert(offset == x);
	}
	assert(bmtReserveBit(bmt, &offset) != 0);
	bmtDelete(bmt);

	// Half-full-size array (this is a corner case);
	bmt = bmtCreate(0x8000000000000000ULL);
	assert(bmtSize(bmt) == 0x8000000000000000ULL);
	assert(bmtSetBranch(bmt, 0, 0) == 0);
	assert(bmtOnes(bmt) == 0x8000000000000000ULL);
	bmtDelete(bmt);

	// Size not a power of 2;
	bmt = bmtCreate(100);		/* Real size = 128 */
	assert(bmtSize(bmt) == 128);
	assert(bmtSetBranch(bmt, 0, 0) == 0);
	assert(bmtOnes(bmt) == 128);
	bmtDelete(bmt);
	bmt = bmtCreate(0x8000000000000001ULL);		/* Real size = 2^64 */
	assert(bmtSize(bmt) == 0);
	assert(bmtSetBranch(bmt, 0, 0) == 0);
	assert(bmtOnes(bmt) == UINT64_MAX);
	bmtDelete(bmt);

	// Clone and compare;
	bmt = bmtCreate(1024);
	assert(bmtSetBranch(bmt, 0, 256) == 0);
	assert(bmtSetBranch(bmt, 512, 128) == 0);
	assert(bmtReserveBit(bmt, &offset) == 0);
	bmtSetBit(bmt, 1023);
	assert(bmtOnes(bmt) == 256+128+2);
	struct BitmapTree* bmt2 = bmtClone(bmt);
	assert(bmtCompare(bmt, bmt2) == 0);
	assert(bmtReserveBit(bmt, &offset) == 0);
	assert(bmtCompare(bmt, bmt2) != 0);
	bmtDelete(bmt2);
	bmtDelete(bmt);

	printf("=== BitmapTree OK\n");
	return 0;
}

