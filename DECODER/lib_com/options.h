
/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#ifndef OPTIONS_H
#define OPTIONS_H

#include "stl.h"

/* ################### Start compiler switches ######################## */
/*                                                                         */
#ifdef _MSC_VER
#pragma warning(disable:4310)     /* cast truncates constant value this affects mainly constants tables*/
#endif

#define SUPPORT_JBM_TRACEFILE     /* support for JBM tracefile, which is needed for 3GPP objective/subjective testing, but not relevant for real-world implementations */

/*                                                                        */
/* ##################### End compiler switches ######################## */

#endif /* OPTIONS_H */
