#ifndef debug_font_H
#define debug_font_H

#include <stdint.h>

/* Windows code page 437 box drawing characters */
#define  BOX_DLR      "\315"  /* ═ */
#define  BOX_DUD      "\272"  /* ║ */
#define  BOX_DUL      "\274"  /* ╝ */
#define  BOX_DUR      "\310"  /* ╚ */
#define  BOX_DDL      "\273"  /* ╗ */
#define  BOX_DDR      "\311"  /* ╔ */
#define  BOX_DUDL     "\271"  /* ╣ */
#define  BOX_DUDR     "\314"  /* ╠ */
#define  BOX_DULR     "\312"  /* ╩ */
#define  BOX_DDLR     "\313"  /* ╦ */
#define  BOX_DUDLR    "\316"  /* ╬ */
#define  BOX_DU_SL    "\275"  /* ╜, not in CP850 */
#define  BOX_DU_SR    "\323"  /* ╙, not in CP850 */
#define  BOX_DD_SL    "\267"  /* ╖, not in CP850 */
#define  BOX_DD_SR    "\326"  /* ╓, not in CP850 */
#define  BOX_DL_SU    "\276"  /* ╛, not in CP850 */
#define  BOX_DL_SD    "\270"  /* ╕, not in CP850 */
#define  BOX_DR_SU    "\324"  /* ╘, not in CP850 */
#define  BOX_DR_SD    "\325"  /* ╒, not in CP850 */
#define  BOX_DU_SLR   "\320"  /* ╨, not in CP850 */
#define  BOX_DD_SLR   "\322"  /* ╥, not in CP850 */
#define  BOX_DL_SUD   "\265"  /* ╡, not in CP850 */
#define  BOX_DR_SUD   "\306"  /* ╞, not in CP850 */
#define  BOX_DLR_SU   "\317"  /* ╧, not in CP850 */
#define  BOX_DLR_SD   "\321"  /* ╤, not in CP850 */
#define  BOX_DLR_SUD  "\330"  /* ╪, not in CP850 */
#define  BOX_DUD_SL   "\266"  /* ╢, not in CP850 */
#define  BOX_DUD_SR   "\307"  /* ╟, not in CP850 */
#define  BOX_DUD_SLR  "\327"  /* ╫, not in CP850 */
#define  BOX_SLR      "\304"  /* ─ */
#define  BOX_SUD      "\263"  /* │ */
#define  BOX_SUL      "\331"  /* ┘ */
#define  BOX_SUR      "\300"  /* └ */
#define  BOX_SDL      "\277"  /* ┐ */
#define  BOX_SDR      "\332"  /* ┌ */
#define  BOX_SULR     "\301"  /* ┴ */
#define  BOX_SDLR     "\302"  /* ┬ */
#define  BOX_SUDL     "\264"  /* ┤ */
#define  BOX_SUDR     "\303"  /* ├ */
#define  BOX_SUDLR    "\305"  /* ┼ */

void dbgLoadFont();
int dbgAppendMessage(const char* fmt, ...);
void dbgUpdateMessage(int line, const char* fmt, ...);
void dbgSetStatusLine(const char* fmt, ...);
void dbgSetPixelRatio(float scale);
void dbgSetInvert(bool inv);
void dbgToggleInvert();
void dbgFlush();

#endif // of debug_font_H
