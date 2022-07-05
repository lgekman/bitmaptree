#pragma once
/*
  SPDX-License-Identifier: Apache-2.0
  Copyright (c) 2022 Lars Ekman
*/

// Internal interface for BitmapTree

#include <bitmaptree.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#define Dx(x) x
#define D(x)
#define CALLOC(x) callocOrDie(x)
#define FULL (void*)1

typedef uint64_t bitmap_t;
#define BM_BITS 6
#define BM_MASK 0x3fUL
#define BM_MAX UINT64_MAX

struct bmtitem {
	unsigned level;
	union {
		struct {
			struct bmtitem* zero;
			struct bmtitem* one;
		};
		bitmap_t bits;
	};
};

struct BitmapTree {
	uint64_t size;
	unsigned levels;
	struct bmtitem* top;
};

static inline void die(char const* fmt, ...)__attribute__ ((__noreturn__));
static inline void die(char const* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	exit(EXIT_FAILURE);
}
static inline void* callocOrDie(size_t size)
{
	void* mem = calloc(1, size);
	if (mem == NULL)
		die("Out of mem");
	return mem;
}

static inline struct bmtitem* expandItem(unsigned level, void* value)
{
	struct bmtitem* n = CALLOC(sizeof(struct bmtitem));
	n->level = level;
	if (level > 0)
		n->zero = n->one = value;
	else if (value == FULL)
		n->bits = UINT64_MAX;
	return n;
}

// Rounded up, so ulog2(7) == 3 and ulog2(UINT_MAX) == 64
static inline unsigned ulog2(uint64_t x)
{
	uint64_t m = 1;
	unsigned l = 0;
	while (x > m) {
		if (m == 0x8000000000000000ULL)
			return 64;
		m = m << 1;
		l++;
	}
	return l;
}
