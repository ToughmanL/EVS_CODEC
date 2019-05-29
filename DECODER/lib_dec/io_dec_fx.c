/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"     /* Compilation switches                   */
#include "stl.h"
#include "cnst_fx.h"       /* Common constants                       */
#include "prot_fx.h"       /* Function prototypes                    */
#include "disclaimer.h" /*for disclaimer*/
#include "basop_util.h"
#include "mime.h"
#include <assert.h>
/* WMC_TOOL_SKIP_FILE */

/*---------------------------------------------------------------------*
* Local functions
*---------------------------------------------------------------------*/

static void usage_dec(void);
static char *to_upper( char *str );

static char * bit_rate_to_string(char *string, Word32 bit_rate)
{
    char *src, *dst;
    /* NO_DATA  is also an allowed bit rate  */
    if ( bit_rate <= 0)
    {
        sprintf(string, "0.00");
        return string;
    }
    assert(bit_rate >= 100);
    src = string + sprintf(string, "%i", bit_rate);
    /* Insert a '.' before last two digits and remove last digit */
    /* What we want is to print %.2f of bit_rate/1000.0 */
    dst = src--;
    *--dst = *--src;
    *--dst = *--src;
    *--dst = '.';

    return string;
}
/*---------------------------------------------------------------------*
 * io_ini_dec()
 *
 * Processing of command line parameters
 *---------------------------------------------------------------------*/

void io_ini_dec_fx(
    const int argc,                /* i  : command line arguments number             */
    char *argv[],             /* i  : command line arguments                    */
    FILE **f_stream,          /* o  : input bitstream file                      */
    FILE **f_synth,           /* o  : output synthesis file                     */
    Word16 *quietMode,             /* o  : limited printouts                         */
    Word16 *noDelayCmp,            /* o  : turn off delay compensation               */
    Decoder_State_fx *st_fx,           /* o  : Decoder static variables structure        */
#ifdef SUPPORT_JBM_TRACEFILE
    char **jbmTraceFileName,   /* o  : VOIP tracefilename                        */
#endif
    char **jbmFECoffsetFileName /* : Output file  for Optimum FEC offset        */
)
{
    short i;
    char   stmp[50];
    Word16 evs_magic, amrwb_magic;
    char bit_rate_string[14];

    print_disclaimer(stderr);

    /*-----------------------------------------------------------------*
     * Initialization
     *-----------------------------------------------------------------*/

    i = 1;
    *f_synth = NULL;
    *f_stream = NULL;
    st_fx->Opt_AMR_WB_fx = 0;
    st_fx->Opt_VOIP_fx = 0;
    set_zero_Word8((Word8 *)stmp, sizeof(stmp));

    st_fx->bitstreamformat = G192;
    st_fx->amrwb_rfc4867_flag = -1;

    IF ( argc <= 1 )
    {
        usage_dec();
    }

    /*-----------------------------------------------------------------*
     * Optional input arguments
     *-----------------------------------------------------------------*/

    WHILE ( i < argc-3 )//i=1
    {
        /*-----------------------------------------------------------------*
         * VOIP mode
         *-----------------------------------------------------------------*/

        IF ( strcmp( to_upper(argv[i]), "-VOIP") == 0)
        {
            st_fx->Opt_VOIP_fx = 1;
            move16();
            i += 1;
        }
#ifdef SUPPORT_JBM_TRACEFILE
        /*-----------------------------------------------------------------*
         * VOIP Tracefile
         *-----------------------------------------------------------------*/

        ELSE IF ( strcmp( to_upper(argv[i]), "-TRACEFILE" ) == 0 )
        {
            *jbmTraceFileName = argv[i+1];
            i = i + 2;
        }
#endif
        /*-----------------------------------------------------------------*
        * FEC offset file
        *-----------------------------------------------------------------*/

        ELSE IF ( strcmp( to_upper(argv[i]), "-FEC_CFG_FILE" ) == 0 )
        {
            st_fx->writeFECoffset = 1;
            *jbmFECoffsetFileName = argv[i+1];
            i = i + 2;
        }

        /*-----------------------------------------------------------------*
         * Quiet mode
         *-----------------------------------------------------------------*/

        ELSE IF ( strcmp( to_upper(argv[i]), "-Q" ) == 0 )
        {
            *quietMode = 1;
            move16();
            i++;
        }

        /*-----------------------------------------------------------------*
         * deactivate delay compensation
         *-----------------------------------------------------------------*/

        ELSE IF ( strcmp( to_upper(argv[i]), "-NO_DELAY_CMP" ) == 0 )
        {
            *noDelayCmp = 1;
            i++;
        }

        /*-----------------------------------------------------------------*
        * MIME input file format
        *-----------------------------------------------------------------*/
        ELSE IF( strcmp( to_upper(argv[i]), "-MIME" ) == 0 )
        {
            st_fx->bitstreamformat = MIME;
            st_fx->amrwb_rfc4867_flag = 0;
            i++;
        }

        /*-----------------------------------------------------------------*
         * Option not recognized
         *-----------------------------------------------------------------*/

        ELSE
        {
            fprintf(stderr, "Error: Unknown option %s\n\n", argv[i]);
            usage_dec();
        }

    } /* end of while  */


    /*-----------------------------------------------------------------*
     * Mandatory input arguments
     *-----------------------------------------------------------------*/

    /*-----------------------------------------------------------------*
     * Output sampling frequency
     *-----------------------------------------------------------------*/

    if( i < argc - 2 )//i=2
    {
        st_fx->output_Fs_fx = (int)atoi( argv[i] ) * 1000;
        if( st_fx->output_Fs_fx != 8000 && st_fx->output_Fs_fx != 16000 && st_fx->output_Fs_fx != 32000 && st_fx->output_Fs_fx != 48000 )
        {
            fprintf(stderr, "Error: %d kHz is not a supported sampling rate\n\n", atoi( argv[i] ) );
            usage_dec();
        }

        i++;
    }
    else
    {
        fprintf (stderr, "Error: Sampling rate is not specified\n\n");
        usage_dec();
    }
    /*-----------------------------------------------------------------*
     * Input bitstream file
     *-----------------------------------------------------------------*/

    if( i < argc - 1 )//i=3
    {
        if ( (*f_stream = fopen(argv[i], "rb")) == NULL)
        {
            fprintf(stderr,"Error: input bitstream file %s cannot be opened\n\n", argv[i]);
            usage_dec();
        }
        /* If MIME/storage format selected, scan for the magic number at the beginning of the bitstream file */
        if( st_fx->bitstreamformat == MIME )
        {
            char buf[13];
            evs_magic   = 1 ;
            amrwb_magic = 1;

            if(NULL == fgets(buf, 13, *f_stream))
            {
                fprintf(stderr,"Error: input bitstream file %s cannot be read\n\n", argv[i]);
                usage_dec();
            }

            /* verify AMRWB magic number */
            if ( strncmp(buf, AMRWB_MAGIC_NUMBER, strlen(AMRWB_MAGIC_NUMBER)))
            {
                amrwb_magic = 0;
            }

            if ( strncmp(buf, EVS_MAGIC_NUMBER, strlen(EVS_MAGIC_NUMBER))) /* strncmp safer than strcmp */
            {
                evs_magic = 0;
            }

            if( evs_magic != 0 )
            {
                if ((fread(&buf,sizeof(char), 4, *f_stream) != 4 ) || !((buf[3] == 1) && (buf[2] == 0) && (buf[1] == 0) &&  (buf[0] == 0)) )
                {
                    fprintf(stderr, "Error: input bitstream file %s specifies unsupported number of evs audio channels\n\n",argv[i]);
                    usage_dec();
                }
            }

            if( evs_magic == 0 &&  amrwb_magic == 0 )
            {
                /* no valid MIME magic number  */
                fprintf(stderr, "Error: input bitstream file %s specifies unsupported MIME magic number (%13s) \n\n",argv[i],buf );
                usage_dec();
            }

            if( evs_magic )
            {
                fprintf( stderr, "Found MIME Magic number %s\n", EVS_MAGIC_NUMBER );
                st_fx->amrwb_rfc4867_flag = 0;
            }
            else
            {
                fprintf( stderr, "Found MIME Magic number %s\n",AMRWB_MAGIC_NUMBER );
                st_fx->amrwb_rfc4867_flag = 1;
                st_fx->Opt_AMR_WB_fx = 1;    /*  needed in case first initial RFC4867 frames/ToCs are lost */
            }
        }
        else if( st_fx->Opt_VOIP_fx == 0 )
        {
            /* G.192 format ....  preread the G.192 sync header */
            UWord16 utmp;
            if ( fread( &utmp, sizeof(unsigned short), 1, *f_stream ) != 1 )
            {
                /* error during pre-reading */
                if( ferror( *f_stream ) )
                {
                    fprintf(stderr, "Error: input G.192 bitstream file %s , can not be read  \n\n",argv[i] );
                }
                else
                {
                    fprintf(stderr, "Error: input G.192 bitstream file %s , has zero size, can not be read  \n\n",argv[i] );
                }
                usage_dec();
            }
            if( (sub(utmp, SYNC_GOOD_FRAME) != 0) && (sub(utmp, SYNC_BAD_FRAME) != 0) )
            {
                /* check for a valid first G.192 synch  word in Sync Header  */
                fprintf(stderr, "Error: input bitstream file %s does not have a valid G.192 synch word value \n\n",argv[i]);
                usage_dec();
            }
            /* now rewind the G.192 bitstream file */
            fseek( *f_stream , 0L, SEEK_SET );
        }
        /*  JBM format */

        fprintf( stderr, "Input bitstream file:   %s\n", argv[i]);
        i++;

    }
    else
    {
        fprintf (stderr, "Error: no input bitstream file specified\n\n");
        usage_dec();
    }

    /*-----------------------------------------------------------------*
     * Output synthesis file
     *-----------------------------------------------------------------*/

    if( i < argc )//i=4
    {
        if ( (*f_synth = fopen(argv[i], "wb")) == NULL )
        {
            fprintf( stderr, "Error: ouput synthesis file %s cannot be opened\n\n", argv[i] );
            usage_dec();
        }

        fprintf( stdout, "Output synthesis file:  %s\n", argv[i] );
        i++;
    }
    else
    {
        fprintf( stderr, "Error: no output synthesis file specified\n\n" );
        usage_dec();
    }
    fprintf( stdout, "\n" );

    if( !st_fx->Opt_VOIP_fx )
    {
        /*-----------------------------------------------------------------*
         * Read information from bitstream
         *-----------------------------------------------------------------*/
        if( st_fx->bitstreamformat == G192 )
        {
            read_indices_fx( st_fx, *f_stream, 1 );     /* rew_flag == 1 , reads future frames */
        }
        else
        {
            read_indices_mime( st_fx, *f_stream, 1 );   /* rew_flag == 1 ,  checks only very first  frame  */
            if( st_fx->amrwb_rfc4867_flag != 0 )
            {
                fseek(*f_stream,strlen(AMRWB_MAGIC_NUMBER),SEEK_SET);    /* restart after 9 bytes */
            }
            else
            {
                fseek(*f_stream,strlen(EVS_MAGIC_NUMBER)+4, SEEK_SET); /* restart after  16 bytes */
            }
        }

        /*-----------------------------------------------------------------*
         * Print info on screen
         *-----------------------------------------------------------------*/
        /*-----------------------------------------------------------------*
         * Print output sampling frequency
         *-----------------------------------------------------------------*/

        fprintf( stdout, "Output sampling rate:   %d Hz\n", st_fx->output_Fs_fx );

        /*-----------------------------------------------------------------*
         * Print bitrate
         *-----------------------------------------------------------------*/

        fprintf( stdout, "Bitrate:                %s kbps\n", bit_rate_to_string(bit_rate_string, st_fx->total_brate_fx) );
        if ( st_fx->total_brate_fx <= 0 )
        {
            if( st_fx->bitstreamformat == G192 )
            {
                fprintf( stdout, "Active Bitrate not identified in bitstream file \n" );
            }
            else
            {
                /* MIME */
                fprintf( stdout, "Active Bitrate not identified from first MIME frame \n" );
            }
        }

    }

    return;
}

/*---------------------------------------------------------------------*
 * to_upper()
 *
 * Capitalize all letters of a string.
 * (normally to_upper() function would be used but it does not work in Unix)
 *---------------------------------------------------------------------*/

static char *to_upper( char *str )
{
    short i;
    char *p = str;

    i = 0;
    while (str[i] != 0)
    {
        if (str[i] >= 'a' && str[i] <= 'z') str[i] -= 0x20;
        i++;
    }

    return p;
}

static void usage_dec( void )
{
    fprintf(stdout,"Usage : EVS_dec.exe [Options] Fs bitstream_file output_file\n\n");

    fprintf(stdout,"Mandatory parameters:\n");
    fprintf(stdout,"---------------------\n");
    fprintf(stdout,"Fs                : Output sampling rate in kHz (8, 16, 32 or 48)\n");
    fprintf(stdout,"bitstream_file    : Input bitstream filename or RTP packet filename (in VOIP mode)\n");
    fprintf(stdout,"output_file       : Output speech filename \n\n");

    fprintf(stdout,"Options:\n");
    fprintf(stdout,"--------\n");
    fprintf(stdout, "-VOIP            : VOIP mode,\n");
#ifdef SUPPORT_JBM_TRACEFILE
    fprintf(stdout, "-Tracefile TF    : Generate trace file named TF,\n");
#endif
    fprintf(stdout, "-no_delay_cmp    : Turn off delay compensation\n");
    fprintf(stdout, "-fec_cfg_file    : Optimal channel aware configuration computed by the JBM   \n");
    fprintf(stdout, "                   as described in Section 6.3.1 of TS26.448. The output is \n");
    fprintf(stdout, "                   written into a .txt file. Each line contains the FER indicator \n");
    fprintf(stdout, "                   (HI|LO) and optimal FEC offset. \n");

    fprintf(stdout, "-mime            : Mime input bitstream file format\n");
    fprintf(stdout, "                   The decoder reads both TS26.445 Annex.2.6 and RFC4867 Mime Storage Format,\n");
    fprintf(stdout, "                   the magic word in the mime input file is used to determine the format.\n");
    fprintf(stdout, "                   default input bitstream file format is G.192\n");
    fprintf(stdout, "-q               : Quiet mode, no frame counter\n");
    fprintf(stdout, "                   default is OFF\n");
    fprintf(stdout, "\n");
    exit(-1);
}
