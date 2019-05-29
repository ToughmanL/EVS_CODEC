/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include "options.h"     /* Compilation switches                   */
#include "cnst_fx.h"       /* Common constants                       */
#include "prot_fx.h"       /* Function prototypes                    */
#include "stl.h"

/*-------------------------------------------------------------------*
 * Enr_1_Az_fx_12Q3()
 *
 * Find Energy of the 1/A(z) impulse response
 *-------------------------------------------------------------------*/
Word16 Enr_1_Az_fx(         /* o  : impulse response energy      Q3  */
    const Word16 Aq[],      /* i  : LP filter coefs              Qx based on the fact that Aq[0] == 1.0 */
    const Word16 len        /* i  : impulse response length      Q0  */
)
{
    Word16 h1[2*L_SUBFR];
    Word16 *y;
    Word16 i, j, a0, q;
    Word32 L_tmp, L_tmp2;

    /* Find the impulse response */

    q = sub( 3, norm_s(Aq[0]) );
    a0 = shr(Aq[0], q); /* Q11 */
    q = sub(4, q);

    /*-----------------------------------------------------------------------*
    * Do the filtering (first two iters unrolled to avoid multiplies with 0)
    *-----------------------------------------------------------------------*/

    y = h1;
    /* h1_in Q11, h1_out Q10 */
    L_tmp = L_mult(a0, 1<<13); /* Q25 = L_mult(Q11,Q13) */
    *y = round_fx(L_tmp); /* Q25 to Q9 */
    L_tmp2 = L_mult(*y, *y); /* Q19 = L_mult(Q9,Q9) */
    y++;

    L_tmp = L_msu(0, Aq[1], y[-1]); /* Q23 = L_mult(Q14,Q9) */
    L_tmp = L_shl(L_tmp, q);
    *y = round_fx(L_tmp); /* Q25 to Q9 */
    L_tmp2 = L_mac(L_tmp2, *y, *y); /* Q19 = L_mult(Q9,Q9) */
    y++;

    /* Skip Zeros */
    FOR (i = 2; i < M; i++)
    {
        L_tmp = L_msu(0, Aq[1], y[-1]);
        FOR (j = 2; j <= i; j++)
        {
            L_tmp = L_msu(L_tmp, Aq[j], y[-j]);
        }

        L_tmp = L_shl(L_tmp, q);
        *y = round_fx(L_tmp);
        L_tmp2 = L_mac(L_tmp2, *y, *y);
        y++;
    }
    /* Normal Filtering */
    FOR (; i < len; i++)
    {
        L_tmp = L_msu(0, Aq[1], y[-1]);
        FOR (j = 2; j <= M; j++)
        {
            L_tmp = L_msu(L_tmp, Aq[j], y[-j]);
        }

        L_tmp = L_shl(L_tmp, q);
        *y = round_fx(L_tmp);
        L_tmp2 = L_mac(L_tmp2, *y, *y);
        y++;
    }

    return round_fx(L_tmp2); /* Q19 to Q3 */
}
