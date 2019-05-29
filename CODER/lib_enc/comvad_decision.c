/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/


#include <stdio.h>
#include <stdlib.h>


#include "basop_util.h"
#include "stl.h"
#include "vad_basop.h"
#include "prot_fx.h"
#include "rom_enc_fx.h"




/* only one is activated in below preprocessing*/
/*#define CLDFB_VAD*/                       /* test on the CLDFB-VAD */

static
Word16 comvad_hangover( Word32 lt_snr_org,              /*(i)original long time SNR*/
                        Word32 snr,                     /*(i) frequency domain SNR */
                        Word32 l_snr,                   /*(i) long time frequency domain
                                                          SNR calculated by l_speech_snr and l_silence_snr*/
                        Word32 snr_flux,                /*(i) average tsnr*/
                        Word16 bw_index,                /*(i) band width index*/
                        Word16 vad_flag,
                        Word16 pre_res_hang_num,        /*(i) residual amount of previous  hangover */
                        Word16 continuous_speech_num2,  /*(i) amount of continuous speech frames*/
                        Word16 noisy_type               /*(i) noisy type*/
                      )
{
    Word32 l_snr_add;
    Word16 speech_flag;


    speech_flag = pre_res_hang_num;
    move16();

    IF(sub(bw_index, CLDFBVAD_SWB_ID) == 0)
    {
        IF(vad_flag)
        {
            speech_flag = 4;
            move16();
            if(L_sub(lt_snr_org, 117440509/* 3.5 Q25 */) > 0)
            {
                speech_flag = 3;
                move16();
            }

            test();
            test();
            IF((sub(continuous_speech_num2, 8) < 0)&& (L_sub(lt_snr_org, 134217724/* 4.0 Q25 */) < 0))
            {
                speech_flag = sub(8, continuous_speech_num2);
            }
            ELSE IF((L_sub(snr_flux, 26843545/* 0.8 Q25 */) > 0 )&&(sub(continuous_speech_num2, 24) > 0))
            {
                IF(L_sub(lt_snr_org, 120795952/* 3.6 Q25 */) > 0)
                {
                    speech_flag = 3;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 87241521/* 2.6 Q25 */) > 0 )
                {
                    speech_flag = 3;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 53687090/* 1.6 Q25 */) > 0 )
                {
                    speech_flag = 4;
                    move16();
                }
                ELSE
                {
                    speech_flag = 5;
                    move16();
                }
                speech_flag = sub(speech_flag,1);
            }

            IF(sub(continuous_speech_num2, 120) < 0)
            {
                test();
                IF(L_sub(snr, 50331647/* 1.5 Q25 */)>0)
                {
                    speech_flag = 9;
                    move16();
                }
                ELSE IF(L_sub(snr, 33554431/* 1.0 Q25 */)>0 && sub(speech_flag, 7)<0)
                {
                    speech_flag = 7;
                    move16();
                }
                ELSE IF(sub(speech_flag,3) <0)
                {
                    speech_flag = 3;
                    move16();
                }
                if(sub(speech_flag,3)>0)
                {
                    speech_flag =sub(speech_flag,2);
                }
            }
            ELSE
            {
                IF(L_sub(lt_snr_org, 120795952/* 3.6 Q25 */) > 0)
                {
                    speech_flag = 1;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 100663293/* 3.0 Q25 */) > 0)
                {
                    speech_flag = 2;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 83886078/* 2.5 Q25 */) > 0)
                {
                    speech_flag = 3;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 67108862/* 2.0 Q25 */) > 0)
                {
                    speech_flag = 3;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 50331647/* 1.5 Q25 */)> 0)
                {
                    speech_flag = 4;
                    move16();
                }
                ELSE
                {
                    speech_flag = 5;
                    move16();
                }
            }

            if(sub(noisy_type, SILENCE)==0)
            {
                speech_flag = 6;
                move16();
            }
        }
    }
    ELSE IF(sub(bw_index, CLDFBVAD_WB_ID) == 0)
    {
        IF(vad_flag)
        {
            IF(L_sub(lt_snr_org, 117440509/* 3.5 Q25 */) > 0)
            {
                speech_flag = 1;
                move16();
            }
            ELSE
            {
                speech_flag = 2;
                move16();
            }

            test();
            test();
            IF((sub(continuous_speech_num2, 8) < 0) && (L_sub(lt_snr_org, 134217724/* 4.0 Q25 */) <0 ))
            {
                speech_flag = sub(8, continuous_speech_num2);
            }
            ELSE IF((L_sub(snr_flux, 30198988/* 0.9 Q25 */) > 0) && (sub(continuous_speech_num2, 50) > 0))
            {
                IF(L_sub(lt_snr_org, 120795952/* 3.6 Q25 */) > 0)
                {
                    speech_flag = 1;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 87241521/* 2.6 Q25 */) > 0)
                {
                    speech_flag = 5;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 53687090/* 1.6 Q25 */) > 0)
                {
                    speech_flag = 6;
                    move16();
                }
                ELSE
                {
                    speech_flag = 7;
                    move16();
                }
                IF(sub(speech_flag , 1)>0)
                {
                    speech_flag = sub(speech_flag , 1);
                }
            }

            IF(sub(continuous_speech_num2, 120)< 0)
            {
                test();
                test();
                test();
                IF(L_sub(snr, 50331647/* 1.5 Q25 */)>0)
                {
                    speech_flag = 6;
                    move16();
                }
                ELSE IF(L_sub(snr, 33554431/* 1.0 Q25 */)>0 && sub(speech_flag, 5) < 0)
                {
                    speech_flag = 5;
                    move16();
                }
                ELSE IF(L_sub(snr, 26843545/* 0.8 Q25 */)>0 && L_sub(lt_snr_org,67108862/* 2.0 Q25 */) < 0 && sub(speech_flag, 4) < 0)
                {
                    speech_flag = 4;
                    move16();
                }
                ELSE IF(sub(speech_flag, 3) < 0)
                {
                    speech_flag = 3;
                    move16();
                }
            }
            ELSE
            {
                IF(L_sub(lt_snr_org,120795952/* 3.6 Q25 */) > 0)
                {
                    speech_flag = 1;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 100663293/* 3.0 Q25 */) > 0)
                {
                    speech_flag = 2;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 83886078/* 2.5 Q25 */) > 0)
                {
                    speech_flag = 2;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 67108862/* 2.0 Q25 */) > 0)
                {
                    speech_flag = 3;
                    move16();
                }
                ELSE {
                    speech_flag = 3;
                    move16();
                }
            }

            if(sub(noisy_type, SILENCE)==0)
            {
                speech_flag = 6;
                move16();
            }
        }
    }
    ELSE /* NB */
    {
        IF(vad_flag)
        {
            l_snr_add = L_add(0x0199999a,MUL_F(l_snr,0x0ccd));

            IF(L_sub(lt_snr_org, 117440509/* 3.5 Q25 */) > 0)
            {
                speech_flag = 3;
                move16();
            }
            ELSE
            {
                speech_flag = 4;
                move16();
            }

            test();
            test();
            IF((sub(continuous_speech_num2,8) < 0)&& (L_sub(lt_snr_org, 134217724/* 4.0 Q25 */) < 0))
            {
                speech_flag = sub(8, continuous_speech_num2);
            }
            ELSE IF((L_sub(snr_flux, l_snr_add) > 0)&&(sub(continuous_speech_num2, 24) > 0))
            {
                IF(L_sub(lt_snr_org, 120795952/* 3.6 Q25 */) > 0)
                {
                    speech_flag = 3;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 87241521/* 2.6 Q25 */) > 0)
                {
                    speech_flag = 8;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 40265317/* 1.2 Q25 */) > 0)
                {
                    speech_flag = 10;
                    move16();
                }
                ELSE
                {
                    speech_flag = 12;
                    move16();
                }

                IF(sub(speech_flag ,2)>0)
                {
                    speech_flag = sub(speech_flag,2);
                }
            }

            IF(sub(continuous_speech_num2, 120) < 0)
            {
                test();
                test();
                IF(L_sub(snr, 50331647/* 1.5 Q25 */)>0)
                {
                    speech_flag = 10;
                    move16();
                }
                ELSE IF(L_sub(snr, 33554431/* 1.0 Q25 */)>0 && sub(speech_flag,7) < 0)
                {
                    speech_flag = 7;
                    move16();
                }
                ELSE IF(sub(speech_flag, 3)<0 && sub(continuous_speech_num2, 12) > 0)
                {
                    speech_flag = 3;
                    move16();
                }
            }
            ELSE
            {
                IF(L_sub(lt_snr_org, 120795952/* 3.6 Q25 */) > 0)
                {
                    speech_flag = 2;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 100663293/* 3.0 Q25 */) > 0)
                {
                    speech_flag = 2;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 83886078/* 2.5 Q25 */) > 0)
                {
                    speech_flag = 3;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 67108862/* 2.0 Q25 */) > 0)
                {
                    speech_flag = 3;
                    move16();
                }
                ELSE IF(L_sub(lt_snr_org, 50331647/* 1.5 Q25 */)> 0)
                {
                    speech_flag = 4;
                    move16();
                }
                ELSE
                {
                    speech_flag = 4;
                    move16();
                }
            }

            if(sub(noisy_type, SILENCE) == 0)
            {
                speech_flag = 2;
                move16();
            }
        }
    }


    IF((sub(vad_flag,1)==0))
    {
        IF((sub(noisy_type, SILENCE) != 0))
        {
            speech_flag--;
        }
        ELSE
        {
            speech_flag = sub(speech_flag, 3);
        }
        speech_flag = s_max(speech_flag, 0);
    }


    return speech_flag;
}



Word16 comvad_decision( T_CldfbVadState *st,
                        Word32 l_snr,                /*(i) long time frequency domain*/
                        Word32 lt_snr_org,           /*(i) original long time SNR*/
                        Word32 lt_snr,               /*(i) long time SNR calculated by fg_energy and bg_energy*/
                        Word32 snr_flux,             /*(i) average tsnr of several frames*/
                        Word32 snr,                  /*(i) frequency domain SNR */
                        Word32 tsnr,                 /*(i) time domain SNR */
                        Word32 frame_energy,         /*(i) current frame energy */
                        Word16 music_backgound_f,    /*(i) background music flag*/
                        Word16 frame_energy_Q,       /*(i) the Scaling of current frame energy*/
                        Word16 *cldfb_addition,        /*o: adjust the harmonized hangover */
                        Word16 vada_flag
                      )
{
    Word16 speech_flag;
    Word32 fg_energy;
    Word32 bg_energy;
    Word32 tmp_snr;
    Word16 vad_flag;
    Word16 vadb_flag;
    Word32 l_silence_snr_count;
    Word32 snr_sub;
    Word32 snr_div_fix32;
    Word32 l_silence_snr;

    Word16 snr_div_fix;
    Word16 Qnorm_silence,Qnorm_silence_count;
    Word16 tmpout;
    Word16 noisy_type;
    Word32 lt_snr_org_cmp;



    /* avoid overflows in the following if conditions */
    tsnr = L_shr(tsnr,1);

    noisy_type = UNKNOWN_NOISE;
    move16();
    speech_flag = st->speech_flag;
    move16();
    fg_energy = L_add(st->fg_energy,0);
    bg_energy = L_add(st->bg_energy,0);

    Qnorm_silence = 0;
    move16();
    Qnorm_silence_count = 0;
    move16();

    test();
    IF(L_sub(st->lf_snr_smooth, LS_MIN_SILENCE_SNR[st->bw_index - CLDFBVAD_NB_ID] ) > 0 && L_sub(lt_snr_org, LT_MIN_SILENCE_SNR[st->bw_index - CLDFBVAD_NB_ID] )> 0)
    {
        noisy_type = SILENCE;
        move16();
    }

    tmp_snr = construct_snr_thresh( st->sp_center,
                                    snr_flux,
                                    lt_snr,
                                    l_snr,
                                    st->continuous_speech_num,
                                    st->continuous_noise_num,
                                    st->fg_energy_est_start,
                                    st->bw_index);

    {
        vad_flag = 0;
        move16();
        if(L_sub(snr, tmp_snr) > 0)
        {
            vad_flag = 1;
            move16();
        }

        if(L_sub(tsnr, 67108862/* 4.0/2.0 Q25 */) > 0 )
        {
            vad_flag = 1;
            move16();
        }
    }

    IF(sub(st->frameloop, 25) > 0)
    {
        test();
        IF(sub(vad_flag, 1) == 0 && L_sub(st->fg_energy_est_start, 1) == 0)
        {
            Word32 frame_energy_mult_fix32,bg_energy_mult_fix32;
            Word16 frame_energy_mult_Q,bg_energy_mult_Q;

            IF(sub(st->fg_energy_count, 512) == 0)
            {
                fg_energy = MUL_F(fg_energy, 0x6000);
                st->fg_energy_count = 384;
                move16();
            }

            frame_energy_mult_fix32 = MUL_F(frame_energy, st->bg_energy_count);
            frame_energy_mult_Q = sub(frame_energy_Q, 15);

            bg_energy_mult_fix32 = MUL_F(bg_energy, 6);
            bg_energy_mult_Q = sub(st->bg_energy_scale, 15);

            IF(sub(frame_energy_mult_Q, bg_energy_mult_Q) > 0)
            {
                frame_energy_mult_fix32 = L_shr(frame_energy_mult_fix32,sub(frame_energy_mult_Q, bg_energy_mult_Q));
            }
            IF(sub(frame_energy_mult_Q, bg_energy_mult_Q) < 0)
            {
                bg_energy_mult_fix32 = L_shr(bg_energy_mult_fix32,limitScale32(sub(bg_energy_mult_Q, frame_energy_mult_Q)));
            }

            IF(L_sub(frame_energy_mult_fix32, bg_energy_mult_fix32) > 0)
            {
                fg_energy = VAD_L_ADD(fg_energy, st->fg_energy_scale, frame_energy, frame_energy_Q, &st->fg_energy_scale);
                st->fg_energy_count = add(st->fg_energy_count, 1);
                move16();
            }
        }
    }

    if(music_backgound_f)
    {
        vad_flag = 1;
        move16();
    }

    IF(sub(vad_flag, 1) == 0)
    {
        IF (st->l_silence_snr == 0)
        {
            snr_div_fix = 0;
            move16();
        }
        ELSE
        {
            Qnorm_silence=sub(norm_l(st->l_silence_snr), 1);
            Qnorm_silence_count=norm_l(st->l_silence_snr_count);
            l_silence_snr = L_shl(st->l_silence_snr,Qnorm_silence);
            l_silence_snr_count = L_shl(st->l_silence_snr_count,Qnorm_silence_count);
            snr_div_fix = div_l(l_silence_snr,extract_h(l_silence_snr_count));
        }
        snr_sub = L_sub(snr, 0x3000000);
        snr_div_fix32 = L_deposit_l(snr_div_fix);
        snr_div_fix32 = L_shr(snr_div_fix32, add(6, sub(Qnorm_silence, Qnorm_silence_count)));


        IF(L_sub(snr_sub, snr_div_fix32) > 0)
        {
            IF(L_sub(st->l_speech_snr_count, 512) == 0)
            {
                st->l_speech_snr = L_add(MUL_F(st->l_speech_snr, 0x6000), L_shr(snr, 9));
                move32();
                st->l_speech_snr_count = L_deposit_l(384+1);
                move32();
            }
            ELSE
            {
                st->l_speech_snr = L_add(st->l_speech_snr, L_shr(snr, 9));
                move32();
                st->l_speech_snr_count = L_add(st->l_speech_snr_count, 1);
                move32();
            }
        }
    }

    lt_snr_org_cmp = L_sub(lt_snr_org, 117440509/* 3.5 Q25 */);

    IF(sub(st->bw_index, CLDFBVAD_NB_ID) == 0)
    {
        Word32 lt_snr_add;

        lt_snr_add = L_add(0x03cccccd, MUL_F(lt_snr, 0x23d7));

        if(L_sub(snr_flux, lt_snr_add) > 0)
        {
            vad_flag = 1;
            move16();
        }

        test();
        test();
        if( (L_sub(snr_flux, 50331647/* 1.5 Q25 */) > 0) && (sub(st->sp_center[3], 1637/* 1.6 Q10 */) > 0) && (lt_snr_org_cmp < 0) )
        {
            vad_flag = 1;
            move16();
        }

        test();
        test();
        if( (L_sub(snr_flux,  40265317/* 1.2 Q25 */) > 0) && (sub(st->sp_center[3], 1944/* 1.9 Q10 */) > 0) && (lt_snr_org_cmp < 0) )
        {
            vad_flag = 1;
            move16();
        }

        test();
        test();
        if((L_sub(snr_flux, 33554431/* 1.0 Q25 */) > 0) && (sub(st->sp_center[3], 3274/* 3.2 Q10 */) > 0) && (lt_snr_org_cmp < 0))
        {
            vad_flag = 1;
            move16();
        }
    }
    ELSE IF(sub(st->bw_index, CLDFBVAD_WB_ID) == 0)
    {
        Word32 lt_snr_add;

        lt_snr_add = L_add(0x04333333, MUL_F(lt_snr, 0x1eb8));

        if(L_sub(snr_flux, lt_snr_add) > 0)
        {
            vad_flag = 1;
            move16();
        }

        test();
        test();
        if((L_sub(snr_flux, 53687090/* 1.6 Q25 */) > 0 ) && (sub(st->sp_center[3], 2558/* 2.5 Q10 */) > 0) && (lt_snr_org_cmp < 0))
        {
            vad_flag = 1;
            move16();
        }

        test();
        test();
        if((L_sub(snr_flux, 40265317/* 1.2 Q25 */) > 0) && (sub(st->sp_center[3], 2864/* 2.8 Q10 */) > 0) && (lt_snr_org_cmp < 0))
        {
            vad_flag = 1;
            move16();
        }

        test();
        test();
        if((L_sub(snr_flux, 33554431/* 1.0 Q25 */) > 0) && (sub(st->sp_center[3], 4604/* 4.5 Q10 */) > 0) && (lt_snr_org_cmp < 0))
        {
            vad_flag = 1;
            move16();
        }
    }
    ELSE
    {
        Word32 lt_snr_add;

        lt_snr_add = L_add(0x04333333, MUL_F(lt_snr, 0x28f5));

        if((L_sub(snr_flux, lt_snr_add) > 0))
        {
            vad_flag = 1;
            move16();
        }

        test();
        test();
        if((L_sub(snr_flux, 56371444/* 1.68 Q25 */) > 0) && (sub(st->sp_center[3], 2823/* 2.76 Q10 */) > 0) && (lt_snr_org_cmp < 0))
        {
            vad_flag = 1;
            move16();
        }

        test();
        test();
        if((L_sub(snr_flux, 41607494/* 1.24 Q25 */) > 0) && (sub(st->sp_center[3], 2987/* 2.92 Q10 */) > 0) && (lt_snr_org_cmp < 0))
        {
            vad_flag = 1;
            move16();
        }

        test();
        test();
        if((L_sub(snr_flux, 36909874/* 1.10 Q25 */) > 0) && (sub(st->sp_center[3], 4706/* 4.6 Q10 */) > 0) && (lt_snr_org_cmp < 0))
        {
            vad_flag = 1;
            move16();
        }
    }


    IF(st->fg_energy_est_start==0)
    {
        tmpout = VAD_L_CMP(frame_energy, frame_energy_Q, 50, 0);

        test();
        test();
        if(sub(st->ltd_stable_rate[0], 2621/* 0.08 Q15 */) > 0 && sub(vad_flag,1) == 0 && (tmpout> 0))
        {
            st->fg_energy_est_start = L_deposit_l(1);
        }
    }

    speech_flag = comvad_hangover(  lt_snr_org,
                                    snr,
                                    l_snr,
                                    snr_flux,
                                    st->bw_index,
                                    vad_flag,
                                    speech_flag,
                                    st->continuous_speech_num2,
                                    noisy_type);



    test();
    IF(vad_flag == 0 && speech_flag > 0)
    {
        speech_flag = sub(speech_flag, 1);
        vad_flag = 1;
        move16();
    }
    vadb_flag = vad_flag;

    IF(sub(st->bw_index, CLDFBVAD_SWB_ID) == 0)
    {
        test();
        test();
        test();
        IF(sub(SILENCE, noisy_type) == 0
           && L_sub(snr, 6710886/* 0.2 Q25 */) > 0
           && vad_flag == 0)
        {
            vad_flag = vada_flag;
            move16();
        }
        ELSE IF(L_sub(st->lf_snr_smooth,352321526/* 10.5 Q25 */)<0 || sub(SILENCE, noisy_type) != 0)
        {
            test();
            test();
            test();
            IF(L_sub(snr_flux, 67108862/* 2.0 Q25 */) > 0
               || (st->continuous_speech_num2 > 40 && L_sub(snr_flux, 60397976/* 1.8 Q25 */) > 0)
               || music_backgound_f == 1)
            {
                vad_flag = s_or(vad_flag, vada_flag);
            }
            ELSE IF(sub(SILENCE, noisy_type) == 0)
            {
                vad_flag = vada_flag;
            }
        }

    }
    ELSE IF(sub(st->bw_index, CLDFBVAD_WB_ID) == 0)
    {
        test();
        test();
        test();
        IF(sub(SILENCE, noisy_type) == 0
           && L_sub(snr, 6710886/* 0.2 Q25 */) > 0
           && vad_flag == 0)
        {
            vad_flag = vada_flag;
            move16();
        }
        ELSE IF(L_sub(st->lf_snr_smooth,352321526/* 10.5 Q25 */)<0 || sub(SILENCE, noisy_type) != 0)
        {
            test();
            test();
            test();
            IF(L_sub(snr_flux, 73819748/* 2.2 Q25 */) > 0
               || (st->continuous_speech_num2 > 40 && L_sub(snr_flux, 50331647/* 1.5 Q25 */) > 0)
               || music_backgound_f == 1)
            {
                vad_flag = s_or(vad_flag, vada_flag);
            }
            ELSE IF(sub(SILENCE, noisy_type) == 0)
            {
                vad_flag = vada_flag;
            }

        }

    }
    ELSE
    {
        IF(sub(SILENCE, noisy_type) == 0)
        {
            test();
            IF(L_sub(st->lf_snr_smooth , 419430388/* 12.5 Q25 */) > 0
            && music_backgound_f == 0)
            {
                vad_flag = vada_flag;
            }
        }
        ELSE
        {
            test();
            test();
            test();
            IF(L_sub(snr_flux, 67108862/* 2.0 Q25 */) > 0
            || (st->continuous_speech_num2 > 30 && L_sub(snr_flux, 50331647/* 1.5 Q25 */) > 0)
            || music_backgound_f == 1)
            {
                vad_flag = s_or(vad_flag, vada_flag);
            }
        }
    }

    IF(vad_flag == 0)
    {
        IF(L_sub(st->l_silence_snr_count, 512) == 0)
        {
            st->l_silence_snr = L_add(MUL_F(st->l_silence_snr, 0x6000),L_shr(snr, 9));
            move32();
            st->l_silence_snr_count = L_deposit_l(384+1);
            move32();
        }
        ELSE IF(L_sub(snr, 26843545/* 0.8 Q25 */) < 0)
        {
            st->l_silence_snr = L_add(st->l_silence_snr, L_shr(snr,9));
            move32();
            st->l_silence_snr_count = L_add(st->l_silence_snr_count, 1);
            move32();
        }
    }

    IF(add(vad_flag, vada_flag) == 0)
    {
        IF(sub(st->bg_energy_count, 512) == 0)
        {
            bg_energy = MUL_F(bg_energy, 0x6000);
            st->bg_energy_count = 384;
            move16();
        }

        IF(L_sub(tsnr, 16777216/* 1.0/2.0 Q25 */) < 0)
        {
            bg_energy = VAD_L_ADD(bg_energy, st->bg_energy_scale, frame_energy, frame_energy_Q, &st->bg_energy_scale);
            st->bg_energy_count = add(st->bg_energy_count, 1);
            move16();
        }
    }

    test();
    st->vad_flag_for_bk_update = vad_flag;
    IF(sub(st->update_count, 12) < 0 && sub(vadb_flag, 1)==0)
    {
        st->warm_hang_num = s_max(20, speech_flag);
    }
    test();
    IF(vad_flag == 0 && st->warm_hang_num > 0)
    {
        st->warm_hang_num = sub(st->warm_hang_num, 1);
        vad_flag = 1;
        move16();
    }




    st->lt_snr_org = lt_snr_org;
    move32();
    st->fg_energy = fg_energy;
    move32();
    st->bg_energy = bg_energy;
    move32();

    st->speech_flag = speech_flag;
    move16();

    move16();
    test();
    IF(sub(noisy_type, SILENCE) == 0
       && sub(st->bw_index, CLDFBVAD_NB_ID) != 0)
    {
        *cldfb_addition = 2;
    }
    ELSE
    {
        *cldfb_addition = 0;

        if(sub(st->bw_index, CLDFBVAD_WB_ID)==0)
        {
            *cldfb_addition = 3;
            move16();
        }
        if(sub(st->bw_index, CLDFBVAD_SWB_ID)==0)
        {
            *cldfb_addition = 1;
            move16();
        }
        if(sub(st->bw_index, CLDFBVAD_NB_ID)==0)
        {
            *cldfb_addition = 1;
            move16();
        }

    }




    return vad_flag;
}

