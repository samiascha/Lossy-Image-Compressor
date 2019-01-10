/******************************************************************************
 * 
 *  Comp 40 Hw 4 Arith
 *
 *  Victor Gao and Sami Ascha
 *
 *  bitpack.c
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include "except.h"
#include "bitpack.h"

/******************************************************************************
 * 
 *  Auxiliary Function Declarations
 *
 ******************************************************************************/

static uint64_t left_shift      (uint64_t num, unsigned width);
static uint64_t logical_right   (uint64_t num, unsigned width);
static  int64_t arithmetic_right( int64_t num, unsigned width);
static uint64_t field_update    (uint64_t word, unsigned width, 
                                            unsigned lsb, int64_t value); 

/*
 * exceptions
 */
Except_T Bitpack_Overflow = {"Overflow packing bits"};


/******************************************************************************
 * 
 *  Public Function Implementations 
 *
 ******************************************************************************/
/*
 * width-test functions
 */

extern bool Bitpack_fitsu(uint64_t n, unsigned width)
{
    assert(width <= 64);

    uint64_t upper = ~0;
    upper = left_shift(upper, width);
    upper = ~upper;

    return n <= upper;
}

extern bool Bitpack_fitss(int64_t n, unsigned width)
{
    assert(width <= 64);

    int64_t lower = (int64_t)left_shift(~0, width - 1);
    int64_t upper = ~lower;

    return lower <= n && n <= upper;
}

/*
 * field-ertaxction functions
 */

extern uint64_t Bitpack_getu(uint64_t word, unsigned width, unsigned lsb)
{
    assert(width <= 64);
    assert(width + lsb <= 64);

    uint64_t mask = ~0;
    mask = left_shift(mask, 64 - width);
    mask = logical_right(mask, 64 - width - lsb);

    word = word & mask;
    word = logical_right(word, lsb);

    return word;
}

extern int64_t Bitpack_gets(uint64_t word, unsigned width, unsigned lsb)
{
    assert(width <= 64);
    assert(width + lsb <= 64);

    uint64_t mask = ~0;
    mask = left_shift(mask, 64 - width);
    mask = logical_right(mask, 64 - width - lsb);

    word = word & mask;
    word = left_shift(word, 64 - width - lsb);
    word = arithmetic_right(word, 64 - width);

    return (int64_t)word;
}

/*
 * field-update functions
 */
extern uint64_t Bitpack_newu(uint64_t word, unsigned width, 
                             unsigned lsb, uint64_t value)
{
    assert(width <= 64);
    assert(width + lsb <= 64);

    if (Bitpack_fitsu(value, width) == false)
        RAISE(Bitpack_Overflow);

    return field_update(word, width, lsb, (int64_t)value);
}

extern uint64_t Bitpack_news(uint64_t word, unsigned width, 
                             unsigned lsb, int64_t value)
{
    assert(width <= 64);
    assert(width + lsb <= 64);

    if (Bitpack_fitss(value, width) == false)
        RAISE(Bitpack_Overflow);

    return field_update(word, width, lsb, value);
}

/******************************************************************************
 * 
 *  Auxiliary Function Implementations 
 *
 ******************************************************************************/

/* 
 * left shift 
 */
static uint64_t left_shift(uint64_t num, unsigned width)
{
    assert(width <= 64);
    if (width < 64)
        num <<= width;
    else 
        num = 0;
    return num;
}

/*
 * logical right shift : zero-fill right shift
 */
static uint64_t logical_right(uint64_t num, unsigned width)
{
    assert(width <= 64);
    if (width < 64)
        num >>= width;
    else 
        num = 0;
    return num; 
}

/*
 * arithmetic right shift : sticky right-shift
 */
static int64_t arithmetic_right(int64_t num, unsigned width)
{
    assert(width <= 64);
    if (width < 64)
        num >>= width;
    else if (num >= 0)
        num = 0;
    else
        num = ~0;

    return num;
}

/* field_update helper function */
static uint64_t field_update(uint64_t word, unsigned width, 
                         unsigned lsb, int64_t value)
{
    uint64_t mask = ~0;
    mask = left_shift(mask, 64 - width);
    mask = logical_right(mask, 64 - width - lsb);

    value = left_shift(value, lsb);
    value &= mask;
    word &= ~mask;
    word |= value; 

    return word;
}
