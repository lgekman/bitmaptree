#pragma once
/*
  SPDX-License-Identifier: Apache-2.0
  Copyright (c) 2022 Lars Ekman
*/

#include <stdint.h>
#include <stddef.h>

struct BitmapTree;

/*
  bmtCreate - Create an empty (all '0') BitmapTree of the desired
  size.  The size will be rounded up to the nearest power of 2 if
  needed. A size of '0' will be interpreted as 2^64 (full size bit
  array).

  The initial memory size for *any* sized BitmapTree is 24 byte.
  return: BitmapTree
 */
struct BitmapTree* bmtCreate(uint64_t size);
struct BitmapTree* bmtClone(struct BitmapTree* bmt);
void bmtDelete(struct BitmapTree* bmt);

// bmtSetBit - set a bit to '1'
void bmtSetBit(struct BitmapTree* bmt, uint64_t offset);

// bmtClearBit - set a bit to '0'
void bmtClearBit(struct BitmapTree* bmt, uint64_t offset);

// bmtReserve - Find the first '0' bit and reserve it by setting it to '1'.
// return; 0 - Bit reserved at 'offset'. != 0 - No free bit found.
int bmtReserveBit(struct BitmapTree* bmt, uint64_t* offset);

// bmtBit - Get the value of a bit. Invalid offset returns '0'.
int bmtBit(struct BitmapTree* bmt, uint64_t offset);

// bmtSetBranch - Set all bits on a "branch" to '1'.
// This is a function unique to BitmapTree. The 'size' must be a power
// of 2 the 'offset' an even multiple of 'size'.
// Size==0 will be translated to size=array-size.
// return: 0 - OK, != 0 - invalid params
int bmtSetBranch(struct BitmapTree* bmt, uint64_t offset, uint64_t size);

// bmtClearBranch - Same as bmtSetBranch() but set a "branch" to '0'.
int bmtClearBranch(struct BitmapTree* bmt, uint64_t offset, uint64_t size);


// ----------------------------------------------------------------------
// Serialize;

typedef void (*bmtWriteFn_t)(void* userRef, void const* data, size_t len);
typedef size_t (*bmtReadFn_t)(void* userRef, void* data, size_t len);


// bmtWrite - Write the bmt
typedef void (*bmtWrite_t)(
	struct BitmapTree* bmt, bmtWriteFn_t writeFn, void* userRef);
extern bmtWrite_t bmtWrite;

// bmtRead - Read a bmt. return NULL on failure
typedef struct BitmapTree* (*bmtRead_t)(bmtReadFn_t readFn, void* userRef);
extern bmtRead_t bmtRead;

// bmtSerializeAlgorithm - Set the method for serialization. Available;
// "tree-store" - Store the tree in the customary fashion (default)
// return; 0 - OK, != 0 - failed
int bmtSerializeMethod(char const* method);

// bmtSerializeMethodRegister - Register a serialization method
// The 'method' pointer is stored so it must be permanent.
// return; 0 - OK, != 0 - failed
int bmtSerializeMethodRegister(
	char const* name, bmtRead_t readFn, bmtWrite_t writeFn, int set);


// ----------------------------------------------------------------------
// Stats;

// bmtSize - return the bitarray size.
uint64_t bmtSize(struct BitmapTree* bmt);

// bmtCompare - return zero if the bmt's are equal
int bmtCompare(struct BitmapTree* bmt1, struct BitmapTree* bmt2);

// bmtOnes - return the number of 'ones' in the bitmap.
uint64_t bmtOnes(struct BitmapTree* bmt);

// bmtNodes - return the number of nodes in the tree.
uint64_t bmtNodes(struct BitmapTree* bmt);

// bmtAllocated - return number of allocated bytes
uint64_t bmtAllocated(struct BitmapTree* bmt);

// bmtPrint - Prints the BitmapTree to stdout
void bmtPrint(struct BitmapTree* bmt);
