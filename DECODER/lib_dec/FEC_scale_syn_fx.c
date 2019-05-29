/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include "options.h"     /* Compilation switches                   */
#include "cnst_fx.h"       /* Common constants                       */
#include "prot_fx.h"       /* Function prototypes                    */
#include "stl.h"
#include "basop_mpy.h"

/*-------------------------------------------------------------------*
 * Local constants
 *-------------------------------------------------------------------*/
#define AGC_FX 32113  /* 0.98f */
#define SCLSYN_LAMBDA (9830/*0.3f Q15*/)

/*========================================================================*/
/* FUNCTION : FEC_scale_syn_fx()										  */
/*------------------------------------------------------------------------*/
/* PURPOSE : Smooth the speech energy evolution when					  */
/*		     recovering after a BAD frame								  */
/*------------------------------------------------------------------------*/
/* INPUT ARGUMENTS :													  */
/* _ (Word16) L_frame       :  length of the frame						  */
/* _ (Word16) *update_flg   :  indication about resynthesis				  */
/* _ (Word16) st_fx->clas_dec: frame classification						  */
/* _ (Word16) last_good	    : last good frame classification     		  */
/* _ (Word16[]) synth       : synthesized speech at Fs = 12k8 Hz   Q_syn  */
/* _ (Word16[])  pitch	  : pitch values for each subframe   	    Q0	  */
/* _ (Word32)  L_enr_old  :energy at the end of previous frame    	Q0	  */
/* _ (Word16) L_enr_q	  : transmitted energy for current frame    Q0    */
/* _ (Word16) coder_type	  : coder type                                */
/* _ (Word16) st_fx->prev_bfi_fx: previous frame BFI                      */
/* _ (Word16) st_fx->last_core_brate_fx: previous frame core bitrate      */
/* _ (Word16[]) mem_tmp	  : temp. initial synthesis filter states   Q_syn */
/* _ (Word16) Q_exc	  : quantized LSPs from frame end    	              */
/* _ (Word16) Q_syn	  : quantized LSPs from frame end    	              */
/*------------------------------------------------------------------------*/
/* INPUT/OUTPUT ARGUMENTS :												  */
/* _ (Word16[]) exc	  : excitation signal without enhancement      Q_exc  */
/* _ (Word16[]) exc2	  : excitation signal with enhancement     Q_exc  */
/* _ (Word16[]) Aq	  :  LP filter coefs (can be modified if BR)   Q12    */
/*------------------------------------------------------------------------*/
/* OUTPUT ARGUMENTS :													  */
/*------------------------------------------------------------------------*/

/* _ (Word16[]) st_fx->mem_syn2	  : initial synthesis filter states Q_syn */
/* _ (Word16) st_fx->old_enr_LP   : LP filter E of last            Q5     */
/*									good voiced frame                     */
/*------------------------------------------------------------------------*/
/* RETURN ARGUMENTS :													  */
/* _ None																  */
/*========================================================================*/


void FEC_scale_syn_fx(
    const Word16 L_frame,          /* i  : length of the frame                     */
    Word16 *update_flg,      /* o: flag indicating re-synthesis after scaling*/
    Word16 clas,             /* i/o: frame classification                    */
    const Word16 last_good,        /* i:   last good frame classification          */
    Word16 *synth,           /* i/o: synthesized speech at Fs = 12k8 Hz      */
    const Word16 *pitch,           /* i:   pitch values for each subframe          */
    Word32 L_enr_old,          /* i:   energy at the end of previous frame     */
    Word32 L_enr_q,            /* i:   transmitted energy for current frame    */
    const Word16 coder_type,       /* i:   coder type                              */
    const Word16 LSF_Q_prediction, /* i  : LSF prediction mode                     */
    Word16 *scaling_flag,    /* i/o: flag to indicate energy control of syn  */
    Word32 *lp_ener_FEC_av,  /* i/o: averaged voiced signal energy           */
    Word32 *lp_ener_FEC_max, /* i/o: averaged voiced signal energy           */
    const Word16 bfi,              /* i:   current  frame BFI                      */
    const Word32 total_brate,      /* i:   total bitrate                           */
    const Word16 prev_bfi,         /* i:   previous frame BFI                      */
    const Word32 last_core_brate,  /* i:   previous frame core bitrate             */
    Word16 *exc,             /* i/o: excitation signal without enhancement   */
    Word16 *exc2,            /* i/o: excitation signal with enhancement      */
    Word16 Aq[],             /* i/o: LP filter coefs (can be modified if BR) */
    Word16 *old_enr_LP,      /* i/o: LP filter E of last good voiced frame   */
    const Word16 *mem_tmp,         /* i:   temp. initial synthesis filter states   */
    Word16 *mem_syn,          /* o:   initial synthesis filter states         */
    Word16 Q_exc,
    Word16 Q_syn
    , Word16 avoid_lpc_burst_on_recovery /* i  : if true the excitation energy is limited if LP has big gain */
    , Word16 force_scaling     /* i: force scaling                             */
)
{
    Word16 i;
    Word32 L_enr1, L_enr2;
    Word16 gain1, gain2, enr_LP;
    Word16 tmp, tmp2, exp, exp2;
    Word16 tmp3;
    Word32 L_tmp;
    Word16 scaling;
    Word32 ener_max, L_enr2_av, L_ener2_max;
    Word16 h1[L_FRAME/2], tilt, pitch_dist, mean_pitch;
    Word16 k;
    Word32 L_mean_pitch;
    enr_LP = 0;
    move16();
    gain2 = 0;
    move16();
    gain1 = 0;
    move16();
    *update_flg = 0;
    move16();
    L_enr_old = L_max(1, L_enr_old); /* to avoid division by zero (*L_enr_old is always >= 0) */
    scaling = 16384;
    move16();    /* Q14*/

    /*-----------------------------------------------------------------*
     * Find the synthesis filter impulse response on voiced
     *-----------------------------------------------------------------*/
    test();
    IF( sub(clas,VOICED_TRANSITION) >= 0 && sub(clas,INACTIVE_CLAS) < 0 )
    {
        IF( sub(L_frame,L_FRAME) == 0 )
        {
            enr_LP = Enr_1_Az_fx(Aq+(NB_SUBFR-1)*(M+1), L_SUBFR );
        }
        ELSE  /* L_frame == L_FRAME16k */
        {
            enr_LP = Enr_1_Az_fx( Aq+(NB_SUBFR16k-1)*(M+1), L_SUBFR ); /*Q3*/
        }
    }

    /*-----------------------------------------------------------------*
    * Define when to scale the synthesis
    *-----------------------------------------------------------------*/

    IF( bfi )
    {
        *scaling_flag = 1;
        move16();	    /* Always check synthesis on bad frames */
    }
    ELSE IF( prev_bfi )
    {
        test();
        IF( ( sub(LSF_Q_prediction,AUTO_REGRESSIVE) == 0 ) || ( sub(LSF_Q_prediction,MOVING_AVERAGE) == 0 ) )
        {
            *scaling_flag = 2;
            move16();			/* Decoded LSFs affected  */
        }
        ELSE IF( sub(coder_type,TRANSITION) != 0 )
        {
            *scaling_flag = 1;
            move16();			/* SN, but not TC mode - LSF still affected by the interpolation */
        }
        ELSE
        {
            *scaling_flag = 0;
            move16();				/* LSF still possibly affected due to interpolation */
        }
        scaling = 24576;		/*1.5 Q14*/	move16();
    }
    ELSE
    {
        test();
        IF( (sub(LSF_Q_prediction,AUTO_REGRESSIVE) == 0) && (sub(*scaling_flag,2) == 0) )
        {
            *scaling_flag = 2;
            move16();				/* Continue with energy control till the end of AR prediction */
        }
        ELSE IF( *scaling_flag > 0 )
        {
            (*scaling_flag) = sub(*scaling_flag,1);					/* If scaling flag was equal to 2, add one control frame to account for the LSF interpolation */
        }
        scaling = 32767;        /*2.0 Q14*/ move16();
    }

    /*-----------------------------------------------------------------*
    * Find the energy/gain at the end of the frame
    *-----------------------------------------------------------------*/

    /*fer_energy( L_frame, clas, synth, pitch[(L_frame>>6)-1], &enr2, L_frame );*/
    frame_ener_fx(L_frame,clas, synth, pitch[sub(shr(L_frame,6),1)], &L_enr2/*Q0*/, 1, Q_syn, 3, 0);

    if( bfi || (L_sub(total_brate,ACELP_7k20) == 0) || (L_sub(total_brate,ACELP_8k00) == 0) )
    {
        /* previous frame erased and no TC frame */
        IF( *scaling_flag > 0 )
        {
            /*enr2 += 0.01f;*/
            L_enr2 = L_max(L_enr2, 1); /* L_enr2 is in Q0 */

            IF( bfi )				/* In all bad frames, limit the gain to 1  */
            {
                /* gain2 = (float)sqrt( enr_old / enr2 );*/
                L_tmp = Sqrt_Ratio32(L_enr_old, 0, L_enr2, 0, &exp2);
                gain2 = round_fx(L_shl(L_tmp, sub(exp2, 1))); /* in Q14 */

                /*if( gain2 > 1.0f )gain2 = 1.0f;*/
                gain2 = s_min(gain2, 16384);

                /* find the energy/gain at the beginning of the frame */
                frame_ener_fx(L_frame,clas, synth, pitch[0], &L_enr1/*Q0*/, 1, Q_syn, 3, 0);

                /*enr1 += 0.1f;*/
                L_enr1 = L_max(L_enr1, 1); /* L_enr2 is in Q0 */

                /*gain1 = (float)sqrt( enr_old / enr1 );*/
                L_tmp = Sqrt_Ratio32(L_enr_old, 0, L_enr1, 0, &exp2);
                gain1 = round_fx(L_shl(L_tmp, sub(exp2, 1))); /* in Q14 */

                /*if( gain1 > 1.0f )gain1 = 1.0f;*/
                gain1 = s_min(gain1, 16384);
            }
            ELSE					/* good frame  */
            {
                IF( L_enr_q == 0 )          /* If E info (FEC protection bits) is not available in the bitstream */
                {
                    L_enr_q = L_enr2;
                    set16_fx( h1, 0, L_FRAME/2 );
                    h1[0] = 1024;
                    move16();
                    /*syn_filt( Aq+(3*(M+1)), M, h1, h1, L_FRAME/2, h1+(M+1), 0 );*/
                    E_UTIL_synthesis(1, Aq+(3*(M+1)), h1, h1, L_FRAME/2, h1+(M+1), 0, M);

                    /*Compute tilt */
                    /*rr0 = dotp( h1, h1, L_FRAME/2-1 ) + 0.1f;*/
                    /*rr1 = dotp( h1, h1+1, L_FRAME/2-1 );*/
                    /*tilt = rr1 / rr0;*/
                    tilt = extract_h(L_shl(get_gain(h1+1, h1, L_FRAME/2-1),15));

                    pitch_dist = 0;
                    move16();
                    L_mean_pitch = L_mult(pitch[0], 8192);
                    move32();
                    FOR( k=0; k<(NB_SUBFR - 1); k++ )
                    {
                        pitch_dist = add(pitch_dist, abs_s(sub(pitch[k+1],pitch[k])));
                        L_mean_pitch = L_mac(L_mean_pitch, pitch[k+1], 8192);
                    }
                    /*pitch_dist /= (float)(NB_SUBFR-1);	*/
                    pitch_dist = mult_r(shl(pitch_dist,4),10923);
                    /*mean_pitch /= (float)(NB_SUBFR);*/
                    mean_pitch = extract_h(L_shl(L_mean_pitch,4));


                    test();
                    test();
                    test();
                    test();
                    test();
                    test();
                    IF( ( sub(tilt,22938)  > 0 ) &&											/* HF resonnant filter */
                        ( (sub(pitch_dist, 8<<4) > 0) || (sub(mean_pitch,PIT_MIN<<4) < 0) ) &&        /* pitch unstable or very short      */
                        ( (prev_bfi) || ( (sub(coder_type,GENERIC) == 0) && (sub(LSF_Q_prediction,AUTO_REGRESSIVE) == 0) ) ) )
                    {
                        /*if( enr_q > scaling * enr_old ){enr_q = scaling * enr_old;}*/
                        L_enr_q = L_min(L_enr_q, L_shl(Mult_32_16(L_enr_old, scaling),1));  /* scaling in Q14*/
                    }
                    ELSE
                    {
                        ener_max = *lp_ener_FEC_max;
                        move32();
                        test();
                        if( sub(clas,VOICED_TRANSITION) == 0 || (sub(clas,INACTIVE_CLAS) >= 0))
                        {
                            ener_max = *lp_ener_FEC_av;
                            move32();
                        }
                        /*if( enr_old > ener_max )ener_max = enr_old;*/
                        ener_max = L_max(ener_max, L_enr_old);

                        /*if( enr_q > scaling * ener_max ){enr_q = scaling * ener_max;}*/
                        L_enr_q = L_min(L_enr_q, L_shl(Mult_32_16(ener_max, scaling),1));   /* scaling in Q14*/
                    }
                }
                /*gain2 = (float)sqrt( enr_q / enr2 );*/
                L_enr_q = L_max(L_enr_q, 1); /* L_enr2 is in Q0 */
                L_tmp = Sqrt_Ratio32(L_enr_q, 0, L_enr2, 0, &exp2);
                gain2 = round_fx(L_shl(L_tmp, sub(exp2, 1))); /* in Q14 */

                /*-----------------------------------------------------------------*
                * Find the energy/gain at the beginning of the frame to ensure smooth transition after erasure(s)
                *-----------------------------------------------------------------*/

                test();
                test();
                test();
                test();
                test();
                test();
                IF( ( (sub(last_good,VOICED_TRANSITION) >= 0 && sub(last_good,INACTIVE_CLAS) < 0 && (sub(clas,UNVOICED_CLAS) == 0 || sub(clas,INACTIVE_CLAS) == 0)) ||
                      L_sub(last_core_brate,SID_1k75) == 0 || L_sub(last_core_brate,SID_2k40) == 0 || L_sub(last_core_brate,FRAME_NO_DATA) == 0 ) && prev_bfi )
                {
                    /* voiced -> unvoiced signal transition */
                    /* CNG -> active signal transition */
                    gain1 = gain2;
                    move16();
                }
                ELSE
                {
                    /* find the energy at the beginning of the frame */
                    frame_ener_fx(L_frame,clas, synth, pitch[0], &L_enr1/*Q0*/, 1, Q_syn, 3, 0);

                    /*enr1 += 0.1f;*/
                    L_enr1 = L_max(L_enr1, 1); /* L_enr1 is in Q0 */

                    /*gain1 = (float)sqrt( enr_old / enr1 );*/
                    L_tmp = Sqrt_Ratio32(L_enr_old, 0, L_enr1, 0, &exp2);
                    gain1 = round_fx(L_shl(L_tmp, sub(exp2, 1))); /* in Q14 */

                    /*if( gain1 > 1.2f )gain1 = 1.2f;*/
                    /* prevent clipping */
                    gain1 = s_min(gain1, 19661);

                    /* prevent amplifying the unvoiced or inactive part of the frame in case an offset is followed by an onset */
                    test();
                    test();
                    if( sub(clas,ONSET) == 0 && sub(gain1,gain2) > 0 && prev_bfi )
                    {
                        gain1 = gain2;
                        move16();
                    }
                }

                L_enr2 = L_enr_q;
                move32();   /* Set the end frame energy to the scaled energy, to be used in the lp_ener_FEC  */
            }

            /*------------------------------------------------------------------------------*
            * Smooth the energy evolution by exponentially evolving from gain1 to gain2
            *------------------------------------------------------------------------------*/

            /*gain2 *= ( 1.0f - AGC );*/
            L_tmp = L_mult(gain2, (Word16)(32768 - AGC_FX));
            FOR( i=0; i<L_frame; i++ )
            {
                /*gain1 = gain1 * AGC + gain2;*/
                gain1 = mac_r(L_tmp, gain1, AGC_FX); /* in Q14 */
                /*exc[i] *= gain1;*/
                exc[i] = mac_r(L_mult(exc[i],  gain1),  exc[i], gain1);
                move16();
                /*exc2[i] *= gain1;*/
                exc2[i] = mac_r(L_mult(exc2[i], gain1), exc2[i], gain1);
                move16();
            }
            /* smoothing is done in excitation domain, so redo synthesis */
            Copy(mem_tmp, mem_syn, M );
            syn_12k8_fx( L_frame, Aq, exc2, synth, mem_syn, 1,Q_exc,Q_syn );
            *update_flg = 1;
            move16();
        }
    }
    ELSE
    {
        /* previous frame erased and no TC frame */
        test();
        IF( prev_bfi && sub(coder_type,TRANSITION) != 0 )
        {
            IF( L_enr_q == 0 )
            {
                L_enr_q = L_max(1,L_enr2); /* sets to 'L_enr2' in 1 clock */
                set16_fx( h1, 0, L_FRAME/2 );
                h1[0] = 1024;
                move16();
                /*syn_filt( Aq+(3*(M+1)), M, h1, h1, L_FRAME/2, h1+(M+1), 0 );*/
                E_UTIL_synthesis(1, Aq+(3*(M+1)), h1, h1, L_FRAME/2, h1+(M+1), 0, M);
                /*Compute tilt */
                /*rr0 = dotp( h1, h1, L_FRAME/2-1 ) + 0.1f;*/
                /*rr1 = dotp( h1, h1+1, L_FRAME/2-1 );*/
                /*tilt = rr1 / rr0;*/
                tilt = extract_h(L_shl(get_gain(h1+1, h1, L_FRAME/2-1),15));
                test();
                test();
                test();
                test();
                test();
                test();
                test();
                test();
                IF( ( ( (L_sub(total_brate,ACELP_13k20) == 0) || (L_sub(total_brate,ACELP_12k85) == 0) || (L_sub(total_brate,ACELP_12k15) == 0) || (L_sub(total_brate,ACELP_11k60) == 0) ||
                (L_sub(total_brate,ACELP_9k60) == 0) ) &&
                ( sub(tilt,22938) > 0 ) &&											    /* HF resonnant filter */
                ( (sub(clas,UNVOICED_CLAS) == 0) || (sub(clas,INACTIVE_CLAS) == 0) ) ) )	    /* unvoiced classification */
                {
                    /*if( enr_q > scaling * enr_old )enr_q = scaling * enr_old;*/
                    L_enr_q = L_min(L_enr_q, L_shl(Mult_32_16(L_enr_old, scaling),1));   /* scaling in Q14*/
                }
                ELSE IF( sub(last_good,VOICED_TRANSITION) >= 0 && sub(last_good,INACTIVE_CLAS) < 0 && sub(clas,VOICED_TRANSITION) >= 0 && sub(clas,INACTIVE_CLAS) < 0 )
                {
                    /* Voiced-voiced recovery */
                    test();
                    IF( *old_enr_LP != 0 && sub(enr_LP, shl(*old_enr_LP, 1)) > 0 )
                    {
                        /* enr_q /= enr_LP */
                        exp = norm_l(L_enr_q);
                        tmp = extract_h(L_shl(L_enr_q, exp));

                        exp2 = norm_s(enr_LP);
                        tmp2 = shl(enr_LP, exp2);

                        exp = sub(exp2, exp);

                        tmp3 = sub(tmp, tmp2);
                        IF (tmp3 > 0)
                        {
                            tmp = shr(tmp, 1);
                            exp = add(exp, 1);
                        }
                        tmp = div_s(tmp, tmp2);

                        /* L_enr_q *= 2 * *old_enr_LP */
                        L_enr_q = L_shl(L_mult(tmp, shl(*old_enr_LP, 1)), exp);
                    }

                    ELSE
                    {
                        test();
                        IF( avoid_lpc_burst_on_recovery && sub(enr_LP, 160) > 0 )
                        {
                            exp = norm_s(enr_LP);
                            tmp = shl(enr_LP, exp);

                            exp2 = 7;
                            move16();
                            tmp2 = 160 << 7; /* 160 = 20.0f in Q3 */
                            exp = sub(exp2, exp);

                            IF (sub(tmp, tmp2) > 0)
                            {
                                tmp = shr(tmp, 1);
                                exp = add(exp, 1);
                            }
                            tmp = div_s(tmp, tmp2); /* tmp*2^exp = enr_LP/20.0 */
                            L_tmp = Isqrt_lc(L_deposit_h(tmp), &exp); /* L_tmp*2^exp = sqrt(20.0/enr_LP) */
                            L_enr_q = L_shl(Mpy_32_32(L_enr_q, L_tmp), exp);
                        }
                    }

                }

                test();
                test();
                test();
                IF( (sub(last_good,VOICED_TRANSITION) >= 0 && sub(last_good,INACTIVE_CLAS) < 0 && sub(clas,VOICED_TRANSITION) >= 0 && sub(clas,INACTIVE_CLAS) < 0)
                    || force_scaling )
                {

                    IF( L_sub(L_enr_q, L_enr_old) > 0) /* Prevent energy to increase on voiced */
                    {
                        L_enr_q = L_add(Mpy_32_16_1(L_enr_old, 32767 - SCLSYN_LAMBDA), Mpy_32_16_1(L_enr_q, SCLSYN_LAMBDA));
                    }
                }
            }

            L_enr_q = L_max(1, L_enr_q);

            /* gain2 = (float)sqrt( enr_q / enr2 );*/
            exp = norm_l(L_enr_q);
            tmp = extract_h(L_shl(L_enr_q, exp));

            exp2 = norm_l(L_enr2);
            tmp2 = extract_h(L_shl(L_enr2, exp2));

            exp2 = sub(exp, exp2); /* Denormalize and substract */

            tmp3 = sub(tmp2, tmp);
            IF (tmp3 > 0)
            {
                tmp2 = shr(tmp2, 1);
                exp2 = add(exp2, 1);
            }

            tmp = div_s(tmp2, tmp);

            L_tmp = L_deposit_h(tmp);
            L_tmp = Isqrt_lc(L_tmp, &exp2);
            gain2 = round_fx(L_shl(L_tmp, sub(exp2, 1))); /* in Q14 */

            /*-----------------------------------------------------------------*
            * Clipping of the smoothing gain at the frame end
            *-----------------------------------------------------------------*/

            gain2 = s_min(gain2, 19661);     /* Gain modification clipping */
            if (L_sub(L_enr_q, 2) < 0)
            {
                gain2 = s_min(gain2, 16384); /* Gain modification clipping */
            }

            /*-----------------------------------------------------------------*
            * Find the energy/gain at the beginning of the frame to ensure smooth transition after erasure(s)
            *-----------------------------------------------------------------*/

            test();
            test();
            test();
            test();
            test();
            test();
            IF( sub(clas,SIN_ONSET) == 0 )   /* slow increase */
            {
                gain1 = shr(gain2, 1);
            }
            /*------------------------------------------------------------*
            * voiced->unvoiced transition recovery
            *------------------------------------------------------------*/
            ELSE IF( (sub(last_good,VOICED_TRANSITION) >= 0 && sub(last_good,INACTIVE_CLAS) < 0 && (sub(clas,UNVOICED_CLAS) == 0 || sub(clas,INACTIVE_CLAS) == 0)) ||          /* voiced->unvoiced transition recovery */
                     L_sub(last_core_brate,SID_1k75) == 0 || L_sub(last_core_brate,SID_2k40) == 0 || L_sub(last_core_brate,FRAME_NO_DATA) == 0)                                      /* CNG -> active signal transition */
            {
                gain1 = gain2;
                move16();
            }
            ELSE
            {
                /*--------------------------------------------------------*
                * Find the energy at the beginning of the frame
                *--------------------------------------------------------*/
                tmp = frame_ener_fx(L_frame,clas, synth, pitch[0], &L_enr1, 0, Q_syn, 3, 0);

                /*gain1 = (float)sqrt( enr_old / enr1 );*/
                exp = norm_l(L_enr_old);
                tmp = extract_h(L_shl(L_enr_old, exp));
                exp2 = norm_l(L_enr1);
                tmp2 = extract_h(L_shl(L_enr1, exp2));

                exp2 = sub(exp, exp2); /* Denormalize and substract */

                tmp3 = sub(tmp2, tmp);

                IF (tmp3 > 0)
                {
                    tmp2 = shr(tmp2, 1);
                    exp2 = add(exp2, 1);
                }

                tmp = div_s(tmp2, tmp);

                L_tmp = L_deposit_h(tmp);
                L_tmp = Isqrt_lc(L_tmp, &exp2);
                gain1 = round_fx(L_shl(L_tmp, sub(exp2, 1))); /* in Q14 */
                /* exp2 is always <= 1 */

                gain1 = s_min(gain1, 19661);

                test();
                test();
                if( avoid_lpc_burst_on_recovery && (sub(enr_LP, 160) > 0) && (sub(enr_LP, shl(*old_enr_LP, 1)) <= 0) )
                {
                    gain1 = s_min(gain1, 16384);
                }

                /*--------------------------------------------------------*
                * Prevent a catastrophy in case of offset followed by onset
                *--------------------------------------------------------*/
                test();
                if( ( sub(clas,ONSET) == 0 ) && (sub(gain1,gain2) > 0) )
                {
                    gain1 = gain2;
                    move16();
                }
            }
            /*-----------------------------------------------------------------*
            * Smooth the energy evolution by exponentially evolving from
            * gain1 to gain2
            *-----------------------------------------------------------------*/

            L_tmp = L_mult(gain2, (Word16)(32768 - AGC_FX));

            FOR( i=0; i<L_frame; i++ )
            {
                gain1 = mac_r(L_tmp, gain1, AGC_FX); /* in Q14 */
                exc[i] = mac_r(L_mult(exc[i],  gain1),  exc[i], gain1);
                move16();
                exc2[i] = mac_r(L_mult(exc2[i], gain1), exc2[i], gain1);
                move16();
            }

            Copy(mem_tmp, mem_syn, M );
            syn_12k8_fx( L_frame, Aq, exc2, synth, mem_syn, 1,Q_exc,Q_syn );
            *update_flg = 1;
            move16();
        }
    }
    /*-----------------------------------------------------------------*
    * Update low-pass filtered energy for voiced frames
    *-----------------------------------------------------------------*/

    test();
    test();
    IF( !bfi && (sub(clas,VOICED_TRANSITION) >= 0 && sub(clas,INACTIVE_CLAS) < 0) )
    {
        IF( sub(clas,VOICED_TRANSITION) == 0 )
        {
            L_enr2_av = L_enr2;
            move32();
            frame_ener_fx(L_frame,VOICED_CLAS, synth,pitch[sub(shr(L_frame,6),1)], &L_ener2_max/*Q0*/, 1, Q_syn, 3, 0);
        }
        ELSE
        {
            L_ener2_max = L_enr2;
            move32();
            frame_ener_fx(L_frame,UNVOICED_CLAS, synth, pitch[sub(shr(L_frame,6),1)], &L_enr2_av/*Q0*/, 1, Q_syn, 3, 0);
        }

        /**lp_ener_FEC_av = 0.2f * enr2_av + 0.8f * *lp_ener_FEC_av;      move32();*/
        *lp_ener_FEC_av = Madd_32_16(Mult_32_16(*lp_ener_FEC_av, 31130), L_enr2_av, 1638);
        move32();
        /**lp_ener_FEC_max = 0.2f * enr2_max + 0.8f * *lp_ener_FEC_max;      move32();*/
        *lp_ener_FEC_max = Madd_32_16(Mult_32_16(*lp_ener_FEC_max, 31130), L_ener2_max, 1638);
        move32();
    }

    /*-----------------------------------------------------------------*
    * Update the LP filter energy for voiced frames
    *-----------------------------------------------------------------*/
    test();
    if( sub(clas,VOICED_TRANSITION) >= 0 && sub(clas,INACTIVE_CLAS) < 0 )
    {
        *old_enr_LP = enr_LP;
        move16();
    }

    return;

}
