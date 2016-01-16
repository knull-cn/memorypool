
#pragma once

#include <stdint.h>

#define ROUND8(msize) ((msize)+7)&0xfffffff8

//直接的内存分配/释放;
#ifdef _TCMALLOC_

#include <google/tcmalloc.h>
#define qmalloc tc_malloc
#define qfree   tc_free

#elif defined _JEMALLOC_
//jemalloc似乎不太稳定,在释放的时候会碰上core.
#include <jemalloc/jemalloc.h>
#define qmalloc je_malloc
#define qfree   je_free

#else//glibc

#include <cstdlib>
#define qmalloc malloc
#define qfree   free

#endif
