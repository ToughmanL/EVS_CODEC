/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "stl.h"
#include "basop_util.h"
#include "rom_com_fx.h"
#include "cnst_fx.h"
#include "prot_fx.h"


/*---------------------------------------------------------------------*
* routine:   lsf_dec_bfi()
*
* Estimate the LSFs in case of FER
* Bad frame, all active speech coders
*---------------------------------------------------------------------*/
void lsf_dec_bfi(
    const Word16 codec_mode,                 /* i: : codec mode: MODE1 | MODE2              */
    Word16*lsf,               /*!< o  : 14Q1*1.28     quantized ISFs                            */
    const Word16*lsfold,              /*!< i  : 14Q1*1.28     past quantized ISF                        */
    Word16*lsf_adaptive_mean,   /*!< i  : 14Q1*1.28     ISF adaptive mean, updated when BFI==0    */
    const Word16 lsfBase[],   /* i  : base for differential lsf coding        */
    Word16*mem_MA,              /*!< i/o: 14Q1*1.28     quantizer memory for MA model             */
    Word16*mem_AR,              /*!< i/o: 14Q1*1.28     quantizer memory for MA model             */
    Word16 stab_fac,            /*!< i  :               ISF stability factor (shifted right by 1) */
    const Word16 last_coder_type,            /*!< i  :               coding type in last good received fr.     */
    Word16 L_frame,
    const Word16 last_good,            /*!< i  :               last good received frame                  */
    const Word16 nbLostCmpt,              /*!< i  :               counter of consecutive bad frames         */
    Word8  plcBackgroundNoiseUpdated,
    Word16 *lsf_q_cng,        /* o  : quantized ISFs for background noise     (14Q1*1.28) */
    Word16 *lsf_cng,
    Word16 *old_lsf_q_cng,   /* o  : old quantized ISFs for background noise */
    const Word16 Last_GSC_pit_band_idx,
    const Word16 Opt_AMR_WB                  /* i  : IO flag                                */
    , const Word8  tcxonly
    ,const short MODE1_bwidth         /* i: coded bandwidth */
)
{
    Word16 i;
    Word16 alpha;
    Word32 tmp;
    Word16 lsf_mean[M];
    const Word16* pt_meansForFading;
    const Word16* pt_meansForMemUpdate;
    Word16 beta;
    Word16 gap;

    IF (sub(codec_mode,MODE1) == 0)
    {
        pt_meansForMemUpdate = lsf_mean;
        /* Update inital guess to something stable, with proper sampling frequency and format (ISF/LSF)*/
        IF ( Opt_AMR_WB)
        {
            pt_meansForFading = Mean_isf_wb;
        }
        ELSE
        {
            /* 12.8kHz ACELP sampling */
            IF( sub(L_frame,L_FRAME) == 0 )
            {
                pt_meansForFading = GEWB_Ave_fx;

                if (sub(MODE1_bwidth,NB)==0)
                {
                    pt_meansForFading = GENB_Ave_fx;
                }
            }
            /* 16kHz ACELP sampling */
            ELSE
            {
                pt_meansForFading = GEWB2_Ave_fx;
            }
        }
    }
    ELSE
    {
        pt_meansForFading = pt_meansForMemUpdate = lsfBase;
        test();
        if (lsf_cng != NULL && plcBackgroundNoiseUpdated)
        {
            pt_meansForFading = lsf_cng;
        }
    }
    IF( sub(nbLostCmpt, 3) <= 0 )
    {
        test();
        test();
        IF( (sub(last_coder_type, UNVOICED) == 0) )       /* Clear unvoiced last good frame   */
        {
            move16();
            alpha = _ALPHA_UU_FX;
        }
        ELSE IF( sub(last_coder_type,AUDIO) == 0 || sub(last_good,INACTIVE_CLAS) == 0 )
        {
            alpha = 32604/*0.995f Q15*/;
            move16();
            test();
            if( Last_GSC_pit_band_idx > 0 && sub(nbLostCmpt, 1) > 0 )
            {
                alpha = 26214/*0.8f Q15*/;
                move16();
            }
        }
        ELSE IF( sub(last_good,UNVOICED_CLAS) == 0 )
        {
            IF( sub(nbLostCmpt,1) <= 0 )
            {
                /* If stable, do not flatten the spectrum in the 1st erased frame  */
                alpha = add(mult(stab_fac, 32768 - _ALPHA_U_FX_X_2), _ALPHA_U_FX_X_2);
            }
            ELSE IF(sub(nbLostCmpt,2) == 0)
            {
                alpha = sub(_ALPHA_U_FX_X_2,shr(_ALPHA_U_FX,1));   /* 0.6 */
            }
            ELSE
            {
                alpha = _ALPHA_U_FX;
                move16();  /* go rapidly to CNG spectrum  */
            }
        }
        ELSE IF( sub(last_good ,UNVOICED_TRANSITION) == 0 )
        {
            alpha = _ALPHA_UT_FX;
            move16();
        }
        ELSE IF( (sub(last_good,VOICED_CLAS) == 0) || (sub(last_good,ONSET) == 0) )
        {
            /* clearly voiced -  mild convergence to the CNG spectrum for the first 3 erased frames */
            move16();
            alpha = _ALPHA_V_FX;
        }
        ELSE IF( sub(last_good ,SIN_ONSET) == 0 )
        {
            alpha = _ALPHA_S_FX;
            move16();
        }
        ELSE
        {
            alpha = _ALPHA_VT_FX;       /*  rapid convergence to the CNG spectrum (long erasure, ONSETS)  */
        }
    }
    ELSE
    {
        Word16 exp = 15;
        alpha = Inv16(nbLostCmpt, &exp); /*1.f/bfi_cnt;*/
        alpha = shl(alpha,exp);
    }
    IF(sub(codec_mode,MODE1) == 0)
    {
        beta = BETA_FEC_FX;
        move16();
    }
    ELSE
    {
        beta = 8192/*0.25f Q15*/;
        move16();
        if (plcBackgroundNoiseUpdated)
        {
            beta = 0/*0.f Q15*/;
            move16();
        }
    }
    FOR (i=0; i<M; i++)
    {

        lsf_mean[i] =   mac_r(L_mult(beta,pt_meansForFading[i]), sub(32767/*1.0F Q15*/,beta), lsf_adaptive_mean[i]);
        move16();

        lsf[i] =  mac_r(L_mult(alpha, lsfold[i]),  sub(32767/*1.0F Q15*/, alpha), lsf_mean[i]);
        move16();

        IF(lsf_q_cng!=NULL)
        {
            lsf_q_cng[i] = mac_r(L_mult(s_max(alpha,26214/*0.8f Q15*/),old_lsf_q_cng[i]), sub(32767/*1.0f Q15*/, s_max(alpha,26214/*0.8f Q15*/)), pt_meansForFading[i]);
            move16();
        }
    }

    /* check LSF stability through LSF ordering */
    IF ( Opt_AMR_WB )
    {
        reorder_isf_fx( lsf, ISF_GAP_FX, M, Fs_2 );
    }
    ELSE
    {
        IF( sub(L_frame,L_FRAME) == 0 )
        {
            IF(sub(codec_mode, MODE1) == 0)
            {
                reorder_lsf_fx(lsf, MODE1_LSF_GAP_FX, M, INT_FS_FX); /*arg1&2: 14Q1*1.18*/
            }
            ELSE
            {
                reorder_lsf_fx(lsf, LSF_GAP_FX, M, INT_FS_FX); /*arg1&2: 14Q1*1.18*/
            }

            IF(lsf_q_cng!=NULL)
            {
                reorder_lsf_fx(lsf_q_cng, LSF_GAP_FX, M, INT_FS_FX);
            }
        }
        ELSE IF ( tcxonly != 0 )
        {
            IF ( sub(L_frame,320) == 0 )
            {
                gap = 143;
            }
            ELSE IF ( sub(L_frame,512) == 0 )
            {
                gap = 90;
            }
            ELSE IF ( sub(L_frame,640) == 0 )
            {
                gap = 72;
            }
            ELSE
            {
                gap = 48;
            }
            reorder_lsf_fx(lsf, gap, M, INT_FS_FX);
        }
        ELSE
        {
            reorder_lsf_fx(lsf, MODE1_LSF_GAP_FX, M, i_mult(L_frame, 50)); /*arg1&2: 14Q1*1.18*/
            IF(lsf_q_cng!=NULL)
            {
                reorder_lsf_fx(lsf_q_cng, LSF_GAP_FX, M, INT_FS_16k_FX);
            }
        }
    }

    /* update the AR memory to be used in the next frame */
    {
        Copy(lsf,mem_AR,M);
    }

    /* update the MA memory to be used in the next frame */
    FOR(i=0; i<M; i++)
    {
        /*factor 0x4000 means 0.5. Together with /2 in mem_MA-assignment,
        this results in an attenuation of the MA Q memory */
        tmp = L_msu(L_mult(lsf[i],0x4000),pt_meansForMemUpdate[i],0x4000);
        /* Update with quantized prediction error for MA model */
        mem_MA[i] = msu_r(tmp, MU_MA_FX/2,mem_MA[i]);
        move16();
    }


    return;
}

Word16 const * PlcGetLsfBase (Word16   const lpcQuantization,
                              Word16   const narrowBand,
                              Word32   const sr_core)
{
    /* Not correct BW */
    IF (lpcQuantization==0)
    {
        /* high rates, return value is never used; the correct value changes
           dynamically and is not available during PLC; therefore, the setting
           is kept as before (without the define PLC_FIX_XSF_HANDLING); the
           correct value would be isf[m] as returned by lpc_unquantize()
           during normal decoding */
        IF(L_sub(sr_core,32000)==0)
        {
            return means_swb_cleanspeech_lsf32k0;
        }
        ELSE IF(L_sub(sr_core,25600)==0)
        {
            return means_swb_cleanspeech_lsf25k6;
        }
        ELSE
        {
            return means_wb_cleanspeech_lsf16k0;
        }
    }

    /* lpcQuntization == 1 is left */

    IF (L_sub(sr_core,16000)==0)
    {
        return GEWB2_Ave_fx;
    }

    /* sr_core == 12.8k is left */

    IF (narrowBand == 0)
    {
        return GEWB_Ave_fx;
    }

    /* narrowBand == 1 is left */
    return GENB_Ave_fx;
}

