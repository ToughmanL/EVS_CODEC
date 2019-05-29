/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include <stdlib.h>
#include <assert.h>
#include "stl.h"
#include "cnst_fx.h"        /* Common constants                       */
#include "prot_fx.h"        /* Function prototypes                    */
#include "options.h"
#include "basop_util.h"
#include "rom_com_fx.h"
#include "mime.h"

/*-------------------------------------------------------------------*
* pack_bit()
*
* insert a bit into packed octet
*-------------------------------------------------------------------*/
static void pack_bit(
    const Word16 bit,    /* i:   bit to be packed */
    UWord8 **pt,         /* i/o: pointer to octet array into which bit will be placed */
    UWord8 *omask        /* i/o: output mask to indicate where in the octet the bit is to be written */
)
{
    if (*omask == 0x80)
    {
        **pt = 0;
    }
    if (bit != 0)
    {
        **pt = **pt | *omask;
    }
    *omask >>= 1;
    if (*omask == 0)
    {
        *omask = 0x80;
        (*pt)++;
    }

    return;
}

/*-------------------------------------------------------------------*
* unpack_bit()
*
* unpack a bit from packed octet
*-------------------------------------------------------------------*/
static Word16 unpack_bit(
    UWord8 **pt,         /* i/o: pointer to octet array from which bit will be read */
    UWord8 *mask         /* i/o: mask to indicate the bit in the octet */
)
{
    Word16 bit;

    bit = (**pt & *mask) != 0;
    *mask >>= 1;
    if (*mask == 0)
    {
        *mask = 0x80;
        (*pt)++;
    }
    return bit;
}

/*-------------------------------------------------------------------*
* rate2AMRWB_IOmode()
*
* lookup AMRWB IO mode
*-------------------------------------------------------------------*/
static Word16 rate2AMRWB_IOmode(
    Word32 rate                    /* i: bit rate */
)
{
    switch ( rate )
    {
    /* EVS AMR-WB IO modes */
    case SID_1k75      :
        return AMRWB_IO_SID;
    case ACELP_6k60    :
        return AMRWB_IO_6600;
    case ACELP_8k85    :
        return AMRWB_IO_8850;
    case ACELP_12k65   :
        return AMRWB_IO_1265;
    case ACELP_14k25   :
        return AMRWB_IO_1425;
    case ACELP_15k85   :
        return AMRWB_IO_1585;
    case ACELP_18k25   :
        return AMRWB_IO_1825;
    case ACELP_19k85   :
        return AMRWB_IO_1985;
    case ACELP_23k05   :
        return AMRWB_IO_2305;
    case ACELP_23k85   :
        return AMRWB_IO_2385;
    default:
        return -1;
    }
}

/*-------------------------------------------------------------------*
* rate2EVSmode()
*
* lookup EVS mode
*-------------------------------------------------------------------*/
static Word16 rate2EVSmode(
    Word32 rate                    /* i: bit rate */
)
{
    switch ( rate )
    {
    /* EVS Primary modes */
    case FRAME_NO_DATA :
        return NO_DATA_TYPE;
    case SID_2k40      :
        return PRIMARY_SID;
    case PPP_NELP_2k80 :
        return PRIMARY_2800;
    case ACELP_7k20    :
        return PRIMARY_7200;
    case ACELP_8k00    :
        return PRIMARY_8000;
    case ACELP_9k60    :
        return PRIMARY_9600;
    case ACELP_13k20   :
        return PRIMARY_13200;
    case ACELP_16k40   :
        return PRIMARY_16400;
    case ACELP_24k40   :
        return PRIMARY_24400;
    case ACELP_32k     :
        return PRIMARY_32000;
    case ACELP_48k     :
        return PRIMARY_48000;
    case ACELP_64k     :
        return PRIMARY_64000;
    case HQ_96k        :
        return PRIMARY_96000;
    case HQ_128k       :
        return PRIMARY_128000;
    default            :
        return rate2AMRWB_IOmode(rate);
    }
}

/*-------------------------------------------------------------------*
 * push_indice_fx( )
 *
 * Push a new indice into the buffer
 *-------------------------------------------------------------------*/

void push_indice_fx(
    Encoder_State_fx *st_fx,       /* i/o: encoder state structure */
    Word16 id,           /* i  : ID of the indice */
    UWord16 value,        /* i  : value of the quantized indice */
    Word16 nb_bits       /* i  : number of bits used to quantize the indice */
)
{
    Word16 i;


    IF ( sub(st_fx->last_ind_fx, id) == 0 )
    {
        /* indice with the same name as the previous one */
        i = st_fx->next_ind_fx;
    }
    ELSE
    {
        /* new indice - find an empty slot in the list */
        i = id;
        move16();
        WHILE (sub(st_fx->ind_list_fx[i].nb_bits, -1) != 0)
        {
            i = add(i, 1);
        }
    }

    /* store the values in the list */
    st_fx->ind_list_fx[i].value = value;
    move16();
    st_fx->ind_list_fx[i].nb_bits = nb_bits;
    move16();

    /* updates */
    st_fx->next_ind_fx = add(i, 1);
    st_fx->last_ind_fx = id;
    move16();
    st_fx->nb_bits_tot_fx = add(st_fx->nb_bits_tot_fx, nb_bits);

    return;
}

/*-------------------------------------------------------------------*
 * push_next_indice_fx()            *
 * Push a new indice into the buffer at the next position
 *-------------------------------------------------------------------*/

void push_next_indice_fx(
    Encoder_State_fx *st_fx,      /* i/o: encoder state structure */
    UWord16 value,       /* i  : value of the quantized indice */
    Word16 nb_bits      /* i  : number of bits used to quantize the indice */
)
{

    /* store the values in the list */
    st_fx->ind_list_fx[st_fx->next_ind_fx].value   = value;
    move16();
    st_fx->ind_list_fx[st_fx->next_ind_fx].nb_bits = nb_bits;
    move16();
    st_fx->next_ind_fx = add(st_fx->next_ind_fx, 1);


    /* update the total number of bits already written */
    st_fx->nb_bits_tot_fx = add(st_fx->nb_bits_tot_fx, nb_bits);

    return;
}


/*-------------------------------------------------------------------*
 * push_next_bits_fx()
 * Push a bit buffer into the buffer at the next position
 *-------------------------------------------------------------------*/

void push_next_bits_fx(
    Encoder_State_fx *st_fx,    /* i/o: encoder state structure */
    Word16 bits[],      /* i  : bit buffer to pack, sequence of single bits */
    Word16 nb_bits      /* i  : number of bits to pack */
)
{
    UWord16 code;
    Word16 i, nb_bits_m15;
    Indice_fx *ptr;

    ptr = &st_fx->ind_list_fx[st_fx->next_ind_fx];
    nb_bits_m15 = sub(nb_bits, 15);
    i = 0;
    move16();
    IF (nb_bits_m15 > 0)
    {
        FOR (; i<nb_bits_m15; i += 16)
        {
            code = s_or(lshl(bits[i], 15), s_or(lshl(bits[i+1], 14), s_or(lshl(bits[i+2], 13), s_or(lshl(bits[i+3], 12),
                                                s_or(lshl(bits[i+4], 11), s_or(lshl(bits[i+5], 10), s_or(lshl(bits[i+6], 9), s_or(lshl(bits[i+7], 8),
                                                        s_or(lshl(bits[i+8], 7), s_or(lshl(bits[i+9], 6), s_or(lshl(bits[i+10], 5), s_or(lshl(bits[i+11], 4),
                                                                s_or(lshl(bits[i+12], 3), s_or(lshl(bits[i+13], 2), s_or(lshl(bits[i+14], 1), bits[i+15])))))))))))))));

            ptr->value   = code;
            move16();
            ptr->nb_bits = 16;
            move16();
            ++ptr;
        }
    }
    IF (sub(i, nb_bits) < 0)
    {
        FOR (; i<nb_bits; ++i)
        {
            ptr->value   = bits[i];
            move16();
            ptr->nb_bits = 1;
            move16();
            ++ptr;
        }
    }
    st_fx->next_ind_fx = (Word16)(ptr - st_fx->ind_list_fx);
    st_fx->nb_bits_tot_fx = add(st_fx->nb_bits_tot_fx, nb_bits);
}

/*-------------------------------------------------------------------*
 * get_next_indice_fx( )
 *
 * Get the next indice from the buffer
 *-------------------------------------------------------------------*/

UWord16 get_next_indice_fx(              /* o  : value of the indice */
    Decoder_State_fx *st_fx,               /* i/o: decoder state structure */
    Word16 nb_bits                       /* i  : number of bits that were used to quantize the indice */
)
{
    UWord16 value;
    Word16 i;

    assert(nb_bits <= 16);
    value = 0;
    move16();

    /* detect corrupted bitstream */
    IF( sub(add(st_fx->next_bit_pos_fx,nb_bits),st_fx->total_num_bits) > 0 )
    {
        st_fx->BER_detect = 1;
        move16();
        return(0);
    }

    FOR (i = 0; i < nb_bits; i++)
    {
        value = lshl(value, 1);
        value = add(value, st_fx->bit_stream_fx[st_fx->next_bit_pos_fx+i]);
    }

    /* update the position in the bitstream */
    st_fx->next_bit_pos_fx = add(st_fx->next_bit_pos_fx, nb_bits);
    return value;
}

/*-------------------------------------------------------------------*
 * get_next_indice_1_fx( )
 *
 * Get the next 1-bit indice from the buffer
 *-------------------------------------------------------------------*/

UWord16 get_next_indice_1_fx(           /* o  : value of the indice */
    Decoder_State_fx *st_fx               /* i/o: decoder state structure */
)
{
    /* detect corrupted bitstream */
    test();
    test();
    test();
    IF(( sub(add(st_fx->next_bit_pos_fx,1),st_fx->total_num_bits) > 0 && sub(st_fx->codec_mode,MODE1) == 0 ) ||
       ( sub(add(st_fx->next_bit_pos_fx,1),add(st_fx->total_num_bits,2*8)) > 0 && sub(st_fx->codec_mode,MODE2) == 0 ) /* add two zero bytes for arithmetic coder flush */
      )
    {
        st_fx->BER_detect = 1;
        move16();
        return(0);
    }

    return st_fx->bit_stream_fx[st_fx->next_bit_pos_fx++];
}

/*-------------------------------------------------------------------*
 * get_next_indice_tmp()
 *
 * update the total number of bits and the position in the bitstream
 *-------------------------------------------------------------------*/

void get_next_indice_tmp_fx(
    Decoder_State_fx *st_fx,         /* o  : decoder state structure */
    Word16 nb_bits                   /* i  : number of bits that were used to quantize the indice */
)
{
    /* update the position in the bitstream */
    st_fx->next_bit_pos_fx = add(st_fx->next_bit_pos_fx, nb_bits);
}

/*-------------------------------------------------------------------*
 * get_indice_fx( )
 *
 * Get indice at specific position in the buffer
 *-------------------------------------------------------------------*/

UWord16 get_indice_fx(               /* o  : value of the indice */
    Decoder_State_fx *st_fx,        /* i/o: decoder state structure */
    Word16 pos,             /* i  : absolute position in the bitstream (update after the read) */
    Word16 nb_bits          /* i  : number of bits that were used to quantize the indice */
)
{
    UWord16 value;
    Word16 i;

    assert(nb_bits <= 16);

    /* detect corrupted bitstream */
    IF( sub(add(pos,nb_bits),st_fx->total_num_bits) > 0 )
    {
        st_fx->BER_detect = 1;
        move16();
        return(0);
    }

    value = 0;
    move16();
    FOR (i = 0; i < nb_bits; i++)
    {
        value = lshl(value, 1);
        value = add(value, st_fx->bit_stream_fx[pos+i]);
    }

    return value;
}

/*-------------------------------------------------------------------*
 * get_indice_1_fx( )
 *
 * Get a 1-bit indice at specific position in the buffer
 *-------------------------------------------------------------------*/

UWord16 get_indice_1_fx(             /* o  : value of the indice */
    Decoder_State_fx *st_fx,         /* i/o: decoder state structure */
    Word16 pos              /* i  : absolute position in the bitstream (update after the read) */
)
{
    /* detect corrupted bitstream */
    IF( sub(add(pos,1),st_fx->total_num_bits) > 0 )
    {
        st_fx->BER_detect = 1;
        move16();
        return(0);
    }

    return st_fx->bit_stream_fx[pos];
}

/*-------------------------------------------------------------------*
 * reset_indices_enc_fx()
 *
 * Reset the buffer of indices
 *-------------------------------------------------------------------*/

void reset_indices_enc_fx(
    Encoder_State_fx *st_fx
)
{
    Word16 i;

    st_fx->nb_bits_tot_fx = 0;
    move16();
    st_fx->next_ind_fx = 0;
    move16();
    st_fx->last_ind_fx = -1;
    move16();

    FOR (i=0; i<MAX_NUM_INDICES; i++)
    {
        st_fx->ind_list_fx[i].nb_bits = -1;
        move16();
    }

    return;
}

/*-------------------------------------------------------------------*
  * reset_indices_dec_fx()
  *
  * Reset the buffer of decoder indices
  *-------------------------------------------------------------------*/

void reset_indices_dec_fx(
    Decoder_State_fx *st_fx
)
{
    st_fx->next_bit_pos_fx = 0;
    move16();

    return;
}


/*-------------------------------------------------------------------*
 * write_indices_fx()
 *
 * Write the buffer of indices to a file
 *-------------------------------------------------------------------*/

void write_indices_fx(
    Encoder_State_fx *st_fx,        /* i/o: encoder state structure */
    FILE *file          /* i  : output bitstream file   */
    , UWord8 *pFrame,     /* i: byte array with bit packet and byte aligned coded speech data */
    Word16 pFrame_size  /* i: size of the binary encoded access unit [bits] */
)
{
    Word16 i, k;
    Word16 stream[2+MAX_BITS_PER_FRAME], *pt_stream;
    Word32  mask;
    UWord8 header;

    if( st_fx->bitstreamformat == G192 )
    {
        /*-----------------------------------------------------------------*
        * Encode Sync Header and Frame Length
        *-----------------------------------------------------------------*/
        pt_stream = stream;
        for (i=0; i<(2 + MAX_BITS_PER_FRAME); ++i)
        {
            stream[i] = 0;
        }
        *pt_stream++ = (Word16)SYNC_GOOD_FRAME;
        *pt_stream++ = st_fx->nb_bits_tot_fx;

        /*----------------------------------------------------------------*
        * Bitstream packing (conversion of individual indices into a serial stream)
        * Writing the serial stream into file
        *----------------------------------------------------------------*/

        for (i=0; i<MAX_NUM_INDICES; i++)
        {
            if (st_fx->ind_list_fx[i].nb_bits != -1)
            {
                /* mask from MSB to LSB */
                mask = 1 << (st_fx->ind_list_fx[i].nb_bits - 1);

                /* write bit by bit */
                for (k=0; k < st_fx->ind_list_fx[i].nb_bits; k++)
                {
                    if ( st_fx->ind_list_fx[i].value & mask )
                    {
                        *pt_stream++ = G192_BIN1;
                    }
                    else
                    {
                        *pt_stream++ = G192_BIN0;
                    }

                    mask >>= 1;
                }
            }
        }

    }
    else
    {
        /* Create and write ToC header */
        /*  qbit always  set to  1 on encoder side  for AMRWBIO ,  no qbit in use for EVS, but set to 0(bad)  */
        header = (UWord8)(st_fx->Opt_AMR_WB_fx << 5 | st_fx->Opt_AMR_WB_fx << 4 | rate2EVSmode(st_fx->nb_bits_tot_fx * 50));
        fwrite( &header, sizeof(UWord8), 1, file );
        /* Write speech bits */
        fwrite( pFrame, sizeof(UWord8), (pFrame_size + 7)>>3, file );
    }

    /* Clearing of indices */
    FOR (i=0; i<MAX_NUM_INDICES; i++)
    {
        st_fx->ind_list_fx[i].nb_bits = -1;
        move16();
    }


    if( st_fx->bitstreamformat == G192 )
    {
        /* write the serial stream into file */
        fwrite( stream, sizeof(unsigned short), 2+stream[1], file );
    }
    /* reset index pointers */
    st_fx->nb_bits_tot_fx = 0;
    st_fx->next_ind_fx = 0;
    st_fx->last_ind_fx = -1;

    return;
}

/*-------------------------------------------------------------------*
 * indices_to_serial()
 *
 * pack indices into serialized payload format
 *-------------------------------------------------------------------*/

void indices_to_serial(
    const Encoder_State_fx *st_fx,             /* i: encoder state structure */
    UWord8 *pFrame,            /* o: byte array with bit packet and byte aligned coded speech data */
    Word16 *pFrame_size       /* o: size of the binary encoded access unit [bits] */
)
{
    Word16 i, k, j;
    Word16 cmi = 0, core_mode=0;
    Word32 mask;
    Word16 amrwb_bits[(ACELP_23k85 / 50)];
    UWord8 omask= 0x80;
    UWord8 *pt_pFrame=pFrame;

    if ( st_fx->Opt_AMR_WB_fx )
    {
        cmi = rate2EVSmode(st_fx->total_brate_fx);
        core_mode = rate2EVSmode(st_fx->nb_bits_tot_fx * 50);

        j=0;
        for (i=0; i<MAX_NUM_INDICES; i++)
        {
            if (st_fx->ind_list_fx[i].nb_bits != -1)
            {
                /* mask from MSB to LSB */
                mask = 1 << (st_fx->ind_list_fx[i].nb_bits - 1);

                /* temporarily save bit */
                for (k=0; k < st_fx->ind_list_fx[i].nb_bits; k++)
                {
                    amrwb_bits[j++] = (st_fx->ind_list_fx[i].value & mask) > 0;
                    mask >>= 1;
                }
            }
        }
    }

    *pFrame_size = st_fx->nb_bits_tot_fx;

    /*----------------------------------------------------------------*
    * Bitstream packing (conversion of individual indices into a serial stream)
    *----------------------------------------------------------------*/
    j=0;
    for (i=0; i<MAX_NUM_INDICES; i++)
    {
        if (st_fx->ind_list_fx[i].nb_bits != -1)
        {
            /* mask from MSB to LSB */
            mask = 1 << (st_fx->ind_list_fx[i].nb_bits - 1);

            /* write bit by bit */
            for (k=0; k < st_fx->ind_list_fx[i].nb_bits; k++)
            {
                if (st_fx->Opt_AMR_WB_fx )
                {
                    pack_bit(amrwb_bits[sort_ptr[core_mode][j++]], &pt_pFrame, &omask);
                }
                else
                {
                    pack_bit(st_fx->ind_list_fx[i].value & mask, &pt_pFrame, &omask);
                    j++;
                }
                mask >>= 1;
            }
        }
    }

    if ( st_fx->Opt_AMR_WB_fx && core_mode == AMRWB_IO_SID)    /* SID UPD frame always written now  .... */
    {
        /* insert STI bit and CMI */
        pack_bit(1, &pt_pFrame, &omask);
        for (mask=0x08; mask>0; mask >>= 1)
        {
            pack_bit(cmi & mask, &pt_pFrame, &omask);
        }
    }
}


/*-------------------------------------------------------------------*
 * indices_to_serial_generic()
 *
 * pack indices into serialized payload format
 *-------------------------------------------------------------------*/

void indices_to_serial_generic(
    const Indice_fx *ind_list,      /* i: indices list */
    const Word16  num_indices,      /* i: number of indices to write */
    UWord8 *pFrame,           /* o: byte array with bit packet and byte aligned coded speech data */
    Word16 *pFrame_size       /* i/o: number of bits in the binary encoded access unit [bits] */
)
{
    Word16 i, k, j;
    Word32 mask;
    UWord8 omask;
    UWord8 *pt_pFrame = pFrame;
    Word16 nb_bits_tot;

    nb_bits_tot = 0;
    move16();
    omask = (0x80 >> (*pFrame_size & 0x7));
    pt_pFrame += shr(*pFrame_size, 3);

    /*----------------------------------------------------------------*
     * Bitstream packing (conversion of individual indices into a serial stream)
     *----------------------------------------------------------------*/
    j=0;
    move16();
    for (i=0; i<num_indices; i++)
    {
        if (ind_list[i].nb_bits != -1)
        {
            /* mask from MSB to LSB */
            mask = 1 << (ind_list[i].nb_bits - 1);

            /* write bit by bit */
            for (k=0; k < ind_list[i].nb_bits; k++)
            {
                pack_bit(ind_list[i].value & mask, &pt_pFrame, &omask);
                j++;
                mask >>= 1;
            }
            nb_bits_tot = add(nb_bits_tot, ind_list[i].nb_bits);
        }
    }

    *pFrame_size = add(*pFrame_size, nb_bits_tot);

    return;
}


static void decoder_selectCodec(
    Decoder_State_fx *st,            /* i/o: decoder state structure                */
    const Word32 total_brate,    /* i  : total bitrate                          */
    const Word16 bit0
)
{
    test();
    test();
    test();
    test();
    test();
    test();
    test();
    test();
    test();
    /* check if we are in AMR-WB IO mode */
    IF( L_sub(total_brate, SID_1k75) == 0 ||
        L_sub(total_brate, ACELP_6k60) == 0  || L_sub(total_brate, ACELP_8k85) == 0  || L_sub(total_brate, ACELP_12k65) == 0 ||
        L_sub(total_brate, ACELP_14k25) == 0 || L_sub(total_brate, ACELP_15k85) == 0 || L_sub(total_brate, ACELP_18k25) == 0 ||
        L_sub(total_brate, ACELP_19k85) == 0 || L_sub(total_brate, ACELP_23k05) == 0 || L_sub(total_brate, ACELP_23k85) == 0 )
    {
        st->Opt_AMR_WB_fx = 1;
        move16();
    }
    ELSE IF ( L_sub(total_brate, FRAME_NO_DATA) != 0 )
    {
        st->Opt_AMR_WB_fx = 0;
        move16();
    }

    /* select MODE1 or MODE2 */
    IF (st->Opt_AMR_WB_fx)
    {
        st->codec_mode = MODE1;
        move16();/**/
    }
    ELSE
    {
        SWITCH ( total_brate )
        {
        case 0:
            st->codec_mode = st->last_codec_mode;
            move16();
            BREAK;
        case 2400:
            st->codec_mode = st->last_codec_mode;
            move16();
            BREAK;
        case 2800:
            st->codec_mode = MODE1;
            move16();
            BREAK;
        case 7200:
            st->codec_mode = MODE1;
            move16();
            BREAK;
        case 8000:
            st->codec_mode = MODE1;
            move16();
            BREAK;
        case 9600:
            st->codec_mode = MODE2;
            move16();
            BREAK;
        case 13200:
            st->codec_mode = MODE1;
            move16();
            BREAK;
        case 16400:
            st->codec_mode = MODE2;
            move16();
            BREAK;
        case 24400:
            st->codec_mode = MODE2;
            move16();
            BREAK;
        case 32000:
            st->codec_mode = MODE1;
            move16();
            BREAK;
        case 48000:
            st->codec_mode = MODE2;
            move16();
            BREAK;
        case 64000:
            st->codec_mode = MODE1;
            move16();
            BREAK;
        case 96000:
            st->codec_mode = MODE2;
            move16();
            BREAK;
        case 128000:
            st->codec_mode = MODE2;
            move16();
            BREAK;
        default    :
            /* validate that total_brate (derived from RTP packet or a file header) is one of the defined bit rates  */
            st->codec_mode = st->last_codec_mode;
            st->bfi_fx = 1;
            move16();
            move16();
            BREAK;
        }
    }

    IF ( st->ini_frame_fx == 0 )
    {
        IF(sub(st->codec_mode,-1) == 0 )
        {
            st->codec_mode = MODE1;
            move16();
        }
        st->last_codec_mode = st->codec_mode;
        move16();
    }

    /* set SID/CNG type */
    IF ( L_sub(total_brate,SID_2k40) == 0 )
    {
        IF ( bit0 == G192_BIN0 )
        {
            st->cng_type_fx = LP_CNG;
            move16();

            /* force MODE1 when selecting LP_CNG */
            st->codec_mode = MODE1;
            move16();
        }
        ELSE
        {
            st->cng_type_fx = FD_CNG;
            test();
            if ( sub(st->last_codec_mode, MODE2) == 0 && L_sub(st->last_total_brate_fx,13200) == 0 )
            {
                st->codec_mode = MODE1;
                move16();
            }
        }
        st->last_cng_type_fx = st->cng_type_fx;     /* CNG type switching at the first correctly received SID frame */
    }


    return;
}




void dec_prm_core(Decoder_State_fx *st)
{
    Word16 n, frame_size_index, num_bits;
    UWord16 lsb;
    Word32 L_tmp;

    frame_size_index = -1;
    move16();
    st->core_fx = -1;
    move16();

    IF (L_sub(st->total_brate_fx, FRAME_NO_DATA) == 0)
    {
        st->m_frame_type = ZERO_FRAME;
        move16();
    }
    ELSE IF (L_sub(st->total_brate_fx, SID_2k40) == 0)
    {
        st->m_frame_type = SID_FRAME;
        move16();
    }
    ELSE
    {
        st->m_frame_type = ACTIVE_FRAME;
        move16();
        Mpy_32_16_ss(st->total_brate_fx, 5243, &L_tmp, &lsb); /* 5243 is 1/50 in Q18. (0+18-15=3) */
        num_bits = extract_l(L_shr(L_tmp, 3)); /* Q0 */
        assert(num_bits == st->total_brate_fx/50);
        FOR (n=0; n<FRAME_SIZE_NB; ++n)
        {
            IF (sub(FrameSizeConfig[n].frame_bits, num_bits) == 0)
            {
                frame_size_index = n;
                move16();
                BREAK;
            }
        }

        /* Get bandwidth mode */
        st->bwidth_fx = get_next_indice_fx(st, FrameSizeConfig[frame_size_index].bandwidth_bits);

        st->bwidth_fx = add(st->bwidth_fx, FrameSizeConfig[frame_size_index].bandwidth_min);

        if (sub(st->bwidth_fx, FB) > 0)
        {
            st->bwidth_fx = FB;
            move16();
            st->BER_detect = 1;
            move16();
        }

        if (sub(st->bwidth_fx, SWB) > 0 && L_sub(st->total_brate_fx, ACELP_16k40) < 0)
        {
            st->bwidth_fx = SWB;
            move16();
            st->BER_detect = 1;
            move16();
        }

        /* Skip reserved bit */
        get_next_indice_tmp_fx(st, FrameSizeConfig[frame_size_index].reserved_bits);

        IF (get_next_indice_1_fx(st) != 0) /* TCX */
        {
            st->core_fx = TCX_20_CORE;
            move16();
            if (get_next_indice_1_fx(st) != 0)
            {
                st->core_fx = HQ_CORE;
                move16();
            }
        }
        ELSE /* ACELP */
        {
            st->core_fx = ACELP_CORE;
            move16();
        }
    }
}

/*-----------------------------------------------------------------*
 * decision_matrix_core_dec()
 *
 * Read core mode signalling bits from the bitstream
 * Set st->core, and st->bwidth if signalled together with the core.
 *-----------------------------------------------------------------*/

void decision_matrix_core_dec(
    Decoder_State_fx *st                 /* i/o: decoder state structure                   */
)
{
    Word16 start_idx;
    Word32 ind;
    Word16 nBits;

    assert(st->bfi_fx != 1);

    st->core_fx = -1;
    move16();
    st->bwidth_fx = -1;
    move16();

    test();
    IF ( L_sub(st->total_brate_fx, FRAME_NO_DATA) == 0 || L_sub(st->total_brate_fx, SID_2k40) == 0 )
    {
        st->core_fx = ACELP_CORE;
        move16();
    }
    /* SC-VBR */
    ELSE IF ( st->total_brate_fx == PPP_NELP_2k80 )
    {
        st->core_fx = ACELP_CORE;
        move16();
        return;
    }

    /*---------------------------------------------------------------------*
     * ACELP/HQ core selection
     *---------------------------------------------------------------------*/

    test();
    IF ( L_sub(st->total_brate_fx, ACELP_24k40) < 0 )
    {
        st->core_fx = ACELP_CORE;
        move16();
    }
    ELSE IF ( L_sub(st->total_brate_fx, ACELP_24k40) >= 0 && L_sub(st->total_brate_fx, ACELP_64k) <= 0 )
    {
        /* read the ACELP/HQ core selection bit */
        st->core_fx = imult1616(get_next_indice_fx( st, 1 ), HQ_CORE);
    }
    ELSE
    {
        st->core_fx = HQ_CORE;
        move16();
    }

    /*-----------------------------------------------------------------*
     * Read ACELP signalling bits from the bitstream
     *-----------------------------------------------------------------*/

    IF ( sub(st->core_fx, ACELP_CORE) == 0 )
    {
        /* find the section in the ACELP signalling table corresponding to bitrate */
        start_idx = 0;
        move16();
        WHILE ( L_sub(acelp_sig_tbl[start_idx], st->total_brate_fx) != 0 )
        {
            start_idx = add(start_idx, 1);
        }

        /* skip the bitrate */
        start_idx = add(start_idx, 1);

        /* retrieve the number of bits */
        nBits = extract_l(acelp_sig_tbl[start_idx]);
        start_idx = add(start_idx, 1);

        /* retrieve the signalling indice */
        ind = acelp_sig_tbl[add(start_idx, get_next_indice_fx( st, nBits ))];
        st->bwidth_fx = extract_l(L_and(L_shr(ind, 3), 0x7));

        /* convert signalling indice into signalling information */
        if ( L_sub(L_and(ind, 0x7), LR_MDCT) == 0 )
        {
            st->core_fx = HQ_CORE;
            move16();
        }
    }

    /*-----------------------------------------------------------------*
     * Read HQ signalling bits from the bitstream
     * Set HQ core type
     *-----------------------------------------------------------------*/

    IF ( sub(st->core_fx, HQ_CORE) == 0 )
    {
        /* read the HQ/TCX core switching flag */
        if ( get_next_indice_fx( st, 1 ) != 0 )
        {
            st->core_fx = TCX_20_CORE;
            move16();
        }

        /* For TCX: read/set band-width (needed for different I/O sampling rate support) */
        test();
        IF( sub(st->core_fx, TCX_20_CORE) == 0 && L_sub(st->total_brate_fx, ACELP_16k40) > 0 )
        {
            ind = get_next_indice_fx( st, 2 );

            IF( ind == 0 )
            {
                st->bwidth_fx = NB;
                move16();
            }
            ELSE IF( L_sub(ind, 1) == 0 )
            {
                st->bwidth_fx = WB;
                move16();
            }
            ELSE IF( L_sub(ind, 2) == 0 )
            {
                st->bwidth_fx = SWB;
                move16();
            }
            ELSE
            {
                st->bwidth_fx = FB;
                move16();
            }
        }
    }

    return;
}

/*-------------------------------------------------------------------*
 * mdct_switching_dec()
 *
 * Set up MDCT core switching if indicated in the bit stream
 *-------------------------------------------------------------------*/

static void mdct_switching_dec(
    Decoder_State_fx *st                 /* i/o: decoder state structure                */
)
{
    IF (st->Opt_AMR_WB_fx != 0)
    {
        return;
    }

    test();
    test();
    IF (L_sub(st->total_brate_fx, ACELP_13k20) == 0 || L_sub(st->total_brate_fx, ACELP_32k) == 0)
    {
        st->mdct_sw_enable = MODE1;
        move16();
    }
    ELSE IF (L_sub(ACELP_16k40, st->total_brate_fx) <= 0 && L_sub(st->total_brate_fx, ACELP_24k40) <= 0)
    {
        st->mdct_sw_enable = MODE2;
        move16();
    }

    test();
    test();
    IF ( sub(st->codec_mode, MODE1) == 0 && sub(st->mdct_sw_enable, MODE1) == 0 )
    {
        /* Read ahead core mode signaling */
        Word16 next_bit_pos_save;
        Word16 core_save;
        Word16 bwidth_save;

        next_bit_pos_save = st->next_bit_pos_fx;
        move16();
        core_save = st->core_fx;
        move16();
        bwidth_save = st->bwidth_fx;
        move16();

        decision_matrix_core_dec(st); /* sets st->core */

        IF (sub(st->core_fx, TCX_20_CORE) == 0)
        {
            /* Trigger TCX */
            st->codec_mode = MODE2;
            move16();
            st->mdct_sw = MODE1;
            move16();
        }
        ELSE
        {
            /* Rewind bitstream */
            st->next_bit_pos_fx = next_bit_pos_save;
            move16();
            IF (st->bfi_fx != 0)
            {
                st->core_fx   = core_save;
                move16();
                st->bwidth_fx = bwidth_save;
                move16();
            }
        }
    }
    ELSE IF (sub(st->codec_mode, MODE2) == 0 && sub(st->mdct_sw_enable, MODE2) == 0)
    {
        /* Read ahead core mode signaling */
        Word16 next_bit_pos_save;
        Word16 core_save;
        Word16 bwidth_save;

        next_bit_pos_save = st->next_bit_pos_fx;
        move16();
        core_save = st->core_fx;
        move16();
        bwidth_save = st->bwidth_fx;
        move16();

        dec_prm_core(st); /* sets st->core */

        IF (sub(st->core_fx, HQ_CORE) == 0)
        {
            /* Trigger HQ_CORE */
            st->codec_mode = MODE1;
            move16();
            st->mdct_sw = MODE2;
            move16();
        }
        ELSE
        {
            /* Rewind bitstream */
            st->next_bit_pos_fx = next_bit_pos_save;
            move16();
            IF (st->bfi_fx != 0)
            {
                st->core_fx   = core_save;
                move16();
            }
            /* always reset bwidth, to not interfere with BER logic */
            st->bwidth_fx = bwidth_save;
            move16();
        }
    }
}

/*-------------------------------------------------------------------*
 * BRATE2IDX_fx()
 *
 * Convert Bitrate to Index Value
 *-------------------------------------------------------------------*/

Word16 BRATE2IDX_fx(Word32 brate)
{
    Word32 L_temp;
    Word32 L_idx;
#define START 9
    extern const Word16 bit_rates_div50[];

    /* This is a Fast Bit Rate Value to Index Value Binary Search */
    L_temp = L_msu0(brate, bit_rates_div50[START], 50);
    L_temp = L_min(6, L_max(-6, L_temp));
    L_idx = L_add(L_temp, START);
    L_temp = L_msu0(brate, bit_rates_div50[L_idx], 50);
    L_temp = L_min(3, L_max(-3, L_temp));
    L_idx = L_add(L_temp, L_idx);
    L_temp = L_msu0(brate, bit_rates_div50[L_idx], 50);
    L_temp = L_min(1, L_max(-2, L_temp));
    L_idx = L_add(L_temp, L_idx);
    L_temp = L_msu0(brate, bit_rates_div50[L_idx], 50);
    if (L_temp != 0) L_idx = L_add(L_idx, 1);
    return (Word16)L_idx;
}


/*-------------------------------------------------------------------*
 * BRATE2IDX16k_fx()
 *
 * Convert Bitrate to Index Value
 *-------------------------------------------------------------------*/

Word16 BRATE2IDX16k_fx(Word32 brate)
{
    Word32 L_temp, L_idx;
#define START_16K 5
    extern const Word16 bit_rates_16k_div50[];

    if(L_sub(brate,ACELP_16k40)==0)
    {
        brate=ACELP_14k80;
    }

    /* This is a Fast Bit Rate Value to Index Value Binary Search */
    L_temp = L_msu0(brate, bit_rates_16k_div50[START_16K], 50);
    L_temp = L_min(3, L_max(-3, L_temp));
    L_idx = L_add(L_temp, START_16K);
    L_temp = L_msu0(brate, bit_rates_16k_div50[L_idx], 50);
    L_temp = L_min(2, L_max(-2, L_temp));
    L_idx = L_add(L_temp, L_idx);
    L_temp = L_msu0(brate, bit_rates_16k_div50[L_idx], 50);
    L_temp = L_min(1, L_max(-1, L_temp));
    L_idx = L_add(L_temp, L_idx);

    return (Word16)L_idx;
}

/*-------------------------------------------------------------------*
 * BIT_ALLOC_IDX_fx()
 *-------------------------------------------------------------------*/

Word32 BIT_ALLOC_IDX_fx(Word32 brate, Word16 ctype, Word16  sfrm, Word16 tc)
{
    Word32 L_temp;
    Word16 temp;
    if (ctype == INACTIVE) /* no sub(ctype, INACTIVE) because it is '0' */
        ctype = GENERIC;
    move16();
    L_temp = L_mac0(-1l*256, 1*256, ctype);

    temp = BRATE2IDX_fx(brate);
    L_temp = L_mac0(L_temp, 4*256, temp);
    if (tc >= 0)
        L_temp = L_mac0(L_temp, (10-4)*256, temp);
    /* So either 'temp' x 4 when 'tc < 0', 'temp' x 10 otherwise */

    L_temp = L_mac0(L_temp, 1*256, s_max(0, tc));

    L_temp = L_mac0(L_temp, s_max(0, sfrm), 1);
    if (sfrm < 0)
        L_temp = L_shr(L_temp, 2);
    L_temp = L_shr(L_temp, 6);

    return L_temp;
}

/*-------------------------------------------------------------------*
 * BIT_ALLOC_IDX_16KHZ_fx()
 *-------------------------------------------------------------------*/

Word32 BIT_ALLOC_IDX_16KHZ_fx(Word32 brate, Word16 ctype, Word16 sfrm, Word16 tc)
{
    Word32 L_temp;
    Word16 temp;
    /* 'ctype' =
       TRANSITION => 2
       GENERIC    => 1
       ALL Other  => 0
       */
    L_temp = L_and(shr(0x0240l, shl(ctype, 1)), 3);

    temp = BRATE2IDX16k_fx(brate);
    L_temp = L_mac0(L_temp, 3, temp);
    if (tc >= 0)
        L_temp = L_mac0(L_temp, (7-3), temp);
    /* So either 'temp' x 3 when 'tc < 0', 'temp' x 7 otherwise */

    L_temp = L_mac0(L_temp, 1, s_max(0, tc));

    IF (sfrm >= 0)
    {
        /* Mult by 5 */
        L_temp = L_add(L_temp, L_shl(L_temp, 2));
        L_temp = L_mac0(L_temp, shr(sfrm, 6), 1);
    }

    return L_temp;
}


/*-------------------------------------------------------------------*
 * read_indices_fx()
 *
 * Read indices from an ITU-T G.192 bitstream to the buffer
 * Simulate packet losses by inserting frame erasures
 *-------------------------------------------------------------------*/

Word16 read_indices_fx(                /* o  : 1 = reading OK, 0 = problem            */
    Decoder_State_fx *st,                /* i/o: decoder state structure                */
    FILE *file,              /* i  : bitstream file                         */
    Word16 rew_flag            /* i  : rewind flag (rewind file after reading)*/
)
{
    Word16 k;
    UWord16 utmp, stream[2+MAX_BITS_PER_FRAME], *pt_stream, *bit_stream_ptr;
    Word16 num_bits;
    Word32 total_brate;
    Word32 L_tmp;
    Word16 curr_ft_good_sp, curr_ft_bad_sp;
    Word16 g192_sid_first,sid_upd_bad, sid_update;
    Word16 speech_bad, speech_lost;
    Word16 num_bits_read;

    st->bfi_fx = 0;
    st->BER_detect = 0;
    st->mdct_sw_enable = 0;
    st->mdct_sw = 0;
    reset_indices_dec_fx( st );

    /* read the Sync Header field from the bitstream */
    /* in case rew_flag is set, read until first good frame is encountered */
    do
    {
        /* read the Sync header */
        if ( fread( &utmp, sizeof(unsigned short), 1, file ) != 1 )
        {
            if( ferror( file ) )
            {
                /* error during reading */
                fprintf(stderr, "\nError reading the bitstream !");
                exit(-1);
            }
            else
            {
                /* end of file reached */
                return 0;
            }
        }

        /* set the BFI indicator according the value of Sync Header */
        if ( sub(utmp, SYNC_BAD_FRAME) == 0 )
        {
            st->bfi_fx = 1;
        }


        else
        {
            st->bfi_fx = 0;
        }

        /* read the Frame Length field from the bitstream */
        if ( fread( &num_bits, sizeof(unsigned short), 1, file ) != 1 )
        {
            if( ferror( file ) )
            {
                /* error during reading */
                fprintf(stderr, "\nError reading the bitstream !");
                exit(-1);
            }
            else
            {
                /* end of file reached */
                return 0;
            }
        }
        /* convert the frame length to total bitrate */
        total_brate = (long)(num_bits* 50);

        /* read ITU-T G.192 serial stream of indices from file to the local buffer */
        /* Validate that the G.192 length is within the defined  bit rate range
        to not allow writing past the end of the "stream" buffer  */
        if( num_bits > MAX_BITS_PER_FRAME )
        {
            fprintf(stderr, "\nError, too large G.192 frame (size(%d))! Exiting ! \n", num_bits);
            exit(-1);
        }

        /*  verify that a  valid  num bits value  is present in the G.192 file */
        /*  only AMRWB or EVS bit rates  or 0(NO DATA)  are  allowed  in G.192 file frame reading  */
        if( rate2EVSmode(total_brate) < 0 ) /* negative value means that a valid rate was not found */
        {
            fprintf(stderr, "\nError, illegal bit rate (%d) in  the  G.192 frame ! Exiting ! \n", total_brate);
            exit(-1);
        }
        pt_stream = stream;

        num_bits_read = (Word16) fread( pt_stream, sizeof(unsigned short), num_bits, file );

        if( num_bits_read != num_bits )
        {
            fprintf(stderr, "\nError, invalid number of bits read ! Exiting ! \n");
            exit(-1);
        }

    }
    while ( rew_flag && (st->bfi_fx || L_sub(total_brate,2800) < 0) );

    /* G.192 RX DTX handler*/
    if( !rew_flag )
    {
        /* handle SID_FIRST, SID_BAD, SPEECH_LOST,  NO_DATA as properly  as possible for the ITU-T  G.192 format  */

        /* (total_brate, bfi , st_CNG)   =  rx_handler(received frame type, [previous frame type],  past CNG state, past core) */
        curr_ft_good_sp = 0;
        curr_ft_bad_sp  = 0;

        if( total_brate > SID_2k40 )
        {
            if( st->bfi_fx == 0 )
            {
                curr_ft_good_sp = 1;
            }
            else
            {
                curr_ft_bad_sp = 1;
            }
        }
        sid_update = 0;
        sid_upd_bad = 0;

        if( total_brate == SID_1k75 || total_brate == SID_2k40 )
        {
            if( st->bfi_fx == 0 )
            {
                sid_update = 1;
            }
            else
            {
                sid_upd_bad = 1;   /* may happen in CS , corrupt but detected sid frame  */
            }
        }

        /* AMRWB  26.173 G.192  file reader (read_serial) does not declare/use SID_BAD ft,
           it declares every bad synch marked frame initially as a lost_speech frame,
           and then the RXDTX handler CNG state decides the decoding mode CNG/SPEECH.
           While In the AMRWB ETSI/3GPP format eid a CRC error in a detected SID_UPDATE frame triggers SID_UPD_BAD.

           Here we inhibit use of the SID-length info, even though it is available in the G.192 file format after STL/EID-XOR .
         */
        if ( sid_upd_bad )
        {
            sid_upd_bad     = 0;
            total_brate     = FRAME_NO_DATA ;         /* treat SID_BAD  as a  stolen signaling frame --> SPEECH LOST */

        }

        g192_sid_first = 0;
        if( st->core_fx == AMR_WB_CORE && st->prev_ft_speech_fx && total_brate == FRAME_NO_DATA && st->bfi_fx == 0 )
        {
            g192_sid_first = 1;   /*  SID_FIRST detected for previous AMRWB/AMRWBIO  active frames only  */
            /*
            It is not possible to perfectly simulate rate switching conditions EVS->AMRWBIO  where:
            the very first SID_FIRST detection is based on a past EVS active frame
            and  a  good length 0  "SID_FIRST"(NO_DATA)   frame is sent in AMRWBIO,
            , due to the one frame state memory in the AMRWB legacy  G.192 SID_FIRST encoding
            */
        }

        speech_bad = 0;
        if( total_brate > SID_2k40 && st->bfi_fx != 0 ) /*   CS-type of CRC failure   frame */
        {
            speech_bad = 1;        /* initial assumption, CNG_state decides what to do */
        }

        speech_lost = 0;
        if( total_brate == 0 && st->bfi_fx != 0 ) /*  unsent  NO_DATA or stolen NO_DATA/signaling  frame  */
        {
            speech_lost = 1;       /* initial assumption, CNG_state decides what to do */
        }

        /* Do not allow decoder to enter CNG-synthesis for  any instantly  received  GOOD+LENGTH==0  frame
           as this frame was never transmitted, one  can not know it is good and has a a length of zero ) */

        if( st->CNG_fx != 0 )
        {
            /* We were in CNG synthesis  */
            if( curr_ft_good_sp != 0  )
            {
                /* only a good speech frame makes you leave CNG synthesis */
                st->CNG_fx = 0;
            }
        }
        else
        {
            /* We were in SPEECH synthesis  */
            /* only a received SID frame can make the decoder enter into CNG synthsis  */
            if( g192_sid_first || sid_update || sid_upd_bad )
            {
                st->CNG_fx = 1;
            }
        }

        /*  handle the  g.192  _simulated_ untransmitted frame,  setting  for decoder  SPEECH synthesis  */
        if ( (st->CNG_fx==0)  &&  (total_brate==0  && st->bfi_fx == 0 ) )
        {
            st->bfi_fx = 1;
            move16(); /*  SPEECH PLC code will now become active as in a real system */
            /* total_brate= 0  */
        }

        /* handle bad speech frame(and bad sid frame) in the decoders CNG synthesis settings pair (total_brate, bfi) */
        if( ((st->CNG_fx != 0) && ( (speech_bad != 0) || (speech_lost != 0) ))  || /* SP_BAD or SPEECH_LOST)   --> stay in CNG */
                ( sid_upd_bad != 0 ))                                                  /* SID_UPD_BAD              --> start CNG */
        {
            st->bfi_fx = 0;
            total_brate = 0;
        }
        /* update for next frame's G.192 file format's  SID_FIRST detection (primarily for AMRWBIO)  */
        st->prev_ft_speech_fx = ((curr_ft_good_sp != 0) || (curr_ft_bad_sp != 0));

        /*   st->total brate= total_brate ;   updated in a good frame below */
    } /* rew_flag */

    /* get total bit-rate */
    if ( st->bfi_fx == 0 && !rew_flag )
    {
        /* select MODE1 or MODE2 */
        decoder_selectCodec( st, total_brate, *pt_stream );
    }

    Mpy_32_16_ss(total_brate, 5243, &L_tmp, &utmp); /* 5243 is 1/50 in Q18. (0+18-15=3) */
    st->total_num_bits = extract_l(L_shr(L_tmp, 3)); /* Q0 */

    /* in case rew_flag is set, rewind the file and return */
    /* (used in io_enc() to print out info about technologies and to initialize the codec) */
    if ( rew_flag )
    {
        rewind( file );
        st->total_brate_fx = total_brate;
        move16();
        return 1;
    }

    /* GOOD frame */
    if ( st->bfi_fx == 0  )
    {
        /* GOOD frame - convert ITU-T G.192 words to short values */
        bit_stream_ptr = st->bit_stream_fx;

        for( k = 0; k< num_bits; ++k)
        {
            *bit_stream_ptr++ = (*pt_stream++ == G192_BIN1 );
        }

        /*add two zero bytes for arithmetic coder flush*/
        for(k=0; k< 2*8; ++k)
        {
            *bit_stream_ptr++ = 0;
        }
        /*a change of the total bitrate should not be
        known to the decoder, if the received frame was lost*/
        st->total_brate_fx = total_brate ;

        mdct_switching_dec(st);
    }

    return 1;
}


/*------------------------------------------------------------------------------------------*
* read_indices_mime()
*
* Read indices from MIME formatted bitstream to the buffer
*   The magic word and number of channnels should be consumed before calling this function
*-------------------------------------------------------------------------------------------*/

Word16 read_indices_mime(                /* o  : 1 = reading OK, 0 = problem            */
    Decoder_State_fx *st,                /* i/o: decoder state structure                */
    FILE *file,                          /* i  : bitstream file                         */
    Word16 rew_flag                      /* i  : rewind flag (rewind file after reading) */
)
{
    Word16 k, isAMRWB_IOmode, cmi, core_mode = -1, qbit,sti;
    UWord8 header;
    UWord8 pFrame[(MAX_BITS_PER_FRAME + 7) >> 3];
    UWord8 mask= 0x80, *pt_pFrame=pFrame;
    UWord16 *bit_stream_ptr;
    Word16 num_bits;
    Word32 total_brate;
    UWord16 utmp;
    Word32 L_tmp;
    Word16 curr_ft_good_sp;
    Word16 amrwb_sid_first, sid_upd_bad, sid_update;
    Word16 speech_bad, speech_lost;
    Word16 no_data;
    Word16 num_bytes_read;

    st->BER_detect = 0;
    st->bfi_fx = 0;
    st->mdct_sw_enable = 0;
    st->mdct_sw = 0;
    reset_indices_dec_fx( st );

    /* read the FT Header field from the bitstream */
    if ( fread( &header, sizeof(UWord8), 1, file ) != 1 )
    {
        if( ferror( file ) )
        {
            /* error during reading */
            fprintf(stderr, "\nError reading the bitstream !");
            exit(-1);
        }
        else
        {
            /* end of file reached */
            return 0;
        }
    }

    /* init local RXDTX flags */
    curr_ft_good_sp = 0;
    speech_lost = 0;
    speech_bad = 0;

    sid_update = 0;
    sid_upd_bad = 0;
    sti = -1;
    amrwb_sid_first = 0;  /* derived from sti  SID_FIRST indicator in AMRWB payload */
    no_data = 0;

    if( st->amrwb_rfc4867_flag != 0 )
    {
        /*   RFC 4867
        5.3 ....
        Each stored speech frame starts with a one-octet frame header with
        the following format:
        0 1 2 3 4 5 6 7
        +-+-+-+-+-+-+-+-+
        |P| FT    |Q|P|P|
        +-+-+-+-+-+-+-+-+
        The FT field and the Q bit are defined in the same way as in
        Section 4.3.2. The P bits are padding and MUST be set to 0, and MUST be ignored. */

        isAMRWB_IOmode   = 1;
        qbit             = (header>>2)&0x01 ;         /* b2 bit       (b7 is the F bit ) */
        st->bfi_fx = !qbit;
        core_mode  = ((header>>3) & 0x0F);     /*  b6..b3      */
        total_brate = AMRWB_IOmode2rate[core_mode];   /* get the frame length from the header */
    }
    else
    {
        /*0 1 2 3 4 5 6 7   MS-bit ---> LS-bit
         +-+-+-+-+-+-+-+-+
         |H|F|E|x| brate |
         +-+-+-+-+-+-+-+-+
          where :
            "E|x|  brate "  is the 6 bit "FT" -field
             x is unused    if E=0, (should be 0 )
             x is the q-bit if E=1, q==1(good), Q==0(bad, maybe bit errors in payload )
             H,F  always   0 in RTP format.
        */
        isAMRWB_IOmode = (header & 0x20) > 0;   /* get EVS mode-from header */ /*    b2   */
        core_mode      = (header & 0x0F);        /* b4,b5,b6,b7 */

        if( isAMRWB_IOmode )
        {
            qbit = (header & 0x10) > 0;      /* get Q bit,    valid for IO rates */ /* b3 */
            total_brate = AMRWB_IOmode2rate[core_mode];
        }
        else
        {
            qbit = 1;  /* assume good q_bit for the unused EVS-mode bit,    complete ToC validity checked later */
            total_brate = PRIMARYmode2rate[ core_mode ];
        }
        st->bfi_fx = !qbit;
    }




    /* set up RX-DTX-handler input */
    if(   core_mode == 14  )
    {
        /* SP_LOST */
        speech_lost=1;
    }
    if ( core_mode  == 15)
    {
        /*  NO_DATA unsent CNG frame OR  any frame marked or injected  as no_data  by e.g a signaling layer or dejitter buffer */
        no_data=1;
    }

    Mpy_32_16_ss(total_brate, 5243, &L_tmp, &utmp); /* 5243 is 1/50 in Q18. (0+18-15=3) */
    num_bits = extract_l(L_shr(L_tmp, 3)); /* Q0 */
    st->total_num_bits = num_bits;

    if( total_brate < 0 )
    {
        /* validate that total_brate (derived from RTP packet or a file header) is one of the defined bit rates  */
        fprintf(stderr, "\n  Error.  Illegal total bit rate (= %d) in MIME ToC header \n",     total_brate );
        /* num_bits = -1;   not needed as BASOP multiplication preserves sign */
    }

    /* Check correctness of ToC headers  */
    if( st->amrwb_rfc4867_flag == 0 )
    {
        /* EVS ToC header (FT field(b2-b7), H bit (b0),    F bit (b1)  ,  (EVS-modebit(b2)=0  unused(Qbit)(b3)==0)   */
        if ( (isAMRWB_IOmode == 0) &&  ((num_bits < 0)  ||  ((header & 0x80) > 0) || ((header & 0x40) > 0)  || (header & 0x30) != 0x00 )  )
        {
            /* incorrect FT header */
            fprintf(stderr, "\nError in EVS  FT ToC header(%02x) ! ",header);
            exit(-1);
        }
        else if( (isAMRWB_IOmode != 0) && ( (num_bits < 0) ||  ((header & 0x80) > 0) || ((header & 0x40) > 0) )  )  /* AMRWBIO */
        {
            /* incorrect IO FT header */
            fprintf(stderr, "\nError in EVS(AMRWBIO)  FT ToC header(%02x) ! ",header);
            exit(-1);
        }
    }
    else
    {
        /* legacy AMRWB ToC,   is only using  Padding bits which MUST be ignored */
        if ( num_bits < 0  )
        {
            /* incorrect FT header */
            fprintf(stderr, "\nError in AMRWB RFC4867  Toc(FT)  header(%02x) !", header);
            exit(-1);
        }
    }

    /* read serial stream of indices from file to the local buffer */
    num_bytes_read = (Word16) fread( pFrame, sizeof(UWord8), (num_bits + 7)>>3, file );
    if( num_bytes_read != (num_bits + 7)>>3 )
    {
        fprintf(stderr, "\nError, invalid number of bytes read ! Exiting ! \n");
        exit(-1);
    }

    /* in case rew_flag is set, rewind the file and return */
    /* (used in io_dec() to attempt print out info about technologies and to initialize the codec ) */
    if ( rew_flag )
    {
        st->total_brate_fx = total_brate;  /* used for the codec banner output */
        if( st->bfi_fx == 0 && speech_lost == 0 && no_data == 0 )
        {
            decoder_selectCodec( st, total_brate, unpack_bit(&pt_pFrame,&mask) ? G192_BIN1 : G192_BIN0);
        }
        return 1;
    }



    /* unpack speech data */
    bit_stream_ptr = st->bit_stream_fx;
    for (k=0; k<num_bits; k++)
    {
        if (isAMRWB_IOmode)
        {
            st->bit_stream_fx[sort_ptr[core_mode][k]] = unpack_bit(&pt_pFrame,&mask);
            bit_stream_ptr++;
        }
        else
        {
            *bit_stream_ptr++ = unpack_bit(&pt_pFrame,&mask);
        }
    }

    /* unpack auxiliary bits */
    /* Note: the cmi bits are unpacked for  demo  purposes;  */
    if (isAMRWB_IOmode && total_brate == SID_1k75)
    {
        sti = unpack_bit(&pt_pFrame,&mask);
        cmi  = unpack_bit(&pt_pFrame,&mask) << 3;
        cmi |= unpack_bit(&pt_pFrame,&mask) << 2;
        cmi |= unpack_bit(&pt_pFrame,&mask) << 1;
        cmi |= unpack_bit(&pt_pFrame,&mask);

        if( sti == 0 )
        {
            total_brate = 0;     /* signal received SID_FIRST as a good frame with no bits */
            for(k=0; k<35; k++)
            {
                st->bfi_fx  |= st->bit_stream_fx[k] ; /* partity check of 35 zeroes,  any single 1 gives BFI */
            }
        }
    }

    /*add two zero bytes for arithmetic coder flush*/
    for(k=0; k< 2*8; ++k)
    {
        *bit_stream_ptr++ = 0;
    }

    /* MIME RX_DTX handler */
    if( !rew_flag )
    {
        /* keep st->CNG_fx , st_bfi_fx and total_brate  updated  for proper synthesis in DTX and FER  */
        if( total_brate > SID_2k40 )
        {
            if( st->bfi_fx == 0 )   /* so  far derived from q bit in AMRWB/AMRWBIO cases   */
            {
                curr_ft_good_sp = 1;
            }
        }

        /* handle q_bit and  lost_sp  clash ,  assume worst case  */
        if( speech_lost != 0 )  /*  overrides  a good q_bit */
        {
            curr_ft_good_sp = 0;
            st->bfi_fx      = 1;     /* override  qbit */
        }

        /* now_bfi_fx has been set based on q_bit and ToC fields */


        /* SID_UPDATE check */
        if( total_brate == SID_1k75 || total_brate == SID_2k40 )
        {
            if( st->bfi_fx == 0 )
            {
                /* typically from q bit  */
                sid_update = 1;
            }
            else
            {
                sid_upd_bad = 1;  /* may happen in saving from e.g. a CS-connection */
            }
        }

        if( isAMRWB_IOmode && total_brate == 0 && sti == 0 )
        {
            if( st->bfi_fx )
            {
                sid_upd_bad = 1;          /*  corrupt sid_first, signaled as bad sid  */
            }
            else
            {
                amrwb_sid_first =  1;     /* 1-sti  */
            }
        }

        if ( sid_upd_bad != 0 && (
                    (isAMRWB_IOmode != 0 && st->Opt_AMR_WB_fx==0 )  || /* switch to    AMRWBIO */
                    (isAMRWB_IOmode != 1 && st->Opt_AMR_WB_fx==1)      /* switch from  AMRWBIO */
                ) )
        {
            /* do not allow a normal start of  CNG synthesis if this SID(with BER or FER) is a switch to/from AMRWBIO  */
            sid_upd_bad = 0; /* revert this detection due to AMRWBIO/EVS mode switch */
            total_brate = 0;
            no_data     = 1;
            assert( st->bfi_fx==1); /* bfi_fx stays 1 */
        }


        if( total_brate > SID_2k40 && st->bfi_fx )  /* typically from q bit  */
        {
            speech_bad = 1;    /* initial assumption,   CNG synt state decides what to actually do */
        }
        /* all frame types decoded */

        /*    update CNG synthesis state */
        /*    Decoder can only  enter CNG-synthesis  for  CNG frame types (sid_upd,  sid_bad, sid_first) */
        if( st->CNG_fx != 0 )
        {
            /* We were in CNG synthesis  */
            if( curr_ft_good_sp != 0  )
            {
                /* only a good speech frame makes decoder leave CNG synthesis */
                st->CNG_fx = 0;
            }
        }
        else
        {
            /*   We were in SPEECH synthesis  */
            /*   only a received SID frame can make the decoder enter into CNG synthesis  */
            if( amrwb_sid_first || sid_update || sid_upd_bad )
            {
                st->CNG_fx = 1;
            }
        }

        /* Now modify bfi flag for the  decoder's  SPEECH/CNG synthesis logic  */
        /*   in SPEECH synthesis, make sure to activate speech plc for a received no_data frame,
             no_data frames may be injected by the network or by the dejitter buffer   */
        /*   modify bfi_flag to stay/move into the correct decoder PLC section  */
        if ( (st->CNG_fx == 0)  &&  ( no_data != 0 )  )
        {
            /*  treat no_data received in speech synthesis as  SP_LOST frames, SPEECH PLC code will now become active */
            st->bfi_fx = 1;
            /* total_brate= 0  . always zero for no_data */
        }

        /* in CNG  */
        /* handle bad speech frame(and bad sid frame) in the decoders CNG synthesis settings pair (total_brate, bfi)  */
        if( ( st->CNG_fx != 0 && ( speech_bad || speech_lost || no_data ))  || /* SP_BAD or SPEECH_LOST)   --> stay in CNG */
                sid_upd_bad )                                                      /* SID_UPD_BAD              --> start/stay  CNG   */
        {

            st->bfi_fx = 0;    /* mark as good to not start speech PLC */
            total_brate= 0;    /* zeroing is needed  for  speech_bad,  sid_bad frames CNG- synthesis  in the decoder subfunctions   */


        }
    }

    /*  now  bfi, total_brate are set by RX-DTX handler::
         bfi==0, total_brate!=0    cng or speech pending  bitrate
         bfi==0, total_brate==0    cng will continue or start(sid_first, evs_sid_bad, amrwb_sid_bad)
         bfi==1, total_brate!=0    speech plc
         bfi==1, total_brate==0 ,  speech plc
    */

    /*  handle available AMRWB/AMRWBIO ToC rate info at startup   */
    if(  ( st->bfi_fx != 0 && rew_flag == 0 && st->ini_frame_fx == 0) && /*  ini_frame can not be used when rewflag is 1  */
            ( (st->amrwb_rfc4867_flag != 0)  || (st->amrwb_rfc4867_flag == 0 && isAMRWB_IOmode != 0  )) )  /*AMRWB ToC */
    {
        Word32 init_rate;

        init_rate = total_brate;          /* default , may have been be modified */
        if (speech_lost != 0 || no_data != 0 )
        {
            init_rate =  ACELP_12k65;
        }
        else if( speech_bad != 0  )
        {
            init_rate   =   AMRWB_IOmode2rate[core_mode];   /* read info from ToC */
        }
        st->total_brate_fx  = init_rate;  /* not updated on bfi as  decoderSelectCodec is not called below */
        st->core_brate_fx   = init_rate;
    }

    if( st->bfi_fx == 0 )
    {
        /* select MODE1 or MODE2 in  MIME */
        decoder_selectCodec( st, total_brate, *st->bit_stream_fx ? G192_BIN1 : G192_BIN0);

        /* a change of the total bitrate should not be known to the decoder, if the received frame was truly lost */
        st->total_brate_fx = total_brate;
        mdct_switching_dec(st);
    }
    /* else{ bfi stay in past synthesis mode(SP,CNG) } */

    return 1;
}

/*-------------------------------------------------------------------*
* berCheck()
*
* Check for bit errors in channel aware signalling.
*-------------------------------------------------------------------*/

static void berCheck(
    Decoder_State_fx *st,     /* i/o: decoder state structure     */
    Word16 *coder_type        /* i/o: coder type                  */
)
{
    /* In case of RF flag = 1, and valid RF packet with primary and partial copy */
    if ( ( sub( st->bwidth_fx, NB) == 0  || sub( st->bwidth_fx, FB) == 0 )
            || (sub(*coder_type,TRANSITION) >= 0 )
       )
    {
        if( sub( st->use_partial_copy, 1 ) == 0 )
        {
            st->use_partial_copy = 0;
            move16();
        }

        st->bfi_fx = 1;
        move16();
        st->bwidth_fx = st->last_bwidth_fx;
        move16();
        st->BER_detect = 1;
        move16();
        *coder_type = GENERIC;
        move16();
    }

    return;
}

/*-------------------------------------------------------------------*
* getPartialCopyInfo()
*
* Check if the frame includes a partial copy for channel aware processing.
*-------------------------------------------------------------------*/

void getPartialCopyInfo(
    Decoder_State_fx *st,              /* i/o: decoder state structure       */
    Word16 *coder_type,
    Word16 *sharpFlag
)
{
    Word16 nBits;
    Word16 ind;
    /* check the rf flag in the packet */
    get_rfFlag( st, &(st->rf_flag), &nBits , &ind);

    /* get rf frame type info */
    get_rfFrameType( st, &(st->rf_frame_type) );

    /* Get the FEC offset info */
    get_rf_fec_offset( st, &(st->rf_fec_offset) );

    /* reset number of target bits in case of rate switching */
    st->rf_target_bits = 0;

    /* Get the number of bits used for RF*/
    IF( sub(st->rf_flag,1) == 0 )
    {
        *coder_type = s_and(ind,0x7);
        st->bwidth_fx = s_and(shr(ind,3), 0x7);
        *sharpFlag = s_and(shr(ind,6), 0x1);
        st->codec_mode = MODE2;
        move16();
        get_rfTargetBits( st->rf_frame_type, &(st->rf_target_bits) );

        IF( sub(st->bfi_fx,FRAMEMODE_FUTURE) == 0 )
        {
            st->use_partial_copy = 1;
            /* now set the frame mode to normal mode */
            test();
            IF(sub(st->rf_frame_type,RF_TCXFD) >= 0 && sub(st->rf_frame_type, RF_TCXTD2) <= 0)
            {
                st->bfi_fx = 1;
                st->core_fx = 1;
            }
            ELSE
            {
                st->bfi_fx = FRAMEMODE_NORMAL;
                st->core_fx = 0;
            }
        }

        /* check for bit errors */
        berCheck( st, coder_type );

        get_next_indice_tmp_fx(st, nBits);

    }
}

/*-------------------------------------------------------------------*
* get_rfFlag()
*
* Check if rf flag is present in the bitstream
*-------------------------------------------------------------------*/

void get_rfFlag(
    Decoder_State_fx *st,                      /* i: decoder state structure       */
    Word16 *rf_flag,                 /* o  : check for the RF flag    */
    Word16 *nBits,
    Word16 *ind
)
{
    Word16 start_idx, nBits_tmp;
    Word16 ind_tmp;

    /* Init */
    *rf_flag = 0;

    /* check for rf_flag in the packet and extract the rf_frame_type and rf_fec_offset */
    test();
    test();
    IF( L_sub(st->total_brate_fx,ACELP_13k20) == 0 && (sub(st->bfi_fx,FRAMEMODE_NORMAL) == 0 || sub(st->bfi_fx, FRAMEMODE_FUTURE) == 0) )
    {
        /* find the section in the ACELP signalling table corresponding to bitrate */
        start_idx = 0;
        WHILE ( L_sub(acelp_sig_tbl[start_idx], st->total_brate_fx) != 0 )
        {
            start_idx++;
            assert((start_idx < MAX_ACELP_SIG) && "ERROR: start_idx larger than acelp_sig_tbl[].\n");
        }

        /* skip the bitrate */
        start_idx = add(start_idx,1);

        /* retrieve the number of bits */
        nBits_tmp = (Word16) acelp_sig_tbl[start_idx++];

        /* retrieve the signalling indice */
        ind_tmp = (Word16) acelp_sig_tbl[start_idx + get_indice_fx( st, 0, nBits_tmp )];

        /* convert signalling indice into RF flag. */
        *rf_flag = s_and(shr(ind_tmp, 7), 0x1);

        if( ind )
        {
            *ind = ind_tmp;
        }

        if( nBits )
        {
            *nBits = nBits_tmp;
        }
    }
}

/*-------------------------------------------------------------------*
* get_rfFrameType()
*
* Extract the rf frame type
*-------------------------------------------------------------------*/

void get_rfFrameType(
    Decoder_State_fx *st,                      /* i  : decoder state structure       */
    Word16 *rf_frame_type            /* o  : RF frame type                 */
)
{
    Word16 num_bits = 0;

    IF( sub(st->rf_flag, 1)== 0)
    {
        /*num_bits = st->total_brate_fx/50;*/
        if( L_sub(st->total_brate_fx, ACELP_13k20) == 0 )
        {
            num_bits = 264;
            move16();     /* @13.2kbps */
        }
        else
        {
            UWord16 lsb;
            Word32 L_tmp;
            Mpy_32_16_ss(st->total_brate_fx, 5243, &L_tmp, &lsb); /* 5243 is 1/50 in Q18. (0+18-15=3) */
            num_bits = extract_l(L_shr(L_tmp, 3));                /* Q0 */
        }

        /* the last three bits in a packet is the RF frame type */
        *rf_frame_type = get_indice_fx( st, num_bits - 3, 3 );
    }
    ELSE
    {
        *rf_frame_type = 0;
    }
}

/*-------------------------------------------------------------------*
* get_rf_fec_offset()
*
* Extract the FEC offset
*-------------------------------------------------------------------*/

void get_rf_fec_offset(
    Decoder_State_fx *st,                      /* i  : decoder state structure       */
    Word16 *rf_fec_offset            /* o  : RF fec offset                 */
)
{
    Word16 num_bits, tmp;

    IF( sub(st->rf_flag,1)== 0)
    {
        /*num_bits = st->total_brate_fx/50;*/
        if( L_sub(st->total_brate_fx, ACELP_13k20) == 0 )
        {
            num_bits = 264;
            move16();     /* @13.2kbps */
        }
        else
        {
            UWord16 lsb;
            Word32 L_tmp;
            Mpy_32_16_ss(st->total_brate_fx, 5243, &L_tmp, &lsb); /* 5243 is 1/50 in Q18. (0+18-15=3) */
            num_bits = extract_l(L_shr(L_tmp, 3));                /* Q0 */
        }

        /* the two bits before the rf frame type contain the fec offset  */
        tmp = get_indice_fx( st, num_bits - 5, 2 );

        if( tmp == 0 )
        {
            *rf_fec_offset = 2;
            move16();
        }
        else
        {
            *rf_fec_offset = add(shl(tmp, 1), 1);
        }
    }
    ELSE
    {
        *rf_fec_offset = 0;
        move16();
    }
}

/*-------------------------------------------------------------------*
* get_rfTargetBits()
*
* Return the number of RF target bits
*-------------------------------------------------------------------*/

void get_rfTargetBits(
    Word16 rf_frame_type,           /* i  : RF frame type                 */
    Word16 *rf_target_bits          /* o  : Number of RF target bits      */
)
{

    /* Number of RF bits for different RF coder types */

    SWITCH (rf_frame_type)
    {
    case RF_NO_DATA:
        *rf_target_bits = 5;
        BREAK;
    case RF_TCXFD:
        *rf_target_bits = 27;
        BREAK;
    case RF_TCXTD1:
        *rf_target_bits = 16;
        BREAK;
    case RF_TCXTD2:
        *rf_target_bits = 16;
        BREAK;
    case RF_ALLPRED:
        /* Es_pred bits 3 bits, LTF: 1, pitch: 8,5,5,5, FCB: 0, gain: 7,0,7,0, Diff GFr: 4*/
        *rf_target_bits = 63;
        BREAK;
    case RF_NOPRED:
        /* Es_pred bits 3 bits, LTF: 0, pitch: 0, FCB: 7,7,7,7, gain: 6,0,6,0, Diff GFr: 2*/
        *rf_target_bits = 66;
        BREAK;
    case RF_GENPRED:
        /* Es_pred bits 3 bits, LTF: 1, pitch: 8,0,8,0, FCB: 6,7,5,5, gain: 5,0,5,0, Diff GFr: 0*/
        *rf_target_bits = 70;
        BREAK;
    case RF_NELP:
        /* gain: 19, Diff GFr: 5 */
        *rf_target_bits =  45;
        BREAK;
    }
}


/*-------------------------------------------------------------------*
 * get_NextCoderType_fx()
 *
 * Extract the coder type of next frame
 *-------------------------------------------------------------------*/

void get_NextCoderType_fx(
    UWord8 *bitsteam,                           /* i : bitstream         */
    Word16 *next_coder_type                    /* o : next coder type   */
)
{
    Word16 k;
    Word16 start_idx;
    Word16 nBits_tmp;
    Word8 bit_stream[ACELP_13k20/50];
    UWord16 tmp;


    FOR( k = 0; k < ACELP_13k20/50; k++ )
    {
        bit_stream[k] = (bitsteam[k / 8] >> (7 - (k % 8))) & 0x1;
    }
    start_idx = 0;
    WHILE ( L_sub(acelp_sig_tbl[start_idx], ACELP_13k20) != 0 )
    {
        start_idx = add(start_idx,1);
        assert((start_idx < MAX_ACELP_SIG) && "ERROR: start_idx larger than acelp_sig_tbl[].\n");
    }

    /* skip the bitrate */
    start_idx = add(start_idx,1);

    tmp = 0;
    move16();
    nBits_tmp = (Word16) acelp_sig_tbl[start_idx++];
    FOR (k = 0; k < nBits_tmp; k++)
    {
        tmp = lshl(tmp, 1);
        tmp = add(tmp, bit_stream[k]);
    }
    /* retrieve the signalling indice */
    *next_coder_type = s_and((Word16)acelp_sig_tbl[start_idx + tmp],0x7);
}

/*-------------------------------------------------------------------*
 * read_indices_from_djb_fx()
 *
 * Read indices from the de-jitter buffer payload (works also for AMR-WB IO mode)
 *-------------------------------------------------------------------*/

void read_indices_from_djb_fx(              /* o  : 1 = reading OK, 0 = problem   */
    Decoder_State_fx *st,                      /* i/o: decoder state structure       */
    UWord8 *pt_stream,               /* i  : bitstream file                */
    Word16 num_bits                  /* i  : input frame length in bits    */
    ,Word16 partialframe              /* i  : partial frame information     */
    ,Word16 next_coder_type           /* i  : next coder type information     */
)
{
    Word16 k;
    UWord16 *bit_stream_ptr;
    Word32 total_brate;
    Word32 L_tmp;
    UWord16 utmp;
    Word16 bit0;

    st->BER_detect = 0;
    bit0 = 0;
    /* There is no FRAME_NO_DATA or BAD frame indicator in RTP, frames are just missing.
     * In case of comfort noise handle missing frame as FRAME_NO_DATA, otherwise use PLC. */
    IF(num_bits != 0)
    {
        st->bfi_fx = 0;
        bit0 = G192_BIN0;
        if( (*pt_stream & 0x80) != 0 )
        {
            bit0 = G192_BIN1;
        }
    }
    ELSE IF(L_sub(st->total_brate_fx,SID_1k75) == 0 ||
            L_sub(st->total_brate_fx,SID_2k40) == 0  ||
            L_sub(st->total_brate_fx,FRAME_NO_DATA) == 0 )
    {
        st->bfi_fx = 0;
    }
    ELSE
    {
        st->bfi_fx = 1;
    }
    IF( sub(partialframe,1) == 0 || sub(st->prev_use_partial_copy,1) == 0 )
    {
        st->next_coder_type = next_coder_type;
    }
    ELSE
    {
        st->next_coder_type = INACTIVE;
    }

    st->mdct_sw_enable = 0;
    st->mdct_sw = 0;
    reset_indices_dec_fx( st );
    total_brate = num_bits * 50;

    IF(sub(partialframe,1) == 0)
    {
        st->bfi_fx = 2;
    }

    Mpy_32_16_ss(total_brate, 5243, &L_tmp, &utmp); /* 5243 is 1/50 in Q18. (0+18-15=3) */
    st->total_num_bits = extract_l(L_shr(L_tmp, 3)); /* Q0 */

    IF ( st->bfi_fx != 1 )
    {
        /* select MODE1 or MODE2 */
        decoder_selectCodec( st, total_brate, bit0 );

        /* convert bitstream from compact bytes to short values and store it in decoder state */
        bit_stream_ptr = st->bit_stream_fx;
        FOR( k = 0; k < num_bits; k++ )
        {
            *bit_stream_ptr++ = (pt_stream[k / 8] >> (7 - (k % 8))) & 0x1;
        }
        /* add two zero bytes for arithmetic coder flush */
        FOR( k=0; k < 8*2; ++k )
        {
            *bit_stream_ptr++ = 0;
        }

        /* a change of the total bitrate should not be
           known to the decoder, if the received frame was
           lost */
        st->total_brate_fx = total_brate;

        mdct_switching_dec(st);
    }
}




/*-------------------------------------------------------------------*
 * get_indice_preview()
 *
 * Indices preview to parse for the presence of partial copy
 *-------------------------------------------------------------------*/
static UWord16 get_indice_preview(
    UWord8 *bitstream,
    Word16 bitstreamSize,
    Word16 pos,
    Word16 nb_bits
)
{
    UWord16 value;
    Word16 i;
    UWord16 bitstreamShort[MAX_BITS_PER_FRAME+16];
    UWord16 *bitstreamShortPtr;

    /* convert bitstream from compact bytes to short values */
    bitstreamShortPtr = bitstreamShort;
    FOR( i = 0; i < bitstreamSize; i++ )
    {
        *bitstreamShortPtr++ = (bitstream[i / 8] >> (7 - (i % 8))) & 0x1;
    }

    assert(nb_bits <= 16);
    value = 0;
    FOR (i = 0; i < nb_bits; i++)
    {
        value = shl(value,1);
        value = add(value,bitstreamShort[pos+i]);
    }
    return value;
}

/*-------------------------------------------------------------------*
 * evs_dec_previewFrame()
 *
 * Signalling index preview
 *-------------------------------------------------------------------*/
void evs_dec_previewFrame(
    UWord8 *bitstream,
    Word16 bitstreamSize,
    Word16 *partialCopyFrameType,
    Word16 *partialCopyOffset
)
{
    Word32 total_brate;
    Word16 start_idx, nBits;
    Word32 ind;
    Word16 rf_flag;

    rf_flag = 0;
    *partialCopyFrameType = 0;
    *partialCopyOffset = 0;
    total_brate = bitstreamSize * 50;

    IF( L_sub(total_brate,ACELP_13k20) == 0 )
    {
        /* find the section in the ACELP signalling table corresponding to bitrate */
        start_idx = 0;
        WHILE ( L_sub(acelp_sig_tbl[start_idx], total_brate) != 0 )
        {
            start_idx = add(start_idx,1);
            assert((start_idx < MAX_ACELP_SIG) && "ERROR: start_idx larger than acelp_sig_tbl[].\n");
        }

        /* skip the bitrate */
        start_idx = add(start_idx,1);
        /* retrieve the number of bits */
        nBits = (Word16) acelp_sig_tbl[start_idx++];

        /* retrieve the signalling indice */
        ind = acelp_sig_tbl[start_idx + get_indice_preview( bitstream, bitstreamSize, 0, nBits )];

        /* convert signalling indice into RF flag. */
        rf_flag = s_and(extract_l(L_shr(ind, 7)), 0x1);
        assert(rf_flag == ((ind >> 7) & 0x1));
        IF(rf_flag != 0)
        {
            /* read the fec offset at which the partial copy is received */
            ind = get_indice_preview( bitstream, bitstreamSize, (bitstreamSize-5), 2 );
            IF(ind== 0) *partialCopyOffset = 2;
            ELSE IF(L_sub(ind,1)==0) *partialCopyOffset = 3;
            ELSE IF(L_sub(ind,2)==0) *partialCopyOffset = 5;
            ELSE IF(L_sub(ind,3)==0) *partialCopyOffset = 7;

            /* the last three bits in a packet is the RF frame type */
            *partialCopyFrameType = get_indice_preview( bitstream, bitstreamSize, bitstreamSize - 3, 3 );
        }
    }
}


