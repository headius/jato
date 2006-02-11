#ifndef __JIT_BASIC_BLOCK_H
#define __JIT_BASIC_BLOCK_H

struct statement;

struct basic_block {
	unsigned long start;
	unsigned long end;
	struct statement *stmt;
};

#define BASIC_BLOCK_INIT(_start, _end) \
	{ .start = _start, .end = _end }

struct basic_block *alloc_basic_block(unsigned long, unsigned long);
void free_basic_block(struct basic_block *);
struct basic_block *bb_split(struct basic_block *, unsigned long);

#endif
