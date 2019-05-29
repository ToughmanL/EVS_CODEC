/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include "options.h"     /* Compilation switches                   */

#include "prot_fx.h"       /* Function prototypes                    */
#include "cnst_fx.h"       /* Common constants                       */
#include "rom_com_fx.h"    /* Static table prototypes                */
#include "stl.h"        /* required for wmc_tool */


#include "options.h"


UWord32 intLimCDivPos_fx(
    UWord32 NUM,
    Word16 DEN
)
{
    UWord32 UL_ru, UL_rl;
    Mpy_32_32_uu(UL_lshl(NUM, 1), intLimCDivInvDQ31[DEN], &UL_ru, &UL_rl);
    return UL_ru;
}


Word32 intLimCDivSigned_fx(
    Word32 NUM,
    Word16 DEN)
{
    Word32 L_tmp;

    L_tmp  = intLimCDivPos_fx( L_abs(NUM) , DEN);
    if (NUM < 0)
    {
        L_tmp= L_negate(L_tmp); /* one op */
    }
    return L_tmp;
}


Word16  shrtCDivSignedApprox( const Word16  num,
                              const Word16  den
                            )
{
    Word16 pool_part;

    pool_part   =  extract_h( L_mult( negate(abs_s(num)), lim_neg_inv_tbl_fx[den] ));
    /* neg_in always, positive out always, so that positive truncation(rounding) is used */
    if ( num < 0 )
    {
        pool_part  = negate(pool_part);  /* make negative,  one op */
    }
    return pool_part;
}

void  nearProjQ15_fx(
    Word16 x,
    Word16 *result
)
{
    const Word16 a[4] = {14967,   -25518,   3415,   32351};
    Word32 b;
    UWord16 lsb;

    b = L_deposit_l(a[0]);
    b = L_shl((Word32)add(a[1], extract_h(L_mult0((Word16)b, x))), 1);
    Mpy_32_16_ss(b, x, &b, &lsb);
    b = L_add((Word32)a[2], b);
    Mpy_32_16_ss(b, x, &b, &lsb);
    b = L_add((Word32)a[3], b);
    *result = extract_l(b);
    return ;
}

/*-------------------------------------------------------------------*
 * obtainEnergyQuantizerDensity_fx()
 *
 *
 *-------------------------------------------------------------------*/
void obtainEnergyQuantizerDensity_fx(
    const Word16 L,
    const Word16 R,
    Word16 *Density )
{
    Word16 Rnrg, den, n;

    den = sub(shl(L, 1), 1);
    IF( den <= 67 )
    {
        Rnrg =  extract_l(intLimCDivPos_fx( L_deposit_l(R) , den));
    }
    ELSE
    {
        n    =  norm_s(den);
        Rnrg =  shr(div_s(R, shl(den, n)), sub(15, n));
    }
    Rnrg = add(Rnrg, 28);

    Rnrg = s_min(Rnrg, 56);
    Rnrg = s_min(Rnrg, sub(R, 96));

    Rnrg     = s_max(Rnrg, 3);
    *Density = obtainEnergyQuantizerDensity_f[Rnrg];
    return;
}


/*-------------------------------------------------------------------*
 * dsDirac2Dirac_fx()
 *
 *
 *-------------------------------------------------------------------*/
void  dsDirac2Dirac_fx(
    const Word16 dsDiracIndex,
    Word16 *diracs
)
{
    *diracs = dsDiracsTab[dsDiracIndex];
    return;
}

void dsDiracPerQuanta_fx(
    const Word16 td,
    const Word16 t_quanta,
    const Word16 dsm,
    const unsigned char* const *frQuanta,
    Word16 *DsIdx
)
{
    const unsigned char *sv;
    Word16 nsv;
    Word16 t_quanta_o;
    Word16 dsIndex;
    Word16 i;

    sv = frQuanta[td];
    nsv = sv[0];

    t_quanta_o = sub(t_quanta, QUANTAQ3OFFSET);

    IF (sub(t_quanta_o, sv[nsv]) >= 0)
    {
        *DsIdx = nsv;
        move16();
        return ;
    }

    IF (sub(t_quanta_o, sv[1]) <= 0)
    {
        *DsIdx = 1;
        move16();
        return ;
    }


    dsIndex = shl(1, frQuanta[0][td]);
    if (sub(t_quanta_o, sv[shr(nsv, 1) ]) > 0 )
    {
        dsIndex = sub(nsv, dsIndex );
    }
    FOR (i = sub(frQuanta[0][td], 1); i >= 0; i--)
    {
        dsIndex = add(dsIndex, shl(sub(shl(lshr(sub(sv[dsIndex], t_quanta_o), 15), 1), 1), i));
    }

    dsIndex = add(dsIndex, lshr(sub(sv[dsIndex], t_quanta_o), 15));
    dsIndex = sub(dsIndex, lshr(sub(1, dsIndex), 15));

    IF (dsm > 0)
    {
        *DsIdx=dsIndex;
        move16();
        return;
    }
    *DsIdx = add(dsIndex, lshr(sub(add(sv[add(dsIndex,1)], sv[dsIndex]), shl(t_quanta_o, 1)), 15));
    return;
}

void QuantaPerDsDirac_fx(
    Word16 td,
    Word16 dsDiracIndex,
    const unsigned char* const* dimFrQuanta,
    Word16 *Quanta
)
{
    *Quanta = dimFrQuanta[td][dsDiracIndex];
    move16();
    if(dsDiracIndex == 0)
    {
        *Quanta = -1; /* single op */ move16();
    }
    *Quanta = add(*Quanta, QUANTAQ3OFFSET);
    return ;
}

void conservativeL1Norm_fx(
    Word16 L,
    Word16 Qvec,
    Word16 Fcons,
    Word16 Qavail,
    Word16 Qreserv,
    Word16 Dspec,
    Word16 *Dvec,
    Word16 *Qspare,
    Word16 *Qreservplus,
    Word16 *Dspecplus
)
{

    Word16 Minit, Mprime;
    Word16 Qtestminus;
    const unsigned char *frQuantaL;

    frQuantaL    = hBitsN[L];

    *Qreservplus = add(Qreserv, sub(Qvec,  QUANTAQ3OFFSET));

    dsDiracPerQuanta_fx(L, Qvec, Fcons, hBitsN, &Minit);

    Mprime = Minit;
    move16();
    DO
    {
        Qtestminus = (short)frQuantaL[Mprime];
        move16();
        *Qspare    = sub(Qavail, Qtestminus);
        Mprime     = sub(Mprime, 1);
    }
    WHILE ( (Mprime >= 0) && sub(*Qspare, QUANTAQ3OFFSET ) < 0 );

    if(Mprime < 0)
    {
        *Qspare =  add(Qavail, QUANTAQ3OFFSET); /* single op */
    }
    dsDirac2Dirac_fx(add(Mprime, 1), Dvec);

    *Dspecplus   = add(Dspec, *Dvec);
    *Qreservplus = sub(*Qreservplus, (short)frQuantaL[Minit]);
    *Qspare      = sub(*Qspare, QUANTAQ3OFFSET);

    return;
}





void bandBitsAdjustment_fx(
    Word16 Brc,
    UWord32 INTrc,
    Word16 Bavail,
    Word16 Nbands,
    Word16 D,
    Word16 L,
    Word16 Bband,
    Word16 Breserv,
    Word16 *Bband_adj,
    Word16 *Brem,
    Word16 *Breservplus)
{
    Word16 Btemp;
    Word16 Bff;
    Word32 L_tmp;

    rangeCoderFinalizationFBits_fx(Brc, INTrc, &Bff);

    IF(sub(D, Nbands) < 0)
    {
        L_tmp        =  L_deposit_l(sub(Breserv, Bff));
        Btemp        =  extract_l(intLimCDivSigned_fx(L_tmp, s_min(D, 3)));  /* result always fits in Word16 */
        *Breservplus =  add(Bband, Breserv);
    }
    ELSE
    {
        Btemp       = 0;
        move16();
        *Breservplus = add(Bband, Bff);
    }
    *Bband_adj = s_min(extract_l(L_mult(L, 40)), Bband);
    *Brem      = sub(Bavail, Bff);
    *Bband_adj = s_min(*Brem, add(*Bband_adj, Btemp));
    *Bband_adj = s_max(0, *Bband_adj);
}



static   Word16  Ratio_base2Q11_fx(             /* o :   Q11 */
    const Word16 opp,      /* i :        Q15 */
    const Word16 near       /* i :        Q15 */
)
{
    Word16 mc, nc, ms, ns, d, z;
    Word16 result;
    Word32 acc;

    ns = norm_s(opp );    /* exponent */
    nc = norm_s(near );    /* exponent */

    ms  = shl(opp, ns);   /* mantissa */
    mc  = shl(near, nc);   /* mantissa */

    acc = L_mac(538500224L, mc, -2776);  /* a0*mc + a1, acc(Q27), a0(Q11), a1(Q27) */
    z = mac_r(acc, ms, -2776);           /* z in Q11, a0 in Q11 */
    d = sub(ms, mc);                     /* d in Q15 */
    z = mult_r(z, d);                    /* z in Q11 */

    result = add(z, shl(sub(nc, ns), 11));

    return result;
}

void  Ratio_rQ3_fx(
    Word16  opp,
    Word16  near,
    Word16  *result
)
{
    Word16 tmp;

    tmp     = add(1<<7 , Ratio_base2Q11_fx(opp, near));
    *result = shr(tmp, 8);
    return ;
}


void densityAngle2RmsProjDec_fx(
    Word16 D,
    Word16 indexphi,
    Word16 *oppQ15,
    Word16 *nearQ15,
    Word16 *oppRatioQ3
)
{
    Word16 phiQ14q;
    Word16 oppTail, nearTail;

    phiQ14q = (Word16)intLimCDivPos_fx(L_shl(L_deposit_l(indexphi), 13), shr(D, 1));
    if (indexphi < 0)
    {
        phiQ14q = 1 << 13;  /* one op */ move16();
    }

    oppTail  = shr(sub(16320, phiQ14q), 15);
    nearTail = shr(sub(phiQ14q, 64), 15);

    IF (s_or(oppTail, nearTail) < 0)
    {
        *oppQ15     = s_and(oppTail, (1 << 15) - 1);
        *nearQ15    = s_and(nearTail, (1 << 15) - 1);
        *oppRatioQ3 = shl(add(1, shl(nearTail, 1)), 14);
    }
    ELSE
    {
        nearProjQ15_fx( shl(sub(1 << 14, phiQ14q), 1), oppQ15);
        nearProjQ15_fx(shl(phiQ14q, 1), nearQ15);
        Ratio_rQ3_fx(*oppQ15, *nearQ15, oppRatioQ3);
    }

    return;
}

void densityAngle2RmsProjEnc_fx(
    Word16 D,
    Word16 phiQ14uq,
    Word16 *indexphi,
    Word16 *oppQ15,
    Word16 *nearQ15,
    Word16 *oppRatioQ3
)
{
    *indexphi = mult_r(shl(D, 1), phiQ14uq);
    if (s_and(D, 1) > 0)
    {
        *indexphi = -1; /* one op */  move16();
    }
    densityAngle2RmsProjDec_fx(D, *indexphi, oppQ15, nearQ15, oppRatioQ3);

    return;
}

void NearOppSplitAdjustment_fx(
    const Word16 qband,
    const Word16 qzero,
    const Word16 Qac,
    const UWord32 INTac,
    const Word16 qglobal,
    const Word16 FlagCons,
    const Word16 Np,
    const Word16 Nhead,
    const Word16 Ntail,
    const Word16 Nnear,
    const Word16 Nopp,
    Word16 oppRQ3,
    Word16 *qnear,
    Word16 *qopp,
    Word16 *qglobalupd
)
{

    Word16 qac, qboth, qskew, qavg, qmin, Midx;
    Word32 L_QIb, L_qnum;
    Word16 QIb, QIa;

    rangeCoderFinalizationFBits_fx(Qac, INTac, &qac);
    qboth = sub(qband, sub(qac, qzero));
    /* skew calc code  */
    qskew =  0 ;
    move16();
    IF (sub(Nhead, 1) > 0)
    {
        qavg  =  extract_h(L_shl(intLimCDivSigned_fx((Word32)qboth, Np),16));  /* qboth may be negative */
        dsDiracPerQuanta_fx(Ntail, qavg, FlagCons, hBitsN, &Midx );
        QuantaPerDsDirac_fx(Nhead, Midx, hBitsN, &qmin);
        qskew = sub(qavg, qmin);
        qskew = s_max(0, qskew);
    }   /* end of skew calc code*/

    QIa    = add(extract_l(intLimCDivPos_fx((UWord32)L_deposit_l(Nopp), Nnear)), 1); /* always positive Word16 out */
    L_qnum = L_sub( L_deposit_l(sub(sub(add(qband, qzero), qac), qskew)), L_mult0(Nopp, oppRQ3));

    L_QIb = L_deposit_l(0);
    IF (L_qnum > 0)
    {
        L_QIb = (Word32) intLimCDivPos_fx(L_qnum,  QIa);
    }
    *qnear =   qboth;
    QIb    =   extract_h(L_shl(L_QIb, 16));  /* may saturate */
    if (sub(QIb, qboth) <= 0)
    {
        *qnear = QIb;
    }
    *qopp       = sub(qboth, *qnear);
    *qglobalupd = sub(qglobal, sub(qac, qzero));

    return;
}


/*--------------------------------------------------------------------------*
 * apply_gain()
 *
 * Apply gain
 *--------------------------------------------------------------------------*/

void apply_gain_fx(
    const Word16 *ord,                       /* i  : Indices for energy order                       */
    const Word16 *band_start,                /* i  : Sub band start indices                         */
    const Word16 *band_end,                  /* i  : Sub band end indices                           */
    const Word16 num_sfm,                    /* i  : Number of bands                                */
    const Word16 *gains,                     /* i  : Band gain vector                       Q12     */
    Word16 *xq                         /* i/o: Float synthesis / Gain adjusted synth  Q15/Q12 */
)
{
    Word16 band,i;
    Word16 g;   /* Q12 */

    FOR ( band = 0; band < num_sfm; band++)
    {
        g = gains[ord[band]];

        FOR( i = band_start[band]; i < band_end[band]; i++)
        {
            /*xq[i] *= g; */
            xq[i] = mult_r(g, xq[i]);
            move16();   /*12+15+1-16=12 */
        }
    }

    return;
}

/*--------------------------------------------------------------------------*
 * fine_gain_quant()
 *
 * Fine gain quantization
 *--------------------------------------------------------------------------*/

void fine_gain_quant_fx(
    Encoder_State_fx *st_fx,
    const Word16 *ord,                       /* i  : Indices for energy order                     */
    const Word16 num_sfm,                    /* i  : Number of bands                              */
    const Word16 *gain_bits,                 /* i  : Gain adjustment bits per sub band            */
    Word16 *fg_pred,                   /* i/o: Predicted gains / Corrected gains        Q12 */
    const Word16 *gopt                       /* i  : Optimal gains                            Q12 */
)
{
    Word16 band;
    Word16 gbits;
    Word16 idx;
    Word16 gain_db,gain_dbq;
    Word16 err;

    Word16 tmp1, tmp2, exp1, exp2;
    Word32 L_tmp;
    UWord16 lsb;

    FOR ( band = 0; band < num_sfm; band++)
    {
        gbits = gain_bits[ord[band]];
        test();
        IF ( fg_pred[band] != 0 && gbits > 0 )
        {
            exp1 = norm_s(gopt[band]);
            exp1 = sub(exp1, 1);
            tmp1 = shl(gopt[band], exp1);
            exp2 = norm_s(fg_pred[band]);
            tmp2 = shl(fg_pred[band], exp2);
            exp1 = add(15, sub(exp1, exp2));
            err = div_s(tmp1, tmp2);
            tmp1 = norm_s(err);
            exp2 = Log2_norm_lc(L_deposit_h(shl(err, tmp1)));
            tmp1 = sub(14, tmp1);
            tmp1 = sub(tmp1, exp1);
            L_tmp = L_Comp(tmp1, exp2);
            Mpy_32_16_ss(L_tmp, 24660, &L_tmp, &lsb);   /* 24660 = 20*log10(2) in Q12 */ /*16+12-15=13 */
            gain_db = round_fx(L_shl(L_tmp, 17));

            idx = squant_fx(gain_db, &gain_dbq, finegain_fx[gbits-1], gain_cb_size[gbits-1]);
            push_indice_fx( st_fx, IND_PVQ_FINE_GAIN, idx, gbits );

            L_tmp = L_mult0(gain_dbq, 21771);   /* 21771=0.05*log2(10) */   /* 14+17=31 */
            L_tmp = L_shr(L_tmp, 15);
            tmp1 = L_Extract_lc(L_tmp, &exp1);
            tmp1 = abs_s(tmp1);
            tmp1 = extract_l(Pow2(14, tmp1));
            exp1 = sub(14, exp1);

            L_tmp = L_mult0(fg_pred[band], tmp1);   /*12+exp1 */
            fg_pred[band] = round_fx(L_shl(L_tmp, sub(16, exp1))); /*12+exp1+16-exp1-16=12 */
        }
    }

    return;
}

/*-------------------------------------------------------------------*
 * srt_vec_ind()
 *
 *  sort vector and save  sorting indeces
 *-------------------------------------------------------------------*/

void srt_vec_ind16_fx (
    const Word16 *linear,      /* linear input */
    Word16 *srt,         /* sorted output*/
    Word16 *I,           /* index for sorted output  */
    Word16 length
)
{
    Word16 pos,npos;
    Word16 idxMem;
    Word16 valMem;

    /*initilize */
    FOR (pos = 0; pos < length; pos++)
    {
        I[pos] = pos;
        move16();
    }

    Copy(linear, srt,length);

    /* now iterate */
    FOR (pos = 0; pos < (length - 1); pos++)
    {
        FOR (npos = (pos + 1); npos < length;  npos++)
        {
            IF (sub(srt[npos], srt[pos]) < 0)
            {
                idxMem    = I[pos];
                move16();
                I[pos]    = I[npos];
                move16();
                I[npos]   = idxMem;
                move16();

                valMem    = srt[pos];
                move16();
                srt[pos]  = srt[npos];
                move16();
                srt[npos] = valMem;
                move16();
            }
        }
    }

    return;
}

/*-----------------------------------------------------------------------------
 * atan2_fx():
 *
 * Approximates arctan piecewise with various 4th to 5th order least square fit
 * polynomials for input in 5 segments:
 *   - 0.0 to 1.0
 *   - 1.0 to 2.0
 *   - 2.0 to 4.0
 *   - 4.0 to 8.0
 *   - 8.0 to infinity
 *---------------------------------------------------------------------------*/
Word16 atan2_fx(  /* o: Angle between 0 and PI/2 radian (Q14) */
    const Word32 y,  /* i: Argument must be positive (Q15) */
    const Word32 x   /* i: Q15 */
)
{
    Word32 acc, arg;
    Word16 man, expo, reciprocal;
    Word16 angle, w, z;

    IF (x == 0)
    {
        return 25736; /* PI/2 in Q14 */
    }
    man = ratio(y, x, &expo); /* man in Q14 */
    expo = sub(expo, (15 - 14)); /* Now, man is considered in Q15 */
    arg = L_shr((Word32)man, expo);

    IF (L_shr(arg, 3+15) != 0)
    /*===============================*
     *      8.0 <= x < infinity      *
     *===============================*/
    {
        /* atan(x) = PI/2 - 1/x + 1/(3x^3) - 1/(5x^5) + ...
         *         ~ PI/2 - 1/x, for x >= 8.
         */
        expo = norm_l(arg);
        man = extract_h(L_shl(arg, expo));
        reciprocal = div_s(0x3fff, man);
        expo = sub(15 + 1, expo);
        reciprocal = shr(reciprocal, expo);   /* Q14 */
        angle = sub(25736, reciprocal);       /* Q14   (PI/2 - 1/x) */

        /* For 8.0 <= x < 10.0, 1/(5x^5) is not completely negligible.
         * For more accurate result, add very small correction term.
         */
        if (L_sub(L_shr(arg, 15), 10L) < 0)
        {
            angle = add(angle, 8); /* Add tiny correction term. */
        }
    }
    ELSE IF (L_shr(arg, 2+15) != 0)
    /*==========================*
     *      4.0 <= x < 8.0      *
     *==========================*/
    {
        /* interval: [3.999, 8.001]
         * atan(x) ~ (((a0*x     +   a1)*x   + a2)*x   + a3)*x   + a4
         *         = (((a0*8*y   +   a1)*8*y + a2)*8*y + a3)*8*y + a4   Substitute 8*y -> x
         *         = (((a0*8^3*y + a1*8^2)*y + a2*8)*y + a3)*8*y + a4
         *         = (((    c0*y +     c1)*y +   c2)*y + c3)*8*y + c4,
         *  where y = x/8
         *   and a0 = -1.28820869667651e-04, a1 = 3.88263533346295e-03,
         *       a2 = -4.64216306484597e-02, a3 = 2.75986060068931e-01,
         *       a4 = 7.49208077809799e-01.
         */
        w = extract_l(L_shr(arg, 3));              /* Q15  y = x/8 */
        acc = L_add(533625337L, 0);                /* Q31  c1 = a1*8^2 */
        z = mac_r(acc, w, -2161);                  /* Q15  c0 = a0*8^3 */
        acc = L_add(-797517542L, 0);               /* Q31  c2 = a2*8 */
        z = mac_r(acc, w, z);                      /* Q15 */
        acc = L_add(592675551L, 0);                /* Q31  c3 = a3 */
        z = mac_r(acc, w, z);                      /* z (in:Q15, out:Q12) */
        acc = L_add(201114012L, 0);                /* Q28  c4 = a4 */
        acc = L_mac(acc, w, z);                    /* Q28 */
        angle = extract_l(L_shr(acc, (28 - 14)));  /* Q14 result of atan(x), where 4 <= x < 8 */
    }
    ELSE IF (L_shr(arg, 1+15) != 0)
    /*==========================*
     *      2.0 <= x < 4.0      *
     *==========================*/
    {
        /* interval: [1.999, 4.001]
         * atan(x) ~ (((a0*x    + a1)*x   +   a2)*x   + a3)*x   + a4
         *         = (((a0*4*y  + a1)*4*y +   a2)*4*y + a3)*4*y + a4   Substitute 4*y -> x
         *         = (((a0*16*y + a1*4)*y +   a2)*4*y + a3)*4*y + a4
         *         = (((a0*32*y + a1*8)*y + a2*2)*2*y + a3)*4*y + a4
         *         = (((   c0*y +   c1)*y +   c2)*2*y + c3)*4*y + c4,
         *  where y = x/4
         *   and a0 = -0.00262378195660943, a1 = 0.04089687039888652,
         *       a2 = -0.25631148958325911, a3 = 0.81685854627399479,
         *       a4 = 0.21358070563097167
         * */
        w = extract_l(L_shr(arg, 2));              /* Q15  y = x/4 */
        acc = L_add(702602883L, 0);                /* Q31  c1 = a1*8 */
        z = mac_r(acc, w, -2751);                  /* Q15  c0 = a0*32 */
        acc = L_add(-1100849465L, 0);              /* Q31  c2 = a2*2 */
        z = mac_r(acc, w, z);                      /* z (in:Q15, out:Q14) */
        acc = L_add(877095185L, 0);                /* Q30  c3 = a3 */
        z = mac_r(acc, w, z);                      /* z (in:Q14, out:Q12) */
        acc = L_add(57332634L, 0);                 /* Q28  c4 = a4 */
        acc = L_mac(acc, w, z);                    /* Q28 */
        angle = extract_l(L_shr(acc, (28 - 14)));  /* Q14  result of atan(x) where 2 <= x < 4 */
    }
    ELSE IF (L_shr(arg, 15) != 0)
    /*==========================*
     *      1.0 <= x < 2.0      *
     *==========================*/
    {
        /* interval: [0.999, 2.001]
         * atan(x) ~ (((a0*x   +  1)*x   + a2)*x   +   a3)*x   + a4
         *         = (((a0*2*y + a1)*2*y + a2)*2*y +   a3)*2*y + a4    Substitute 2*y -> x
         *         = (((a0*4*y + a1*2)*y + a2)*2*y +   a3)*2*y + a4
         *         = (((a0*4*y + a1*2)*y + a2)*y   + a3/2)*4*y + a4
         *         = (((  c0*y +   c1)*y + c2)*y   +   c3)*4*y + c4,
         *  where y = x/2
         *   and a0 = -0.0160706457245251, a1 = 0.1527106504065224,
         *       a2 = -0.6123208404800871, a3 = 1.3307896976322915,
         *       a4 = -0.0697089375247448
         */
        w = extract_l(L_shr(arg, 1));              /* Q15  y= x/2 */
        acc = L_add(655887249L, 0);                /* Q31  c1 = a1*2 */
        z = mac_r(acc, w, -2106);                  /* Q15  c0 = a0*4 */
        acc = L_add(-1314948992L, 0);              /* Q31  c2 = a2 */
        z = mac_r(acc, w, z);
        acc = L_add(1428924557L, 0);               /* Q31  c3 = a3/2 */
        z = mac_r(acc, w, z);                      /* z (in:Q15, out:Q13) */
        acc = L_add(-37424701L, 0);                /* Q29  c4 = a4 */
        acc = L_mac(acc, w, z);                    /* Q29 */
        angle = extract_l(L_shr(acc, (29 - 14)));  /* Q14  result of atan(x) where 1 <= x < 2 */
    }
    ELSE
    /*==========================*
     *      0.0 <= x < 1.0      *
     *==========================*/
    {
        /* interval: [-0.001, 1.001]
         * atan(x) ~ ((((a0*x   +   a1)*x   + a2)*x + a3)*x + a4)*x + a5
         *         = ((((a0*2*x + a1*2)*x/2 + a2)*x + a3)*x + a4)*x + a5
         *         = ((((  c0*x +   c1)*x/2 + c2)*x + c3)*x + c4)*x + c5
         *  where
         *    a0 = -5.41182677118661e-02, a1 = 2.76690449232515e-01,
         *    a2 = -4.63358392562492e-01, a3 = 2.87188466598566e-02,
         *    a4 =  9.97438122814383e-01, a5 = 5.36158556179092e-05.
         */
        w = extract_l(arg);                         /* Q15 */
        acc = L_add(1188376431L, 0);                /* Q31  c1 = a1*2 */
        z = mac_r(acc, w, -3547);                   /* Q15  c0 = a0*2 */
        acc = L_add(-995054571L, 0);                /* Q31  c2 = a2 */
        z = extract_h(L_mac0(acc, w, z));           /* Q15  non-fractional mode multiply */
        acc = L_add(61673254L, 0);                  /* Q31  c3 = a3 */
        z = mac_r(acc, w, z);
        acc = L_add(2141982059L, 0);                /* Q31  c4 = a4 */
        z = mac_r(acc, w, z);
        acc = L_add(115139L, 0);                    /* Q31  c5 = a5 */
        acc = L_mac(acc, w, z);                     /* Q31 */
        angle = extract_l(L_shr(acc, 31 - 14));     /* Q14  result of atan(x), where 0 <= x < 1 */
    }

    return angle;  /* Q14 between 0 and PI/2 radian. */
}



