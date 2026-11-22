#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL3/SDL.h>

#include "game_sites.h"
#include "hle.h"
#include "input_bind.h"
#include "language.h"
#include "nvn/glyphs.h"
#include "settings.h"

const TextLanguage g_text_languages[] = {{"Auto", NULL, NULL}};
const VoiceLanguage g_voice_languages[] = {{"Auto", NULL}};
const int g_ntext_languages = 1, g_nvoice_languages = 1;
GameSites g_site;
bool rt_resource(const char *name, const char *dev_rel, char *out, size_t n) {
    (void)name, (void)dev_rel, (void)out, (void)n;
    return false;
}
int input_pad_family(void) { return 0; }
int input_last_device(void) { return 0; }
bool pc_cursor_preview_active(void) { return false; }
bool pc_cursor_sensor_active(void) { return false; }
bool fsr3_upscaler_enabled(void) { return false; }
float winsize_refresh_rate(void) { return 0; }

static size_t append_sec(uint8_t *b, size_t o, const char tag[4], const void *body, uint32_t body_sz) {
    uint32_t sz = 8 + body_sz;
    memcpy(b + o, tag, 4);
    memcpy(b + o + 4, &sz, 4);
    memcpy(b + o + 8, body, body_sz);
    return o + sz;
}

static size_t append_pane(uint8_t *b, size_t o, const char tag[4], const char *name, float x, float y, float rz, float sx) {
    uint8_t body[96] = {0};
    strncpy((char *)body + 4, name, 24);
    float sy = 1.0f, w = 48.0f, h = 48.0f;
    memcpy(body + 36, &x, 4);
    memcpy(body + 40, &y, 4);
    memcpy(body + 56, &rz, 4);
    memcpy(body + 60, &sx, 4);
    memcpy(body + 64, &sy, 4);
    memcpy(body + 68, &w, 4);
    memcpy(body + 72, &h, 4);
    return append_sec(b, o, tag, body, sizeof body);
}

static size_t append_dim_txt(uint8_t *b, size_t o, const char *name, char letter, float x, float y) {
    uint8_t body[0x70] = {0};
    strncpy((char *)body + 4, name, 24);
    memcpy(body + 36, &x, 4);
    memcpy(body + 40, &y, 4);
    float one = 1.0f, fw = 20.4f, fh = 25.5f;
    memcpy(body + 60, &one, 4);
    memcpy(body + 64, &one, 4);
    uint32_t toff = 0x6C;
    memcpy(body + 0x5C, &toff, 4);
    memcpy(body + 0x68, &fw, 4);
    memcpy(body + 0x6C, &fh, 4);
    uint8_t full[0x78] = {0};
    memcpy(full, body, sizeof body);
    full[0x64] = (uint8_t)letter;
    return append_sec(b, o, "txt1", full, sizeof full);
}

static size_t build_synthetic_padocommand(uint8_t *b, int which) {
    memset(b, 0, 2048);
    memcpy(b, "FLYT\xFF\xFE\x14\x00\x00\x02\x08\x05", 12);
    size_t o = 0x14;
    uint8_t txl[40] = {0};
    txl[0] = 1;
    uint32_t toff = 8;
    memcpy(txl + 4, &toff, 4);
    strcpy((char *)txl + 8, (which == 0 || which == 3) ? "Nt_KeyBtnSubBaseAL_00^s" : "Nt_KeyBtnSubBaseBL_00^s");
    o = append_sec(b, o, "txl1", txl, sizeof txl);
    uint8_t fnl[28] = {0};
    fnl[0] = 1;
    uint32_t foff = 8;
    memcpy(fnl + 4, &foff, 4);
    strcpy((char *)fnl + 8, "Normal_00.bffnt");
    o = append_sec(b, o, "fnl1", fnl, sizeof fnl);
    o = append_pane(b, o, "pic1", "P_Base_00", 0.0f, 0.0f, (which == 2 || which == 3) ? 180.0f : 0.0f,
                    which == 2 ? -1.0f : 1.0f);
    static const char letters[4] = {'A', 'B', 'X', 'Y'};
    static const char *const tnames[4] = {"T_A_00", "T_B_00", "T_X_00", "T_Y_00"};
    static const float dim_xy[4][2] = {{26.2f, 0.8f}, {1.0f, -26.0f}, {0.8f, 26.0f}, {-25.5f, -0.2f}};
    for (int i = 0; i < 4; i++)
        if (i != which) o = append_dim_txt(b, o, tnames[i], letters[i], dim_xy[i][0], dim_xy[i][1]);
    static const float act_xy[4][2] = {{26.0f, 0.0f}, {0.0f, -26.0f}, {0.0f, 26.0f}, {-26.0f, 0.0f}};
    o = append_pane(b, o, "pan1", "N_BtnA_00", act_xy[which][0], act_xy[which][1], 0.0f, 1.0f);
    o = append_pane(b, o, "wnd1", "W_Base_00", which == 3 ? 33.0f : -33.0f, 0.0f, 0.0f, which == 3 ? -1.0f : 1.0f);
    o = append_pane(b, o, "txt1", "T_Guide_01", which == 3 ? -23.0f : 21.0f, 0.0f, 0.0f, 1.0f);
    o = append_pane(b, o, "pic1", "P_CaptureUse", which == 3 ? -60.0f : 60.0f, 0.0f, 0.0f, 1.0f);
    o = append_pane(b, o, "pan1", "N_Capture_00", act_xy[which][0] + (which == 3 ? -60.0f : 60.0f), act_xy[which][1], 0.0f, 1.0f);
    o = append_pane(b, o, "txt1", "T_Guide_00", which == 3 ? 37.0f : -39.0f, 0.0f, 0.0f, 1.0f);
    o = append_pane(b, o, "pic1", "Nt_KeyTexA_01", which == 3 ? 60.0f : -60.0f, 0.0f, 0.0f, 1.0f);
    o = append_pane(b, o, "pic1", "P_CaptureUse_01", act_xy[which][0] + (which == 3 ? -60.0f : 60.0f), act_xy[which][1], 0.0f, 1.0f);
    uint32_t total = (uint32_t)o;
    memcpy(b + 0xC, &total, 4);
    return o;
}

static size_t build_synthetic_transparent_guides_flyt(uint8_t *b) {
    memset(b, 0, 2048);
    memcpy(b, "FLYT\xFF\xFE\x14\x00\x00\x02\x08\x05", 12);
    size_t o = 0x14;
    uint8_t fnl[28] = {0};
    fnl[0] = 1;
    uint32_t foff = 8;
    memcpy(fnl + 4, &foff, 4);
    strcpy((char *)fnl + 8, "Normal_00.bffnt");
    o = append_sec(b, o, "fnl1", fnl, sizeof fnl);
    o = append_pane(b, o, "pan1", "N_Btn", 26.0f, 0.0f, 0.0f, 1.0f);
    static const struct { const char *tag; const char *name; } panes[] = {
        {"pic1", "P_Base_00"},
        {"wnd1", "W_Base_00"},
        {"pic1", "P_Icon_00"},
        {"pic1", "P_Capture_00"},
        {"pic1", "P_Bg_00"},
        {"pic1", "P_Black_00"},
        {"wnd1", "W_Bg_00"},
    };
    for (size_t i = 0; i < sizeof panes / sizeof panes[0]; i++) {
        size_t off = o;
        o = append_pane(b, o, panes[i].tag, panes[i].name, 0.0f, 0.0f, 0.0f, 1.0f);
        b[off + 0x0A] = 0xFF;
    }
    o = append_dim_txt(b, o, "T_A_00", 'A', 26.2f, 0.8f);
    uint32_t total = (uint32_t)o;
    memcpy(b + 0xC, &total, 4);
    return o;
}

static const uint8_t *find_pane(const uint8_t *b, size_t n, const char *name) {
    size_t o = b[6] | b[7] << 8;
    while (o + 8 <= n) {
        uint32_t sz;
        memcpy(&sz, b + o + 4, 4);
        if (sz < 8 || o + sz > n) break;
        if (sz >= 84 && (!memcmp(b + o, "pan1", 4) || !memcmp(b + o, "pic1", 4) ||
                         !memcmp(b + o, "wnd1", 4) || !memcmp(b + o, "txt1", 4))) {
            if (!strncmp((const char *)b + o + 12, name, 24)) return b + o;
        }
        o += sz;
    }
    return NULL;
}

static bool nearf(float a, float b) { return fabsf(a - b) < 1e-4f; }

int main(void) {
    // Legacy savagewind.ini load (with obsolete motion_vectors = objects) + save round-trip
    const char *tmp = "build/test_hud_button_layout.ini";
    setenv("SAVAGEWIND_SETTINGS", tmp, 1);
    {
        FILE *fp = fopen(tmp, "w");
        assert(fp != NULL);
        fprintf(fp, "settings_version = 4\ntransparent_guides = 1\nmotion_vectors = objects\nbutton_layout = gamecube\n");
        fclose(fp);
        g_settings.transparent_guides = false;
        g_settings.button_layout = BUTTON_LAYOUT_AUTO;
        settings_load();
        assert(g_settings.transparent_guides == true);
        assert(g_settings.button_layout == BUTTON_LAYOUT_GAMECUBE);
        settings_save();
        fp = fopen(tmp, "r");
        assert(fp != NULL);
        char buf[8192] = {0};
        fread(buf, 1, sizeof buf - 1, fp);
        fclose(fp);
        unlink(tmp);
        assert(strstr(buf, "button_layout = gamecube") != NULL);
        assert(strstr(buf, "transparent_guides = 1") != NULL);
    }

    bind_reset(BIND_DEV_PAD);
    bind_reset(BIND_DEV_KEY);

    int pos[4];
    g_settings.button_layout = BUTTON_LAYOUT_AUTO;
    glyphs_hud_positions(pos);
    assert(pos[0] == 0 && pos[1] == 1 && pos[2] == 2 && pos[3] == 3);

    // Automatic follows g_bind_pad swaps (e.g. PS5 Cross / Circle swapped)
    g_bind_pad[BIND_A] = SDL_GAMEPAD_BUTTON_SOUTH;
    g_bind_pad[BIND_B] = SDL_GAMEPAD_BUTTON_EAST;
    glyphs_hud_positions(pos);
    assert(pos[0] == 1 && pos[1] == 0 && pos[2] == 2 && pos[3] == 3);

    // Automatic also follows per-action swaps when g_bind_pad is at default
    bind_reset(BIND_DEV_PAD);
    g_act_pad[0] = SDL_GAMEPAD_BUTTON_SOUTH;   // Interact
    g_act_pad[14] = SDL_GAMEPAD_BUTTON_EAST;   // Cancel
    glyphs_hud_positions(pos);
    assert(pos[0] == 1 && pos[1] == 0 && pos[2] == 2 && pos[3] == 3);
    bind_reset(BIND_DEV_PAD);

    // Explicit presets
    g_settings.button_layout = BUTTON_LAYOUT_SWAP_AB;
    glyphs_hud_positions(pos);
    assert(pos[0] == 1 && pos[1] == 0 && pos[2] == 2 && pos[3] == 3);

    g_settings.button_layout = BUTTON_LAYOUT_XBOX;
    glyphs_hud_positions(pos);
    assert(pos[0] == 1 && pos[1] == 0 && pos[2] == 3 && pos[3] == 2);

    g_settings.button_layout = BUTTON_LAYOUT_SWAP_XY;
    glyphs_hud_positions(pos);
    assert(pos[0] == 0 && pos[1] == 1 && pos[2] == 3 && pos[3] == 2);

    g_settings.button_layout = BUTTON_LAYOUT_GAMECUBE;
    glyphs_hud_positions(pos);
    assert(pos[0] == 1 && pos[1] == 3 && pos[2] == 0 && pos[3] == 2);

    // Patch synthetic PaDoCommandA/B/X/Y_00 under GameCube layout (A=South, B=West, X=East, Y=North)
    int panes = 0;
    for (int w = 0; w < 4; w++) {
        uint8_t src[2048], dst[2112];
        size_t sn = build_synthetic_padocommand(src, w);
        size_t dn = glyphs_patch_hud_flyt(src, sn, dst, w, pos, false, &panes);
        assert(dn > sn);
        const uint8_t *btn = find_pane(dst, dn, "N_BtnA_00");
        const uint8_t *base = find_pane(dst, dn, "P_Base_00");
        const uint8_t *wbase = find_pane(dst, dn, "W_Base_00");
        assert(btn && base && wbase);
        float bx, by, rz, sx, wx, wsx;
        memcpy(&bx, btn + 44, 4);
        memcpy(&by, btn + 48, 4);
        memcpy(&rz, base + 64, 4);
        memcpy(&sx, base + 68, 4);
        memcpy(&wx, wbase + 44, 4);
        memcpy(&wsx, wbase + 68, 4);
        int act = pos[w];
        static const float exp_act[4][2] = {{26.0f, 0.0f}, {0.0f, -26.0f}, {0.0f, 26.0f}, {-26.0f, 0.0f}};
        assert(nearf(bx, exp_act[act][0]) && nearf(by, exp_act[act][1]));
        assert(nearf(rz, (act == 2 || act == 3) ? 180.0f : 0.0f));
        assert(nearf(sx, act == 2 ? -1.0f : 1.0f));
        assert(nearf(wx, act == 3 ? 33.0f : -33.0f));
        assert(nearf(wsx, act == 3 ? -1.0f : 1.0f));
    }
    assert(panes == 12);

    // Transparent control guides FLYT alpha zeroing
    {
        uint8_t src[2048], dst_on[2112], dst_off[2112];
        size_t sn = build_synthetic_transparent_guides_flyt(src);
        int def_pos[4] = {0, 1, 2, 3};
        int p_on = 0, p_off = 0;

        size_t dn_on = glyphs_patch_hud_flyt(src, sn, dst_on, 0, def_pos, true, &p_on);
        assert(dn_on > sn && p_on == 1);
        assert(find_pane(dst_on, dn_on, "P_Base_00")[0x0A] == 0);
        assert(find_pane(dst_on, dn_on, "W_Base_00")[0x0A] == 0);
        assert(find_pane(dst_on, dn_on, "P_Capture_00")[0x0A] == 0);
        assert(find_pane(dst_on, dn_on, "P_Bg_00")[0x0A] == 0);
        assert(find_pane(dst_on, dn_on, "P_Black_00")[0x0A] == 0);
        assert(find_pane(dst_on, dn_on, "W_Bg_00")[0x0A] == 0);
        assert(find_pane(dst_on, dn_on, "P_Icon_00")[0x0A] == 0xFF);

        size_t dn_off = glyphs_patch_hud_flyt(src, sn, dst_off, 0, def_pos, false, &p_off);
        assert(dn_off > sn && p_off == 1);
        assert(find_pane(dst_off, dn_off, "P_Base_00")[0x0A] == 0xFF);
        assert(find_pane(dst_off, dn_off, "W_Base_00")[0x0A] == 0xFF);
        assert(find_pane(dst_off, dn_off, "P_Capture_00")[0x0A] == 0xFF);
        assert(find_pane(dst_off, dn_off, "P_Bg_00")[0x0A] == 0xFF);
        assert(find_pane(dst_off, dn_off, "P_Black_00")[0x0A] == 0xFF);
        assert(find_pane(dst_off, dn_off, "W_Bg_00")[0x0A] == 0xFF);
        assert(find_pane(dst_off, dn_off, "P_Icon_00")[0x0A] == 0xFF);
    }

    // Boot state tracking for HUD restart detection
    {
        g_settings.button_layout = BUTTON_LAYOUT_XBOX;
        g_settings.transparent_guides = false;
        glyphs_hud_positions(pos);
        assert(!glyphs_hud_restart_needed());
        glyphs_hud_note_boot(pos, false);
        assert(!glyphs_hud_restart_needed());
        g_settings.transparent_guides = true;
        assert(glyphs_hud_restart_needed());
        g_settings.transparent_guides = false;
        assert(!glyphs_hud_restart_needed());
        g_settings.button_layout = BUTTON_LAYOUT_SWAP_AB;
        assert(glyphs_hud_restart_needed());
    }

    puts("PASS: HUD button layout, transparent guides, FLYT patching, and settings");
    return 0;
}
