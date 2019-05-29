/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

/*! @file basop_util_jbm.h basop utility functions for JBM. */

#ifndef BASOP_UTIL_JBM_H
#define BASOP_UTIL_JBM_H

#include "typedef.h"

/*! Adds two uint32_t values with overflow like plain C. */
Word32 rtpTs_add( Word32 ts1, Word32 ts2 );

/*! Subtracts two uint32_t values with overflow like plain C. */
Word32 rtpTs_sub( Word32 ts1, Word32 ts2 );

#endif
