/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/


#include <assert.h>
#include "rom_com_fx.h"
#include "stat_com.h"
#include "options.h"
#include "stl.h"
#include "prot_fx.h"
#include "basop_util.h"

static
void decode_frame_type(Decoder_State_fx *st)
{
    Word32 L_tmp;
    Word16 num_bits;
    UWord16 lsb;
    Word16 frame_size_index;
    Word16 n;
    Word32 total_brate;
    Word16 frame_len_indicator;

    frame_size_index = 0;
    move16();
    total_brate = st->total_brate_fx;
    move16();

    Mpy_32_16_ss(st->total_brate_fx, 5243, &L_tmp, &lsb); /* 5243 is 1/50 in Q18. (0+18-15=3) */
    num_bits = extract_l(L_shr(L_tmp, 3)); /* Q0 */


    /* Get Frame Type (NULL,SID,ACTIVE) and Frame Mode (2kbps, 4kbps,...) */

    IF (sub(st->mdct_sw, MODE1) == 0)
    {
        st->m_frame_type = ACTIVE_FRAME;
        move16();

        FOR (n=0; n<FRAME_SIZE_NB; n++)
        {
            IF (sub(FrameSizeConfig[n].frame_bits, num_bits) == 0)
            {
                frame_size_index = n;
                move16();
                BREAK;
            }
        }

    }
    ELSE
    {

        /* ZERO Frame */
        IF (  L_sub(st->total_brate_fx, FRAME_NO_DATA) == 0 )
        {
            st->bwidth_fx = st->last_bwidth_fx;
            move16();
            st->m_frame_type = ZERO_FRAME;
            move16();
        }

        /* SID frame */
        ELSE IF ( L_sub(st->total_brate_fx, SID_2k40) == 0 )
        {
            st->cng_type_fx = get_next_indice_fx(st, 1);
            if( sub(st->cng_type_fx, FD_CNG) != 0 )
            {
                st->BER_detect = 1;
            }
            st->m_frame_type = SID_FRAME;
            move16();
            frame_size_index = 1;
            move16();
            st->bwidth_fx = get_next_indice_fx(st, 2);
            move16();

            frame_len_indicator = get_next_indice_fx(st, 1);
            IF ( sub(st->bwidth_fx, NB) == 0 )
            {
                if( frame_len_indicator )
                {
                    st->BER_detect = 1;
                    move16();
                }
                frame_len_indicator = 0;
                move16();
            }
            IF ( frame_len_indicator == 0 )
            {
                st->L_frame_fx = L_FRAME;
                st->total_brate_fx = 9600;
            }
            ELSE
            {
                st->L_frame_fx = L_FRAME16k;
                IF ( st->last_total_brate_fx==16400 || st->last_total_brate_fx==24400 )
                {
                    st->total_brate_fx = st->last_total_brate_fx;
                }
                ELSE
                {
                    st->total_brate_fx = 16400;
                }
            }

            Mpy_32_16_ss(st->total_brate_fx, 5243, &L_tmp, &lsb); /* 5243 is 1/50 in Q18. (0+18-15=3) */
            num_bits = extract_l(L_shr(L_tmp, 3)); /* Q0 */
            FOR (n=0; n<FRAME_SIZE_NB; n++)
            {
                IF (sub(FrameSizeConfig[n].frame_bits, num_bits) == 0)
                {
                    frame_size_index = n;
                    move16();
                    BREAK;
                }
            }
        }

        /* EVS MODES */
        ELSE
        {
            /* Get Frame mode */
            st->m_frame_type = ACTIVE_FRAME;
            move16();

            FOR (n=0; n<FRAME_SIZE_NB; n++)
            {

                IF (sub(FrameSizeConfig[n].frame_bits, num_bits)==0)
                {
                    frame_size_index =  n;
                    move16();
                    BREAK;
                }
            }


            IF (st->rf_flag == 0)
            {

                /* Get bandwidth mode */
                st->bwidth_fx = get_next_indice_fx(st, FrameSizeConfig[frame_size_index].bandwidth_bits);

                st->bwidth_fx = add(st->bwidth_fx, FrameSizeConfig[frame_size_index].bandwidth_min);

            }
            ELSE
            {
                st->bwidth_fx += FrameSizeConfig[frame_size_index].bandwidth_min;
            }

            IF (sub(st->bwidth_fx, FB) > 0)
            {
                st->bwidth_fx = FB;
                move16();
                st->BER_detect = 1;
                move16();
            }

            test();
            IF ( sub(st->bwidth_fx, SWB) > 0 && L_sub(st->total_brate_fx, ACELP_16k40) < 0 )
            {
                st->bwidth_fx = SWB;
                move16();
                st->BER_detect = 1;
                move16();
            }

            /* Get reserved bits */
            IF (FrameSizeConfig[frame_size_index].reserved_bits && st->rf_flag == 0)
            {
                Word16 dummyBit;
                dummyBit = (Word8)get_next_indice_fx(st, 1);
                move16();
                if (dummyBit != 0)
                {
                    st->BER_detect = 1;
                    move16();
                }
                assert( FrameSizeConfig[frame_size_index].reserved_bits == 1);
            }
        }
    }

    st->rate_switching_init = 0;
    move16();

    if( st->last_codec_mode != MODE2 || !st->BER_detect )
    {
        /* Mode  or Rate Change */
        test();
        test();
        test();
        IF ( (sub(st->m_frame_type, ACTIVE_FRAME) == 0 || sub(st->m_frame_type, SID_FRAME) == 0) && (s_or(s_or(s_or(s_or(L_sub(st->total_brate_fx, st->last_total_brate_fx)!=0, sub(st->bwidth_fx,st->last_bwidth_fx)!=0), sub(st->last_codec_mode, MODE1) == 0), sub(st->rf_flag, st->rf_flag_last) != 0),st->force_lpd_reset)) )
        {
            st->rate_switching_init = 1;
            move16();

            /* Reconf Core */
            mode_switch_decoder_LPD( st, st->bwidth_fx, st->total_brate_fx, frame_size_index );

            /* Reconf CLDFB */
            IF( sub (i_mult(st->cldfbAna_fx->no_channels,st->cldfbAna_fx->no_col), st->L_frame_fx) != 0 )
            {
                Word16 newCldfbBands = CLDFB_getNumChannels(L_mult0(st->L_frame_fx,50));

                resampleCldfb( st->cldfbAna_fx, newCldfbBands, st->L_frame_fx, 0 );
                IF ( sub (st->L_frame_fx,L_FRAME16k) <= 0 )
                {
                    resampleCldfb( st->cldfbBPF_fx, newCldfbBands, st->L_frame_fx, 0 );
                }
            }
            IF ( sub(st->bwidth_fx,NB)==0 )
            {
                st->cldfbSyn_fx->bandsToZero =  sub (st->cldfbSyn_fx->no_channels,10);
            }
            ELSE
            {
                st->cldfbSyn_fx->bandsToZero = 0;
            }

            /* Reconf FD-CNG */
            L_tmp = st->total_brate_fx;
            move32();
            test();
            if( sub(st->rf_flag,1) == 0 && L_sub(st->total_brate_fx,ACELP_13k20) == 0 )
            {
                L_tmp = ACELP_9k60;
                move32();
            }
            configureFdCngDec( st->hFdCngDec_fx, st->bwidth_fx, L_tmp, st->L_frame_fx );
            test();
            test();
            IF ( (sub(st->last_L_frame_fx,st->L_frame_fx)!=0) && (sub(st->L_frame_fx,320)<=0) && (sub(st->last_L_frame_fx,320)<=0) )
            {
                lerp( st->hFdCngDec_fx->hFdCngCom->olapBufferSynth2, st->hFdCngDec_fx->hFdCngCom->olapBufferSynth2, st->L_frame_fx*2, st->last_L_frame_fx*2 );
                test();
                IF ( sub(st->m_frame_type, SID_FRAME) == 0 && sub(st->hFdCngDec_fx->hFdCngCom->frame_type_previous, ACTIVE_FRAME) != 0 )
                {
                    lerp( st->hFdCngDec_fx->hFdCngCom->olapBufferSynth, st->hFdCngDec_fx->hFdCngCom->olapBufferSynth, st->L_frame_fx*2, st->last_L_frame_fx*2 );

                    IF( sub(st->L_frame_fx, L_FRAME) == 0 )
                    {
                        FOR( n=0; n < st->L_frame_fx*2; n++ )
                        {
                            st->hFdCngDec_fx->hFdCngCom->olapBufferSynth[n] = mult_r( st->hFdCngDec_fx->hFdCngCom->olapBufferSynth[n], 20480 );
                        }
                    }
                    ELSE
                    {
                        FOR( n=0; n < st->L_frame_fx*2; n++ )
                        {
                            st->hFdCngDec_fx->hFdCngCom->olapBufferSynth[n] = mult_r( shl(st->hFdCngDec_fx->hFdCngCom->olapBufferSynth[n],1), 26214 );
                        }
                    }
                }
            }
            IF ( sub(st->bwidth_fx,st->last_bwidth_fx)!=0 )
            {
                st->hFdCngDec_fx->hFdCngCom->msFrCnt_init_counter = 0;
                st->hFdCngDec_fx->hFdCngCom->init_old = 32767;
            }

            /* Reconf BPF */
            st->p_bpf_noise_buf=NULL;
            IF (st->tcxonly == 0)
            {
                st->p_bpf_noise_buf = st->bpf_noise_buf;
            }
        }
    }

    st->total_brate_fx = total_brate;

}


Word16 dec_acelp_tcx_frame(Decoder_State_fx *st,
                           Word16 *coder_type,
                           Word16 *concealWholeFrame,
                           Word16 *pcmBuf,
                           Word16 * bpf_noise_buf,
                           Word16 * pcmbufFB,
                           Word32 bwe_exc_extended[], /* i/o: bandwidth extended excitation       */
                           Word16 *voice_factors,     /* o  : voicing factors                     */
                           Word16 pitch_buf[]         /* o  : floating pitch for each subframe    */
                          )
{
    Word16              num_bits;
    Word32              L_tmp;
    UWord16             lsb;
    Word16              bitsRead;
    Word16              tmp;
    Word16              i;
    Word16              start_bit_pos;
    Word16              param[DEC_NPRM_DIV*NB_DIV+NPRM_LPC_NEW];
    Word16              old_bwe_exc[(PIT16k_MAX + (L_FRAME16k + 1) + L_SUBFR16k) * 2]; /* excitation buffer */
    Word16              *ptr_bwe_exc;              /* pointer to BWE excitation signal in the current frame */

    Mpy_32_16_ss(st->total_brate_fx, 5243, &L_tmp, &lsb); /* 5243 is 1/50 in Q18. (0+18-15=3) */
    num_bits = extract_l(L_shr(L_tmp, 3)); /* Q0 */

    /* -------------------------------------------------------------- */
    /* Set the bit-stream                                             */
    /* -------------------------------------------------------------- */


    start_bit_pos = st->next_bit_pos_fx;
    move16();

    if( sub(st->rf_flag,1) == 0 )
    {
        start_bit_pos = sub(start_bit_pos,2);
    }
    /* -------------------------------------------------------------- */
    /* IDENTIFY FRAME TYPE                                            */
    /* -------------------------------------------------------------- */

    st->m_old_frame_type = st->m_frame_type;
    move16();

    IF ( *concealWholeFrame == 0)
    {

        Word16 m_frame_type = st->m_frame_type;
        Word16 bwidth = st->bwidth_fx;
        Word16 cng_type = st->cng_type_fx;
        Word16 L_frame = st->L_frame_fx;
        Word32 total_brate = st->last_total_brate_fx;

        decode_frame_type( st );

        st->force_lpd_reset = 0;

        test();
        test();
        test();
        test();
        IF( (sub(st->last_codec_mode, MODE2) == 0) && (st->BER_detect || (st->prev_bfi_fx && (sub(st->m_frame_type, ZERO_FRAME) == 0) && (sub(st->m_old_frame_type, ACTIVE_FRAME) == 0) ) ) )
        {
            /* Copy back parameters from previous frame, because there is a high risk they are corrupt
             * DO concealment with configuration used in previous frame                                */
            st->m_frame_type = m_frame_type;
            move16();
            st->bwidth_fx = bwidth;
            move16();
            st->cng_type_fx = cng_type;
            move16();
            st->L_frame_fx = L_frame;
            move16();
            if( st->ini_frame_fx != 0 )
            {
                st->total_brate_fx = total_brate;
                move32();
            }

            *concealWholeFrame = 1;
            move16();
            st->m_decodeMode = DEC_CONCEALMENT_EXT;
            move16();
            st->BER_detect = 0;
            move16();

            test();
            test();
            IF( (sub(st->bwidth_fx, st->last_bwidth_fx) != 0 ) || (sub(st->rf_flag, st->rf_flag_last) != 0) || (L_sub(st->total_brate_fx, st->last_total_brate_fx) != 0) )
            {
                st->force_lpd_reset = 1;
                move16();
            }

            st->core_brate_fx = st->last_core_brate_fx;
            move32();
            st->bfi_fx = 1;
            move16();
            IF(st->ini_frame_fx == 0 )
            {
                st->tcx_cfg.tcx_coded_lines = getNumTcxCodedLines(SWB);
            }
        }
        ELSE
        {
            st->core_brate_fx = st->total_brate_fx;
            move32();
            bpf_noise_buf = st->p_bpf_noise_buf;
            move16();
        }
    }

    IF( *concealWholeFrame != 0 )
    {
        /* add two zero bytes for arithmetic coder flush */
        FOR( i=0; i<8*2; i++ )
        {
            st->bit_stream_fx[i] = 0;
        }
    }


    IF (  s_and(sub(st->m_frame_type,SID_FRAME) != 0, sub(st->m_frame_type,ZERO_FRAME) != 0) )   /* test */
    {

        /* -------------------------------------------------------------- */
        /* DECODE CORE                                                    */
        /* -------------------------------------------------------------- */

        tmp = sub(num_bits, sub(st->next_bit_pos_fx, start_bit_pos));
        bitsRead = 0;
        move16();

        set16_fx( old_bwe_exc + PIT16k_MAX * 2, 0, ((L_FRAME16k + 1) + L_SUBFR16k) * 2 );
        ptr_bwe_exc = old_bwe_exc + PIT16k_MAX * 2;
        Copy( st->old_bwe_exc_fx, old_bwe_exc, PIT16k_MAX * 2 );

        IF ( sub(st->m_decodeMode, DEC_NO_FRAM_LOSS) == 0 )
        {
            decoder_LPD(pcmBuf,
                        pcmbufFB,
                        &tmp,
                        st,
                        bpf_noise_buf,
                        0,
                        &bitsRead,
                        coder_type,
                        param,
                        pitch_buf,
                        voice_factors,
                        ptr_bwe_exc
                       );

            test();
            test();
            test();
            test();
            test();
            IF( st->rate_switching_init == 0 && sub((st->last_codec_mode), MODE2) == 0 && !(st->use_partial_copy && sub(st->rf_frame_type, RF_TCXFD) >= 0 && sub(st->rf_frame_type, RF_TCXTD2) <= 0) && sub(st->bfi_fx, 1) == 0 )
            {
                *concealWholeFrame = 1;
                move16();
                st->m_decodeMode = DEC_CONCEALMENT_EXT;
                move16();
                st->BER_detect = 0;
                move16();
            }

        }
        ELSE IF (sub(st->m_decodeMode, DEC_CONCEALMENT_EXT) == 0)
        {
            /* Decode the LPD data */
            decoder_LPD( pcmBuf,
                         pcmbufFB,
                         NULL,
                         st,
                         bpf_noise_buf,
                         1,
                         &bitsRead,
                         coder_type,
                         NULL,
                         pitch_buf,
                         voice_factors,
                         ptr_bwe_exc
                       );
        }

        test();
        test();
        test();
        test();
        IF( ( st->bfi_fx == 0 && (sub(st->prev_bfi_fx, 1) == 0 || sub(st->prev_use_partial_copy,1) == 0)) || ((sub(st->last_vbr_hw_BWE_disable_dec_fx,1) == 0) && (st->vbr_hw_BWE_disable_dec_fx == 0)) )
        {
            st->bwe_non_lin_prev_scale_fx = 0;
            set16_fx( st->old_bwe_exc_extended_fx, 0, NL_BUFF_OFFSET );
        }

        test();
        test();
        IF( st->core_fx == ACELP_CORE && st->igf != 0 && st->con_tcx == 0)
        {
            non_linearity_fx( ptr_bwe_exc, bwe_exc_extended, L_FRAME32k, &st->bwe_non_lin_prev_scale_fx, st->Q_exc,
                              *coder_type, voice_factors, st->L_frame_fx
                            );

            /* update the old BWE exe memory */
            Copy( &old_bwe_exc[L_FRAME32k], st->old_bwe_exc_fx, PIT16k_MAX * 2 );
        }
        ELSE
        {
            set16_fx( st->old_bwe_exc_extended_fx, 0, NL_BUFF_OFFSET );
            set16_fx( st->old_bwe_exc_fx, 0, PIT16k_MAX * 2 );
            st->bwe_non_lin_prev_scale_fx = 0;
            move32();
        }

        /* for ACELP mode, skip core data to read TD-BWE side info */
        test();
        test();
        IF( (!st->bfi_fx) && sub(st->core_fx,ACELP_CORE) == 0 && st->total_brate_fx > 0)
        {
            /* target bs-position "-3": -2 as earlier "start_bit_pos -= 2;" are included in st->rf_target_bits
                                        -1 as flag-bit not considered in rf_target_bits */
            IF (sub(st->rf_flag, 1)==0)
            {
                get_next_indice_tmp_fx(st, start_bit_pos + num_bits - st->rf_target_bits - 3 - get_tbe_bits_fx(st->total_brate_fx, st->bwidth_fx, st->rf_flag) - st->next_bit_pos_fx);
            }
            ELSE
            {
                get_next_indice_tmp_fx(st, start_bit_pos + num_bits - st->rf_target_bits - get_tbe_bits_fx(st->total_brate_fx, st->bwidth_fx, st->rf_flag) - st->next_bit_pos_fx);
            }
            tbe_read_bitstream_fx(st);
        }

        /* updates */
        st->last_voice_factor_fx = voice_factors[st->nb_subfr-1];;
        move16();
        st->last_coder_type_fx = *coder_type;

    }
    ELSE
    {

        IF ( sub(st->m_frame_type,SID_FRAME) == 0)
        {
            FdCng_decodeSID(st->hFdCngDec_fx->hFdCngCom, st);
        }

        /* updates */
        st->last_voice_factor_fx = 0;
        move16();
        st->last_coder_type_fx = INACTIVE;
        move16();
    }

    return 0;
}
