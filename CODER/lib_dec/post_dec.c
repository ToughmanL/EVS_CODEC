/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/



#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "options.h"
#include "prot_fx.h"
#include "basop_util.h"
#include "stl.h"


/*---------------------------------------------------------------------*
 * Function prototypes
 *---------------------------------------------------------------------*/

static void bass_pf_1sf_delay( Word16 *syn, Word16 *T_sf, Word16 *gainT_sf, Word16 l_frame,
                               Word16 l_subfr, Word16 *bpf_noise_buf, Word16 *gain_factor_param,
                               Word8 disable_bpf, Word32 *lp_error_ener, Word32 *mem_error );

/*---------------------------------------------------------------------*
 * post_decoder()
 *
 * Perform post-processing
 *---------------------------------------------------------------------*/

void post_decoder(
    Decoder_State_fx *st,
    Word16 coder_type,
    Word16 synth_buf[],
    Word16 pit_gain[],
    Word16 pitch[],
    Word16 signal_out[],
    Word16 *bpf_noise_buf
)
{
    Word16 L_frame, nb_subfr;
    Word16 *synth, *synth2;
    Word16 pfstat_on_previous;
    Word16 pitch_gain_adjust[NB_SUBFR16k];
    Word16 tmp, tmp_noise;
    Word16 synth2_pe[L_FRAME_MAX];
    Word16 synth_buf2[PIT_MAX_16k+1+L_FRAME_MAX+M];
    Word32 bitrate;
    Word8  tmp8;


    L_frame = st->L_frame_fx;
    move16();
    nb_subfr = st->nb_subfr;
    move16();
    pfstat_on_previous = st->pfstat.on;
    move16();
    st->pfstat.on = 0;
    move16();

    bitrate = L_add(st->total_brate_fx, 0);
    if(st->core_brate_fx <= SID_2k40)
    {
        bitrate = L_add(st->last_active_brate_fx, 0);
    }


    /*Adapt Bpf: copy old and current adapt bpf parameters*/
    set16_fx(pitch_gain_adjust, st->bpf_gain_param, nb_subfr);

    synth = synth_buf + st->old_synth_len;
    synth2 = synth_buf2 + NBPSF_PIT_MAX;
    Copy( st->pst_old_syn_fx, synth_buf2, NBPSF_PIT_MAX );

    IF ( st->tcxonly != 0 )
    {
        Copy( synth, synth2, L_frame );
        IF ( pfstat_on_previous )
        {
            Copy( st->pfstat.mem_pf_in+L_SYN_MEM-M, synth-M, M );
            Residu3_fx ( st->old_Aq_12_8_fx, synth, synth_buf, L_SUBFR, 1 );
            E_UTIL_synthesis ( 1, st->old_Aq_12_8_fx, synth_buf, synth2, L_SUBFR, st->pfstat.mem_stp+L_SYN_MEM-M, 0, M );
            scale_st ( synth, synth2, &st->pfstat.gain_prec, L_SUBFR );
            blend_subfr2(synth2+L_SUBFR/2, synth+L_SUBFR/2, synth2+L_SUBFR/2);
        }
    }
    ELSE
    {
        /*Formant enhancement*/
        IF ( sub(st->last_bwidth_fx,NB)==0 )
        {
            Copy( synth, synth2_pe, L_frame );
            tmp = synth[-1];
            move16();

            preemph_copy_fx( synth2_pe, synth2_pe, st->preemph_fac, L_frame, &tmp);

            tmp = 0;
            move16();
            test();
            test();
            if ((L_sub(st->lp_noise, LP_NOISE_THRESH) > 0) ||
            (st->core_fx != ACELP_CORE) ||
            (sub(coder_type, UNVOICED) == 0))
            {
                tmp = 1;
                move16();
            }

            if(pfstat_on_previous==0)
            {
                st->pfstat.reset = 1;
                move16();
            }
            IF ( sub(st->bwidth_fx,NB) == 0)
            {
                st->pfstat.on = 1;
                move16();
                tmp_noise = 0;
                nb_post_filt( L_frame, &(st->pfstat), &tmp_noise, 0, synth2_pe, st->mem_Aq, pitch, GENERIC, st->BER_detect, tmp );
            }
            ELSE
            {
                st->pfstat.on = 0;
                move16();
                tmp_noise = 0;
                nb_post_filt( L_frame, &(st->pfstat), &tmp_noise, 0, synth2_pe, st->mem_Aq, pitch, AUDIO, st->BER_detect, tmp );
            }

            Copy(synth2_pe, synth2, L_frame);

            tmp = synth2[-1];
            move16();
            deemph_fx( synth2, st->preemph_fac, L_frame, &tmp );
        }
        ELSE
        {
            if(pfstat_on_previous==0)
            {
                st->pfstat.reset = 1;
                move16();
            }
            IF ( sub(st->last_bwidth_fx,WB)>=0 )
            {
                st->pfstat.on = 1;
                move16();
                formant_post_filt( &(st->pfstat), synth, st->mem_Aq, synth2, L_frame, st->lp_noise, bitrate, 0 );
            }
            ELSE
            {
                st->pfstat.on = 0;
                move16();
                formant_post_filt( &(st->pfstat), synth, st->mem_Aq, synth2, L_frame, st->lp_noise, bitrate, 1 );
            }
        }

        /*Bass Post-filter */
        tmp8 = 0;
        move16();
        test();
        if( L_sub(st->lp_noise,LP_NOISE_THRESH) > 0 && st->narrowBand )
        {
            tmp8 = 1;
            move16();
        }
        bass_pf_1sf_delay( synth2, pitch, pit_gain, L_frame, L_SUBFR, bpf_noise_buf, pitch_gain_adjust,
        tmp8, &(st->lp_error_ener), &(st->mem_error) );
    }

    /* Output */
    Copy( synth2, signal_out, L_frame );

    /* Update synth2 memory */
    Copy( synth_buf2 + L_frame, st->pst_old_syn_fx, NBPSF_PIT_MAX );


    return;
}


/*---------------------------------------------------------------------*
 * bass_pf_1sf_delay()
 *
 * Perform low-frequency postfiltering
 *---------------------------------------------------------------------*/

static void bass_pf_1sf_delay(
    Word16 *syn,                /* (i) : 12.8kHz synthesis to postfilter                Q0 */
    Word16 *T_sf,               /* (i) : Pitch period for all subframes (T_sf[16])      Q0 */
    Word16 *gainT_sf,           /* (i) : Pitch gain for all subframes (gainT_sf[16])    Q14 */
    Word16 l_frame,             /* (i) : frame length (should be multiple of l_subfr)   Q0 */
    Word16 l_subfr,          /* (i) : sub-frame length (60/64)                       Q0 */
    Word16 *bpf_noise_buf,      /* (i) : harmoninc filtered signal                      Q0 */
    Word16 *gain_factor_param,  /* (i) : gain factor param 0-> no BPF, 3-> full BPF     */
    Word8 disable_bpf,
    Word32 *lp_error_ener,
    Word32 *mem_error
)
{
    Word16 i, sf, i_subfr, T, lg, s1, st, tmp16;
    Word16 gain;
    Word32 tmp, nrg, lp_error, tmp32;
    Word32 ener2;


    assert(bpf_noise_buf != NULL);

    sf = 0;
    move16();
    lp_error = L_shl(*mem_error, 0);

    FOR (i_subfr = 0; i_subfr < l_frame; i_subfr += l_subfr)
    {
        T = T_sf[sf];
        move16();

        lg = sub(sub(l_frame, T), i_subfr);
        if (lg < 0)
        {
            lg = 0;
            move16();
        }
        if (lg > l_subfr)
        {
            lg = l_subfr;
            move16();
        }

        test();
        IF (disable_bpf == 0 && gainT_sf[sf] > 0)
        {
            /* get headroom for used part of syn */
            tmp16 = add(l_subfr, T);
            if (lg>0)
            {
                tmp16 = add(lg, shl(T, 1));
            }
            s1 = getScaleFactor16(syn + sub(i_subfr, T), tmp16);
            s1 = sub(s1, 3);

            tmp = L_deposit_l(1);
            nrg = L_deposit_l(1);

            IF (lg > 0)
            {
                FOR (i = 0; i < lg; i++)
                {
                    tmp32 = L_mult(syn[i+i_subfr-T], 0x4000);
                    tmp32 = L_mac(tmp32, syn[i+i_subfr+T], 0x4000);
                    tmp16 = round_fx(L_shl(tmp32, s1)); /* Q0+s1 */

                    tmp = L_mac0(tmp, shl(syn[i+i_subfr], s1), tmp16); /* Q0+2*s1 */
                    nrg = L_mac0(nrg, tmp16, tmp16); /* Q0+2*s1 */
                }
            }

            IF (sub(lg, l_subfr) < 0)
            {
                FOR (i = lg; i < l_subfr; i++)
                {
                    tmp16 = shl(syn[i+i_subfr-T], s1); /* Q0+s1 */
                    tmp = L_mac0(tmp, shl(syn[i+i_subfr], s1), tmp16); /* Q0+2*s1 */
                    nrg = L_mac0(nrg, tmp16, tmp16); /* Q0+2*s1 */
                }
            }

            /* gain = tmp/nrg; */
            gain = BASOP_Util_Divide3232_Scale(tmp, nrg, &tmp16);
            BASOP_SATURATE_WARNING_OFF;
            gain = shl(gain, tmp16); /* Q15 */
            BASOP_SATURATE_WARNING_ON;

            if (gain < 0)
            {
                gain = 0;
                move16();
            }

            st = sub(norm_l(lp_error), 3);
            test();
            if ((sub(st, s1) < 0) && (lp_error != 0))
            {
                s1 = st;
                move16();
            }

            ener2 = L_deposit_l(0);

            IF (lg > 0)
            {
                FOR (i = 0; i < lg; i++)
                {
                    tmp32 = L_msu0(0, gain, syn[i+i_subfr-T]);
                    tmp32 = L_msu0(tmp32, gain, syn[i+i_subfr+T]);
                    tmp16 = mac_r(tmp32, gain, syn[i+i_subfr]); /* Q0 */

                    lp_error = Mpy_32_16_1(lp_error, 29491/*0.9f Q15*/);
                    lp_error = L_mac(lp_error, tmp16, 0x1000);  /* Q13 */

                    tmp16 = round_fx(L_shl(lp_error, s1)); /* Q0+s1-3 */
                    ener2 = L_mac0(ener2, tmp16, tmp16); /* Q0+(s1-3)*2 */
                }
            }

            IF (sub(lg, l_subfr) < 0)
            {
                FOR (i = lg; i < l_subfr; i++)
                {
                    tmp32 = L_mult0(gain, syn[i+i_subfr]);
                    tmp32 = L_msu0(tmp32, gain, syn[i+i_subfr-T]); /* Q0 */
                    tmp16 = round_fx(tmp32);

                    lp_error = Mpy_32_16_1(lp_error, 29491/*0.9f Q15*/);
                    lp_error = L_mac(lp_error, tmp16, 0x1000);  /* Q13 */

                    tmp16 = round_fx(L_shl(lp_error, s1)); /* Q0+s1-3 */
                    ener2 = L_mac0(ener2, tmp16, tmp16); /* Q0+(s1-3)*2 */
                }
            }

            st = shl(sub(s1, 3), 1);

            IF (ener2 > 0)
            {
                ener2 = L_shr(BASOP_Util_Log2(ener2), 9); /* 15Q16 */
                ener2 = L_add(ener2, L_deposit_h(sub(31, st)));
            }
            ELSE
            {
                ener2 = L_add(0xFFF95B2C, 0); /* log2(0.01) (15Q16) */
            }

            *lp_error_ener = L_add(Mpy_32_16_1(L_sub(*lp_error_ener, ener2), 32440/*0.99f Q15*/), ener2); /* 15Q16 */

            st = add(st, 6);
            ener2 = L_sub(*lp_error_ener, L_deposit_h(sub(31, st)));
            IF (ener2 >= 0)
            {
                tmp16 = add(extract_h(ener2), 1);
                ener2 = L_sub(ener2, L_deposit_h(tmp16));
                tmp = L_shr(tmp, tmp16);
                nrg = L_shr(nrg, tmp16);
            }
            ener2 = BASOP_Util_InvLog2(L_shl(ener2, 9));  /* Q0+2*s1 */

            tmp32 = L_add(L_shr(nrg, 1), L_shr(ener2, 1));
            if (tmp32 == 0) tmp32 = L_deposit_l(1);
            tmp16 = BASOP_Util_Divide3232_Scale(tmp, tmp32, &st);
            BASOP_SATURATE_WARNING_OFF;
            tmp16 = shl(tmp16, sub(st, 2)); /* Q15 */

            if (sub(tmp16, 16384/*0.5f Q15*/) > 0)
            {
                tmp16 = 16384/*0.5f Q15*/;
                move16();
            }
            if (tmp16 < 0)
            {
                tmp16 = 0;
                move16();
            }
            BASOP_SATURATE_WARNING_ON;

            /*Adjust gain*/
            /* full gain = gainLTP*0.5*/
            /* adaptive gain = gainLTP*0.5*max(0.5f*gain_factor_param[sf],0.125f)*/
            tmp16 = round_fx(L_shl(L_mult0(tmp16, s_max(shl(gain_factor_param[sf],2),1)),13));


            /* calculate noise based on voiced pitch */
            IF (lg > 0)
            {
                FOR (i = 0; i < lg; i++)
                {
                    tmp32 = L_msu0(0, tmp16, syn[i+i_subfr-T]);
                    tmp32 = L_msu0(tmp32, tmp16, syn[i+i_subfr+T]);
                    tmp32 = L_mac(tmp32, tmp16, syn[i+i_subfr]);
                    bpf_noise_buf[i+i_subfr] = round_fx(tmp32); /* Q0 */
                }
            }

            IF (sub(lg, l_subfr) < 0)
            {
                FOR (i = lg; i < l_subfr; i++)
                {
                    tmp32 = L_mult0(tmp16, syn[i+i_subfr]);
                    tmp32 = L_msu0(tmp32, tmp16, syn[i+i_subfr-T]);
                    bpf_noise_buf[i+i_subfr] = round_fx(tmp32); /* Q0 */
                }
            }
        }
        ELSE
        {
            set16_fx(bpf_noise_buf+i_subfr, 0, l_subfr);
        }

        sf = add(sf, 1);
    }

    *mem_error = lp_error;
    move32();


    return;
}

/*---------------------------------------------------------------------*
 * cldfb_synth_set_bandsToZero()
 *
 *
 *---------------------------------------------------------------------*/

void cldfb_synth_set_bandsToZero(
    Decoder_State_fx *st,
    Word32 **rAnalysis,
    Word32 **iAnalysis,
    const Word16 nTimeSlots,
    const CLDFB_SCALE_FACTOR scaleFactor
)
{
    Word32 nrgQ31;
    Word32 nrg_band[CLDFB_NO_CHANNELS_MAX], tempQ31, max_nrg;
    Word16 realQ1, imagQ1, flag, offset, WBcnt;
    Word16 perc_detect, perc_miss;
    Word16 i, k, tmp1, tmp2, tmp3, tmp, update_perc;

    realQ1 = 0;
    move16();
    imagQ1 = 0;
    move16();

    set32_fx( nrg_band, 0, CLDFB_NO_CHANNELS_MAX );
    max_nrg = 0;

    offset = 250;
    WBcnt = 20;
    perc_miss = 13107; /*0.80 in Q14*/
    perc_detect = 14746; /*0.90 in Q14*/

    IF(sub(st->VAD,1) == 0)
    {
        st->active_frame_cnt_bwddec = add(st->active_frame_cnt_bwddec,1);
        st->total_frame_cnt_bwddec = add(st->total_frame_cnt_bwddec,1);
        if(sub(st->active_frame_cnt_bwddec, 99) > 0)
        {
            st->active_frame_cnt_bwddec = 100;
            move16();
        }
        if(sub(st->total_frame_cnt_bwddec, 500) > 0)
        {
            st->total_frame_cnt_bwddec = 500;
            move16();
        }

        FOR (i = 0; i < (st->cldfbSyn_fx->no_channels - st->cldfbSyn_fx->bandsToZero); i++)
        {
            nrgQ31 = 0;
            move32();
            FOR (k = 0; k < nTimeSlots; k++)
            {
                /* use 16-bit precision of real and imag buffers */
                realQ1 = extract_l(L_shr(rAnalysis[k][i], 31-(15+scaleFactor.lb_scale)+3));
                imagQ1 = extract_l(L_shr(iAnalysis[k][i], 31-(15+scaleFactor.lb_scale)+3)); /* Q(-3), headroom */
                nrgQ31 = L_mac0(nrgQ31, realQ1, realQ1);
                nrgQ31 = L_mac0(nrgQ31, imagQ1, imagQ1); /* keep in Q(-6) */
            }
            nrg_band[i] = (nrgQ31);
            move16();
            test();
            if(L_sub(nrg_band[i], max_nrg) > 0 && sub(i,11) >= 0)
            {
                max_nrg = nrg_band[i];
                move16();
            }
        }
        FOR(; i < st->cldfbSyn_fx->no_channels; i++)
        {
            nrg_band[i] = 0;
            move16();
        }

        nrgQ31 = 0;
        move16();
        FOR(i = 2; i < 9; i++)
        {
            nrgQ31 = L_add(nrgQ31, Mult_32_16(nrg_band[i], 4681));
        }

        tempQ31 = L_shr(nrgQ31,9);

        st->avg_nrg_LT = L_add(Mult_32_16(st->avg_nrg_LT, 32440), Mult_32_16(tempQ31, 327)); /*0.99*avg_nrg_LT + 0.01*tempQ31*/
        update_perc = 1;
        move16();
        if(st->ini_frame_fx >= 25 && tempQ31 < Mult_32_16(st->avg_nrg_LT, 164))
        {
            update_perc = 0;
            move16();
        }

        flag = 1;
        move16();
        if(max_nrg >= tempQ31)
        {
            flag = 0;
            move16();
        }

        FOR(i = 0; i < WBcnt-1; i++)
        {
            st->flag_buffer[i] = st->flag_buffer[i+1];
            move16();
        }
        st->flag_buffer[WBcnt-1] = flag;
        move16();

        /*long term percentage*/
        IF(sub(update_perc, 1) == 0)
        {
            IF(flag != 0)
            {
                tmp1 = sub(16384, st->perc_bwddec); /*Q14*/

                tmp = norm_s(st->active_frame_cnt_bwddec);
                tmp3 = shl(st->active_frame_cnt_bwddec, tmp); /*Qtmp*/

                tmp2 = div_s(16384,tmp3); /* 1/active_frames in Q15 + Q14 - Qtmp = Q29 - Qtmp */
                tmp2 = mult_r(tmp2, tmp1); /*(1-perc)*(1/active_frames) in Q14 + Q29 - Qtmp - Q15 = Q28 - Qtmp*/
                st->perc_bwddec = add(st->perc_bwddec, shl(tmp2, sub(tmp, 14))); /* Q14 */
            }
            ELSE
            {
                tmp1 = (st->perc_bwddec); /*Q14*/

                tmp = norm_s(st->active_frame_cnt_bwddec);
                tmp3 = shl(st->active_frame_cnt_bwddec, tmp); /*Qtmp*/

                tmp2 = div_s(16384,tmp3); /* 1/active_frames in Q15 + Q14 - Qtmp = Q29 - Qtmp */
                tmp2 = mult_r(tmp2, tmp1); /*(perc)*(1/active_frames) in Q14 + Q29 - Qtmp - Q15 = Q28 - Qtmp*/
                st->perc_bwddec = sub(st->perc_bwddec, shl(tmp2, sub(tmp, 14))); /* Q14 */
            }
        }
        test();
        IF(sub(st->total_frame_cnt_bwddec, offset) > 0 && sub(st->active_frame_cnt_bwddec, 50) > 0)
        {
            IF( (st->perc_bwddec >= perc_detect || (st->perc_bwddec >= perc_miss && st->last_flag_filter_NB)) && (sum16_fx(st->flag_buffer, WBcnt) != 0)) /*decision hysterysis*/
            {
                st->cldfbSyn_fx->bandsToZero = sub( st->cldfbSyn_fx->no_channels, 10 );
                move16();
                st->last_flag_filter_NB = 1;
                move16(); /*VAD processing must be dependent on hysterysis, as if hysterysis fails, but threshold passes, we dont want next vad frames to have NB only*/
            }
            ELSE
            {
                st->last_flag_filter_NB = 0;
                move16();
            }
        }
        ELSE
        {
            st->last_flag_filter_NB = 0;
            move16();
        }
        IF(sum16_fx(st->flag_buffer, WBcnt) == 0)
        {
            st->perc_bwddec = 0;
            st->active_frame_cnt_bwddec = 0;
            move16();
            st->total_frame_cnt_bwddec = 0;
            move16();
            st->last_flag_filter_NB = 0;
            move16();
        }
    }
    ELSE
    {
        IF(st->last_flag_filter_NB == 1)
        {
            st->cldfbSyn_fx->bandsToZero = st->last_active_bandsToZero_bwdec;
            move16();
        }
        st->total_frame_cnt_bwddec = add(st->total_frame_cnt_bwddec, 1);
        if(sub(st->total_frame_cnt_bwddec, 500) > 0)
        {
            st->total_frame_cnt_bwddec = 500;
            move16();
        }
    }

    st->last_active_bandsToZero_bwdec = st->cldfbSyn_fx->bandsToZero;

    return;
}
