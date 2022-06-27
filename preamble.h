#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>


typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef float  f32;
typedef double f64;

typedef u8   byte_t;
typedef char utf8_t;  /* Unicode byte        */
//typedef u32  rune_t;  /* Unicode code point  */

typedef int_fast32_t word_t;


#ifdef DEBUG
#define DEBUG_BLOCK(x) x
#else
#define DEBUG_BLOCK(x)
#endif

#define ASSERT(x)       do { if (!(x)) { fprintf(stderr, "%s:%d [Assert '%s']: '%s'",   __FILE__, __LINE__, __FUNCTION__, #x);                               fflush(stderr); raise(SIGABRT); }} while(0)
#define ASSERTF(x, ...) do { if (!(x)) { fprintf(stderr, "%s:%d [Assert '%s']: '%s': ", __FILE__, __LINE__, __FUNCTION__, #x); fprintf(stderr, __VA_ARGS__); fflush(stderr); raise(SIGABRT); }} while(0)
