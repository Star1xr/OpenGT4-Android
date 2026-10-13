/*
 * EE MMI (128-bit multimedia) instructions for recompiled code. See mmi.c.
 * Register arguments are GPR numbers.
 */
#ifndef OPENGT4_EE_MMI_H
#define OPENGT4_EE_MMI_H

#include "ee.h"

void ee_mmi_paddb(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_paddh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_paddw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_psubb(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_psubh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_psubw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_paddsb(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_paddsh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_paddsw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_psubsb(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_psubsh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_psubsw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_paddub(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_padduh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_psubub(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_psubuh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pmaxh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pminh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pmaxw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pminw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pceqb(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pceqh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pceqw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pcgtb(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pcgth(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pcgtw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pnor(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pabsw(ee_ctx *c, int rd, int rt);
void ee_mmi_pabsh(ee_ctx *c, int rd, int rt);
void ee_mmi_psllh(ee_ctx *c, int rd, int rt, int sa);
void ee_mmi_psrlh(ee_ctx *c, int rd, int rt, int sa);
void ee_mmi_psrah(ee_ctx *c, int rd, int rt, int sa);
void ee_mmi_psllw(ee_ctx *c, int rd, int rt, int sa);
void ee_mmi_psrlw(ee_ctx *c, int rd, int rt, int sa);
void ee_mmi_psraw(ee_ctx *c, int rd, int rt, int sa);
void ee_mmi_pextlw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pextuw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pextlh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pextuh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pextlb(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pextub(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_ppacb(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_ppach(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_ppacw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_ppac5(ee_ctx *c, int rd, int rt);
void ee_mmi_pext5(ee_ctx *c, int rd, int rt);
void ee_mmi_prevh(ee_ctx *c, int rd, int rt);
void ee_mmi_prot3w(ee_ctx *c, int rd, int rt);
void ee_mmi_pexew(ee_ctx *c, int rd, int rt);
void ee_mmi_pexcw(ee_ctx *c, int rd, int rt);
void ee_mmi_pinteh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pmfhi(ee_ctx *c, int rd);
void ee_mmi_pmflo(ee_ctx *c, int rd);
void ee_mmi_pmthi(ee_ctx *c, int rs);
void ee_mmi_pmtlo(ee_ctx *c, int rs);
void ee_mmi_pmfhl_lw(ee_ctx *c, int rd);
void ee_mmi_pmfhl_uw(ee_ctx *c, int rd);
void ee_mmi_pmfhl_lh(ee_ctx *c, int rd);
void ee_mmi_pmulth(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pmaddh(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pmultw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_pmultuw(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_mult1(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_div1(ee_ctx *c, int rs, int rt);
void ee_mmi_divu1(ee_ctx *c, int rs, int rt);
void ee_mmi_madd(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_maddu(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_madd1(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_maddu1(ee_ctx *c, int rd, int rs, int rt);
void ee_mmi_mfsa(ee_ctx *c, int rd);
void ee_mmi_mtsa(ee_ctx *c, int rs);
void ee_mmi_mtsab(ee_ctx *c, int rs, uint32_t imm);
void ee_mmi_mtsah(ee_ctx *c, int rs, uint32_t imm);
void ee_mmi_qfsrv(ee_ctx *c, int rd, int rs, int rt);

#endif /* OPENGT4_EE_MMI_H */
