/*****************************************************************************
 * 
 *  Comp 40 Hw 4 Arith
 *
 *  Victor Gao and Sami Ascha
 *
 *  compress40.c
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "assert.h"
#include "a2methods.h"
#include "a2plain.h"
#include "a2blocked.h"
#include "pnm.h"
#include "except.h"
#include "uarray.h"
#include "bitpack.h"
#include "uarray2.h"
#include "pack.h"
#include "compress40.h"

/******************************************************************************
 * 
 *  Macros
 *
 ******************************************************************************/

#define BS 2
#define A2 A2Methods_UArray2
#define DENOM 255.00

/******************************************************************************
 * 
 *  Data Structures
 *
 ******************************************************************************/

/* struct to hold info from compressed file for use in reading function */
typedef struct Compression_info
{
    unsigned width, height;
    UArray_T bitArray;

} Compression_info;

/* Closure for use in apply function "pack_Bitwords" */
typedef struct closure_pack_Bitwords
{
    int height;
    int width;
    UArray_T bitArray;
    UArray_T block;
} closure_pack_Bitwords;

/* Closure for use in apply function "applyBitword" */
typedef struct closure_decompress
{
    UArray_T bitArray;
    unsigned height;
} closure_decompress;

/* struct for use in apply function "RGBConversion" */
typedef struct RGBConversionStruct {

    A2Methods_T methods;
    A2 pixels;

} RGBConversionStruct;

/******************************************************************************
 *
 *  Auxiliary functions for compression
 *
 ******************************************************************************/
static A2 RGB_to_YPP (Pnm_ppm image, A2Methods_T methods);
static UArray_T YPP_to_bitwords (A2 array2_YPP, A2Methods_T methods);
static UArray_T pixelBlocks_to_bitwords (Pnm_ppm image, A2Methods_T methods);
static UArray_T Col_to_row(UArray_T bitwords, unsigned w, unsigned h);
static void trim(unsigned *width, unsigned *height);
static void YPPConversion(int col, int row, A2 pixels, void* elem, void *cl);
static void pack_Bitwords(int col, int row, A2 pixels, void* elem, void *cl);
static void compression_output(UArray_T bitArray, Pnm_ppm image);


/******************************************************************************
 *
 *  Auxiliary functions and data structures for decompression
 *
 ******************************************************************************/
static Compression_info read_compressed_file(FILE *input); 
static UArray_T Row_to_col(UArray_T bitwords, unsigned w, unsigned h);  
static Pnm_ppm create_pixmap (unsigned w, unsigned h, A2Methods_T methods);
static A2 bitwords_to_YPP (Compression_info info, A2Methods_T methods);
static A2 YPP_to_RGB (Pnm_ppm image, A2Methods_T methods, A2 YPP_array);
static A2 bitwords_to_RGB (Compression_info info, 
                           A2Methods_T methods, Pnm_ppm pixmap);                      
static void check_rgb (float *num);
static void RGBConversion(int col, int row, A2 pixels, void *elem, void *cl);
static void applyBitword (int col, int row, A2 pixels, void *elem, void *cl);

/******************************************************************************
 *
 * Function implementations for compression
 *
 ******************************************************************************/

/******************************************************************************
 * Function: compress40
 * Arguments: FILE*
 * Return Type: void
 * Purpose: The function that brings it all together and uses all of our
 *          auxiliary functions to perform the compression.
 ******************************************************************************/

extern void compress40(FILE *input)
{

  /* Set methods */
  A2Methods_T methods = uarray2_methods_blocked;

  /* Read file into pnm_ppm */
  Pnm_ppm image = Pnm_ppmread(input, methods);
 
  /* Raise exception if corrupt file */
  if (image == NULL)
    RAISE(Pnm_Badformat);
  
  /* The line that essentially does it all. Convert from RGB to CV,
     perform DCT on 2x2 blocks and pack them into bitwords which are
     then stored into UArray_T for use in output function */
  UArray_T bitArray = pixelBlocks_to_bitwords(image, methods);

  /* Write the bitwords to disk according to the specs guidelines*/
  compression_output(bitArray, image);

  /* Free allocated memory */
  UArray_free(&bitArray);
  Pnm_ppmfree(&image);
}


/******************************************************************************
 * Function: pixelBlocks_to_bitwords
 * Arguments: Pnm_ppm, A2Methods_T
 * Return Type: UArray_T
 * Purpose: Convert 2x2 RGB blocks to bitwords and store them in single UArray
 *          by calling helper functions RGB_to_YPP (which converts to
 *          Component Video) and YPP_to_bitwords (which packs the blocks
 *          into bitwords)
 ******************************************************************************/

static UArray_T pixelBlocks_to_bitwords (Pnm_ppm image, A2Methods_T methods)
{
    /* First we convert RGB to Component Video*/
    A2 YppArray = RGB_to_YPP(image, methods);

    /*Then we pack each CV block into array of bitwords*/
    UArray_T bitArray = YPP_to_bitwords(YppArray, methods);

    methods->free(&YppArray);
    
    return bitArray;
}

/******************************************************************************
 * Function: compression_output
 * Arguments: UArray_T (of bitwords), Pnm_ppm
 * Return Type: void
 * Purpose: Write each 32-bit code word to disk in big-endian order using
 *          putchar() function. Code words are written in row-major order.
 *          Header with width and height is printed at the beginning.
 ******************************************************************************/

static void compression_output (UArray_T uarray_bitwords, Pnm_ppm image)
{
    unsigned width = image->width;
    unsigned height = image->height;
    trim(&width, &height);

    UArray_T uarray_bitwords_row = Col_to_row(uarray_bitwords, width, height);

    printf("COMP40 Compressed image format 2\n%u %u", width, height);
    printf("\n");

    for (int i = 0; i < UArray_length(uarray_bitwords_row); i++)
    {
        uint64_t bitword = *(uint64_t *)UArray_at(uarray_bitwords_row, i); 

        for (int j = 3; j >=0; j--)
        {
            char byte = (char)Bitpack_getu(bitword, 8, j * 8);
            putchar(byte);
        }       
    }

    UArray_free(&uarray_bitwords_row);
} 

/******************************************************************************
 * Function: trim
 * Arguments: two unsigned values
 * Return Type: void
 * Purpose: trim odd widths and heights to even values so that 2x2 block
            traversal can be maintained
 ******************************************************************************/

static void trim(unsigned *width, unsigned *height) 
{
    if (*width % 2 != 0)
        *width -= 1;

    if (*height % 2 != 0)
        *height -= 1;
}


/******************************************************************************
 * Function: RGB_to_YPP
 * Arguments: Pnm_ppm, A2Methods_T
 * Return Type: A2Methods_UArray2
 * Purpose: convert RGB values to Component Video using apply function
 *          "YPPConversion". Height and width are trimmed and new 2D array
 *          is created to hold CV values. Called in pixelBlocks_to_bitwords 
 *          when making full conversion from RGB to packed bitwords
 ******************************************************************************/

static A2 RGB_to_YPP (Pnm_ppm image, A2Methods_T methods) 
{
    unsigned w = image->width;
    unsigned h = image->height;

    trim(&w, &h);

    A2 array2_YPP = methods->new_with_blocksize(w, h, sizeof(struct Ypp), BS);

    methods->map_block_major(array2_YPP, YPPConversion, image);

    return array2_YPP;
}

/******************************************************************************
 * Function: YPPConversion
 * Arguments: ints col and row, A2Methods_UArray2, void* elem and void* closure
 * Return Type: void
 * Purpose: apply function for "RGB_to_YPP", converts and scales RGB values to
 *          floats and then converts them to Component Video using YPP struct
 ******************************************************************************/

static void YPPConversion(int col, int row, A2 pixels, void* elem, void *cl) 
{

    (void) pixels;
    Pnm_ppm image = (Pnm_ppm)cl;

    Pnm_rgb rgb = (Pnm_rgb)image->methods->at(image->pixels, col, row);

    float d = (float) image->denominator;
    float r = (float) rgb->red   / d;
    float g = (float) rgb->green / d;
    float b = (float) rgb->blue  / d;

    Ypp tempYpp = malloc(sizeof(struct Ypp));


    /*RGB to Component Video conversion, performed on each element*/
    tempYpp->Y  =     0.299 * r +    0.587 * g + 0.114    * b;
    tempYpp->Pb = -0.168736 * r - 0.331264 * g + 0.5      * b;
    tempYpp->Pr =       0.5 * r - 0.418688 * g - 0.081312 * b;

    *((Ypp)elem) = *tempYpp;

    free(tempYpp);
}

/******************************************************************************
 * Function: YPP_to_bitwords
 * Arguments: A2Methods_UArray2, A2Methods_T
 * Return type: UArray_T
 * Purpose: converts Component Video values in 2D Array to bitwords using
 *      apply function "pack_Bitwords". Returns UArray containing
 *      bitwords. Called in pixelBlocks_to_bitwords when making full conversion
 *      from RGB to packed bitwords
 ******************************************************************************/

static UArray_T YPP_to_bitwords (A2 YppArray, A2Methods_T methods) {


    unsigned w = methods->width(YppArray);
    unsigned h = methods->height(YppArray);
    unsigned numBlocks = (w / BS) * (h / BS);

    UArray_T bitArray = UArray_new(numBlocks, sizeof(uint64_t));
    UArray_T block = UArray_new(BS * BS, sizeof(struct Ypp));

    closure_pack_Bitwords cl = {.height = h,
                                .width = w,
                                .bitArray = bitArray,
                                .block = block};

    methods->map_block_major(YppArray, pack_Bitwords, &cl);

    UArray_free(&block);

    return bitArray;
}

/******************************************************************************
 * Function: pack_Bitwords
 * Arguments: ints col and row, A2Methods_UArray2, void* elem and void* closure
 * Return Type: void
 * Purpose: apply function for "YPP_tobitwords", converts YPP values to
            bitwords using packing() function
 ******************************************************************************/
static void pack_Bitwords(int col, int row, A2 pixels, void* elem, void *cl) 
{
    (void) pixels;
    struct closure_pack_Bitwords curr = *(struct closure_pack_Bitwords *)cl;
    UArray_T block = curr.block;

    int h = curr.height;
    int w = curr.width;
    (void) w;   
    int i = col, j = row;

    /* number of elements we traversed */
    int k = j % BS + (i % BS) * BS + (j / BS) * BS * BS + (i / BS) * BS * h;
    /* block index */
    int num_b = k / 4; 
    
    *(Ypp)UArray_at(block, k % 4) = *(Ypp)elem;

    if (k % 4 == 3)
    {
        /* perform DCT and pack each block into a bitword using packing fxn */
        uint64_t bitword = packing(block);

        *(uint64_t *)UArray_at(curr.bitArray, num_b) =  bitword;    
    }
}

/******************************************************************************
 *
 * Function implementations for Decompression
 *
 ******************************************************************************/

/******************************************************************************
 * Function: decompress40
 * Arguments: FILE*
 * Return Type: void
 * Purpose: The function that brings it all together and uses all of our
 *          auxiliary functions to perform the decompression.
 ******************************************************************************/

extern void decompress40 (FILE *input)
{
    /* set methods */
    A2Methods_T methods = uarray2_methods_blocked;

    /* read information from compressed file */
    Compression_info info = read_compressed_file(input);

    /* create Pnm_ppm instance for use in output */
    Pnm_ppm pixmap = create_pixmap(info.width, info.height, methods);

    /* Decompress the bitwords ead from input into RGB pixels */
    pixmap->pixels = bitwords_to_RGB(info, methods, pixmap);

    /* write to disk */
    Pnm_ppmwrite(stdout, pixmap);

    methods->free(&pixmap->pixels);

    free(pixmap);
}

/******************************************************************************
 * Function: bitwords_to_RGB
 * Arguments: Compression_info, A2Methods_T, Pnm_ppm 
 * Return Type: A2
 * Purpose: This function decompressed the bitwords read from input file
            into a 2D array of RGB pixels.
 ******************************************************************************/

static A2 bitwords_to_RGB (Compression_info info, 
                           A2Methods_T methods, Pnm_ppm pixmap)
{
    A2 YPP_array = bitwords_to_YPP(info, methods);
    A2 RGB_pixels = YPP_to_RGB(pixmap, methods, YPP_array);

    methods->free(&YPP_array);
    return RGB_pixels;
}

/******************************************************************************
 * Function: read_compressed_file
 * Arguments: FILE*
 * Return Type: struct Compression_info
 * Purpose: read the compressed file and store info (width, height, bitwords)
            into struct
 ******************************************************************************/

static Compression_info read_compressed_file(FILE *input) 
{
    unsigned height, width;
    int read = fscanf(input, "COMP40 Compressed image format 2\n%u %u", 
                                                            &width, &height);

    assert(read == 2);
    int c = getc(input);
    assert(c == '\n');

    int ulength = (width*height)/4;

    UArray_T tempBit = UArray_new(ulength, sizeof(uint64_t));

    for (int i = 0; i < ulength; i++)
    {
        uint64_t word = 0;
        for (int j = 3; j >= 0; j--)
        {
            uint64_t byte = getc(input);
            assert(!feof(input));
            word = Bitpack_newu(word, 8, 8 * j, byte);
        }   
        *(uint64_t *)UArray_at(tempBit, i) = word;

    }

    UArray_T bitArray = Row_to_col(tempBit, width, height);

    UArray_free(&tempBit);

    Compression_info curr = {.width = width, 
                             .height = height, 
                             .bitArray = bitArray};

    return curr;

}

/******************************************************************************
 * Function: create_pixmap
 * Arguments: two unsigned values, A2Methods_T
 * Return Type: Pnm_ppm
 * Purpose: Populate Pnm_ppm struct to be used in final image output 
 ******************************************************************************/

static Pnm_ppm create_pixmap (unsigned w, unsigned h, A2Methods_T methods) 
{
    Pnm_ppm pixmap = malloc(sizeof(*pixmap));
    pixmap->width = w;
    pixmap->height = h;
    pixmap->denominator = DENOM;
    pixmap->methods = methods;
    pixmap->pixels = NULL;
    
    return pixmap;
}

/******************************************************************************
 * Function: applyBitword
 * Arguments: ints col and row, A2Methods_UArray2, void* elem and void* closure
 * Return Type: void
 * Purpose: apply function for "bitwords_to_YPP", unpacks bitwords and populates
            each element in 2D array with CV values
 ******************************************************************************/
 
static void applyBitword (int col, int row, A2 pixels, void *elem, void *cl)
{
    (void) pixels;
    closure_decompress curr = *(closure_decompress *)cl;

    int i = col, j = row;
    int h = curr.height;
    UArray_T bitArray = curr.bitArray;

    int q = j % BS + (i % BS) * BS + (j / BS) * BS * BS + (i / BS) * BS * h;
    int num_b = q / (BS * BS);

    UArray_T block = unpacking(*(uint64_t *)UArray_at(bitArray, num_b));

    int k = q % (BS * BS);


    *(Ypp)elem = *(Ypp)UArray_at(block, k);

    UArray_free(&block);
}

/******************************************************************************
 * Function: bitwords_to_YPP
 * Arguments: struct Compression_info, A2Methods_T
 * Return type: A2Methods_UArray2
 * Purpose: converts bitwords to Component Video values using apply function
 *          "applyBitword"
 ******************************************************************************/

static A2 bitwords_to_YPP (Compression_info info, A2Methods_T methods)
{
    unsigned w = info.width;
    unsigned h = info.height;
    A2 YPP_array = methods->new_with_blocksize(w, h, sizeof(struct Ypp), BS);
    
    closure_decompress curr = {.height = info.height,
                               .bitArray = info.bitArray};


    methods->map_block_major(YPP_array, applyBitword, &curr);

    UArray_free(&info.bitArray);



    return YPP_array;
}

/******************************************************************************
 * Function: check_rgb
 * Arguments: float pointer
 * Return type: void
 * Purpose: checks to see if RGB values are between 1 and 0, sets them to 1
 *          if value is above one and 0 if value is below 0
 ******************************************************************************/

static void check_rgb (float *num)
{
    if (*num > 1.0)
        *num = 1.0;

    if (*num < 0)
        *num = 0;
}

/******************************************************************************
 * Function: RGBConversion
 * Arguments: ints col and row, A2Methods_UArray2, void* elem and void* closure
 * Return Type: void
 * Purpose: apply function for "YPP_to_RGB", converts component video values to
            RGB values to create final Pnm_ppm pixel array
 ******************************************************************************/

static void RGBConversion(int col, int row, A2 pixels, void *elem, void *cl) 
{
    (void) pixels;
    RGBConversionStruct temp = *(RGBConversionStruct*)cl;

    A2Methods_T methods = temp.methods;
    A2 RGBPixels = temp.pixels;

    Ypp tempYpp = (Ypp)elem;

    Pnm_rgb rgb = (Pnm_rgb)methods->at(RGBPixels, col, row);

    float d  = DENOM;
    float y  = tempYpp->Y;
    float pb = tempYpp->Pb;
    float pr = tempYpp->Pr;

    /*conversion from Component Video to RGB */
    float r = 1.0 * y + 0.0      * pb +    1.402 * pr;
    float g = 1.0 * y - 0.344136 * pb - 0.714136 * pr;
    float b = 1.0 * y + 1.772    * pb +      0.0 * pr;

    check_rgb(&r);
    check_rgb(&g);
    check_rgb(&b);
 
    unsigned red   = (unsigned)round(r * d);
    unsigned green = (unsigned)round(g * d);
    unsigned blue  = (unsigned)round(b * d); 

    rgb->red = red; 
    rgb->green = green; 
    rgb->blue = blue; 

}

/******************************************************************************
 * Function: YPP_to_RGB
 * Arguments: Pnm_ppm, A2Methods_T, A2Methods_UArray2
 * Return Type: A2Methods_UArray2
 * Purpose: convert Component Video values to RGB using apply function
 *          "RGBConversion". New 2D array is created to hold RGB values. 
            Called in decompress40 and set to pixel member of Pnm_ppm struct
 *          when making full conversion from bitwords to Pnm_ppm
 ******************************************************************************/

static A2 YPP_to_RGB (Pnm_ppm image, A2Methods_T methods, A2 YPP_array) 
{
    unsigned w = image->width;
    unsigned h = image->height;
    A2 pixels = methods->new_with_blocksize(w, h, sizeof(struct Pnm_rgb), BS);

    RGBConversionStruct temp = { .methods = methods, .pixels = pixels };

    methods->map_block_major(YPP_array, RGBConversion, &temp);

    return pixels;
}


/* Input/output helper functions below to satisfy the spec.
 *  --> Our programs uses block major mapping, which as we found out
 *      traverses the blocks in column major. Here we are reindexing
 *      the bitword array that we are writing/reading to fit the spec's
 *      row-major requirement
 */

static UArray_T Col_to_row(UArray_T bitwords, unsigned w, unsigned h) 
{
    int len = UArray_length(bitwords);
    UArray_T array_row = UArray_new(len, sizeof(uint64_t));
    unsigned w_b = w / BS;
    unsigned h_b = h / BS; 
    int index = 0;
    for (unsigned i = 0; i < h_b; i++)
        for (unsigned j = 0; j < w_b; j++)
        {
            uint64_t temp = *(uint64_t *)UArray_at(bitwords, i + j * h_b);
            *(uint64_t *)UArray_at(array_row, index) = temp;
            index++;
        }

    return array_row;       
}

static UArray_T Row_to_col(UArray_T bitwords, unsigned w, unsigned h)
{
    int len = UArray_length(bitwords);
    UArray_T array_col = UArray_new(len, sizeof(uint64_t));

    unsigned w_b = w / BS;
    unsigned h_b = h / BS;

    int index = 0;
    for (unsigned j = 0; j < w_b; j++)
        for (unsigned i = 0; i < h_b; i++)
        {
            uint64_t temp = *(uint64_t *)UArray_at(bitwords, j + i * w_b);
            *(uint64_t *)UArray_at(array_col, index) = temp;
            index++;
        }

    return array_col;       
}



