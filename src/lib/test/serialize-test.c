/*
  SPDX-License-Identifier: Apache-2.0
  Copyright (c) 2022 Lars Ekman
*/

#include "bmt.h"
#include <assert.h>
#include <time.h>
#include <string.h>

static void ipv4ipamSim(int count);

static void countBytes(void* ref, void const* data, size_t len)
{
	uint64_t* cnt = ref;
	cnt += len;
}

#define WBCHUNK 4096
struct writeBuffDescriptor {
	unsigned allocated;
	void* data;
	unsigned cursor;
};
static struct writeBuffDescriptor* buffOpenWrite(void)
{
	struct writeBuffDescriptor* d = CALLOC(sizeof(struct writeBuffDescriptor));
	d->data = CALLOC(WBCHUNK);
	d->allocated = WBCHUNK;
	return d;
}
static void buffOpenRead(struct writeBuffDescriptor* d)
{
	d->allocated = d->cursor;
	d->cursor = 0;
}
static void buffClose(struct writeBuffDescriptor* d)
{
	if (d != NULL) {
		free(d->data);
		free(d);
	}
}
static void buffWrite(void* ref, void const* data, size_t len)
{
	struct writeBuffDescriptor* d = ref;
	if ((d->cursor + len) > d->allocated) {
		d->allocated += (len + WBCHUNK);
		d->data = realloc(d->data, d->allocated);
	}
	memcpy(d->data + d->cursor, data, len);
	d->cursor += len;
}
static size_t buffRead(void* ref, void* data, size_t len)
{
	struct writeBuffDescriptor* d = ref;
	D(printf(
		   "buffRead; len=%lu, cursor=%u, allocated=%u\n",
		   len, d->cursor, d->allocated));
	if ((d->cursor + len) > d->allocated)
		return -1;
	memcpy(data, d->data + d->cursor, len);
	d->cursor += len;
	return len;
}


int main(int argc, char* argv[])
{
	if (argc > 1) {
		ipv4ipamSim(24);
		return 0;
	}

	struct BitmapTree* bmt;
	uint64_t byteCount;

	// Useful debug printouts;
	D(bmtPrint(bmt);printf("---\n"));
	D(printf("offset=%lu\n", offset));
	D(printf("bmtNodes()=%lu\n", bmtNodes(bmt)));
	D(printf("bmtOnes()=%lu\n", bmtOnes(bmt)));
	D(printf("bmtAllocated()=%lu\n", bmtAllocated(bmt)));

	// Basic write;
	bmt = bmtCreate(1024);
	assert(bmtSetBranch(bmt, 512, 128) == 0);
	byteCount = 0;
	bmtWrite(bmt, countBytes, &byteCount);
	bmtSetBit(bmt, 4);
	D(printf("Write bytes; %lu\n", byteCount));
	byteCount = 0;
	bmtWrite(bmt, countBytes, &byteCount);
	bmtDelete(bmt);

	// Special case; write a full 2^64 bitarray
	bmt = bmtCreate(0);
	byteCount = 0;
	bmtWrite(bmt, countBytes, &byteCount);
	D(printf("Write bytes; %lu\n", byteCount));
	bmtDelete(bmt);

	// Write/read-back;
	struct BitmapTree* bmt2 = NULL;
	struct writeBuffDescriptor* d;
	bmt = bmtCreate(0);
	// Empty tree
	d = buffOpenWrite();
	bmtWrite(bmt, buffWrite, d);
	buffOpenRead(d);
	bmt2 = bmtRead(buffRead, d);
	assert(bmt2 != NULL);
	buffClose(d);
	assert(bmtCompare(bmt, bmt2) == 0);
	bmtDelete(bmt2);
	// Full tree
	bmtSetBranch(bmt, 0, 0);
	d = buffOpenWrite();
	bmtWrite(bmt, buffWrite, d);
	buffOpenRead(d);
	bmt2 = bmtRead(buffRead, d);
	assert(bmt2 != NULL);
	buffClose(d);
	assert(bmtCompare(bmt, bmt2) == 0);
	bmtDelete(bmt2);
	// Half-full table
	bmtClearBranch(bmt, 0, 0x8000000000000000UL);
	d = buffOpenWrite();
	bmtWrite(bmt, buffWrite, d);
	buffOpenRead(d);
	bmt2 = bmtRead(buffRead, d);
	assert(bmt2 != NULL);
	buffClose(d);
	D(bmtPrint(bmt2);printf("---\n"));
	assert(bmtCompare(bmt, bmt2) == 0);
	bmtDelete(bmt2);
	// Go for it!
	bmtSetBit(bmt, 0);
	d = buffOpenWrite();
	bmtWrite(bmt, buffWrite, d);
	D(printf("writeBuff; cursor=%u\n", d->cursor));
	buffOpenRead(d);
	bmt2 = bmtRead(buffRead, d);
	assert(bmt2 != NULL);
	buffClose(d);
	assert(bmtCompare(bmt, bmt2) == 0);
	bmtDelete(bmt2);
	
	bmtDelete(bmt);

	printf("=== serialize OK\n");
	return 0;
}

/*
  Make 
 */
static void ipv4ipamSim(int count)
{
	srand(time(NULL));
	uint64_t sum = 0;
	struct BitmapTree* bmt;
	for (int i = 0; i < count; i++) {
		bmt = bmtCreate(0x100000000ULL);
		bmtSetBranch(bmt, 0, 0x1000ULL);
		for (int x = 0; x < 8000; x++) {
			bmtClearBit(bmt, rand() & 0xffff);
		}
		uint64_t byteCount = 0;
		bmtWrite(bmt, countBytes, &byteCount);
		D(printf("bmtAllocated()=%lu\n", bmtAllocated(bmt)));
		D(printf("Write bytes; %lu\n", byteCount));
		sum += byteCount;
		bmtDelete(bmt);
	}
	Dx(printf("Average write; %lu\n", sum / count));
}
