/*
  SPDX-License-Identifier: Apache-2.0
  Copyright (c) 2022 Lars Ekman
*/

#include "bmt.h"

/*
  The tree is stored as;

    uint16_t 0bvvvv00000000E0zz
      vvvv - version (0)
      E - Endian. 0-little-endian
      zz - Bitmask size. 00 = 64-bit, 01=32-bit, 10=16-bit
    uint8_t 0bEzxxxxxx
      E - set to '1' if the tree is empty or full
      z - If the tree is empty or full; 0=empty, 1=full
      xxxxxx - ulog2(size). 0 interpreted as 64
    (nodes...)

  Nodes are stored as;

    Bitmap-node;
      0b00000000, bitmap
    Node;
      0bzzzzoooo
        zzzz - The "zero" leg.
        oooo - The "one" leg.
        Leg encoding; 0b100 - NULL, 0b101 - FULL, 0b111 - pointer

  Stored Size worst case;

    The worst case is when all possibe bit-sets (uint64_t) are used
    but none is empty or full (in which case they would be pruned).

	For a BitmapTree of size (always a power of 2) the tree 
    depth is at most;

      depth = size/64     # (64 is the number of bits in an uint64_t)

    the number of nodes will be;

	  maxnodes = depth * (depth+1) / 2   # O(size^2)

    Example; a 2^16 BitmapTree;

      depth = 2^16 / 64 = 2^10 = 1024
      nodes = 1024 * 1025 / 2 = 524800

      On top of that we have the actual bit-sets which are;

      size/8 = 8192

      An uncompressed bitarray would only use 8KB.

  Stored Size best case;

    A completely empty (or full) BitmapTree will take 3 byte for *any*
    size, including the max 2^64 (in which is 2097152 PetaByte uncompressed)

*/

#define WRITE(x) writeFn(userRef, &x, sizeof(x));

static void writeNodes(struct bmtitem* n, bmtWriteFn_t writeFn, void* userRef)
{
	uint8_t b = 0;
	if (n->level == 0) {
		// Bitmap-node
		WRITE(b);
		WRITE(n->bits);
		return;
	}
	if (n->zero == NULL)
		b = 0x40;
	else if (n->zero == FULL)
		b = 0x50;
	else
		b = 0x70;

	if (n->one == NULL)
		b += 0x04;
	else if (n->one == FULL)
		b += 0x05;
	else
		b += 0x07;

	WRITE(b);

	if (b & 0x20)
		writeNodes(n->zero, writeFn, userRef);

	if (b & 0x02)
		writeNodes(n->one, writeFn, userRef);
}

static void treeWrite(
	struct BitmapTree* bmt, bmtWriteFn_t writeFn, void* userRef)
{
	uint16_t version = 0;
	WRITE(version);
	uint8_t b = ulog2(bmt->size);
	if (bmt->top == NULL || bmt->top == FULL) {
		b |= 0x80;				/* Set the empty-bit */
		if (bmt->top == FULL)
			b |= 0x40;
		WRITE(b);
		return;
	}
	WRITE(b);
	writeNodes(bmt->top, writeFn, userRef);
}

#define READ(x) if (readFn(userRef, &x, sizeof(x)) != sizeof(x)) goto errquit

static struct bmtitem* readNodes(
	unsigned level, bmtReadFn_t readFn, void* userRef)
{
	struct bmtitem* n = CALLOC(sizeof(struct bmtitem));
	uint8_t b;

	READ(b);
	D(printf("Node byte; %02x, level=%u\n", b, level));
	n->level = level;
	
	if (b == 0) {
		// A bitmap node
		if (level > 0)
			goto errquit;
		READ(n->bits);
		return n;
	}
	
	switch (b & 0xf0) {
	case 0x40:
		break;
	case 0x50:
		n->zero = FULL;
		break;
	case 0x70:
		n->zero = readNodes(level - 1, readFn, userRef);
		break;
	default:
		goto errquit;
	}

	switch (b & 0x0f) {
	case 0x04:
		break;
	case 0x05:
		n->one = FULL;
		break;
	case 0x07:
		n->one = readNodes(level - 1, readFn, userRef);
		break;
	default:
		goto errquit;
	}

	return n;

errquit:
	free(n);
	return NULL;
}

static struct BitmapTree* treeRead(bmtReadFn_t readFn, void* userRef)
{
	struct BitmapTree* bmt = NULL;
	uint16_t version;
	uint8_t b;

	READ(version);
	if (version != 0) {
		Dx(printf("Invalid version; %u\n", version));
		return NULL;
	}

	READ(b);
	D(printf("Bmt byte; %02x\n", b));
	bmt = CALLOC(sizeof(struct BitmapTree));
	unsigned logsize = b & 0x3f;
	if (logsize > 0) {
		bmt->size = 1ULL << logsize;
		bmt->levels = logsize - BM_BITS;
	} else {
		bmt->levels = 64 - BM_BITS;
	}

	if (b & 0x80) {
		// Empty or full
		if (b & 0x40)
			bmt->top = FULL;
		return bmt;
	}

	bmt->top = readNodes(bmt->levels, readFn, userRef);
	if (bmt->top != NULL)
		return bmt;

errquit:
	bmtDelete(bmt);
	return NULL;
}


__attribute__ ((__constructor__)) static void registerMethod(void) {
	bmtSerializeMethodRegister("tree-store", treeRead, treeWrite, 1);
}
