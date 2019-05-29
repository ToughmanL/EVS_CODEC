/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "options.h"
#include "stl.h"
#include "prot_fx.h"
#include "cnst_fx.h"
#include "rom_com_fx.h"
#include "rom_enc_fx.h"

/*---------------------------------------------------------------------------

   function name: SetModeIndex
   description:   function for configuring the codec between frames - currently bit rate and bandwidth only
                  must not be called while a frame is encoded - hence mutexes must be used in MT environments

   format:        BANDWIDTH*16 + BITRATE (mode index)

  ---------------------------------------------------------------------------*/

void SetModeIndex(
    Encoder_State_fx *st,
    Word32    total_brate,
    Word16    bwidth,
    const Word16 shift)
{

    /* Reconfigure the core coder */
    test();
    test();
    test();
    IF(
        (L_sub(st->last_total_brate_fx,total_brate) != 0) ||
        (sub(st->last_bwidth_fx,bwidth) != 0)  ||
        (sub(st->last_codec_mode,MODE1) == 0 )
        || (sub(st->rf_mode_last,st->rf_mode) != 0 )
    )
    {
        core_coder_mode_switch( st, st->bwidth_fx, total_brate, shift);
    }


    return;
}
