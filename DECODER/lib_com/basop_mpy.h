/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#ifndef __BASOP_MPY_H
#define __BASOP_MPY_H

#include "stl.h"
#include "options.h"

/**
 * \brief 	32*16 Bit fractional Multiplication using 40 bit OPS
 *          Performs a multiplication of a 32-bit variable x by
 *          a 16-bit variable y, returning a 32-bit value.
 *
 * \param[i] x
 * \param[i] y
 *
 * \return x*y
 */
Word32 Mpy_32_16_1(Word32 x,
                   Word16 y);

/**
 * \brief 	32*16 Bit fractional Multiplication using 40 bit OPS
 *          Performs a multiplication of a 32-bit variable x by
 *          a 16-bit variable y incl. rounding, returning a 32-bit value.
 *
 * \param[i] x
 * \param[i] y
 *
 * \return x*y
 */
Word32 Mpy_32_16_r(Word32 x,
                   Word16 y);

/**
 * \brief 	32*32 Bit fractional Multiplication using 40 bit OPS
 *
 *          Performs a multiplication of a 32-bit variable x by
 *          a 32-bit variable y, returning a 32-bit value.
 *
 * \param[i] x
 * \param[i] y
 *
 * \return x*y
 */
Word32 Mpy_32_32(Word32 x,
                 Word32 y);

/**
 * \brief 	32*32 Bit fractional Multiplication using 40 bit OPS including rounding
 *
 *          Performs a multiplication of a 32-bit variable x by
 *          a 32-bit variable y, returning a 32-bit value.
 *
 * \param[i] x
 * \param[i] y
 *
 * \return x*y
 */
Word32 Mpy_32_32_r(Word32 x, Word32 y);

/**
 * \brief 	32*16 Bit integer Multiplication using 40 bit OPS
 *
 *          Performs a multiplication of a 32-bit variable x by
 *          a 16-bit variable y, returning a 32-bit value.
 *
 * \param[i] x
 * \param[i] y
 *
 * \return x*y
 */
Word32 Mpy_32_16_2(Word32 x,
                   Word16 y);


/**
 * \brief 	32*16 Bit complex fractional multiplication using 40 Bit and 32 Bit operators
 *
 *          The function mixes 40 Bit and 32 Bit operators, thus it must not be applied
 *          inside of loops where 32 and 16 bit operators are used.
 *
 * \param[i] c_Re
 * \param[i] c_Im
 * \param[i] a_Re
 * \param[i] a_Im
 * \param[i] b_Re
 * \param[i] b_Im
 *
 * \return none
 */
void cplxMpy_32_16(Word32 *c_Re,
                   Word32 *c_Im,
                   const Word32 a_Re,
                   const Word32 a_Im,
                   const Word16 b_Re,
                   const Word16 b_Im
                  );

#define MUL_F(A,B) Mpy_32_16_1((A),(B))

#endif /* __BASOP_SETTINGS_H */
