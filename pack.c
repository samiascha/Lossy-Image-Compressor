/******************************************************************************
 * 
 *  Comp 40 Hw 4 Arith
 *
 *  Victor Gao and Sami Ascha
 *
 *  pack.c
 *
 ******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "uarray.h"
#include "bitpack.h"
#include "arith40.h"
#include "math.h"
#include "mem.h"
#include "pack.h"

/******************************************************************************
 * 
 *  Macros
 *
 ******************************************************************************/

#define BS 2


#define LSBiPr 0
#define LSBiPb 4
#define LSBd 8
#define LSBc 14
#define LSBb 20
#define LSBa 26

#define WIDTHiPr 4
#define WIDTHiPb 4
#define WIDTHd 6
#define WIDTHc 6
#define WIDTHb 6
#define WIDTHa 6

#define COEFFa  63.00
#define COEFFb 103.33
#define COEFFc 103.33
#define COEFFd 103.33


/******************************************************************************
 * 
 *  Data structures
 *
 ******************************************************************************/

/*Codeword struct containining values that will be packed into the final bitword*/

typedef struct Codeword 
{
    unsigned int a;
    signed int b, c, d;
    signed iPb, iPr;

} *Codeword;

/*Struct containined average Pb and Pr values for each block*/

typedef struct PPavg {

        float aPb, aPr;

} PPavg;


/******************************************************************************
 * 
 *  Auxiliary Function Declarations
 *
 ******************************************************************************/

/*
 * auxiliary functions for codeword compression
 */
static Codeword generate_Codeword (UArray_T block);
static uint64_t code_to_bit       (Codeword word);
static void     DCT               (UArray_T block, Codeword word);
static void     Quantize          (UArray_T block, Codeword word);
static void     check_range       (float *val);
static void     check_PbPr        (float *val); 
static PPavg    P_average         (UArray_T block);


/*
 * auxiliary functions for bitword decompression
 */
static void inverse_DCT (UArray_T block, Codeword word);
static void Dequantize(UArray_T block, Codeword word);
static Codeword bit_to_code(uint64_t bitword);
static UArray_T generate_block(Codeword word);




/******************************************************************************
 * 
 *  Function Implementations for codeword packing
 *
 ******************************************************************************/


/******************************************************************************
 * Function: packing
 * Arguments: UArray_T block
 * Return Type: uint64_t
 * Purpose: The function that brings it all together and uses all of our
 *          auxiliary functions to pack the bitwords.
 ******************************************************************************/
uint64_t packing(UArray_T block) {

    /* Codeword struct is filled */
    Codeword code = generate_Codeword(block);

    /*Bitword is created*/
    uint64_t bitword = code_to_bit(code);
    free(code); // free #1

    return bitword;

}


/******************************************************************************
 * Function: generate_Codeword
 * Arguments: UArray_T block
 * Return Type: Codeword struct
 * Purpose: Generate the codeword of a given block (held in a UArray_T).
            Discrete Cosine Transformation is performed and values are
            quantized using associated helper function. Codeword struct is 
            returned for use in packing function.
 ******************************************************************************/
static Codeword generate_Codeword (UArray_T block) {

    Codeword code = malloc(sizeof(*code)); // free #1
    DCT(block, code);   
    Quantize(block, code);

    return code;
}

/******************************************************************************
 * Function: code_to_bit
 * Arguments: Codeword struct
 * Return Type: uint64_t
 * Purpose: Bitword is created using information from Codeword struct. Bitpack
            interface is used to store the Codeword values into a 32 bit word,
            as required by the spec.
 ******************************************************************************/
static uint64_t code_to_bit (Codeword code)
{

    uint64_t word = 0;
    word = Bitpack_newu(word, WIDTHiPr, LSBiPr, code->iPr);
    word = Bitpack_newu(word, WIDTHiPb, LSBiPb, code->iPb);
    //printf("d = %d\n", code->d);
    word = Bitpack_news(word, WIDTHd, LSBd, code->d);
    //printf("c = %d\n", code->c);
    word = Bitpack_news(word, WIDTHc, LSBc, code->c);
    //printf("b = %d\n", code->b);
    word = Bitpack_news(word, WIDTHb, LSBb, code->b);
    //printf("a = %u\n", code->a);
    word = Bitpack_newu(word, WIDTHa, LSBa, code->a);

    return word;
}


/******************************************************************************
 * Function: DCT
 * Arguments: UArray_T, Codeword struct
 * Return Type: void
 * Purpose: Discrete Cosine Transformation is performed on each 2x2 block and
            relevant Codeword values are set for use in packing function.
 ******************************************************************************/
void DCT (UArray_T block, Codeword word) 
{

    /* Get Y values for DCT */
    float Y1 = ((Ypp)UArray_at(block, 0))->Y;
    float Y3 = ((Ypp)UArray_at(block, 1))->Y;
    float Y2 = ((Ypp)UArray_at(block, 2))->Y;
    float Y4 = ((Ypp)UArray_at(block, 3))->Y;


    /* DCT equations */
    float a = (Y4 + Y3 + Y2 + Y1) / 4.0;
    float b = (Y4 + Y3 - Y2 - Y1) / 4.0;
    float c = (Y4 - Y3 + Y2 - Y1) / 4.0;
    float d = (Y4 - Y3 - Y2 + Y1) / 4.0;

    /*Set codeword value (a) and quantize*/
    word->a = (unsigned)round(a * COEFFa);

    check_range(&b);
    check_range(&c);
    check_range(&d);

    /*Set codeword values (b, c, d) and quantize*/
    word->b = (signed)round(b * COEFFb);
    word->c = (signed)round(c * COEFFc);
    word->d = (signed)round(d * COEFFb);

} 




/*
 * Check the range of DCT values (b, c, d)
 * Set to -.3 if value is below and
 * .3 if value is above
 */
static void check_range(float *val)
{
    if (*val < -0.3)
        *val = -0.3;
    if (*val > 0.3)
        *val = 0.3;
    return;
}

/*
 * Check the range of values (Pb, Pr)
 * Set to -.5 if value is below and
 * .5 if value is above
 */
static void check_PbPr(float *val) 
{
    if (*val < -0.5)
        *val = -0.5;
    if (*val > 0.5)
        *val = 0.5;
    return;
}



/******************************************************************************
 * Function: P_average
 * Arguments: UArray_T
 * Return Type: PPavg struct
 * Purpose: Calculate average Pb and Pr values for a given block (contained in
            UArray_T)
 ******************************************************************************/
PPavg P_average (UArray_T block)
{
    float sum_pb = 0, sum_pr = 0;
    for (int i = 0; i < 4; i++)
    {
        sum_pb += ((Ypp)UArray_at(block, i))->Pb;
        sum_pr += ((Ypp)UArray_at(block, i))->Pr;
    }

    PPavg temp = {.aPb = sum_pb / 4.0, .aPr = sum_pr / 4.0};
    return temp;
}



/******************************************************************************
 * Function: Quantize
 * Arguments: UArray_T, Codeword struct
 * Return Type: void
 * Purpose: Quantize Pb and Pr average values to get index of chroma using
            helper function provided in arith40.h
 ******************************************************************************/
void Quantize (UArray_T block, Codeword word)
{
    PPavg pp = P_average(block);

    /* Quantization */
    check_PbPr(&(pp.aPb));
    check_PbPr(&(pp.aPr));

    /* Set codeword values (iPb, iPr) using arith40.h helper function*/
    word->iPb = Arith40_index_of_chroma(pp.aPb);
    word->iPr = Arith40_index_of_chroma(pp.aPr);

}



/******************************************************************************
 * 
 *  Function Implementations for bitword unpacking
 *
 ******************************************************************************/

/******************************************************************************
 * Function: packing
 * Arguments: uint64_t
 * Return Type: UArray_T
 * Purpose: Function to unpack the bitwords using auxiliary functions below.
 ******************************************************************************/
UArray_T unpacking(uint64_t bitword) 
{


    /*Read bitwords and save them into Codeword struct*/
    Codeword code = bit_to_code(bitword);

    /*Generate a block using Codeword structs*/ 
    UArray_T block = generate_block(code);

    free(code); 
    return block;

} 


/******************************************************************************
 * Function: bit_to_code
 * Arguments: uint64_t
 * Return Type: Codeword struct
 * Purpose: Convert bitword to Codeword.
            Codeword struct is allocated, filled with relevant values using
            Bitpack_get functions to retrieve them. 
 ******************************************************************************/
Codeword bit_to_code(uint64_t bitword) 
{

    Codeword code = malloc(sizeof(*code)); 

    code->a = Bitpack_getu(bitword, WIDTHa, LSBa);
    code->b = Bitpack_gets(bitword, WIDTHb, LSBb);
    code->c = Bitpack_gets(bitword, WIDTHc, LSBc);
    code->d = Bitpack_gets(bitword, WIDTHd, LSBd);

    code->iPr = Bitpack_getu(bitword, WIDTHiPr, LSBiPr);
    code->iPb = Bitpack_getu(bitword, WIDTHiPb, LSBiPb);

    return code;
} 



/******************************************************************************
 * Function: generate_block
 * Arguments: Codeword struct
 * Return Type: UArray_T
 * Purpose: Generate a block to be contained in a UArray_T. Inverse DCT and
            quantize is performed on block and associated Codeword.
 ******************************************************************************/

UArray_T generate_block(Codeword code) {


    UArray_T block = UArray_new(4, sizeof(struct Ypp));

    inverse_DCT(block, code);
    Dequantize(block, code);

    return block;
} 



/******************************************************************************
 * Function: inverse_DCT
 * Arguments: UArray_T, Codeword struct
 * Return Type: void
 * Purpose: Inverse Discrete Cosine Transformation is performed on each block.
            Component Video values (in YPP struct) are set accordingly
 ******************************************************************************/
void inverse_DCT(UArray_T block, Codeword code)
{
    
    /*dequantize a,b,c, and d*/
    float a = (float)code->a / COEFFa;
    float b = (float)code->b / COEFFb;
    float c = (float)code->c / COEFFc;
    float d = (float)code->d / COEFFd;


    /*inverse DCT equations*/
    float Y1 = a - b - c + d;
    float Y2 = a - b + c - d;
    float Y3 = a + b - c - d;
    float Y4 = a + b + c + d;


    /* set values in blocks */
    ((Ypp)UArray_at(block, 0))->Y = Y1;
    ((Ypp)UArray_at(block, 1))->Y = Y3;
    ((Ypp)UArray_at(block, 2))->Y = Y2;
    ((Ypp)UArray_at(block, 3))->Y = Y4;

} 

/******************************************************************************
 * Function: Dequantize
 * Arguments: UArray_T, Codeword struct
 * Return Type: void
 * Purpose: Deuantize Pb and Pr average values to get chroma of index using
            helper function provided in arith40.h. Update Pb and Pr values
            for each block.
 ******************************************************************************/

void Dequantize(UArray_T block, Codeword code)
{
    float avg_Pb = Arith40_chroma_of_index(code->iPb);
    float avg_Pr = Arith40_chroma_of_index(code->iPr);
    
    for (int i = 0; i < 4; i ++)
    {
        ((Ypp)UArray_at(block, i))->Pb = avg_Pb;
        ((Ypp)UArray_at(block, i))->Pr = avg_Pr;
    }
} 
