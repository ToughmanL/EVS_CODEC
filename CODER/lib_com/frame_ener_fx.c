/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include <assert.h>
#include "options.h"     /* Compilation switches                   */
#include "cnst_fx.h"     /* Common constants                       */
#include "prot_fx.h"     /* Function prototypes                    */
#include "stl.h"

/*----------------------------------------------------------------------------------*
 * frame_ener()
 *
 * Estimation of pitch-synchronous (voiced) or mean half-frame (unvoiced) energy
 *----------------------------------------------------------------------------------*/
Word16 frame_ener_fx(
    const Word16 L_frame,   /* i  : length of the frame                            */
    const Word16 clas,      /* i  : frame classification                           */
    const Word16 *synth,    /* i  : synthesized speech at Fs = 12k8 Hz       Q_new */
    const Word16 pitch,     /* i  : pitch period                             Q0    */
    Word32 *enr_q,    /* o  : pitch-synchronous or half_frame energy   Q0    */
    const Word16 offset,    /* i  : speech pointer offset (0 or L_FRAME)           */
    const Word16 Q_new,     /* i  : Scaling factor                                 */
    Word16 shift,     /* i  : Shift need to obtain 12 bits vectors           */
    const Word16 enc        /* i  : Encoder/decoder                                */
)
{
    Word16 len, exp_enrq, exp_tmp, pos;
    Word16 i;
    const Word16 *pt_synth;
    Word32 Ltmp;

    exp_enrq = 0;
    move16();
    test();
    test();
    IF( (sub(clas, VOICED_CLAS) == 0) || (sub(clas, ONSET) == 0) || (sub(clas, SIN_ONSET) == 0) )              /* current frame is voiced */
    {
        /* current frame is voiced */
        len = pitch;
        move16(); /* pitch value at the end of frame */
        pt_synth = synth;
        move16();
        if (offset != 0)
        {
            pt_synth = synth + sub(L_frame, len);
        }
        emaximum_fx(Q_new, pt_synth, len, enr_q);
        move16();/* pitch synchronous E */
        IF (enc != 0)
        {
            exp_enrq  = norm_l(*enr_q);
            *enr_q    = L_shl(*enr_q, exp_enrq);
            move32();
            exp_enrq  = sub(exp_enrq, 2);
        }
    }
    ELSE
    {
        /* current frame is unvoiced */
        Word16 L_frame2, exp2, enr_q_tmp;

        L_frame2 = shr(L_frame,1);
        pos = 0;
        move16();

        if (offset != 0)
        {
            pos = sub(L_frame, L_frame2);
        }
        Ltmp = L_mult(synth[pos], synth[pos]);
        FOR (i = 1; i < L_frame2; i++)
        {
            Ltmp = L_mac(Ltmp, synth[pos+i], synth[pos+i]);
        }
        test();
        IF (L_sub(Ltmp, MAX_32) == 0 || enc != 0)
        {
            /* scale down when overflow occurs */
            *enr_q = Energy_scale(synth+pos, L_frame2, shift, &exp_enrq);
            move32();
        }
        ELSE
        {
            shift = 0;
            move16();
            /* Normalize acc in Q31 (energy already calculated) */
            pos = norm_l(Ltmp);
            Ltmp = L_shl(Ltmp, pos);
            exp_enrq = sub(30, pos); /* exponent = 0..30 */
            *enr_q = Ltmp;
            move32();
        }

        /* enr2 = 1.0f/L_FRAME2 * dot_product(synth, synth, L_FRAME2) */
        exp_enrq = sub(exp_enrq, shl(shift, 1));

        IF (enc != 0)
        {
            assert(L_frame == 256 || L_frame == 320);

            exp_tmp = add(shl(Q_new, 1), -2+7); /*  L_subfr == L_SUBFR */
            exp_enrq = sub(exp_enrq, exp_tmp);
            exp_enrq = sub(31, exp_enrq);

            IF(sub(L_frame, 320) == 0)
            {
                *enr_q = Mult_32_16(*enr_q, 26214);     /*x 0.8 to get /160*/
                i = norm_l(*enr_q);
                *enr_q = L_shl(*enr_q, i);
                exp_enrq = add(i, exp_enrq);
            }
        }
        ELSE
        {
            exp_enrq = sub(exp_enrq, add(Q_new, Q_new));
            enr_q_tmp /*Q30 exp2+exp_enrq*/ = BASOP_Util_Divide3216_Scale(*enr_q /*Q31*/, L_frame2 /*Q0*/, &exp2);
            *enr_q = L_shr(L_deposit_l(enr_q_tmp),sub(30,add(exp2,exp_enrq))); /*Q0*/
            *enr_q  = L_add(*enr_q, 1);
            move32();
            exp_enrq = 0;
            move16();
        }






    }

    return exp_enrq;
}

/*------------------------------------------------------------------------*
 * frame_energy()
 *
 * Compute pitch-synchronous energy at the frame end
 *------------------------------------------------------------------------*/
Word16 frame_energy_fx(        /* o  : Frame energy in                               Q8 */
    Word16 L_frame,
    const Word16 *pitch,       /* i  : pitch values for each subframe                Q6 */
    const Word16 *speech,      /* i  : pointer to speech signal for E computation  Q_syn*/
    const Word16 lp_speech,    /* i  : long term active speech energy average      Q8   */
    Word16 *frame_ener,  /* o  : pitch-synchronous energy at frame end       Q8   */
    const Word16 Q_syn         /* i  : Synthesis scaling                                */
)
{
    Word32 Ltmp;
    const Word16 *pt1;
    Word16 tmp16, exp1, exp2, tmp1, tmp2;
    Word16 len, enern;

    /* len = (0.5f * (pitch[2]/64.0 + pitch[3]/64.0) + 0.5f) */
    len = mult_r(add(pitch[2], pitch[3]), 256);

    if(sub(len,L_SUBFR) < 0 )
    {
        len = shl(len, 1);
    }
    pt1 = speech + sub(L_frame,len);

    /* *frame_ener = 10.0f * log10(dot_product(pt1, pt1, len) / (float)len) */

    tmp1 = norm_s(len);
    tmp2 = shl(len, tmp1);
    tmp1 = sub(15, tmp1);

    Ltmp = Dot_productSq16HQ( 0, pt1, len, &exp1);
    exp1 = sub(exp1, shl(Q_syn, 1));
    exp1 = sub(exp1, 1); /* compensation of leftshift caused by mac operation in dot_productSq16HQ */
    tmp16 = BASOP_Util_Divide3216_Scale( Ltmp, len, &exp2);

    exp1 = add(exp1, exp2);
    exp1 = add(exp1, 1); /* compensate result of division Q-1 */


    tmp2 = norm_s(tmp16);
    Ltmp = L_shl(L_deposit_h(tmp16),tmp2); /*Q16, (exp1-tmp2)  =  Q31, exp1-tmp2+15*/

    Ltmp = BASOP_Util_Log2(Ltmp);/*Q(31-6) = Q25*/
    exp1 = sub(15+exp1,tmp2);

    /*add ld(2^exp1)=exp1 but check format, first*/
    tmp16=sub(sub(15,norm_s(exp1)),5); /*factor to shift Ltmp and exp1 with (shr) to avoid overflows when adding*/
    Ltmp= L_shr(Ltmp,tmp16); /*Q25, tmp16*/
    exp2 = shr(exp1,tmp16);  /*Q0 , tmp16*/
    Ltmp = L_add(Ltmp,L_shl(L_deposit_l(exp2),25)); /*Q25, tmp16, normalized*/

    /*make 10*log10 out of log2*/
    Ltmp = Mpy_32_16_1(Ltmp,LG10); /*Q25,tmp16 * Q13 = Q23, tmp16*/
    *frame_ener = extract_h(L_shl(Ltmp,add(tmp16,1)));/*Q8*/                    move16();
    enern = sub( *frame_ener  ,lp_speech); /*Q8*/

    return enern;
}
