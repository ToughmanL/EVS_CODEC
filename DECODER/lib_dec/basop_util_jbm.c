/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

/*! @file basop_util_jbm.c basop utility functions for JBM. */

#include "basop_util.h"
#include "basop_util_jbm.h"
#include "options.h"
#include "stl.h"

/* Adds two uint32_t values with overflow like plain C. */
Word32 rtpTs_add( Word32 ts1, Word32 ts2 )
{
    Word32 ret;

    Carry = 0;
    ret = L_add_c(ts1, ts2);
    Carry = 0;
    Overflow = 0;

    return ret;
}

/* Subtracts two uint32_t values with overflow like plain C. */
Word32 rtpTs_sub( Word32 ts1, Word32 ts2 )
{
    Word32 ret;

    BASOP_SATURATE_WARNING_OFF
    Carry = 1;
    ret = L_sub_c(ts1, ts2);
    BASOP_SATURATE_WARNING_ON
    Carry = 0;
    Overflow = 0;

    return ret;
}
