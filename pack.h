/******************************************************************************
 * 
 *  Comp 40 Hw 4 Arith
 *
 *  Victor Gao and Sami Ascha
 *
 *  compress.h
 *
 ******************************************************************************/
#ifndef PACK_INCLUDED
#define PACK_INCLUDED

#include <stdio.h>
#include <stdint.h>
#include "uarray.h"

/******************************************************************************
 * 
 *  Data structures
 *
 ******************************************************************************/

/* component video */
typedef struct Ypp 
{
        float Y, Pb, Pr;
} *Ypp;


/******************************************************************************
 * 
 *  Function declarations
 *
 ******************************************************************************/

/*
 * packing: 
 * convert 2 X 2 block (stored in a 1D array) into a bitword     
 */
extern uint64_t packing(UArray_T block);

/*
 * unpacking: 
 * convert a bitword into a 2 X 2 block (stored in a 1D array)   
 */
extern UArray_T unpacking(uint64_t bitword);

#endif
