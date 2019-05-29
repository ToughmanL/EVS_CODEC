/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include <stdlib.h>
#include "options.h"     /* Compilation switches                   */
#include "cnst_fx.h"       /* Common constants                       */
#include "prot_fx.h"       /* Function prototypes                    */
#include "rom_com_fx.h"    /* Static table prototypes      */
#include "stl.h"
#include "basop_util.h"

static Word16 get_pvq_splits_fx(Decoder_State_fx *st_fx, const Word16 band_bits, const Word16 sfmsize, Word16 *bits);

static  void densitySymbolIndexDecode_fx(Decoder_State_fx *st_fx,  const Word16  density, const Word16  opp_sz, const Word16   near_sz, Word16 *index_phi);

/* Decode band with PVQ */
static void pvq_decode_band_fx(
    Decoder_State_fx *st_fx,
    Word16 *pulse_vector,
    Word16 *npulses,
    Word16 *coefs_quant,
    const Word16 sfmsize,
    const Word16 band_bits,
    Word16 *bits_left,
    const Word16 strict_bits
)
{

    Word16 K_val;

    Word16 j, Np;
    Word16 part_start[MAX_SPLITS+1], dim_part[MAX_SPLITS+1], bits_part[MAX_SPLITS+1];
    Word16 pool_tot, pool_part, dim_parts;
    Word16 g_part[MAX_SPLITS];
    Word16 g_part_neg[MAX_SPLITS];
    Word16 sg_part[MAX_SPLITS+1];
    Word16 idx_sort[MAX_SPLITS+1];
    Word16 js, band_bits_tot, split_bit;
    Np = get_pvq_splits_fx(st_fx, band_bits, sfmsize, &split_bit);
    band_bits_tot = sub(band_bits, split_bit);

    dim_parts   =    extract_h(L_mult(negate(sfmsize),lim_neg_inv_tbl_fx[Np]));
    set16_fx(dim_part, dim_parts, sub(Np, 1));
    dim_part[Np-1] = sub(sfmsize, i_mult2(dim_parts, sub(Np, 1)));
    move16();

    part_start[0] = 0;
    move16();
    FOR (j = 1; j < Np; j++)
    {
        part_start[j] = add(part_start[j-1], dim_part[j-1]);
        move16();
    }

    /* Encode energies */
    set16_fx( g_part_neg, -32768, Np );
    IF( sub(Np, 1) > 0 )
    {
        decode_energies_fx( st_fx, Np, dim_part, bits_part, g_part_neg, band_bits_tot, bits_left, sfmsize, strict_bits );
    }
    ELSE
    {
        bits_part[0] = band_bits_tot;
        move16();
    }

    pool_tot = 0;
    move16();
    pool_part = 0;
    move16();
    BASOP_SATURATE_WARNING_OFF
    FOR (j = 0; j < Np; j++)
    {
        g_part[j] = negate(g_part_neg[j]);
    }
    BASOP_SATURATE_WARNING_ON
    srt_vec_ind16_fx(g_part, sg_part, idx_sort, Np);
    FOR(j = 0; j < Np; j++)
    {
        js            = idx_sort[Np-1-j];
        pool_part     = shrtCDivSignedApprox(pool_tot, sub(Np, j) );
        bits_part[js] = s_max(0, s_min(add(bits_part[js], pool_part), 256));
        move16();

        conservativeL1Norm_fx(dim_part[js],bits_part[js], strict_bits, *bits_left, pool_tot , *npulses, /* inputs */
                              &K_val, bits_left, &pool_tot, npulses);                                      /* outputs */

        IF(  K_val > 0 )
        {

            pvq_decode_fx(st_fx, coefs_quant + part_start[js], pulse_vector + part_start[js],
                          K_val, dim_part[js], g_part_neg[js]);
        }
        ELSE
        {
            set16_fx(coefs_quant + part_start[js], 0, dim_part[js]);
            set16_fx(pulse_vector + part_start[js], 0, dim_part[js]);
        }
    }

    return;
}

void pvq_decode_frame_fx(
    Decoder_State_fx *st_fx,
    Word16 *coefs_quant,      /* o  : quantized coefficients */
    Word16 *npulses,          /* o  : number of pulses per band */
    Word16 *pulse_vector,     /* o  : non-normalized pulse shapes */
    const Word16 *sfm_start,        /* i  : indices of first coefficients in the bands */
    const Word16 *sfm_end,          /* i  : indices of last coefficients in the bands */
    const Word16 *sfmsize,          /* i  : band sizes */
    const Word16 nb_sfm,            /* i  : total number of bands */
    const Word16 *R,                /* i  : bitallocation per band */
    const Word16 pvq_bits,          /* i  : number of bits avaiable */
    const Word16 core               /* i  : core */
)
{
    Word16 i, j;
    Word16 band_bits, bits_left;
    Word16 bit_pool = 0;
    Word16 coded_bands, bands_to_code;
    Word16 bits;
    Word16 R_sort[NB_SFM];
    Word16 is, i_sort[NB_SFM];
    Word16 strict_bits;

    rc_dec_init_fx(st_fx, pvq_bits);

    bits = shl(sub(pvq_bits, RC_BITS_RESERVED), 3);

    bands_to_code = 0;
    move16();
    FOR (i = 0; i < nb_sfm; i++)
    {
        if (R[i] > 0)
        {
            bands_to_code = add(bands_to_code, 1);
        }
    }

    IF (core == ACELP_CORE)
    {
        strict_bits = 1;
        move16();
        srt_vec_ind16_fx (R, R_sort, i_sort, nb_sfm);
    }
    ELSE
    {
        strict_bits = 0;
        move16();
        FOR (i = 0; i < nb_sfm; i++)
        {
            i_sort[i] = i;
            move16();
        }
    }

    coded_bands = 0;
    move16();
    FOR (i = 0; i < nb_sfm; i++)
    {
        is = i_sort[i];
        move16();
        IF(R[is] > 0)
        {
            bandBitsAdjustment_fx(st_fx->rc_num_bits_fx, st_fx->rc_range_fx, bits, bands_to_code, bands_to_code-coded_bands, sfmsize[is] ,R[is], bit_pool, /* inputs  */
                                  &band_bits, &bits_left, &bit_pool);                                                                          /* outputs */

            pvq_decode_band_fx( st_fx, &pulse_vector[sfm_start[is]], &npulses[is],
                                &coefs_quant[sfm_start[is]], sfmsize[is], band_bits,
                                &bits_left, strict_bits);

            /* Updates */
            coded_bands = add(coded_bands, 1);
        }
        ELSE
        {
            FOR (j = sfm_start[is]; j < sfm_end[is]; j++)
            {
                coefs_quant[j] = 0;
                move16();
                pulse_vector[j] = 0;
                move16();
            }
        }
    }

    rc_dec_finish_fx(st_fx);
}

/*-------------------------------------------------------------------*
 * pvq_core_dec()
 *
 *-------------------------------------------------------------------*/

Word16 pvq_core_dec_fx(
    Decoder_State_fx *st_fx,
    const Word16 *sfm_start,
    const Word16 *sfm_end,
    const Word16 *sfmsize,
    Word16 coefs_quant[],               /* o  : output   MDCT     */
    Word16 *Q_coefs,
    Word16 bits_tot,
    Word16 nb_sfm,
    Word16 *R,    /* Q3 */
    Word16 *Rs,
    Word16 *npulses,
    Word16 *maxpulse,
    const Word16 core
)
{
    Word16 i;
    Word16 R_upd;
    Word16 ord[NB_SFM_MAX];
    Word16 pulse_vector[L_FRAME48k];
    Word16 pvq_bits;
    Word16 gain_bits_array[NB_SFM];
    Word16 fg_pred[NB_SFM_MAX];

    st_fx->ber_occured_in_pvq = 0;
    move16();

    R_upd = shl(bits_tot, 3);
    assign_gain_bits_fx( core, nb_sfm, sfmsize, R, gain_bits_array, &R_upd );

    pvq_bits = shr(R_upd, 3);

    pvq_decode_frame_fx(st_fx, coefs_quant, npulses, pulse_vector, sfm_start,
                        sfm_end, sfmsize, nb_sfm, R, pvq_bits, core );

    IF( Rs != NULL )
    {
        FOR(i = 0; i < nb_sfm; i++)
        {
            if (npulses[i] <= 0)
            {
                Rs[i] = 0;
                move16(); /* Update Rs in case no pulses were assigned */
            }
        }
    }

    FOR (i=0; i < nb_sfm; i++)
    {
        ord[i] = i;
        move16();
        if (npulses[i] <= 0)
        {
            R[i] = 0;
            move16(); /* Update in case no pulses were assigned */
        }
    }

    get_max_pulses_fx( sfm_start, sfm_end, ord, npulses, nb_sfm, pulse_vector, maxpulse );

    fine_gain_pred_fx( sfm_start, sfm_end, sfmsize, ord, npulses, maxpulse, R,
                       nb_sfm, coefs_quant, pulse_vector, fg_pred, core );

    fine_gain_dec_fx( st_fx, ord, nb_sfm, gain_bits_array, fg_pred);

    IF( st_fx->ber_occured_in_pvq != 0 )
    {
        set16_fx( fg_pred, 1, nb_sfm );  /* low complex ECU action in case of detetected BER in PVQ decoding */
    }
    apply_gain_fx(ord, sfm_start, sfm_end, nb_sfm, fg_pred, coefs_quant);
    *Q_coefs = 12;

    return bits_tot;
}

void decode_energies_fx(
    Decoder_State_fx *st_fx,
    Word16 Np,
    Word16 *dim_part,
    Word16 *bits_part,
    Word16 *g_part,     /* Q15 */
    short qband,
    Word16 *bits_left,
    Word16 dim,
    const Word16 strict_bits
)
{
    Word16 density;
    Word16  i, l_Np, r_Np;
    Word16 l_bits, r_bits, l_dim, r_dim;
    Word16 il, ir;
    Word16 oppRQ3, qzero;
    Word16 l_gain, r_gain;
    Word16 index_phi=-1;

    l_Np = shr(Np, 1);
    r_Np = sub(Np, l_Np);

    l_bits = 0;
    move16();
    l_dim = 0;
    move16();
    FOR (i = 0; i < l_Np; i++)
    {
        l_dim = add(l_dim, dim_part[i]);
    }
    r_dim = sub(dim, l_dim);

    obtainEnergyQuantizerDensity_fx(dim, qband, &density);
    rangeCoderFinalizationFBits_fx(st_fx->rc_num_bits_fx, st_fx->rc_range_fx, &qzero);

    densitySymbolIndexDecode_fx( st_fx,  density, r_dim, l_dim, &index_phi);
    densityAngle2RmsProjDec_fx(density, index_phi, &ir, &il, &oppRQ3);




    l_gain = il;  /* Q15 */                                                     move16();
    r_gain = ir;  /* Q15 */                                                     move16();

    FOR (i = 0; i < l_Np; i++)
    {
        g_part[i] = mult_r(l_gain, g_part[i]);
        move16();
    }

    FOR (i = l_Np; i < Np; i++)
    {
        g_part[i] = mult_r(r_gain, g_part[i]);
        move16();
    }

    NearOppSplitAdjustment_fx( qband, qzero, st_fx->rc_num_bits_fx, st_fx->rc_range_fx,  *bits_left,
                               strict_bits, Np, dim_part[0], dim_part[Np-1],
                               l_dim, r_dim, oppRQ3,
                               &l_bits, &r_bits, bits_left);


    IF (sub(l_Np, 1) > 0)
    {
        decode_energies_fx( st_fx, l_Np, dim_part, bits_part, g_part, l_bits, bits_left, l_dim, strict_bits );
    }
    ELSE
    {
        bits_part[0] = l_bits;
        move16();
    }

    IF (sub(r_Np, 1) > 0)
    {
        decode_energies_fx( st_fx, r_Np, &dim_part[l_Np], &bits_part[l_Np], &g_part[l_Np], r_bits, bits_left, r_dim, strict_bits );
    }
    ELSE
    {
        bits_part[1] = r_bits;
        move16();
    }

    return;
}

static  void densitySymbolIndexDecode_fx(Decoder_State_fx *st_fx,
        const Word16  density,
        const Word16  opp_sz,
        const Word16  near_sz,
        Word16 *index_phi
                                        )
{
    Word16 density1, density2;
    Word32 tmp1;
    Word16 tmp2;
    Word16 c, density_alpha, density_c;
    Word32 sym_freq, cum_freq, tot, dec_freq;
    Word16 angle, expo, r;
    UWord16 lsb;
    Word32 acc;
    Word16 alpha=0;

    IF( s_and((Word16)0xFFFE, density) == 0 )
    {
        /* odd density exit */
        *index_phi = -1;
        return;
    }

    sym_freq = L_deposit_l(1);

    angle = atan2_fx(SQRT_DIM_fx[opp_sz], SQRT_DIM_fx[near_sz]);
    angle = shl(angle, 1);
    angle = mult_r(angle, 20861);
    c = mult_r(density, angle);
    density_c = sub(density, c);

    tot =  L_mac0(1L, density, add(density, 1));
    IF (c == 0)
    {
        dec_freq = rc_decode_fx(st_fx, tot);

        density1 = add(density, 1);
        acc = L_mult0(density1, density1);
        acc = L_sub(acc, dec_freq);
        alpha = add(getSqrtWord32(acc), density1);
        sym_freq = L_mac(1L, sub(density, alpha), 1);
        cum_freq = L_mac0(L_mult(alpha, density), alpha, 1);
    }
    ELSE IF (sub(c, density) == 0)
    {
        dec_freq = rc_decode_fx(st_fx, tot);

        alpha = getSqrtWord32(dec_freq);
        sym_freq = L_add(L_shl(alpha, 1), 1);
        cum_freq = L_mult0(alpha, alpha);
    }
    ELSE
    {
        acc = L_mult0(density, c);
        Mpy_32_16_ss(acc, density_c, &acc, &lsb);
        acc = L_or(L_shl(acc, 16), L_and(lsb, 0xffffL)); /* Concatenate acc and lsb forming 48 bits; upshift 16 bits; keep 32 MSB. */
        acc = L_shr(acc, 1);  /* Compensate fractional mode multiply (Mpy_32_16_ss) */
        tot = L_add(L_add(acc, (Word32)density), 1L);

        dec_freq = rc_decode_fx(st_fx, tot);

        acc = L_mult0(sub(density_c, 1), density_c);
        Mpy_32_16_ss(acc, c, &acc, &lsb);
        acc = L_or(L_shl(acc, 16), L_and(lsb, 0xffffL)); /* Concatenate acc and lsb forming 48 bits; upshift 16 bits; keep 32 MSB. */
        acc = L_shr(acc, 1);  /* Compensate fractional mode multiply (Mpy_32_16_ss) */
        acc = L_add(acc, L_add(density, 1));
        acc = L_sub(acc, L_add(c, 1));
        acc = L_sub(tot, acc);
        IF (L_sub(dec_freq, acc ) < 0)
        {
            acc = L_add((Word32)density_c, L_shl(dec_freq, 2));
            acc = L_sub(acc, 2);
            Mpy_32_16_ss(acc, density_c, &acc, &lsb);
            acc = L_or(L_shl(acc, 16), L_and(lsb, 0xffffL)); /* Concatenate acc and lsb forming 48 bits; upshift 16 bits; keep 32 MSB. */
            acc = L_shr(acc, 1);  /* Compensate fractional mode multiply (Mpy_32_16_ss) */
            acc = L_add(acc, 1);
            tmp2 = getSqrtWord32(acc);
            acc = L_add(density_c, tmp2);
            acc = L_sub(acc, 1);
            r = ratio(acc, L_shl(density_c,1), &expo);
            alpha = shr(r, add(14, expo));

            acc = L_mult(alpha, density_c);
            sym_freq = L_add(acc, 1);
            acc = L_mult0(sub(alpha, 1), density_c);
            acc = L_add(acc, 1);
            cum_freq = L_mult0(alpha, extract_l(acc));
        }
        ELSE
        {
            density1 = add(density, 1);
            density2 = add(shl(density, 1), 1);

            acc = L_mult0(density, density1);
            Mpy_32_16_ss(acc, c, &acc, &lsb);
            acc = L_or(L_shl(acc, 16), L_and(lsb, 0xffffL)); /* Concatenate acc and lsb forming 48-bit; upshift 16 bits; keep 32 MSB. */
            acc = L_shr(acc, 1);  /* Compensate fractional mode multiply (Mpy_32_16_ss) */
            acc = L_add(density1, acc);
            acc = L_add(dec_freq, acc);
            acc = L_sub(tot, acc);
            Mpy_32_16_ss(acc, c, &acc, &lsb);
            acc = L_or(L_shl(acc, 16), L_and(lsb, 0xffffL));
            acc = L_shr(acc, 1 - 2);

            tmp2 = extract_l(L_mac0(1L, c, density2));
            tmp1 = L_mult0(tmp2, tmp2);
            tmp1 = L_add(tmp1, acc);
            tmp2 = getSqrtWord32(tmp1); /* floor */
            if (L_msu0(tmp1, tmp2, tmp2) != 0)
            {
                tmp2 = add(tmp2, 1);    /* convert to ceil */
            }

            acc = L_mult0(c, density2);
            acc = L_add(acc, 1);
            acc = L_sub(acc, tmp2);
            r = ratio(acc, L_shl(c, 1), &expo);
            alpha = shr(r, add(14, expo));

            density_alpha = sub(density, alpha);
            sym_freq = L_mac(1L, density_alpha, c);
            acc = L_mult0(density_alpha, add(density_alpha, 1));
            Mpy_32_16_ss(acc, c, &acc, &lsb);
            acc = L_or(L_shl(acc, 16), L_and(lsb, 0xffffL)); /* Concatenate acc and lsb forming 48-bit; upshift 16 bits; keep 32 MSB. */
            acc = L_shr(acc, 1);  /* Compensate fractional mode multiply (Mpy_32_16_ss) */
            acc = L_sub(acc, alpha);
            acc = L_add(acc, density1);
            cum_freq = L_sub(tot, acc);
        }
    }

    rc_dec_update_fx(st_fx, cum_freq, sym_freq);
    *index_phi = alpha;
    return;
}


/*--------------------------------------------------------------------------*
 * get_pvq_splits()
 *
 * Retrieve the number of segments
 *--------------------------------------------------------------------------*/

static Word16 get_pvq_splits_fx(   /* o  : Number of segments           */
    Decoder_State_fx *st_fx,             /* i/o: Decoder state                */
    const Word16 band_bits,        /* i  : Band bit rate                */
    const Word16 sfmsize,          /* i  : Band width                   */
    Word16 *bits             /* o  : Used bits                    */
)
{
    Word16 Np, i;
    Word32 acc, flag;

    IF (band_bits == 0)
    {
        Np = 1;
        move16();
    }
    ELSE
    {
        acc = L_mult0(band_bits, 0x7a44);
        Np = extract_l(L_shr(acc, 23));    /* Get integer part. */
        if (L_and(acc, 0x7fffffL) != 0)    /* If fractional part != 0, add 1. */
        {
            Np = add(Np, 1);  /* ceiling operation */
        }
    }
    *bits = 0;
    move16();
    IF (sub(Np, MAX_SPLITS) < 0)
    {
        acc = L_mult0(8*THR_ADD_SPLIT, sfmsize);
        IF (L_sub(band_bits, acc) > 0)
        {
            flag = rc_dec_bits_fx(st_fx, 1);
            *bits = 8;
            move16();
            if (flag != 0)
            {
                Np = add(Np, 1);
            }
        }
    }
    /* Check constraints for number of splits */
    /* The following code assumes that PVQ_MAX_BAND_SIZE is 64 */
    i = shr(sfmsize, 6);  /* 6 = log2(64) = log2(PVQ_MAX_BAND_SIZE) */
    if (s_and(sfmsize, 0x3f) != 0)
    {
        i = add(i, 1);  /* ceiling operation */
    }

    Np = s_max(i, Np);
    Np = s_min(MAX_SPLITS, Np);
    Np = s_min(sfmsize, Np);     /* The code line assumes that MIN_BAND_SIZE is 1 */
    return Np;
}


