/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include "options.h"        /* Compilation switches                   */
#include "stl.h"
#include "prot_fx.h"


/*--------------------------------------------------------------------------
*  get_delay_fx()
*
*  Function returns various types of delays in the codec in ms.
*--------------------------------------------------------------------------*/

Word32 get_delay_fx(            /* o  : delay value in ms                         */
    const Word16 what_delay,    /* i  : what delay? (ENC or DEC)                  */
    const Word32 io_fs          /* i  : input/output sampling frequency           */
)
{
    Word32 delay = 0;

    IF( sub(what_delay,ENC) == 0 )
    {
        delay = (DELAY_FIR_RESAMPL_NS + ACELP_LOOK_NS);
        move32();
    }
    ELSE
    {
        IF( L_sub(io_fs,8000) == 0 )
        {
            delay = DELAY_CLDFB_NS;
            move32();
        }
        ELSE
        {
            delay = DELAY_BWE_TOTAL_NS;
            move32();
        }
    }

    return delay;
}
