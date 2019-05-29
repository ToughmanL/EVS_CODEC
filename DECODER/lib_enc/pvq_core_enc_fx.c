#include "options.h"
/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include <stdlib.h>
#include "options.h"        /* Compilation switches                   */
#include "rom_com_fx.h"     /* Static table prototypes                */
#include "prot_fx.h"        /* Function prototypes                    */
#include "stl.h"

static Word16 calc_pvq_splits_fx(Encoder_State_fx *st_fx, const Word16 band_bits, const Word16 sfmsize, const Word16 *y, const Word16 Q_y,
                                 Word16 *bits);

static void densityIndexSymbolEncode_fx( Encoder_State_fx *st_fx, const Word16 density , const Word16  r_dim, const Word16 l_dim , const Word16 index_phi );

/* Encode band with PVQ */

static void pvq_encode_band_fx(
    Encoder_State_fx *st_fx,
    const Word16 *coefs_norm,
    const Word16 Q_coefs,
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
    Word32 enr, E_part[MAX_SPLITS+1];
    Word16 part_start[MAX_SPLITS+1], dim_part[MAX_SPLITS+1], bits_part[MAX_SPLITS+1];
    Word16 pool_tot, pool_part, dim_parts;
    Word16 g_part[MAX_SPLITS];
    Word16 g_part_neg[MAX_SPLITS];
    Word16 sg_part[MAX_SPLITS+1];
    Word16 idx_sort[MAX_SPLITS+1];
    Word16 js, band_bits_tot, split_bit;
    Word16 tmp;
    Word32 L_coefs_quant_fx[PVQ_MAX_BAND_SIZE];

    Np = calc_pvq_splits_fx(st_fx, band_bits, sfmsize, coefs_norm, Q_coefs, &split_bit);
    band_bits_tot = sub(band_bits, split_bit);

    enr = L_deposit_l(0);
    FOR (j = 0; j < sfmsize; j++)
    {
        /* This is of paramount importance that the number of bits down-shift here
         * MUST BE IDENTICAL to that used to compute "l_enr" in encode_energies_fx().
         * If the number of bits down-shift is changed here, that in encode_energies_fx()
         * must be changed to the same amount as well!
         */
        tmp = shr(coefs_norm[j], 2);
        enr = L_mac0(enr, tmp, tmp);
    }

    dim_parts    =   extract_h(L_mult(negate(sfmsize),lim_neg_inv_tbl_fx[Np]));
    set16_fx( dim_part, dim_parts, sub(Np, 1) );
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
    set16_fx(g_part_neg, -32768, Np);  /* -1.0 in Q15 */
    IF (sub(Np, 1) > 0)
    {
        encode_energies_fx( st_fx, coefs_norm, Q_coefs, Np, dim_part, E_part, bits_part, g_part_neg, band_bits_tot, bits_left, enr, sfmsize, strict_bits );
    }
    ELSE
    {
        bits_part[0] = band_bits_tot;
        move16();
    }

    pool_tot  = 0;   /* Word16 */
    pool_part = 0;   /* Word16 */

    FOR (j = 0; j < Np; j++)
    {
        g_part[j] = negate(g_part_neg[j]);
    }
    srt_vec_ind16_fx(g_part, sg_part, idx_sort, Np);
    FOR(j = 0; j < Np; j++)
    {
        js            = idx_sort[Np-1-j];
        pool_part     = shrtCDivSignedApprox( pool_tot, sub(Np, j) );
        bits_part[js] = s_max(0, s_min(add(bits_part[js], pool_part), 256));

        conservativeL1Norm_fx(dim_part[js], bits_part[js], strict_bits, *bits_left, pool_tot, *npulses, /* inputs */
                              &K_val, bits_left, &pool_tot, npulses);                                   /* outputs */

        IF(  K_val > 0 )
        {
            pvq_encode_fx(st_fx,coefs_norm + part_start[js], pulse_vector + part_start[js],
                          coefs_quant + part_start[js], L_coefs_quant_fx, K_val, dim_part[js], g_part_neg[js]);
        }
        ELSE
        {
            set16_fx(coefs_quant + part_start[js], 0, dim_part[js]);
            set16_fx(pulse_vector + part_start[js], 0, dim_part[js]);
        }
    }

    return;
}

void pvq_encode_frame_fx(
    Encoder_State_fx *st_fx,
    const Word16 *coefs_norm,       /* i  : normalized coefficients to encode */
    Word16 Q_coefs,           /* i  : Q-point of coefs_norm[] */
    Word16 *coefs_quant,      /* o  : quantized coefficients */
    Word16 *gopt,             /* o  : optimal shape gains */
    Word16 *npulses,          /* o  : number of pulses per band */
    Word16 *pulse_vector,     /* o  : non-normalized pulse shapes */
    const Word16 *sfm_start,        /* i  : indices of first coefficients in the bands */
    const Word16 *sfm_end,          /* i  : indices of last coefficients in the bands */
    const Word16 *sfmsize,          /* i  : band sizes */
    const Word16 nb_sfm,            /* i  : total number of bands */
    const Word16 *R,                /* i  : bitallocation per band Q3 */
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
    Word16 exp, tmp, shift;
    Word32 xy_corr, yy_corr;
    rc_enc_init_fx(st_fx, pvq_bits);

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
        srt_vec_ind16_fx(R, R_sort, i_sort, nb_sfm);
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
        gopt[is] = 0;
        move16();
        IF (R[is] > 0)
        {
            bandBitsAdjustment_fx(st_fx->rc_num_bits_fx, st_fx->rc_range_fx, bits, bands_to_code, bands_to_code-coded_bands, sfmsize[is] ,R[is], bit_pool, /* inputs  */
                                  &band_bits, &bits_left, &bit_pool);                                                                                         /* outputs */
            pvq_encode_band_fx( st_fx, &coefs_norm[sfm_start[is]], Q_coefs, &pulse_vector[sfm_start[is]],
                                &npulses[is], &coefs_quant[sfm_start[is]], sfmsize[is], band_bits,
                                &bits_left, strict_bits);

            xy_corr = L_deposit_l(0);
            yy_corr = L_deposit_l(1);
            shift = band_len_ener_shift[ band_len_idx[ shr(sfmsize[is], 3)] ];
            move16();
            FOR (j = 0; j < sfmsize[i]; j++)
            {
                tmp = shr(coefs_quant[sfm_start[is]+j], shift);
                xy_corr = L_mac0(xy_corr, shr(coefs_norm[sfm_start[is]+j], 1), tmp); /* Q_coefs-1+15-shift */
                yy_corr = L_mac0(yy_corr, tmp, tmp);    /* 2*(15-shift) */
            }

            tmp = ratio(xy_corr, yy_corr, &exp);
            gopt[is] = shl(tmp, sub(sub(sub(14, Q_coefs), shift), exp));

            if (gopt[is] == 0)
            {
                gopt[is] = 1;
                move16();
            }

            /* Updates */
            coded_bands = add(coded_bands, 1);
        }
        ELSE
        {
            FOR (j = sfm_start[is]; j < sfm_end[is]; j++)
            {
                coefs_quant[j]  = 0;
                move16();
                pulse_vector[j] = 0;
                move16();
            }
        }
    }

    rc_enc_finish_fx(st_fx);

    return;
}

/*---------------------------------------------------------------------*
 * pvq_core_enc()
 *
 * Main Generic Audio Encoder Routine
 *---------------------------------------------------------------------*/

Word16 pvq_core_enc_fx(
    Encoder_State_fx *st_fx,
    Word16 coefs_norm[],
    Word16 coefs_quant[],
    Word16 *Q_coefs,
    Word16 bits_tot,                           /* total number of bits */
    Word16 nb_sfm,
    const Word16 *sfm_start,
    const Word16 *sfm_end,
    const Word16 *sfmsize,
    Word16 *R,        /* Q3   */
    Word16 *Rs,
    Word16 *npulses,
    Word16 *maxpulse,
    const Word16 core
)
{
    Word16 i;
    Word16 R_upd;    /* Q3 */
    Word16 ord[NB_SFM_MAX];
    Word16 fg_pred[NB_SFM_MAX];
    Word16 pvq_bits;

    Word16 pulse_vector[L_FRAME48k];
    Word16 gopt[NB_SFM];
    Word16 gain_bits_array[NB_SFM];
    Word16 gain_bits_tot;


    R_upd         = shl(bits_tot, 3);
    gain_bits_tot = assign_gain_bits_fx( core, nb_sfm, sfmsize, R, gain_bits_array, &R_upd );
    pvq_bits      = shr(R_upd, 3);

    pvq_encode_frame_fx( st_fx, coefs_norm, *Q_coefs, coefs_quant, gopt, npulses, pulse_vector, sfm_start, sfm_end, sfmsize, nb_sfm, R, pvq_bits, core );

    bits_tot = add(pvq_bits, gain_bits_tot);


    IF( Rs != NULL )
    {
        FOR (i=0; i < nb_sfm; i++)
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

    /* Fine gain prediction */
    fine_gain_pred_fx( sfm_start, sfm_end, sfmsize, ord, npulses, maxpulse, R, nb_sfm,
                       coefs_quant, pulse_vector, fg_pred, core);

    fine_gain_quant_fx(st_fx, ord, nb_sfm, gain_bits_array, fg_pred, gopt);

    apply_gain_fx(ord, sfm_start, sfm_end, nb_sfm, fg_pred, coefs_quant);
    *Q_coefs = 12;

    return bits_tot;
}

void encode_energies_fx(
    Encoder_State_fx *st_fx,
    const Word16 *coefs, /* In block-floating point format with a common block-exponent
                            not used in this function. Hence, the block-exponent is
                            not in the parameter list. */
    const Word16 Q_coefs,
    Word16 Np,
    Word16 *dim_part,
    Word32 *E_part,      /* 32-bit Q15 */
    Word16 *bits_part,
    Word16 *g_part,      /* Q15 */
    Word16 qband,
    Word16 *bits_left,
    Word32 enr,
    Word16 dim,
    const Word16 strict_bits
)
{
    Word16 i, j, l_Np, r_Np;
    Word16 l_bits, r_bits, l_dim, r_dim;
    Word32 l_enr, r_enr;
    Word16 l_gain, r_gain;
    Word16 il, ir;
    Word16  density;
    Word16  phi;
    Word16 index_phi = -1;
    Word16 oppRQ3, qzero ;
    Word16 angle;
    Word32 sqrt_r_enr, sqrt_l_enr;
    Word16 exp1, exp2, tmp;
    l_Np = shr(Np, 1);
    r_Np = sub(Np, l_Np);

    l_enr = L_deposit_l(0);
    l_bits = 0;
    move16();
    l_dim = 0;
    move16();
    FOR (i = 0; i < l_Np; i++)
    {
        l_dim = add(l_dim, dim_part[i]);
    }
    FOR (j = 0; j < l_dim; j++)
    {
        /* This is of paramount importance that the number of bits down-shift
         * must be identical to that used to compute "enr" in pvq_encode_band_fx().
         * Otherwise, the subsequent "r_enr" will be computed incorrectly.
         */
        tmp = shr(coefs[j], 2);
        l_enr = L_mac0(l_enr, tmp, tmp);
    }
    r_enr = L_sub(enr, l_enr);
    r_dim = sub(dim, l_dim);

    obtainEnergyQuantizerDensity_fx(dim, qband, &density);
    sqrt_r_enr = Sqrt_l(r_enr, &exp1);
    sqrt_l_enr = Sqrt_l(l_enr, &exp2);

    IF (sub(exp1, exp2) < 0)
    {
        sqrt_l_enr = L_shr(sqrt_l_enr, shr(sub(exp2, exp1), 1));
    }
    ELSE IF (sub(exp2, exp1) < 0)
    {
        sqrt_r_enr = L_shr(sqrt_r_enr, shr(sub(exp1, exp2), 1));
        exp1 = exp2;
        move16();
    }

    exp1 = add(shl(sub(Q_coefs, 2), 1), add(31, exp1)); /* 2x exponent */
    IF (sub(s_and(exp1, 1), 1) == 0)
    {
        sqrt_r_enr = Mult_32_16(sqrt_r_enr, 23170);     /* Q(exp1/2) */
        sqrt_l_enr = Mult_32_16(sqrt_l_enr, 23170);     /* Q(exp1/2) */
    }

    IF (sqrt_r_enr != 0)
    {
        angle = atan2_fx(sqrt_r_enr, sqrt_l_enr);       /* Q14 */
    }
    ELSE
    {
        angle = 0;
        move16();
    }

    phi   = mult_r(angle, 20861);

    rangeCoderFinalizationFBits_fx((Word16)st_fx->rc_num_bits_fx, (UWord32)st_fx->rc_range_fx, &qzero);
    densityAngle2RmsProjEnc_fx(density, phi , &index_phi, &ir, &il, &oppRQ3);
    densityIndexSymbolEncode_fx( st_fx, density , r_dim,  l_dim , index_phi );



    l_gain = il;  /* Q15 */                                                     move16();
    r_gain = ir;  /* Q15 */                                                     move16();

    FOR (i = 0; i < l_Np; i++)
    {
        g_part[i] = mult_r(l_gain, g_part[i]);
    }

    FOR (i = l_Np; i < Np; i++)
    {
        g_part[i] = mult_r(r_gain, g_part[i]);
    }

    NearOppSplitAdjustment_fx( qband, qzero, st_fx->rc_num_bits_fx, st_fx->rc_range_fx,  *bits_left,
                               strict_bits, Np, dim_part[0], dim_part[Np-1],
                               l_dim, r_dim, oppRQ3,
                               &l_bits, &r_bits, bits_left);

    IF (sub(l_Np, 1) > 0)
    {
        encode_energies_fx( st_fx, coefs, Q_coefs, l_Np, dim_part, E_part, bits_part, g_part, l_bits, bits_left, l_enr, l_dim, strict_bits );
    }
    ELSE
    {
        E_part[0] = l_enr;
        move32();
        bits_part[0] = l_bits;
        move16();
    }
    IF (sub(r_Np, 1) > 0)
    {
        encode_energies_fx( st_fx, &coefs[l_dim], Q_coefs, r_Np, &dim_part[l_Np], &E_part[l_Np], &bits_part[l_Np], &g_part[l_Np], r_bits, bits_left, r_enr, r_dim, strict_bits );
    }
    ELSE
    {
        E_part[1] = r_enr;
        move32();
        bits_part[1] = r_bits;
        move16();
    }

    return;
}


void densityIndexSymbolEncode_fx(
    Encoder_State_fx *st_fx,
    const Word16 density ,
    const Word16 r_dim,
    const Word16 l_dim,
    const Word16 index_phi
)
{
    Word16 angle, c, densitySubIndex, densitySubC;
    Word32 sym_freq, cum_freq, tot;
    Word32 acc;
    UWord16 lsb;

    sym_freq = L_deposit_l(1);

    IF( s_and((Word16)0xFFFE, density) != 0  )  /* even */
    {
        angle = atan2_fx(SQRT_DIM_fx[r_dim], SQRT_DIM_fx[l_dim]);
        angle = shl(angle, 1);
        angle = mult_r(angle, 20861);
        c = mult_r(density, angle);

        densitySubIndex = sub(density, index_phi);
        densitySubC = sub(density, c);

        IF (c == 0)
        {
            tot =  L_mac0(1L, density, add(density, 1));
            sym_freq = L_deposit_l(add(shl(sub(density, index_phi), 1), 1));
            cum_freq = L_mac0(L_mult(index_phi, density), index_phi, 1);
        }
        ELSE IF (sub(c, density) == 0)
        {
            tot = L_mac0(1L, density, add(density, 1));
            sym_freq = L_deposit_l(add(shl(index_phi, 1), 1));
            cum_freq = L_mult0(index_phi, index_phi);
        }
        ELSE
        {
            acc = L_mult0(density, c);  /* density*c */
            Mpy_32_16_ss(acc, densitySubC, &acc, &lsb);  /* density*c*(density - c) */
            acc = L_or(L_shl(acc, 16), L_and(lsb, 0xffffL)); /* Concatenate acc and lsb forming 48-bit; upshift 16 bits; keep 32 MSB. */
            acc = L_shr(acc, 1);  /* Compensate fractional mode multiply (Mpy_32_16_ss) */
            tot = L_add(L_add(acc, (Word32)density), 1L);  /* density*c*(density - c) + density + 1 */
            IF (sub(index_phi, c) <= 0)
            {
                sym_freq = L_mac(1L, index_phi, densitySubC);
                acc = L_mult0(densitySubC, sub(index_phi, 1));
                acc = L_add(acc, 1);
                Mpy_32_16_ss(acc, index_phi, &acc, &lsb);
                acc = L_or(L_shl(acc, 16), L_and(lsb, 0xffffL));
                cum_freq = L_shr(acc, 1);
            }
            ELSE
            {
                sym_freq = L_mac(1L, densitySubIndex, c);
                acc = L_mult0(densitySubIndex, add(densitySubIndex, 1));
                Mpy_32_16_ss(acc, c, &acc, &lsb);
                acc = L_or(L_shl(acc, 16), L_and(lsb, 0xffffL));
                acc = L_shr(acc, 1);
                acc = L_sub(acc, (Word32)index_phi);
                acc = L_add(add(density, 1), acc);
                cum_freq = L_sub(tot, acc);
            }
        }

        rc_encode_fx(st_fx, cum_freq, sym_freq, tot);
    }

    return;
}

/*--------------------------------------------------------------------------*
 * calc_pvq_splits()
 *
 * Calculate the number of segments needed
 *--------------------------------------------------------------------------*/

static Word16 calc_pvq_splits_fx(      /* o  : Number of segments           */
    Encoder_State_fx *st_fx,           /* i/o: Encoder state                */
    const Word16 band_bits,            /* i  : Band bit rate                */
    const Word16 sfmsize,              /* i  : Band width                   */
    const Word16 *y,                   /* i  : Target vector                */
    const Word16 Q_y,                  /* i  : Q point of y                 */
    Word16 *bits                 /* o  : Consumed bits                */
)
{
    Word16 Np;
    Word16 Npart;
    Word16 i,j;
    Word16 E[MAX_SPLITS];
    Word16 Emean, eTotal;
    Word16 tmp, expo;
    const Word16 *pY;
    Word16 max_dev,qPoint;
    Word32 acc, acc1;

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
            Npart = extract_l((Word32)intLimCDivPos_fx(UL_deposit_l((UWord16)sfmsize), Np));
            *bits  = 8;
            move16();
            eTotal = 0;
            move16();
            pY     = y;
            qPoint = shl(Q_y, 1);     /* acc in 2*Q_y */
            FOR (i = 0; i < Np; i++)
            {
                acc = L_deposit_l(0);
                FOR (j = 0; j < Npart; j++)
                {
                    acc= L_mac0(acc, *pY, *pY);
                    pY++;
                }
                acc = L_shr(acc, qPoint);

                E[i]   = sub(30, norm_l(L_max(1L,acc)));  /* L_max needed to handle low subvector levels */
                eTotal = add(eTotal, E[i]);
            }

            Emean = ratio(eTotal, Np, &expo);
            Emean = shr(Emean, add(14 - 8, expo));  /* Emean in Q8 */

            max_dev = -1;
            move16();
            FOR (i = 0; i < Np; i++)
            {
                tmp     = abs_s(sub(shl(E[i], 8), Emean));  /* in Q8 */
                max_dev = s_max(tmp, max_dev);
            }

            acc = L_sub(L_shl(Np, 5 + 3 + 8), L_shl(band_bits, 8)); /* NB: max_dev in Q8 */
            acc1 = L_shl(L_mult0(Np, max_dev), 3);

            tmp = 0;
            move16() ;
            if( L_sub(acc1, acc) > 0)
            {
                tmp = 1;
                move16();
            }
            rc_enc_bits_fx(st_fx, tmp, 1);
            Np = add(Np, tmp);
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


