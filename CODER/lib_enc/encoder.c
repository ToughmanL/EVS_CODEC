/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/


#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include "options.h"
#include "stl.h"
#include "disclaimer.h"
#include "g192.h"
#include "stat_enc_fx.h"
#include "prot_fx.h"
#pragma comment(lib,"ws2_32.lib")

/*------------------------------------------------------------------------------------------*
 * Global variables
 *------------------------------------------------------------------------------------------*/
long frame = 0;                 /* Counter of frames */

/*------------------------------------------------------------------------------------------*
 * Local constants
 *------------------------------------------------------------------------------------------*/

int main()
{
	char Inputfile[50]="J:\\SAMPLE\\EVS_SAMPLE\\16kHz\\sample0000.pcm";
	char Outputfile[50]="J:\\SAMPLE\\EVS_SAMPLE\\16kHz\\sample0000.txt";
	coder(Inputfile,Outputfile);
	return 0;
}

int coder(char Inputfile[] ,char Outputfile[])
{
    FILE             *f_stream = NULL;                    /* output bitstream file */
    Indice_fx ind_list[MAX_NUM_INDICES];                  /* list of indices */
    Encoder_State_fx *st_fx;                              /*Encoder state struct*/
    Word16           enc_delay;
    FILE *f_input;                                        /* input signal file */
    FILE *f_rate;                                         /* bitrate switching profile file */
    FILE *f_bwidth;                                       /* bandwidth switching profile file */
    FILE *f_rf = NULL;                                    /* Channel aware configuration file */
    Word16 input_frame;
    Word16 tmps;
    Word16 n_samples;
    Word16 data[L_FRAME48k];                              /* Input buffer */
    Word32 bwidth_profile_cnt = 0;                        /* counter of frames for bandwidth switching profile file */
    Word16 quietMode = 0;
    Word16 noDelayCmp = 0;
    Word16 Opt_RF_ON_loc, rf_fec_offset_loc;


    UWord8 pFrame[(MAX_BITS_PER_FRAME + 7) >> 3];
    Word16 pFrame_size = 0;

	int argc=7;//����Ҫ����6
	char **argv=(char**)malloc(4*sizeof(char*));
	argv[1]="-MAX_BAND";//��ߴ���
	argv[2]="-WB";//����ģʽ
	argv[3]="12650";//������
	argv[4]="16";//������
	argv[5]=Inputfile;//�����ļ�
	argv[6]=Outputfile;//����ļ�


    /* start WMOPS counting */
    BASOP_init

    /*Inits*/
    f_bwidth = f_rate = NULL;

    /*------------------------------------------------------------------------------------------*
     * Allocate memory for static variables
     * Processing of command-line parameters
     * Encoder initialization
     *------------------------------------------------------------------------------------------*/

    if ( (st_fx = (Encoder_State_fx *) calloc( 1, sizeof(Encoder_State_fx) ) ) == NULL )
    {
        fprintf(stderr, "Can not allocate memory for encoder state structure\n");
        exit(-1);
    }

    io_ini_enc_fx( argc, argv, &f_input, &f_stream, &f_rate, &f_bwidth,
                   &f_rf,
                   &quietMode, &noDelayCmp, st_fx);

    /*input_frame = (short)(st->input_Fs / 50);*/
    st_fx->input_frame_fx = extract_l(Mult_32_16(st_fx->input_Fs_fx , 0x0290));
    input_frame = st_fx->input_frame_fx;

    Opt_RF_ON_loc = st_fx->Opt_RF_ON;
    rf_fec_offset_loc = st_fx->rf_fec_offset;

    st_fx->ind_list_fx = ind_list;
    init_encoder_fx( st_fx );

    /*------------------------------------------------------------------------------------------*
     * Compensate for encoder delay (bitstream aligned with input signal)
     * Compensate for the rest of codec delay (local synthesis aligned with decoded signal and original signal)
     *------------------------------------------------------------------------------------------*/

    enc_delay = NS2SA(st_fx->input_Fs_fx, get_delay_fx(ENC, st_fx->input_Fs_fx));

    if ( noDelayCmp == 0 )
    {
        /* read samples and throw them away */
        if( (tmps = (Word16)fread(data, sizeof(short), enc_delay, f_input)) != enc_delay )
        {
        }

    }

    /*------------------------------------------------------------------------------------------*
     * Loop for every frame of input data
     * - Read the input data
     * - Select the best operating mode
     * - Run the encoder
     * - Write the parameters into output bitstream file
     *------------------------------------------------------------------------------------------*/
    BASOP_end_noprint;
    BASOP_init;

#if (WMOPS)
    Init_WMOPS_counter();
    Reset_WMOPS_counter();
    setFrameRate(48000, 960);
#endif


    if (quietMode == 0)
    {
        fprintf( stdout, "\n------ Running the encoder ------\n\n" );
        fprintf( stdout, "Frames processed:       " );
    }
    else
    {
        fprintf( stdout, "\n-- Start the encoder (quiet mode) --\n\n" );
    }


    /*Encode-a-frame loop start*/
    while( (n_samples = (short)fread(data, sizeof(short), input_frame, f_input)) > 0 )
    {
#if (WMOPS)
        Reset_WMOPS_counter();
#endif
        SUB_WMOPS_INIT("enc");

        IF(f_rf != NULL)
        {
            read_next_rfparam_fx(
                &st_fx->rf_fec_offset, &st_fx->rf_fec_indicator, f_rf);
            rf_fec_offset_loc = st_fx->rf_fec_offset;
        }

        IF(f_rate != NULL)
        {
            /* read next bitrate from profile file (only if invoked on the cmd line) */
            read_next_brate_fx( &st_fx->total_brate_fx, st_fx->last_total_brate_fx,
                                f_rate, st_fx->input_Fs_fx, &st_fx->Opt_AMR_WB_fx, &st_fx->Opt_SC_VBR_fx, &st_fx->codec_mode );
        }

        IF (f_bwidth != NULL)
        {
            /* read next bandwidth from profile file (only if invoked on the cmd line) */
            read_next_bwidth_fx( &st_fx->max_bwidth_fx, f_bwidth, &bwidth_profile_cnt, st_fx->input_Fs_fx );
        }

        IF( ( st_fx->Opt_RF_ON && ( L_sub( st_fx->total_brate_fx, ACELP_13k20 ) != 0 ||  L_sub( st_fx->input_Fs_fx, 8000 ) == 0 || st_fx->max_bwidth_fx == NB ) )
            || st_fx->rf_fec_offset == 0 )
        {
            IF( L_sub( st_fx->total_brate_fx, ACELP_13k20) == 0 )
            {
                st_fx->codec_mode = MODE1;
                reset_rf_indices(st_fx);
            }
            st_fx->Opt_RF_ON = 0;
            st_fx->rf_fec_offset = 0;

        }

        IF( Opt_RF_ON_loc && rf_fec_offset_loc != 0 && L_sub( st_fx->total_brate_fx, ACELP_13k20 ) == 0 && L_sub( st_fx->input_Fs_fx, 8000 ) != 0 && st_fx->max_bwidth_fx != NB )
        {
            st_fx->codec_mode = MODE2;
            IF(st_fx->Opt_RF_ON == 0)
            {
                reset_rf_indices(st_fx);
            }
            st_fx->Opt_RF_ON = 1;
            st_fx->rf_fec_offset = rf_fec_offset_loc;
        }

        /* in case of 8kHz sampling rate or when in "max_band NB" mode, limit the total bitrate to 24.40 kbps */
        IF ( (L_sub( st_fx->input_Fs_fx, 8000 ) == 0 || (st_fx->max_bwidth_fx == NB)) && L_sub( st_fx->total_brate_fx, ACELP_24k40 ) > 0 )
        {
            st_fx->total_brate_fx = ACELP_24k40;
            st_fx->codec_mode = MODE2;
        }


        /* run the main encoding routine */


        IF ( st_fx->Opt_AMR_WB_fx )
        {
            SUB_WMOPS_INIT("amr_wb_enc");
            amr_wb_enc_fx( st_fx, data, n_samples);
            END_SUB_WMOPS;
        }
        ELSE
        {
            SUB_WMOPS_INIT("evs_enc");
            /* EVS encoder*/
            evs_enc_fx( st_fx, data, n_samples);
            END_SUB_WMOPS;
        }
        /* pack indices into serialized payload format */
        if( st_fx->bitstreamformat == MIME )
        {
            indices_to_serial(st_fx, pFrame, &pFrame_size);
        }

        /* write indices into bitstream file */
        write_indices_fx( st_fx, f_stream, pFrame, pFrame_size );

        END_SUB_WMOPS;
        /* update WMPOS counting (end of frame) */


        fflush(stderr);

        frame++;
        if (quietMode == 0)
        {
            fprintf( stdout, "%-8ld\b\b\b\b\b\b\b\b", frame );
        }

#if (WMOPS)
        fwc();
#endif
    }
    /* ----- Encode-a-frame loop end ----- */


    if (quietMode == 0)
    {
        fprintf( stdout, "\n\n" );
        fprintf(stderr, "Encoding finished\n\n");
    }
    else
    {
        fprintf(stderr, "Encoding of %ld frames finished\n\n", frame);
    }




#if (WMOPS)
    fwc();
    printf("\nEncoder complexity\n");
    WMOPS_output(0);
    printf("\n");
#endif

    /* Close Encoder, Close files and free ressources */
    BASOP_init


    IF(st_fx != NULL)
    {
        /* common delete function */
        destroy_encoder_fx( st_fx );
        free(st_fx);
    }

    BASOP_end_noprint


    IF(f_input)
    fclose(f_input);
    IF(f_stream)
    fclose(f_stream);
    IF(f_rate)
    fclose(f_rate);
    IF(f_bwidth)
    fclose(f_bwidth);


    return 0;
}
