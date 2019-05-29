/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/


#include "basop_util.h"
#include "stl.h"
#include "vad_basop.h"
#include "prot_fx.h"

void bg_music_decision(T_CldfbVadState *st,
                       Word16  *music_backgound_f,  /*(i)  background music flag*/
                       Word32  frame_energy,        /*(i)  current frame energy 1*/
                       Word16  frame_energy_Q       /*(i)  the Scaling of current frame energy*/
                      )
{

    Word16 *f_tonality_rate = st->f_tonality_rate;
    Word16 *ltd_stable_rate = st->ltd_stable_rate;
    Word16 *sp_center = st->sp_center;
    Word16 *sSFM = st->sfm;
    Word16 cmp_result;
    Word32 music_background_frame;
    Word32 tmp1;
    Word16 tmp1_Q;
    Word16 tmp_norm;
    Word32 frame_energy_Mcount;
    Word16 fg_energy_Qtmp;


    music_background_frame = L_add(0,0);
    tmp1 = L_mult(st->fg_energy_count,18842);
    tmp1_Q = norm_l(tmp1);
    tmp1 = L_shl(tmp1,tmp1_Q);
    tmp_norm = extract_h(tmp1);
    frame_energy_Mcount = MUL_F(frame_energy,tmp_norm);

    fg_energy_Qtmp = sub(frame_energy_Q, 18);
    fg_energy_Qtmp = add(fg_energy_Qtmp, tmp1_Q);

    cmp_result = VAD_L_CMP(frame_energy_Mcount,fg_energy_Qtmp,st->fg_energy,st->fg_energy_scale);

    test();
    IF((sub(f_tonality_rate[1],9830/* 0.6 Q14 */)>0)
       ||(sub(f_tonality_rate[0],14089/* 0.86 Q14 */)>0))
    {

        test();
        test();
        test();
        test();
        if((sub(ltd_stable_rate[0], 2359/* 0.072 Q15 */)<0)
                &&(sub(sp_center[0],1228/* 1.2 Q10 */)>0)
                &&((sub(sSFM[0],24903/* 0.76 Q15 */)<0)
                   ||(sub(sSFM[1],28835/* 0.88 Q15 */)<0)
                   ||(sub(sSFM[2],31456/* 0.96 Q15 */)<0)))
        {
            music_background_frame = L_add(1,0);
        }
    }

    test();
    test();
    IF(music_background_frame
       &&(cmp_result > 0)
       &&(L_sub(st->fg_energy_est_start,1)==0))
    {
        st->music_background_rate = add(mult(st->music_background_rate,31949), 819);
        move16();
    }
    ELSE IF(music_background_frame)
    {
        st->music_background_rate = add(mult(st->music_background_rate,32702), 66);
        move16();
    }
    ELSE
    {
        st->music_background_rate = mult(st->music_background_rate,32670);
        move16();
    }

    *music_backgound_f = 0;
    move16();
    if(sub(st->music_background_rate, 16384)> 0)
    {
        *music_backgound_f = 1;
        move16();
    }

}

Word16 update_decision(T_CldfbVadState *st,
                       Word32 frame_energy, /*(i) current frame energy*/
                       Word32 HB_Power,/*(i) current frame high frequency energy*/
                       Word16 frameloop,/*(i) amount of  frames*/
                       Word16 bw,  /*(i) band width index*/
                       Word16 frame_energy_Q,  /*(i) the Scaling of current frame energy*/
                       Word16 HB_Power_Q,	/*(i) the Scaling of current frame high frequency energy*/
                       Word32 snr, /*(i) frequency domain SNR */
                       Word32 tsnr, /*(i) time domain SNR */
                       Word16 vad_flag,
                       Word16 music_backgound_f/*(i) background music flag*/
                      )
{
    Word16 *sp_center = st->sp_center;
    Word16 *ltd_stable_rate = st->ltd_stable_rate;
    Word16 *sSFM = st->sfm;
    Word16 *f_tonality_rate = st->f_tonality_rate;

    Word16 update_flag;
    Word16 tonality_flag;
    Word32 tmp1;
    Word16 tmp1_Q;
    Word16 tmp_norm;
    Word32 frame_energy_Mcount;
    Word32 lt_bg_highf_eng_trbl;
    Word16 flag_high_eng;
    Word16 fg_energy_Qtmp;
    Word16 cmp_result;
    Word16 tmpout;
    Word16 sp_center3_diff;

    Word16 Q_counter;
    Word16 Q_sum;
    Word32 sum_num,counter_den;
    Word16 div_r;

    Word16 div_r_Q;
    Word32 div_r_32;


    update_flag = 1;
    move16();
    tonality_flag = 0;
    move16();

    lt_bg_highf_eng_trbl = MUL_F(st->lt_bg_highf_eng,24576);
    IF (sub(14,HB_Power_Q)>0)
    {
        lt_bg_highf_eng_trbl = L_shr(lt_bg_highf_eng_trbl,sub(14,HB_Power_Q));
    }
    ELSE
    {
        HB_Power = L_shr(HB_Power, s_min(31, sub(HB_Power_Q, 14)));
    }

    flag_high_eng = 0;
    move16();
    if (L_sub(HB_Power,lt_bg_highf_eng_trbl)>0)
    {
        flag_high_eng = 1;
        move16();
    }

    sp_center3_diff = sub(sp_center[3], st->lt_noise_sp_center3);

    IF(sub(frameloop, 50) > 0)
    {
        if(sub(ltd_stable_rate[0],3932/* 0.12 Q15 */)>0)

        {
            update_flag = 0;
            move16();
        }

        tmpout= VAD_L_CMP(frame_energy, sub(frame_energy_Q,2), st->frame_energy_smooth,st->frame_energy_smooth_scale);
        test();
        IF((sub(bw, CLDFBVAD_NB_ID) != 0) && tmpout>0 )
        {
            test();
            if((flag_high_eng) && (sub(sp_center3_diff, 409)> 0))
            {
                update_flag = 0;
                move16();
            }
            test();
            if((sub(sp_center[3],2864/* 2.8 Q10 */) >0)&&(sub(ltd_stable_rate[0], 655/* 0.02 Q15 */)>0))
            {
                update_flag = 0;
                move16();
            }
        }
    }

    test();
    if((sub(f_tonality_rate[1], 8192/* 0.5 Q14 */)>0 )
            && (sub(ltd_stable_rate[0],3277/* 0.1 Q15 */)>0))
    {
        update_flag = 0;
        move16();
    }

    test();
    test();
    IF((sub(sSFM[1], 30146/* 0.92 Q15 */) <0 )
       && (sub(sSFM[0], 30146/* 0.92 Q15 */)< 0)
       && (sub(sSFM[2], 30146/* 0.92 Q15 */)< 0))
    {
        update_flag = 0;
        move16();
    }

    test();
    test();
    IF((sub(sSFM[0], 26214/* 0.8 Q15 */)<0 )
       || (sub(sSFM[1],25558/* 0.78 Q15 */) <0 )
       || (sub(sSFM[2],26214/* 0.8 Q15 */) <0 ))
    {
        update_flag = 0;
        move16();
    }

    /*if(frame_energy > 32*st->frame_energy_smooth)*/
    tmpout= VAD_L_CMP(frame_energy, frame_energy_Q, st->frame_energy_smooth,sub(st->frame_energy_smooth_scale,5));

    if(tmpout>0)
    {
        update_flag = 0;
        move16();
    }
    tmp1 = L_mult(st->fg_energy_count,18842);
    tmp1_Q = norm_l(tmp1);
    tmp1 = L_shl(tmp1,tmp1_Q);
    tmp_norm = extract_h(tmp1);
    frame_energy_Mcount = MUL_F(frame_energy,tmp_norm);

    fg_energy_Qtmp = sub(frame_energy_Q, 18);
    fg_energy_Qtmp = add(fg_energy_Qtmp, tmp1_Q);

    cmp_result = VAD_L_CMP(frame_energy_Mcount,fg_energy_Qtmp,st->fg_energy,st->fg_energy_scale);

    tmpout = VAD_L_CMP(frame_energy, frame_energy_Q, 3, 0);

    test();
    test();
    if((cmp_result>0)
            &&(L_sub(st->fg_energy_est_start,1)==0)
            &&(tmpout>0))
    {
        update_flag = 0;
        move16();
    }

    test();
    IF((sub(f_tonality_rate[1],9830/* 0.6 Q14 */)>0)
       ||(sub(f_tonality_rate[0],14089/* 0.86 Q14 */)>0))
    {
        update_flag = 0;
        move16();
        tonality_flag = 1;
        move16();
    }

    st->tonality_rate3 = mult(st->tonality_rate3, 32211);
    move16();
    if(tonality_flag)
    {
        st->tonality_rate3 = add(mult(st->tonality_rate3,32211), 557);
        move16();
    }

    if(sub(st->tonality_rate3, 16384)>0)
    {
        update_flag = 0;
        move16();
    }

    test();
    if((sub(sp_center[0], 4092/* 4.0 Q10 */)> 0)
            && (sub(ltd_stable_rate[0], 1311/* 0.04 Q15 */)>0))
    {
        update_flag = 0;
        move16();
    }

    test();
    test();
    IF((sub(f_tonality_rate[1], 7536/* 0.46 Q14 */)> 0)
       && ((sub(sSFM[1],30473/* 0.93 Q15 */) > 0)
           ||(sub(ltd_stable_rate[0], 2949/* 0.09 Q15 */)>0)))

    {
        update_flag = 0;
        move16();
    }

    test();
    test();
    test();
    IF( (sub(sSFM[1],30473/* 0.93 Q15 */) < 0
         && sub(sSFM[0],  30146/* 0.92 Q15 */)< 0
         && sub(sSFM[2], 31784/* 0.97 Q15 */)< 0)
        && (sub(f_tonality_rate[1], 8192/* 0.5 Q14 */)> 0))
    {
        update_flag = 0;
        move16();
    }

    test();
    test();
    IF((f_tonality_rate[1] > 7045/* 0.43 Q14 */)
       &&(sSFM[0] < 31129/* 0.95 Q15 */)
       &&(sp_center[1] > 1985/* 1.94 Q10 */))
    {
        update_flag = 0;
        move16();
    }

    IF(sub(update_flag, 1)==0)
    {
        if(sub(st->update_count, 1000) < 0)
        {
            st->update_count = add(st->update_count,1);
        }
    }


    IF(update_flag)
    {
        st->lt_noise_sp_center3 = add(mult(st->lt_noise_sp_center3,29491),mult(sp_center[3],3277));
        move16();
    }



    tmpout= VAD_L_CMP(frame_energy, frame_energy_Q, st->frame_energy_smooth,sub(st->frame_energy_smooth_scale,2));


    test();
    test();
    test();
    test();
    if((tmpout>0)
            &&(sub(frameloop, 100) < 0)
            &&(sub(f_tonality_rate[1],9174/* 0.56 Q14 */)<0)
            &&((sub(sp_center[0], 1391/* 1.36 Q10 */)<0)||sub(ltd_stable_rate[0],983/* 0.03 Q15 */)<0))
    {
        update_flag = 1;
        move16();
    }

    test();
    test();
    test();
    test();
    test();
    test();
    if((L_sub(snr,10066329/* 0.3 Q25 */) <0) && tmpout < 0
            &&(L_sub(L_shr(tsnr,1), 20132659/* 1.2/2.0 Q25 */)<0)
            &&(vad_flag==0)
            &&(sub(st->f_tonality_rate[1],8192/* 0.5 Q14 */)<0)
            &&(music_backgound_f==0)
            &&(sub(st->ltd_stable_rate[3],3277/* 0.1 Q15 */)<0))
    {
        update_flag = 1;
        move16();
    }

    test();
    test();
    test();
    IF(vad_flag && L_sub(snr, 33554431/* 1.0 Q25 */)>0 && sub(bw, CLDFBVAD_SWB_ID)==0 && tmpout > 0)
    {
        update_flag = 0;
    }

    test();
    test();
    test();
    IF(vad_flag && L_sub(snr, 50331647/* 1.5 Q25 */)>0 && sub(bw, CLDFBVAD_SWB_ID)!=0 && tmpout > 0)
    {
        update_flag = 0;
    }


    IF(update_flag == 0)
    {
        st->updateNumWithSnr = 0;
    }
    ELSE
    {
        test();
        test();
        IF(vad_flag && L_sub(snr, 100663293/* 3.0 Q25 */)>0  && sub(st->updateNumWithSnr, 10) < 0)
        {
            update_flag = add(0, 0);
            st->updateNumWithSnr = add(st->updateNumWithSnr, 1);
        }
    }


    test();
    IF(vad_flag==0||sub(update_flag,1) == 0)
    {
        Word16 tmp_fix;
        tmp_fix = sub(st->sp_center[2],st->lt_noise_sp_center0);
        tmp_fix = abs_s(tmp_fix);
        if (sub(tmp_fix, 2558/* 2.5 Q10 */)>0)
        {
            tmp_fix = 2558/* 2.5 Q10 */;
            move16();
        }
        st->lt_noise_sp_center_diff_sum = L_add(st->lt_noise_sp_center_diff_sum ,tmp_fix);
        move32();

        st->lt_noise_sp_center_diff_counter = L_add(st->lt_noise_sp_center_diff_counter,1);
        move32();
        IF(L_sub(st->lt_noise_sp_center_diff_counter, 128)==0)
        {
            st->lt_noise_sp_center_diff_sum = MUL_F(st->lt_noise_sp_center_diff_sum,24576);
            move32();
            st->lt_noise_sp_center_diff_counter = 96;
            move32();
        }

        move16();
        move16();
        IF(sub((Word16)abs_s(sub(st->sp_center[0],st->lt_noise_sp_center0)), 2455/* 2.4 Q10 */) > 0)
        {
            st->lt_noise_sp_center0 = add(mult(st->lt_noise_sp_center0,32637),mult(st->sp_center[0],131));
            move16();
        }
        ELSE IF(sub((Word16)abs_s(sub(st->sp_center[0],st->lt_noise_sp_center0)), 1023/* 1.0 Q10 */) >0)
        {
            st->lt_noise_sp_center0 = add(mult(st->lt_noise_sp_center0,32440),mult(st->sp_center[0],328));
            move16();
        }
        ELSE
        {
            st->lt_noise_sp_center0 = add(mult(st->lt_noise_sp_center0,31457),mult(st->sp_center[0],1311));
            move16();
        }
    }

    Q_sum = sub(norm_l(st->lt_noise_sp_center_diff_sum), 1);
    Q_counter = norm_l(st->lt_noise_sp_center_diff_counter);
    sum_num = L_shl(st->lt_noise_sp_center_diff_sum,Q_sum);
    counter_den = L_shl(st->lt_noise_sp_center_diff_counter,Q_counter);

    IF(extract_h(counter_den)==0)
    {
        div_r = EVS_SW_MAX;
        move16();
    }
    ELSE
    {
        div_r = div_l(sum_num,extract_h(counter_den));
    }

    div_r = mult(div_r,24576);
    div_r_32 = VAD_L_ADD(L_deposit_l(div_r),add(sub(Q_sum,Q_counter),22), 9830, 15,&div_r_Q);
    div_r =extract_l(L_shr(div_r_32, sub(div_r_Q, SP_CENTER_Q)));

    test();
    if((sub(abs_s(sub(st->sp_center[2],st->lt_noise_sp_center0)), div_r)> 0) && (sub(frameloop, 200)>0))
    {
        update_flag = 0;
        move16();
    }


    return update_flag;
}
