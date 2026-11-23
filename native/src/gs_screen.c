// gsKit_init_screen with its steps in the log and no wait without an end (EE only).
// This is that function and its gsKit_set_buffer_attributes, from gsKit's ee/gs/src/gsInit.c
// (https://github.com/ps2dev/gsKit, Academic Free License 2.0), with these differences:
//  - a VIDEO line after each step;
//  - the waits for the GIF channel and for the vertical blank give up after about a second, and say so;
//  - the board's display offsets (_GetGsDxDyOffset, for modes other than NTSC and PAL) are not asked for: the
//    picture may sit a few pixels off centre in 480p and 720p.
// Why: on the console a switch to 480p or 720p never gave a picture, and the library's function could not say
// where it stopped. The cause was outside: the loader the game was started with (Neutrino with -gsm=fp2, which
// shows every 480i game in 480p) replaces SetGsCrt, and its replacement ignores any mode but NTSC and PAL, so
// after the GS's reset no mode was set at all. love_gs.c's gs_forced() now sees that and love_video.c offers no
// modes then; the steps and the bounded waits stay for the next surprise.
#ifdef _EE
#include <stdio.h>

#include <gsKit.h>
#include <dmaKit.h>
#include <kernel.h>

#define TRIES 20000000L // polls of a register: about a second at the least

static void attributes(GSGLOBAL *g) {
    g->StartXOffset = 0;
    g->StartYOffset = 0;
    switch (g->Mode) {
    case GS_MODE_DTV_480P: g->StartX = 232, g->StartY = 35, g->DW = 1440, g->DH = 480; break;
    case GS_MODE_DTV_576P: g->StartX = 255, g->StartY = 44, g->DW = 1440, g->DH = 576; break;
    case GS_MODE_DTV_720P: g->StartX = 306, g->StartY = 24, g->DW = 1280, g->DH = 720; break;
    case GS_MODE_DTV_1080I: g->StartX = 236, g->StartY = 38, g->DW = 1920, g->DH = 1080; break;
    case GS_MODE_PAL: g->StartX = 520, g->StartY = 40, g->DW = 2880, g->DH = 576; break;
    default: g->StartX = 492, g->StartY = 34, g->DW = 2880, g->DH = 480; break; // NTSC
    }
    g->MagH = (g->DW / g->Width) - 1;
    g->MagV = (g->DH / g->Height) - 1;
    // Keep the framebuffer in the center of the screen
    g->StartX += (g->DW - ((g->MagH + 1) * g->Width)) / 2;
    g->StartY += (g->DH - ((g->MagV + 1) * g->Height)) / 2;
    if (g->Interlace == GS_INTERLACED) g->StartY &= ~1; // do not change the odd/even start position
    g->DW = (g->MagH + 1) * g->Width;
    g->DH = (g->MagV + 1) * g->Height;
    if (g->Interlace == GS_INTERLACED && g->Field == GS_FRAME) g->MagV--;
}

// 1: the GIF channel went idle
static int gif_idle(void) {
    volatile unsigned *chcr = (volatile unsigned *)0x1000A000; // D2_CHCR; bit 8: a transfer is running
    for (long i = 0; i < TRIES; i++)
        if (!(*chcr & 0x100)) return 1;
    return 0;
}
// 1: a vertical blank began
static int blank(void) {
    *GS_CSR = 8;
    for (long i = 0; i < TRIES; i++)
        if (*GS_CSR & 8) return 1;
    return 0;
}

void gs_screen_init(GSGLOBAL *g) {
    u64 *p, *store;
    int size = 19, dither = g->Dithering == GS_SETTING_ON && (g->PSM == GS_PSM_CT16 || g->PSM == GS_PSM_CT16S);
    if (dither) size++;
    attributes(g);
    printf("VIDEO display at %d,%d size %dx%d magnified %d,%d\n", g->StartX, g->StartY, g->DW, g->DH, g->MagH, g->MagV);
    // On the console (no loader forcing the mode) SetGsCrt for 480p, called as gsKit does right after the GS's
    // reset, changed the screen's mode and never came back, twice. A reset GS gives no vertical blank until a mode
    // is set, and the guess is that the kernel waits for one before it changes to a progressive mode. So for the
    // modes other than NTSC and PAL the GS is left running the mode it has (480i, with its blanks) and only asked
    // for the new one, as a game that changes to progressive from its own menu does.
    if (g->Mode == GS_MODE_NTSC || g->Mode == GS_MODE_PAL) {
        GS_RESET();
        __asm__("sync.p; nop;");
        *GS_CSR = 0x00000000;
        GsPutIMR(0x00007F00); // masks all the GS's interrupts
    }
    // Without the reset the console did change to 480p, and still the call did not return: the screen kept the
    // list's last picture (in 480i's display registers, so cut). What this program has that a plain one has not,
    // by the time a mode is chosen, is a handler on the vertical blank's interrupt (love_pad.c's, from the first
    // read of the pad). If the kernel waits for a blank by watching the interrupt's flag, a handler that is
    // served first takes the flag away every time. So both blank interrupts are off around the call.
    int start = DisableIntc(INTC_VBLANK_S), end = DisableIntc(INTC_VBLANK_E);
    SetGsCrt(g->Interlace, g->Mode, g->Field);
    if (start) EnableIntc(INTC_VBLANK_S);
    if (end) EnableIntc(INTC_VBLANK_E);
    EIntr();
    printf("VIDEO mode asked of the GS (blank interrupts were %s and %s)\n", start ? "on" : "off", end ? "on" : "off");
    g->FirstFrame = GS_SETTING_ON;
    if (g->ZBuffering == GS_SETTING_OFF) g->Test->ZTE = GS_SETTING_ON, g->Test->ZTST = 1;
    DIntr();
    GS_SET_PMODE(0, 1, 0, 1, 0, 0x80);
    GS_SET_DISPFB1(0, g->Width / 64, g->PSM, 0, 0);
    GS_SET_DISPFB2(0, g->Width / 64, g->PSM, 0, 0);
    GS_SET_DISPLAY1(g->StartX + g->StartXOffset, g->StartY + g->StartYOffset, g->MagH, g->MagV, g->DW - 1, g->DH - 1);
    GS_SET_DISPLAY2(g->StartX + g->StartXOffset, g->StartY + g->StartYOffset, g->MagH, g->MagV, g->DW - 1, g->DH - 1);
    GS_SET_BGCOLOR(g->BGColor->Red, g->BGColor->Green, g->BGColor->Blue);
    EIntr();
    printf("VIDEO display registers set\n");
    g->CurrentPointer = 0;
    g->ScreenBuffer[0] = gsKit_vram_alloc(g, gsKit_texture_size(g->Width, g->Height, g->PSM), GSKIT_ALLOC_SYSBUFFER);
    g->ScreenBuffer[1] = g->DoubleBuffering == GS_SETTING_OFF
                             ? g->ScreenBuffer[0]
                             : gsKit_vram_alloc(g, gsKit_texture_size(g->Width, g->Height, g->PSM), GSKIT_ALLOC_SYSBUFFER);
    if (g->ZBuffering == GS_SETTING_ON) g->ZBuffer = gsKit_vram_alloc(g, gsKit_texture_size(g->Width, g->Height, g->PSMZ), GSKIT_ALLOC_SYSBUFFER);
    g->TexturePointer = g->CurrentPointer;

    p = store = (u64 *)g->dma_misc;
    *p++ = GIF_TAG(size - 1, 1, 0, 0, GSKIT_GIF_FLG_PACKED, 1);
    *p++ = GIF_AD;
    *p++ = 1, *p++ = GS_PRMODECONT;
    for (int c = 0; c < 2; c++) { // the two drawing contexts
        *p++ = GS_SETREG_FRAME_1(g->ScreenBuffer[c] / 8192, g->Width / 64, g->PSM, 0), *p++ = c ? GS_FRAME_2 : GS_FRAME_1;
        *p++ = GS_SETREG_XYOFFSET_1(g->OffsetX, g->OffsetY), *p++ = c ? GS_XYOFFSET_2 : GS_XYOFFSET_1;
        *p++ = GS_SETREG_SCISSOR_1(0, g->Width - 1, 0, g->Height - 1), *p++ = c ? GS_SCISSOR_2 : GS_SCISSOR_1;
        *p++ = GS_SETREG_TEST(g->Test->ATE, g->Test->ATST, g->Test->AREF, g->Test->AFAIL, g->Test->DATE, g->Test->DATM, g->Test->ZTE, g->Test->ZTST);
        *p++ = c ? GS_TEST_2 : GS_TEST_1;
        *p++ = GS_SETREG_CLAMP(g->Clamp->WMS, g->Clamp->WMT, g->Clamp->MINU, g->Clamp->MAXU, g->Clamp->MINV, g->Clamp->MAXV);
        *p++ = c ? GS_CLAMP_2 : GS_CLAMP_1;
        if (g->ZBuffering == GS_SETTING_ON) *p++ = GS_SETREG_ZBUF_1(g->ZBuffer / 8192, g->PSMZ, 0);
        else *p++ = GS_SETREG_ZBUF_1(0, g->PSM, 1);
        *p++ = c ? GS_ZBUF_2 : GS_ZBUF_1;
        if (!c) *p++ = GS_SETREG_COLCLAMP(255), *p++ = GS_COLCLAMP;
    }
    *p++ = GS_BLEND_BACK2FRONT, *p++ = GS_ALPHA_1;
    *p++ = GS_BLEND_BACK2FRONT, *p++ = GS_ALPHA_2;
    *p++ = GS_SETREG_DIMX(g->DitherMatrix[0], g->DitherMatrix[1], g->DitherMatrix[2], g->DitherMatrix[3], g->DitherMatrix[4], g->DitherMatrix[5],
                          g->DitherMatrix[6], g->DitherMatrix[7], g->DitherMatrix[8], g->DitherMatrix[9], g->DitherMatrix[10], g->DitherMatrix[11],
                          g->DitherMatrix[12], g->DitherMatrix[13], g->DitherMatrix[14], g->DitherMatrix[15]);
    *p++ = GS_DIMX;
    *p++ = GS_SETREG_TEXA(0x00, 0, 0x80), *p++ = GS_TEXA;
    if (dither) *p++ = 1, *p++ = GS_DTHE;
    dmaKit_send_ucab(DMA_CHANNEL_GIF, store, size);
    printf("VIDEO drawing registers sent: %s\n", gif_idle() ? "taken" : "the GIF channel is still busy after a second");

    // a single frame, as gsKit does, to be done with whatever the first one needs
    gsKit_clear(g, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00));
    gsKit_queue_exec(g);
    printf("VIDEO first frame drawn\n");
    if (!blank()) printf("VIDEO no vertical blank within a second\n");
    gsKit_setactive(g); // (gsKit_sync_flip's other half; FirstFrame: no buffer swap yet)
    gsKit_queue_reset(g->Os_Queue);
    printf("VIDEO screen ready\n");
}
#endif

#ifdef _EE
// -DVIDEO_PROBE: a diagnostic start. At each stage of the platform's start the GS is asked for 480p and then for
// 480i again, with a line before and after: on the console SetGsCrt for 480p does not return once the mode list
// is on screen, while the same call made for a loader (Neutrino's GSM) at the first frame did. The last line of
// the log says after which stage it stops returning.
void gs_probe(const char *where) {
#ifdef VIDEO_PROBE
    printf("PROBE %s: asking for 480p\n", where);
    SetGsCrt(GS_NONINTERLACED, GS_MODE_DTV_480P, GS_FRAME);
    EIntr();
    printf("PROBE %s: 480p returned, asking for 480i\n", where);
    SetGsCrt(GS_INTERLACED, GS_MODE_NTSC, GS_FIELD);
    EIntr();
    printf("PROBE %s: 480i returned\n", where);
#endif
}
#endif
