/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/


#include "options.h"     /* Compilation switches                   */
#include "cnst_fx.h"       /* Common constants                       */
#include "rom_com_fx.h"    /* Static table prototypes                */
#include "prot_fx.h"       /* Function prototypes                    */
#include "stl.h"

/*-------------------------------------------------------------------*
 * FEC_lsf_estim()
 *
 * - LSP calculation
 * - A(z) calculation
 *-------------------------------------------------------------------*/

void FEC_lsf2lsp_interp(
    Decoder_State_fx *st,              /* i/o: Decoder static memory                        */
    const Word16 L_frame,          /* i  : length of the frame                          */
    Word16 *Aq,              /* o  : calculated A(z) for 4 subframes              */
    Word16 *lsf,             /* o  : estimated LSF vector                         */
    Word16 *lsp              /* o  : estimated LSP vector                         */
)
{

    /* convert LSFs to LSPs */
    IF ( st->Opt_AMR_WB_fx )
    {
        E_LPC_isf_isp_conversion( lsf, lsp, M);
    }
    ELSE
    {
        IF( sub(L_frame,L_FRAME) == 0 )
        {
            lsf2lsp_fx( lsf, lsp, M, INT_FS_FX );
        }
        ELSE /* L_frame == L_FRAME16k */
        {
            lsf2lsp_fx( lsf, lsp, M, INT_FS_16k_FX);
        }
    }

    /*----------------------------------------------------------------------*
     * Interpolate LSP vector and find A(z)
     *----------------------------------------------------------------------*/

    IF ( st->Opt_AMR_WB_fx )
    {
        int_lsp_fx( L_frame, st->lsp_old_fx, lsp, Aq, M, interpol_isp_amr_wb_fx, 1 );
    }
    ELSE
    {
        int_lsp_fx( L_frame, st->lsp_old_fx, lsp, Aq, M, interpol_frac_fx, 0 );
    }

    return;
}
