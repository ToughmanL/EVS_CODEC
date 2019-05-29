/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "prot_fx.h"
#include "basop_util.h"
#include "rom_com_fx.h"
#include "options.h"
#include "stl.h"

/* #if defined(_WIN32) && (_MSC_VER <= 1200)  /\* disable global optimizations to overcome an internal compiler error *\/ */
#if defined(_MSC_VER) && (_MSC_VER <= 1200)  /* disable global optimizations to overcome an internal compiler error */
#pragma optimize("g", off)
#endif

/*-------------------------------------------------------------------*
* decoder_LPD()
*
* Core decoder
*--------------------------------------------------------------------*/

void decoder_LPD(
    Word16 signal_out[],       /* output: signal with LPD delay (7 subfrs) */
    Word16 signal_outFB[],
    Word16 *total_nbbits,     /* i/o:    number of bits / decoded bits    */
    Decoder_State_fx *st,
    Word16 * bpf_noise_buf,
    Word16 bfi,
    Word16 *bitsRead,
    Word16 *coder_type,
    Word16 param[],
    Word16 *pitch_buf,
    Word16 *voice_factors,
    Word16 *ptr_bwe_exc
)
{
    Word16 *param_lpc;
    Word16 synth_buf[OLD_SYNTH_SIZE_DEC+L_FRAME_PLUS+M];
    Word16 *synth;
    Word16 synth_bufFB[OLD_SYNTH_SIZE_DEC+L_FRAME_PLUS+M];
    Word16 *synthFB;
    Word16 lsf[(NB_DIV+1)*M],lsp[(NB_DIV+1)*M],lspmid[M],lsfmid[M];
    Word16 Aq[(NB_SUBFR16k+1)*(M+1)];              /* Dyn RAM: it can be reduced by M+1 if insert branch for nb_subfr==5*/
    Word16 pitch[NB_SUBFR16k];
    Word16 pit_gain[NB_SUBFR16k];
    Word16 i, k, *prm;
    Word16 L_frame,nb_subfr;
    Word16 L_frameTCX;
    Word16 Aind[M+1], lspind[M];
    Word16 tmp_old[M+1], tmp_new[M+1], enr_old, enr_new;
    Word16 xspnew_uw[NB_DIV*M], xsfnew_uw[NB_DIV*M];
    Word16 const* xsfBase;                      /* base for differential XSF coding */
    Word16  past_core_mode;

    Word16 lsf_q_1st_rf[M], lsf_q_rf[M], lsp_q_rf[M];
    Word32 lsp_diff;
    Word16 LSF_Q_prediction;
    Word16 tcx_last_overlap_mode, tcx_current_overlap_mode;

    st->core_fx = 0;            /* to avoid compilation warnings */
    prm = NULL;                 /* to avoid compilation warnings */

    /*--------------------------------------------------------------------------------*
     * INIT
     *--------------------------------------------------------------------------------*/

    enr_old = 0;
    move16();
    enr_new = 0;
    move16();

    LSF_Q_prediction= -1;
    move16();

    param_lpc = param+DEC_NPRM_DIV*NB_DIV;

    past_core_mode = st->last_core_bfi;
    move16();

    test();
    test();
    if(st->use_partial_copy && sub(st->rf_frame_type, RF_TCXFD) >= 0 && sub(st->rf_frame_type, RF_TCXTD2) <= 0)
    {
        bfi = st->bfi_fx;
        move16();
    }

    /*Adjust bit per frame*/
    if (bfi == 0)
    {
        st->bits_frame_core = sub(st->bits_frame, bitsRead[0]);
    }

    /* Framing parameters */
    L_frame  = st->L_frame_fx;
    move16();
    L_frameTCX = st->L_frameTCX;
    move16();
    nb_subfr = st->nb_subfr;
    move16();


    /* Initialize pointers */
    synth = synth_buf + st->old_synth_len;
    /*st->old_synth: Q_0*/
    Copy(st->old_synth, synth_buf, st->old_synth_len);
    set16_fx(synth, 0, L_FRAME_PLUS + M);

    synthFB = synth_bufFB + st->old_synth_lenFB;
    Copy(st->old_synthFB_fx, synth_bufFB, st->old_synth_lenFB);
    set16_fx(synthFB, 0, L_FRAME_PLUS + M);

    /*For post-processing (post-filtering+blind BWE)*/
    IF (st->tcxonly==0)
    {
        /* for bass postfilter */
        set16_fx(pitch, L_SUBFR, nb_subfr);

        set16_fx(pit_gain, 0, nb_subfr);
    }

    /* PLC: [Common: Memory update]
     * PLC: Update the number of lost frames */
    IF ( bfi != 0)
    {
        move16();
        st->nbLostCmpt = add(st->nbLostCmpt, 1);
    }


    /*--------------------------------------------------------------------------------*
     * BITSTREAM DECODING
     *--------------------------------------------------------------------------------*/

    IF (bfi == 0)
    {
        /* PLC: [TCX: Tonal Concealment] */
        st->second_last_core = st->last_core_fx;
        move16();
        tcx_last_overlap_mode = st->tcx_cfg.tcx_last_overlap_mode;
        move16();
        tcx_current_overlap_mode = st->tcx_cfg.tcx_curr_overlap_mode;
        move16();
        dec_prm(&(st->core_fx), &(st->last_core_fx), coder_type, param, param_lpc, total_nbbits, st, L_frame, bitsRead);
        IF(!st->rate_switching_init && sub((st->last_codec_mode), MODE2) == 0 && st->BER_detect)
        {
            *coder_type = st->last_coder_type_fx;
            move16();
            st->last_core_fx = st->second_last_core;
            move16();
            st->tcx_cfg.tcx_last_overlap_mode = tcx_last_overlap_mode;
            move16();
            st->tcx_cfg.tcx_curr_overlap_mode = tcx_current_overlap_mode;
            move16();
            st->bfi_fx = 1;
            move16();
            bfi = 1;
            move16();
            st->flagGuidedAcelp = 0;
            move16();
            st->nbLostCmpt++;
            move16();
            st->core_brate_fx = st->last_core_brate_fx;
            move16();
            st->core_fx = GetPLCModeDecision( st );
        }
    }
    ELSE
    {

        test();
        test();
        IF( st->use_partial_copy && sub(st->rf_frame_type, RF_TCXFD) >= 0 && sub(st->rf_frame_type, RF_TCXTD2) <= 0 )
        {
            dec_prm( &(st->core_fx), &(st->last_core_fx), coder_type, param, param_lpc, total_nbbits, st, L_frame, bitsRead );
        }

        if (sub(st->nbLostCmpt, 1) > 0)
        {
            st->flagGuidedAcelp = 0;
            move16();
        }
    }

    /* PLC: [Common: mode decision]
     * PLC: Decide which Concealment to use. Update pitch lags if needed */
    IF ( bfi!=0 )
    {
        st->core_fx = GetPLCModeDecision(st);
    }

    IF ( bfi == 0 )
    {
        IF( sub(st->prev_bfi_fx, 1)==0 )
        {
            st->prev_nbLostCmpt = st->nbLostCmpt;
            move16();
        }
        ELSE
        {
            st->prev_nbLostCmpt = 0;
            move16();
        }
        move16();
        st->nbLostCmpt = 0;
    }

    /*--------------------------------------------------------------------------------*
     * LPC PARAMETERS
     *--------------------------------------------------------------------------------*/

    test();
    test();
    test();
    test();
    IF( (bfi == 0 ) || ( bfi != 0 && st->use_partial_copy != 0 && sub(st->rf_frame_type,RF_TCXFD) == 0) )
    {
        test();
        test();
        test();
        test();
        test();
        IF(sub(st->use_partial_copy,1)==0 && ( sub(st->rf_frame_type, RF_TCXFD) < 0 || sub(st->rf_frame_type, RF_TCXTD2) > 0))
        {
            IF( sub((Word16)st->envWeighted,1)==0 )
            {
                Copy( st->lspold_uw, st->lsp_old_fx, M );
                Copy( st->lsfold_uw, st->lsf_old_fx, M );
                st->envWeighted = 0;
                move16();
            }

            /* first stage VQ, 8 bits; reuse TCX high rate codebook */
            set16_fx(lsf_q_1st_rf, 0, M);
            vlpc_1st_dec(param_lpc[0], lsf_q_1st_rf );

            /* second stage vq */
            /* quantized lsf from two stages  */
            /*v_add(lsf_q_1st_rf, lsf_q_diff_cb_8b_rf + M * param_lpc[1], lsf_q_rf, M);*/
            FOR (i=0; i<M; i++)
            {
                Word16 tmp = lsf_q_diff_cb_8b_rf[i+ M*param_lpc[1]]; /*tmp = unquantized lsf difference (2nd VQ) in Q6*/
                tmp = shr(mult_r(tmp,20972),4); /* bring tmp to Qx2.56 for addition */
                lsf_q_rf[i] = add(lsf_q_1st_rf[i], tmp);
            }

            v_sort( lsf_q_rf, 0, M-1 );
            reorder_lsf_fx( lsf_q_rf, LSF_GAP_FX, M, st->sr_core );

            /* current n-th ACELP frame and its corresponding partial copy  */
            /*lsf2lsp( lsf_q_rf, lsp_q_rf, M, st->sr_core );*/
            E_LPC_lsf_lsp_conversion( lsf_q_rf, lsp_q_rf, M );

            /* copy the old and current lsfs and lsps into the lsf[] and lsp[] buffer for interpolation  */
            Copy(st->lsf_old_fx, &lsf[0], M);
            Copy(st->lsp_old_fx, &lsp[0], M);
            Copy(lsf_q_rf, &lsf[M], M);
            Copy(lsp_q_rf, &lsp[M], M);
            lsp_diff = 0;
            FOR( i=0; i < M; i++ )
            {
                lsp_diff = L_add(lsp_diff,(Word32)abs_s(sub(lsp[i+M],lsp[i])));
            }

            IF( sub(st->core_fx,ACELP_CORE) == 0 && sub(st->last_core_fx,ACELP_CORE) == 0
                && L_sub(lsp_diff, 52428 ) < 0 && L_sub(lsp_diff,3932) >0 && sub(st->next_coder_type,GENERIC ) == 0
                && !st->prev_use_partial_copy && sub(st->last_coder_type_fx,UNVOICED) == 0 && sub(st->rf_frame_type,RF_GENPRED) >= 0 )
            {
                Copy( &lsp[0], &lsp[M], M );
            }

            /* update mem_MA and mem_AR memories  */
            lsf_update_memory( (Word16)st->narrowBand, &lsf[M], st->mem_MA_fx, st->mem_MA_fx, M );
            Copy(&lsf[M], st->mem_AR_fx, M);

            FOR( k=0; k<st->numlpc; ++k )
            {
                Copy( &lsp[(k+1)*M], &xspnew_uw[k*M], M );
                Copy( &lsf[(k+1)*M], &xsfnew_uw[k*M], M );
            }
        }
        ELSE
        IF ((st->enableTcxLpc !=0 && sub(st->core_fx , ACELP_CORE)!= 0) || (bfi && st->use_partial_copy && st->rf_frame_type == RF_TCXFD))
        {
            Word16 tcx_lpc_cdk;
            IF(bfi && st->use_partial_copy && sub(st->rf_frame_type, RF_TCXFD) == 0)
            {
                tcx_lpc_cdk = tcxlpc_get_cdk(GENERIC);
            }
            ELSE
            {
                tcx_lpc_cdk = tcxlpc_get_cdk(*coder_type); /* inlined */
            }

            Copy(st->lsf_old_fx, &lsf[0], M);
            Copy(st->lsp_old_fx, &lsp[0], M);

            D_lsf_tcxlpc( param_lpc, &lsf[M], lspind, st->narrowBand, tcx_lpc_cdk, st->mem_MA_fx );

            E_LPC_lsf_lsp_conversion( &lsf[M], &lsp[M], M );

            lsf_update_memory( (Word16)st->narrowBand, &lsf[M], st->mem_MA_fx, st->mem_MA_fx, M );
            Copy(&lsf[M], st->mem_AR_fx, M);

            st->envWeighted = 1;
            move16();

            E_LPC_lsp_unweight( &lsp[M], xspnew_uw, xsfnew_uw, st->inv_gamma, M );
        }
        ELSE
        {

            IF (st->envWeighted)
            {
                Copy(st->lspold_uw, st->lsp_old_fx, M);
                Copy(st->lsfold_uw, st->lsf_old_fx, M);
                st->envWeighted = 0;
                move16();
            }
            IF (sub(st->core_fx, TCX_20_CORE) == 0)
            {
                lpc_unquantize( st, st->lsf_old_fx, st->lsp_old_fx, lsf, lsp, M, st->lpcQuantization, param_lpc, st->numlpc, st->core_fx,
                st->mem_MA_fx, st->mem_AR_fx, lspmid, lsfmid, AUDIO, st->acelp_cfg.midLpc, st->narrowBand, &(st->seed_acelp),
                st->sr_core, &st->mid_lsf_int_fx, st->prev_bfi_fx, &LSF_Q_prediction, &st->safety_net_fx );
            }
            ELSE
            {
                lpc_unquantize( st, st->lsf_old_fx, st->lsp_old_fx, lsf, lsp, M, st->lpcQuantization, param_lpc, st->numlpc, st->core_fx,
                st->mem_MA_fx, st->mem_AR_fx, lspmid, lsfmid, *coder_type, st->acelp_cfg.midLpc, st->narrowBand, &(st->seed_acelp),
                st->sr_core, &st->mid_lsf_int_fx, st->prev_bfi_fx, &LSF_Q_prediction, &st->safety_net_fx );
                IF(sub(st->prev_use_partial_copy,1)==0 && sub(st->last_core_fx,ACELP_CORE) == 0 && sub(st->core_fx,ACELP_CORE) == 0
                && sub(st->prev_rf_frame_type, RF_GENPRED) >= 0 && sub(*coder_type,UNVOICED) == 0 )
                {
                    IF ( st->lpcQuantization && st->acelp_cfg.midLpc )
                    {
                        Copy(lspmid, &lsp[0], M );
                        Copy(&lsp[M], lspmid, M );
                    }
                }
            }

            FOR(k=0; k<st->numlpc; ++k)
            {
                Copy(&lsp[(k+1)*M], &xspnew_uw[k*M], M);
                Copy(&lsf[(k+1)*M], &xsfnew_uw[k*M], M);
            }
        }

        /* PLC: [LPD: LPC concealment]
         * built the moving average for the LPC concealment */

        FOR (k=0; k<st->numlpc; k++)
        {
            FOR (i=0; i<M; i++)
            {
                move16();
                st->lsf_adaptive_mean_fx[i] = add(add(mult_r(st->lsfoldbfi1_fx[i], 10923/*1.0f/3.0f Q15*/), mult_r(st->lsfoldbfi0_fx[i], 10923/*1.0f/3.0f Q15*/)), mult_r(xsfnew_uw[k*M+i], 10923/*1.0f/3.0f Q15*/));
                move16();
                st->lsfoldbfi1_fx[i] = st->lsfoldbfi0_fx[i];
                move16();
                st->lsfoldbfi0_fx[i] = xsfnew_uw[k*M+i];
            }
        }
    }
    ELSE
    {
        /* PLC: [LPD: LPC concealment]
         * Conceal the LPC from the lost frame */
        st->numlpc = 2;
        move16();
        test();
        if ( st->tcxonly == 0 || sub(st->core_fx, TCX_10_CORE) < 0 )
        {
            move16();
            st->numlpc = 1;
        }
        IF(sub(st->nbLostCmpt,1)==0)
        {
            Copy(st->lsf_old_fx, st->old_lsf_q_cng, M);
            Copy(st->lsp_old_fx, st->old_lsp_q_cng, M);
        }
        move16();
        xsfBase = PlcGetLsfBase (st->lpcQuantization,
        (Word16)st->narrowBand,
        st->sr_core);

        dlpc_bfi( st->L_frame_fx,
        xsfnew_uw,
        st->lsfold_uw,
        st->last_good_fx,
        st->nbLostCmpt,
        st->mem_MA_fx,
        st->mem_AR_fx,
        &(st->stab_fac_fx),
        st->lsf_adaptive_mean_fx,
        st->numlpc,
        st->lsf_cng,
        st->plcBackgroundNoiseUpdated,
        st->lsf_q_cng,
        st->old_lsf_q_cng,
        xsfBase,
        st->tcxonly
                );

        st->envWeighted = 0;
        move16();

        Copy( st->lspold_uw, lsp, M );
        Copy( st->lsfold_uw, lsf, M );

        FOR ( k = 0; k < st->numlpc; k++ )
        {
            Copy( &xsfnew_uw[k*M], &lsf[(k+1)*M], M );

            IF ( st->tcxonly )
            {
                E_LPC_lsf_lsp_conversion(&lsf[(k+1)*M], &lsp[(k+1)*M], M);
                E_LPC_lsf_lsp_conversion(st->lsf_q_cng, st->lsp_q_cng, M);
            }
            ELSE
            {
                lsf2lsp_fx( &lsf[(k+1)*M], &lsp[(k+1)*M], M, st->sr_core );
                lsf2lsp_fx( st->lsf_q_cng, st->lsp_q_cng, M, st->sr_core );
            }

            Copy( &lsp[(k+1)*M], &xspnew_uw[k*M], M );
        }
    }

    /*--------------------------------------------------------------*
     * Rate switching
     *---------------------------------------------------------------*/
    IF( st->rate_switching_reset!=0 )
    {
        Copy( &(lsf[M]),&(lsf[0]), M );
        Copy( &(lsp[M]),&(lsp[0]), M );
        Copy( &(lsf[M]),st->lsf_old_fx, M );
        Copy( &(lsp[M]),st->lsp_old_fx, M );
        Copy( &(lsf[M]),lsfmid, M );
        Copy( &(lsp[M]),lspmid, M );
        E_LPC_f_lsp_a_conversion(st->lsp_old_fx, st->old_Aq_12_8_fx, M);
    }


    IF(st->enablePlcWaveadjust)
    {
        if(sub(st->core_fx, ACELP_CORE)  == 0)
        {
            st->tonality_flag = 0;
            move16();
        }
        if(bfi)
        {
            st->plcInfo.nbLostCmpt++;
            move16();
        }
    }

    /*--------------------------------------------------------------------------------*
     * ACELP
     *--------------------------------------------------------------------------------*/
    test();
    test();
    test();
    IF( (st->prev_bfi_fx!=0) && (bfi==0) && (sub(*coder_type,VOICED)==0) && sub(st->prev_nbLostCmpt,4)>0 )
    {
        st->dec_glr_idx = 1;
        move16();
        st->reset_mem_AR = 1;
        move16();
    }

    IF ( st->core_fx == ACELP_CORE )
    {
        IF (st->tcxonly==0)
        {
            /* Set pointer to parameters */
            prm = param;

            /* Stability Factor */
            IF ( bfi == 0 )
            {
                st->stab_fac_fx = lsf_stab_fx(&lsf[M], &lsf[0], 0, st->L_frame_fx);
            }

            test();
            IF( bfi==0 && st->prev_bfi_fx != 0 )
            {
                /* check if LSP interpolation can be relaxed or if LPC power can be diffused*/
                E_LPC_f_lsp_a_conversion(&lsp[0], tmp_old, M);
                enr_old = Enr_1_Az_fx(tmp_old, 2*L_SUBFR);

                E_LPC_f_lsp_a_conversion(&lsp[M], tmp_new, M);
                enr_new = Enr_1_Az_fx(tmp_new, 2*L_SUBFR);
            }

            test();
            test();
            test();
            test();
            IF( (bfi == 0) && ((sub(st->dec_glr_idx, 1) == 0) || ((st->safety_net_fx==0) && (shr(enr_new,11) > 0) && (sub(shr(enr_new,1),enr_old)>0))) && (st->prev_bfi_fx != 0 ) )
            {
                Word16 reset_q = 0;
                if( sub(st->dec_glr_idx, 1) == 0 )
                {
                    reset_q = 1;
                    move16();
                }
                RecLpcSpecPowDiffuseLc( &lsp[M], &lsp[0], &lsf[M], st, reset_q);

                int_lsp_fx( L_frame, &lsp[0], &lsp[M], Aq, M, interpol_frac_fx, 0 );

                Copy(&lsf[M], xsfnew_uw, M);
            }
            ELSE
            {
                /* LPC Interpolation for ACELP */
                test();
                IF ( bfi==0 && st->acelp_cfg.midLpc )
                {
                    st->relax_prev_lsf_interp_fx = 0;
                    move16();
                    IF (st->prev_bfi_fx)
                    {
                        /* check if LSP interpolation can be relaxed */
                        IF ( sub(enr_new, shr(enr_old, 2)) < 0 )
                        {
                            st->relax_prev_lsf_interp_fx = -1;
                            move16();
                            test();
                            test();
                            test();
                            test();
                            if ( sub(st->clas_dec, UNVOICED_CLAS) == 0 || sub(st->clas_dec, SIN_ONSET) == 0 || sub(st->clas_dec, INACTIVE_CLAS) == 0 || sub(*coder_type, GENERIC) == 0 || sub(*coder_type, TRANSITION) == 0 )
                            {
                                st->relax_prev_lsf_interp_fx = 1;
                                move16();
                            }
                        }
                    }

                    test();
                    test();
                    test();
                    test();
                    if (st->stab_fac_fx == 0 && st->old_bfi_cnt_fx > 0 && sub(st->clas_dec, VOICED_CLAS) != 0 && sub(st->clas_dec, ONSET) != 0 && st->relax_prev_lsf_interp_fx == 0 )
                    {
                        st->relax_prev_lsf_interp_fx = 2;
                        move16();
                    }
                    int_lsp4_fx( L_frame, &lsp[0], lspmid, &lsp[M], Aq, M, st->relax_prev_lsf_interp_fx );
                }
                ELSE
                {
                    int_lsp_fx( L_frame, &lsp[0], &lsp[M], Aq, M, interpol_frac_fx, 0 );
                    int_lsp_fx( L_frame, st->old_lsp_q_cng, st->lsp_q_cng, st->Aq_cng, M, interpol_frac_fx, 0 );
                }
            }
        }

        test();
        IF (bfi!=0 && st->last_core_fx != ACELP_CORE)
        {
            /* PLC: [TCX: TD PLC] */
            con_tcx( st, &synthFB[0] );
            lerp( synthFB, synth, st->L_frame_fx, st->L_frameTCX );
            st->con_tcx = 1;
            move16();
            set16_fx (st->mem_pitch_gain+2,round_fx(L_shl(st->Mode2_lp_gainp , 1)), st->nb_subfr);
        }
        ELSE
        {
            /* ACELP decoder */
            IF (sub(st->L_frame_fx,L_FRAME)== 0)
            {
                Copy(Aq+2*(M+1), st->cur_sub_Aq_fx, (M+1));
            }
            ELSE
            {
                Copy(Aq+3*(M+1), st->cur_sub_Aq_fx, (M+1));
            }
            IF ( bfi != 0 )
            {
                /* PLC: [ACELP: general]
                 * PLC: Use the ACELP like concealment */
                con_acelp(Aq,
                st->core_ext_mode,
                &synth[0],
                pitch,
                pit_gain,
                st->stab_fac_fx,
                st,
                &st->Q_exc,
                &st->Q_syn, /*Q format of st->mem_syn*/
                pitch_buf,
                voice_factors,
                ptr_bwe_exc
                         );

                Copy(&st->mem_pitch_gain[2], &st->mem_pitch_gain[st->nb_subfr+2], st->nb_subfr);
                set16_fx(&st->mem_pitch_gain[2],0,st->nb_subfr);
            }
            ELSE
            {
                decoder_acelp(st, *coder_type, prm, Aq, st->acelp_cfg, &synth[0],
                pitch, pit_gain, st->stab_fac_fx, pitch_buf, voice_factors, LSF_Q_prediction, ptr_bwe_exc);
                IF(st->flagGuidedAcelp > 0)
                {
                    st->guidedT0 = s_max(s_min(add(st->T0_4th, st->guidedT0), NBPSF_PIT_MAX), PIT_MIN_16k);
                }

                FOR (i=0; i<st->nb_subfr; i++)
                {
                    move16();
                    move16();
                    st->mem_pitch_gain[2+(2*st->nb_subfr-1)-i] = st->mem_pitch_gain[2+ (st->nb_subfr-1) -i];
                    st->mem_pitch_gain[2+(st->nb_subfr-1)-i] = pit_gain[i];
                }
            }
        }


        /* LPC for ACELP/BBWE */
        test();
        test();
        IF( st->narrowBand || (L_sub(st->sr_core, 12800) == 0) || (L_sub(st->sr_core, 16000) == 0) )
        {
            Copy(Aq, st->mem_Aq, nb_subfr*(M+1));
        }

        /* PLC: [TCX: Tonal Concealment] */
        /* Signal that this frame is not TCX */
        TonalMDCTConceal_UpdateState(&st->tonalMDCTconceal, 0, 0, 0, 0);

        IF (bfi==0)
        {
            st->second_last_tns_active = st->last_tns_active;
            st->last_tns_active = 0;
            st->tcxltp_last_gain_unmodified = 0;
            move16();
        }

    }



    /*--------------------------------------------------------------------------------*
     * TCX20
     *--------------------------------------------------------------------------------*/

    IF ( sub(st->core_fx, TCX_20_CORE) == 0 )
    {
        /* Set pointer to parameters */
        prm = param;

        /* Stability Factor */
        IF ( bfi == 0 )
        {
            IF ( st->tcxonly != 0 )
            {
                st->stab_fac_fx = lsf_stab_fx(&lsf[M], &lsf[0],0, L_FRAME);
            }
            ELSE
            {
                st->stab_fac_fx = lsf_stab_fx(&lsf[M], &lsf[0],0, st->L_frame_fx);
            }
        }

        IF (st->enableTcxLpc)
        {
            /* Convert quantized xSP to A */
            E_LPC_f_lsp_a_conversion(&lsp[M], Aq, M);
        }
        ELSE
        {
            IF (st->tcxonly == 0)
            {
                test();
                test();
                test();
                IF( (bfi == 0) && (st->prev_bfi_fx != 0 ) && (st->safety_net_fx==0) && (st->rate_switching_reset != 0) )
                {
                    /* diffuse LPC power on rate switching*/
                    RecLpcSpecPowDiffuseLc( &lsp[M], &lsp[0], &lsf[M], st, 0 );
                    int_lsp_fx( L_frame, &lsp[0], &lsp[M], Aq, M, interpol_frac_fx, 0 );
                    Copy(&lsf[M], xsfnew_uw, M);
                }
                ELSE
                {
                    /* LPC Interpolation for TCX */
                    E_LPC_int_lpc_tcx(&lsp[0], &lsp[M], Aq);
                }
            }
            ELSE
            {
                E_LPC_f_lsp_a_conversion(&lsp[M], Aq, M);
            }
        }

        test();
        IF ( bfi == 0 && st->tcx_lpc_shaped_ari != 0 )
        {
            E_LPC_f_lsp_a_conversion(lspind, Aind, M);
        }

        /* TCX decoder */
        decoder_tcx(&st->tcx_cfg,
                    prm,
                    Aq,
                    Aind,
                    L_frame,
                    L_frameTCX,
                    st->tcx_cfg.tcx_coded_lines,
                    &synth[0],
                    &synthFB[0],
                    st,
                    *coder_type,
                    bfi,
                    0,
                    st->stab_fac_fx
                   );

    }

    /*--------------------------------------------------------------------------------*
     * TCX10
     *--------------------------------------------------------------------------------*/

    IF ( sub(st->core_fx, TCX_10_CORE) == 0 )
    {
        FOR (k=0; k<2; k++)
        {

            /* Set pointer to parameters */
            prm = param + (k*DEC_NPRM_DIV);

            /* Stability Factor */
            IF ( bfi==0 )
            {
                st->stab_fac_fx = lsf_stab_fx(&lsf[(k+1)*M], &lsf[k*M],0, L_FRAME);
            }

            E_LPC_f_lsp_a_conversion(&lsp[(k+1)*M], Aq, M);

            /* TCX decoder */

            IGFDecRestoreTCX10SubFrameData( &st->hIGFDec, k );
            decoder_tcx(&st->tcx_cfg,
                        prm,
                        Aq,
                        Aind,
                        shr(L_frame, 1),
                        shr(L_frameTCX, 1),
                        shr(st->tcx_cfg.tcx_coded_lines, 1),
                        &synth[k*L_frame/2],
                        &synthFB[k*L_frameTCX/2],
                        st,
                        *coder_type,
                        bfi,
                        k,
                        st->stab_fac_fx
                       );
        }

    }


    test();
    IF (sub(st->core_fx, TCX_10_CORE) == 0 || sub(st->core_fx, TCX_20_CORE) == 0)
    {
        test();
        test();
        IF(st->enablePlcWaveadjust ||                       /* bfi      */
           (L_sub(st->last_total_brate_fx, HQ_48k) >= 0 &&  /* recovery */
            sub(st->last_codec_mode, MODE2) == 0) )
        {
            /* waveform adjustment */
            concealment_signal_tuning_fx( bfi, st->core_fx,
                                          synthFB, &st->plcInfo, st->nbLostCmpt, st->prev_bfi_fx,
                                          st->tonalMDCTconceal.secondLastPcmOut,
                                          past_core_mode,st->tonalMDCTconceal.lastPcmOut, st );
            test();
            test();
            test();
            IF ((bfi || st->prev_bfi_fx) && st->plcInfo.Pitch_fx && (sub(st->plcInfo.concealment_method, TCX_NONTONAL) == 0))
            {
                lerp( synthFB, synth, L_frame, L_frameTCX );
                test();
                if( !bfi && st->prev_bfi_fx )
                {
                    st->plcInfo.Pitch_fx = 0;
                    move16();
                }
            }
        }

        IF (!bfi)
        {
            TonalMDCTConceal_SaveTimeSignal( &st->tonalMDCTconceal, synthFB, L_frameTCX );
        }
        decoder_tcx_post( st, synth, synthFB, Aq, bfi );

        IF (sub(st->core_fx, TCX_20_CORE) == 0)
        {
            /* LPC Interpolation for BBWE/post-processing */
            test();
            test();
            IF( st->narrowBand || (L_sub(st->sr_core, 12800) == 0) || (L_sub(st->sr_core, 16000) == 0) )
            {
                int_lsp_fx( L_frame, st->lspold_uw, xspnew_uw, Aq, M, interpol_frac_fx, 0 );
                Copy(Aq, st->mem_Aq, nb_subfr*(M+1));
            }
        }
    }

    /* PLC: [Common: Classification] */

    IF( L_sub( st->sr_core, 16000) <= 0 )
    {
        test();
        test();
        test();
        IF ( sub(st->core_fx, TCX_20_CORE) == 0 || sub(st->core_fx, TCX_10_CORE) == 0 || (st->tcxonly && st->bfi_fx) )
        {
            Word16 pitch_C[NB_SUBFR16k];
            Word16 core_ext_mode, LTP_Gain;

            set16_fx(pitch_C, shl(round_fx(st->old_fpitch), 6), NB_SUBFR16k);

            core_ext_mode = GENERIC;
            move16();
            if(st->tcxonly == 0)
            {
                core_ext_mode = st->core_ext_mode;
                move16();
            }

            LTP_Gain = -32768/*-1.0f Q15*/;
            if(st->tcxltp)
            {
                LTP_Gain = st->tcxltp_last_gain_unmodified;
                move16();
            }


            FEC_clas_estim_fx(
                st,
                /*Opt_AMR_WB*/0, /*A*/
                st->L_frame_fx,
                &(st->clas_dec),
                core_ext_mode,
                pitch_C,
                synth,
                &st->lp_ener_FER_fx,
                /**decision_hyst*/NULL,     /* i/o: hysteresis of the music/speech decision          */
                /**UV_cnt*/ NULL,           /* i/o: number of consecutive frames classified as       */
                /**LT_UV_cnt*/ NULL,        /* i/o: long term consecutive frames classified as UV    */
                /**Last_ener*/ NULL,        /* i/o: last_energy frame                                */
                /**locattack*/ NULL,        /* i/o: detection of attack (mainly to localized speech burst) */
                /**lt_diff_etot*/NULL,      /* i/o: long-term total energy variation                 */
                /**amr_io_class*/ NULL,     /* i/o: classification for AMR-WB IO mode                */
                /*bitrate*/ 0  ,            /* i  : Decoded bitrate                                  */
                0,                          /* i  : Synthesis scaling                                */
                /**class_para*/ NULL,       /* o  : classification para. fmerit1                     */
                st->mem_syn_clas_estim_fx,  /* i/o: memory of the synthesis signal for frame class estimation */
                &st->classifier_Q_mem_syn,  /* i/o: exponent for memory of synthesis signal for frame class estimation */
                LTP_Gain,                   /* i  : means LTP Gain                                   */
                1  /*CLASSIFIER_TCX*/,      /* i  : signal classifier mode                           */
                bfi,                        /* i  : bad frame indicator                              */
                st->last_core_brate_fx      /* i  : bitrate of previous frame                        */
            );

        }
    }

    /*--------------------------------------------------------------------------------*
     * END
     *--------------------------------------------------------------------------------*/

    test();
    IF( bfi && sub(st->last_core_bfi , ACELP_CORE) != 0 )
    {
        /* Update FEC_scale_syn parameters */
        IF(st->tcxltp_gain == 0)
        {
            frame_ener_fx( L_frame, UNVOICED_CLAS, synth, shr(L_frame,1), &st->enr_old_fx, L_frame, 0, 0, 0 );
        }
        ELSE
        {
            Word16 pitch_Q0;
            pitch_Q0 = round_fx(st->old_fpitch);
            frame_ener_fx( L_frame, st->clas_dec, synth, pitch_Q0, &st->enr_old_fx, L_frame, 0, 0, 0 );
        }
    }


    test();
    test();
    IF(!bfi && sub(st->clas_dec, VOICED_TRANSITION) >= 0 && sub(st->clas_dec, INACTIVE_CLAS) < 0)
    {
        Word16 offset;

        IF(sub(st->core_fx, ACELP_CORE)==0)
        {
            offset = sub(st->nb_subfr,1);
            offset = imult1616(offset,add(M,1));
        }
        ELSE
        {
            offset = 0;
            move16();
        }
        /* use latest LPC set */
        st->old_enr_LP = Enr_1_Az_fx(Aq+offset, L_SUBFR); /*Q3*/
    }


    /* Update */
    Copy(synth_buf+L_frame, st->old_synth, st->old_synth_len);

    Copy( st->old_synthFB_fx + L_frameTCX - NS2SA_fx2(st->output_Fs_fx, PH_ECU_MEM_NS), st->synth_history_fx, NS2SA_fx2(st->output_Fs_fx, PH_ECU_MEM_NS) );
    Copy(synth_bufFB+L_frameTCX, st->old_synthFB_fx, st->old_synth_lenFB);
    Copy_Scale_sig( st->old_out_fx+NS2SA_fx2(st->output_Fs_fx, N_ZERO_MDCT_NS), st->old_synthFB_fx+st->old_synth_lenFB, NS2SA_fx2(st->output_Fs_fx, PH_ECU_LOOKAHEAD_NS), negate(st->Q_old_wtda));


    Copy(&xspnew_uw[(st->numlpc-1)*M], st->lspold_uw, M);
    Copy(&xsfnew_uw[(st->numlpc-1)*M], st->lsfold_uw, M);

    IF (bfi)
    {
        Copy(st->lspold_uw, st->lsp_old_fx, M); /* for recovery */
        Copy(st->lsfold_uw, st->lsf_old_fx, M); /* for recovery */
    }
    ELSE
    {
        Copy(&lsp[st->numlpc*M], st->lsp_old_fx, M);
        Copy(&lsf[st->numlpc*M], st->lsf_old_fx, M);
    }
    Copy(st->lsp_q_cng, st->old_lsp_q_cng, M);
    Copy(st->lsf_q_cng, st->old_lsf_q_cng, M);

    /* Update LP_CNG parameters */
    IF( st->tcxonly == 0 )
    {
        /* update CNG parameters in active frames */
        IF (sub(st->bwidth_fx,NB) == 0 && st->enableTcxLpc !=0 && sub(st->core_fx , ACELP_CORE)!= 0)
        {
            Word16 buf[L_LP], res[L_FRAME], A[M+1], Qexc, r_l[M+1], r_h[M+1], lsptmp[M], Q_r, tmp;

            assert(st->L_frame_fx==L_FRAME);
            Copy(synth+L_FRAME-L_LP, buf, L_LP);
            tmp = synth[L_FRAME-L_LP-1];
            Qexc = E_UTIL_f_preemph3(buf, st->preemph_fac, L_LP, &tmp, 1);
            autocorr_fx( buf, M, r_h, r_l, &Q_r, L_LP, Assym_window_W16fx, 0, 0 );
            lag_wind(r_h, r_l, M, INT_FS_FX, LAGW_WEAK);
            E_LPC_lev_dur(r_h, r_l, A, NULL, M, NULL);
            E_LPC_a_lsp_conversion(A, lsptmp, &xspnew_uw[0], M);
            Residu3_fx(A, buf+L_LP-L_FRAME, res, L_FRAME, 1);
            cng_params_upd_fx( lsptmp, res, st->L_frame_fx, &st->ho_circ_ptr_fx,
                               st->ho_ener_circ_fx, &st->ho_circ_size_fx, st->ho_lsp_circ_fx,
                               Qexc, DEC, st->ho_env_circ_fx, NULL, NULL, NULL, NULL, st->last_active_brate_fx );
        }
        ELSE
        {
            cng_params_upd_fx( &lsp[M], st->old_exc_fx+L_EXC_MEM_DEC-st->L_frame_fx,
            st->L_frame_fx, &st->ho_circ_ptr_fx, st->ho_ener_circ_fx,
            &st->ho_circ_size_fx, st->ho_lsp_circ_fx, st->Q_exc, DEC,
            st->ho_env_circ_fx, NULL, NULL, NULL, NULL, st->last_active_brate_fx );
        }

        /* Set 16k LSP flag for CNG buffer */
        st->ho_16k_lsp_fx[st->ho_circ_ptr_fx] = 1;
        move16();
        if ( sub(st->L_frame_fx,L_FRAME) == 0 )
        {
            st->ho_16k_lsp_fx[st->ho_circ_ptr_fx] = 0;
            move16();
        }
    }

    move16();
    st->last_is_cng = 0;

    /* Postfiltering */
    post_decoder( st,
                  *coder_type,
                  synth_buf,
                  pit_gain,
                  pitch,
                  signal_out,
                  bpf_noise_buf
                );

    IF( signal_outFB )
    {
        Copy( synthFB, signal_outFB, L_frameTCX );
    }

    IF( st->enablePlcWaveadjust)
    {
        if(!bfi)
        {
            st->plcInfo.nbLostCmpt = L_deposit_l(0);
        }

        IF (st->core_fx == 0)
        {
            set_state(st->plcInfo.Transient, st->core_fx, MAX_POST_LEN);
        }
    }


    return;
}


