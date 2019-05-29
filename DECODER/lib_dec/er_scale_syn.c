/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

/*This file is up to date with trunk rev 36531*/

#include <stdio.h>
#include <assert.h>
#include "prot_fx.h"
#include "basop_util.h"
#include "cnst_fx.h"
#include "stl.h"


/*----------------------------------------------------------------------------------*
* Damping_fact()
*
*  Estimate damping factor
*----------------------------------------------------------------------------------*/

/*This BASOP version was ported based on trunk rev. 27621 and updated based on trunk rev. 29287, re-updated based on trunk rev. 32244  */
Word16 Damping_fact(                    /* o : damping factor                                       *//*Q14*/
    const Word16 coder_type,        /* i : coding type in last good received frame              */
    const Word16 nbLostCmpt,        /* i : counter of consecutive bfi frames                    */
    const Word16 last_good,         /* i : last good frame class                                */
    const Word16 stab_fac,          /* i : ISF stability factor                                 *//*Q15*/
    Word32 *lp_gainp,         /*i/o: damped pitch gain                                    *//*2Q29 Word32!*/
    const Word16 core               /* i : current coding mode                                  */
)
{
    Word16 alpha; /*Q14*/
    Word16 gain; /*Q14*/
    Word32 lp_tmp;
    Word16 s_gainp;
    Word32 gain32;

    IF (core == ACELP_CORE)
    {
        alpha = mult_r(_ALPHA_VT_FX,16384); /* rapid convergence to 0 *//*Q14*/
        test();
        test();
        test();/*ELSEIF*/
        test();
        test();/*ELSEIF*/
        IF( ( sub(coder_type, UNVOICED) == 0) && (sub(nbLostCmpt, 3) <= 0))       /* Clear unvoiced last good frame   */
        {
            alpha = mult_r(_ALPHA_UU_FX,16384); /*Q14*/
        }
        ELSE IF( sub(last_good, UNVOICED_CLAS) == 0 )
        {
            IF( sub(nbLostCmpt,1) == 0 )
            {
                /* If stable, do not decrease the energy, pitch gain = 0 */
                /* * (1.0f - 2.0f*_ALPHA_U_FX) + 2.0f*MODE2_ALPHA_U; */ /* [0.8, 1.0] */
                alpha = add(mult_r(stab_fac,sub(16384,_ALPHA_U_FX)),_ALPHA_U_FX);
            }
            ELSE IF (sub(nbLostCmpt, 2) == 0 )
            {
                /*alpha = _ALPHA_U_FX * 1.5f;*/   /* 0.6 */
                alpha = mult_r(_ALPHA_U_FX, 24576/*1.5f Q14*/); /*Q14*/
            }
            ELSE
            {
                alpha = mult_r(_ALPHA_U_FX,16384); /*Q14*/  /* 0.4 go rapidly to CNG gain, pitch gain = 0 */
            }
        }
        ELSE IF( sub(last_good, UNVOICED_TRANSITION) == 0 )
        {
            alpha = mult_r(_ALPHA_UT_FX,16384); /*Q14*/
        }
        ELSE IF( (sub(last_good, ONSET) == 0) && (sub(nbLostCmpt, 3) <= 0 ) && (sub(coder_type, GENERIC) == 0))
        {
            alpha = 13107/*0.8f Q14*/;    /*Q14*/
        }
        ELSE if( ( (sub(last_good, VOICED_CLAS) == 0) || (sub(last_good,ONSET) == 0) ) && (sub(nbLostCmpt, 3) <= 0) )
        {
            alpha = mult_r(_ALPHA_V_FX,16384);      /* constant for the first 3 erased frames */
        }

        IF (sub(last_good, VOICED_CLAS) >= 0 )
        {
            move16();
            lp_tmp = *lp_gainp;

            IF( sub(nbLostCmpt, 1) == 0 ) /* if first erased frame in a block, reset harmonic gain */
            {
                /*lp_gainp_E = 1;*/ /*For sqrt, because *lp_gain is Q14 */

                /*gain = (float)sqrt( *lp_gainp );*/  /* move pitch gain towards 1 for voiced to remove energy fluctuations */
                /*BASOP_Util_Sqrt_MantExp(lp_gainp,&lp_gainp_E);*/
                s_gainp = 31-29;
                move16();
                gain32 = Sqrt32(lp_tmp, &s_gainp);

                gain = round_fx(L_shl(gain32,s_gainp)); /* Q15*/
                gain = s_min(gain, 32113/*0.98f Q15*/);    /*Q15*/
                gain = s_max(gain, 27853/*0.85f Q15*/);    /*Q15*/
                alpha = mult_r(alpha , gain);  /*Q14*/
            }
            ELSE IF ( sub(nbLostCmpt, 2) == 0 )
            {
                /*0.6  + 0.35*stab_fac*/
                alpha = mult_r(mac_r(1288490240l/*0.6f Q31*/, 11469/*0.35f Q15*/, stab_fac), round_fx(L_shl(lp_tmp,1)));
            }
            ELSE
            {
                /*0.7 + 0.2*stab_fac*/                            move16();
                lp_tmp = Mpy_32_16_1(lp_tmp, mac_r(1503238528l/*0.7f Q31*/, 6554/*0.2f Q15*/, stab_fac)); /*2Q29*/
                alpha = round_fx(L_shl(lp_tmp,1));      /*1Q14*/
            }
            move16();
            *lp_gainp = lp_tmp; /*store*/
        }
    }
    ELSE
    {
        alpha = mac_r(375809632l/*0.35f Q30*/,6554/*0.4f Q14*/,stab_fac); /*Q14*/
        if (sub(nbLostCmpt,2)< 0 )
        {
            alpha = mac_r(751619264l/*0.70f Q30*/,4915/*0.3f Q14*/,stab_fac); /*Q14*/
        }
        if (sub(nbLostCmpt, 2)==0)
        {
            alpha = mac_r(483183808l/*0.45f Q30*/,6554/*0.4f Q14*/,stab_fac); /*Q14*/
        }

    }
    return alpha;
}
