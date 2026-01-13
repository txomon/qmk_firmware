#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_split_5x6(
      KC_ESCAPE      , KC_1           , KC_2           , KC_3           , KC_4           , KC_5           ,                KC_6           , KC_7           , KC_8           , KC_9           , KC_0           , KC_MINUS       ,
      KC_GRAVE       , KC_Q           , KC_W           , KC_E           , KC_R           , KC_T           ,                KC_Y           , KC_U           , KC_I           , KC_O           , KC_P           , KC_LBRC        ,
      KC_TAB         , LSFT_T(KC_A)   , LCTL_T(KC_S)   , LALT_T(KC_D)   , LGUI_T(KC_F)   , KC_G           ,                RALT_T(KC_H)   , RGUI_T(KC_J)   , LALT_T(KC_K)   , RCTL_T(KC_L)   , RSFT_T(KC_SCLN), KC_QUOTE       ,
      KC_EQUAL       , KC_Z           , KC_X           , KC_C           , KC_V           , KC_B           , KC_MPLY        , KC_DELETE      , KC_N           , KC_M           , KC_COMMA       , KC_DOT         , KC_SLASH       , KC_BSLS        ,
      MO(3)          , MO(2)          , KC_DELETE      , KC_SPACE       , MO(1)          ,                KC_ENTER       , KC_SPACE       , KC_BSPC        , MO(2)          , MO(3)
    ),
    [1] = LAYOUT_split_5x6(
      KC_TRNS        , KC_F1          , KC_F2          , KC_F3          , KC_F4          , KC_F5          ,                KC_F6          , KC_F7          , KC_F8          , KC_F9          , KC_F10         , KC_EQUAL       ,
      KC_TRNS        , KC_TRNS        , KC_TRNS        , RALT(KC_G)     , KC_TRNS        , KC_TRNS        ,                KC_TRNS        , RALT(KC_J)     , RALT(KC_B)     , RALT(KC_DOT)   , KC_LBRC        , KC_RBRC        ,
      KC_TRNS        , RALT(KC_X)     , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        ,                KC_EQUAL       , LSFT(KC_EQUAL) , KC_TRNS        , KC_TRNS        , RALT(KC_N)     , KC_NONUS_HASH  ,
      KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        ,
      KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        ,                KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS
    ),
    [2] = LAYOUT_split_5x6(
      LSFT(KC_GRAVE) , KC_F11         , KC_F12         , KC_F3          , KC_F4          , KC_F5          ,                KC_F6          , KC_F7          , KC_F8          , KC_F9          , KC_F10         , KC_F11         ,
      KC_TAB         , KC_INSERT      , KC_PSCR        , KC_APP         , KC_TRNS        , KC_TRNS        ,                KC_PGUP        , LSFT(KC_7)     , KC_UP          , LSFT(KC_9)     , LSFT(KC_0)     , KC_F12         ,
      KC_TRNS        , KC_LSFT        , KC_LCTL        , KC_LALT        , KC_LGUI        , KC_CAPS        ,                KC_PGDN        , KC_LEFT        , KC_DOWN        , KC_RIGHT       , KC_DELETE      , KC_BSPC        ,
      KC_TRNS        , KC_UNDO        , KC_CUT         , KC_COPY        , KC_PSTE        , MS_BTN3        , MS_BTN2        , MS_BTN1        , MS_BTN4        , KC_HOME        , KC_TRNS        , KC_END         , KC_TRNS        , KC_TRNS        ,
      KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        ,                KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS
    ),
    [3] = LAYOUT_split_5x6(
      KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , DB_TOGG        , KC_TRNS        ,                DB_TOGG        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , RM_HUEU        ,
      KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , EE_CLR         ,                EE_CLR         , KC_TRNS        , KC_TRNS        , KC_TRNS        , RM_PREV        , RM_NEXT        ,
      KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , QK_BOOT        ,                QK_BOOT        , KC_TRNS        , KC_TRNS        , KC_TRNS        , RM_VALD        , RM_VALU        ,
      KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_MPLY        , RM_TOGG        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , RM_SPDD        , RM_SPDU        ,
      KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        ,                KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS        , KC_TRNS
    )
};

#ifdef ENCODER_MAP_ENABLE
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
    [0] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(MS_WHLD, MS_WHLU) },
    [1] = { ENCODER_CCW_CW(KC_MPRV, KC_MNXT), ENCODER_CCW_CW(KC_PGDN, KC_PGUP) },
    [2] = { ENCODER_CCW_CW(MS_LEFT, MS_RGHT), ENCODER_CCW_CW(MS_DOWN, MS_UP) },
    [3] = { ENCODER_CCW_CW(RM_VALD, RM_VALU), ENCODER_CCW_CW(RM_PREV, RM_NEXT) }
};
#endif

led_config_t g_led_config = { {
  {   25, 24, 16, 15, 6, 5 },
  {   26, 23, 17, 14, 7, 4 },
  {   27, 22, 18, 13, 8, 3 },
  {   28, 21, 19, 12, 9, 2 },
  {   20, 11, 10, 1, 0, NO_LED },
  {   54, 53, 45, 44, 35, 34 },
  {   55, 52, 46, 43, 36, 33 },
  {   56, 51, 47, 42, 37, 32 },
  {   57, 50, 48, 41, 38, 31 },
  {   49, 40, 39, 30, 29, NO_LED }
}, {
  { 110, 63 }, { 100, 60 }, { 100, 45 }, { 100, 30 }, { 100, 15 }, { 100, 0 },
  { 80, 0 },  { 80, 15 },  { 80, 30 },  { 80, 45 },  { 80, 60 },
  { 60, 60 },  { 60, 45 },  { 60, 30 },  { 60, 15 },  { 60, 0 },
  { 40, 0 },  { 40, 15 },  { 40, 30 },  { 40, 45 },  { 40, 60 },
  { 20, 45 },  { 20, 30 },  { 20, 15 },  { 20, 0 },
  { 0,  0 },  { 0,  15 },  { 0, 30 },   { 0, 45 },
  { 110, 63 }, { 120, 60 }, { 120, 45 }, { 120, 30 }, { 120, 15 }, { 120, 0 },
  { 137, 0 },  { 137, 15 }, { 137, 30 }, { 137, 45 }, { 137, 60 },
  { 154, 60 }, { 154, 45 }, { 154, 30 }, { 154, 15 }, { 154, 0 },
  { 171, 0 },  { 171, 15 }, { 171, 30 }, { 171, 45 }, { 171, 60 },
  { 188, 45 }, { 188, 30 }, { 188, 15 }, { 188, 0 },
  { 223, 0 }, { 223,  15 }, { 223, 30 }, { 223, 45 },
}, {
  4,4,4,4,4,4, 4,4,4,4,4,4, 4,4,4,4,4,4, 4,4,4,4,4,4, 4,4,4,4,4,
  4,4,4,4,4,4, 4,4,4,4,4,4, 4,4,4,4,4,4, 4,4,4,4,4,4, 4,4,4,4,4
} };

#ifdef RULE_LIGHTING_ENABLE
/* Flash-based rule lighting rules from 2026-01-06-lighting-vendorid.vil
 * Define rules here - rest of array is automatically zero-initialized
 * Empty slots (sat_idle == OFF) are skipped by the animation
 */
const rule_lighting_entry_t rules[RULE_LIGHTING_ENTRIES] = {
    // Rule 0: Letter keys (A-Z)
    {
        .layer_enable = 0,
        .caps_enable = 0,
        .layer = 0,
        .mods = 0,
        .keycode_start = KC_A,
        .keycode_end = KC_Z,
        .sat_idle = VIAL_RGB_SAT_PURE,       // 3 = vivid
        .h_idle = 35,                         // ~140 degrees (cyan-ish)
        .sat_pressed = VIAL_RGB_SAT_OFF,      // 0 = off on press
        .h_pressed = 0,
    },
    // Rule 1: Mod-Tap keys (0x2000-0x3fff)
    {
        .layer_enable = 0,
        .caps_enable = 0,
        .layer = 0,
        .mods = 0,
        .keycode_start = 0x2000,              // QK_MOD_TAP range
        .keycode_end = 0x3fff,
        .sat_idle = VIAL_RGB_SAT_PURE,
        .h_idle = 40,                         // ~160 degrees (blue)
        .sat_pressed = VIAL_RGB_SAT_OFF,
        .h_pressed = 0,
    },
    // Rule 2: Number row (1-0)
    {
        .layer_enable = 0,
        .caps_enable = 0,
        .layer = 0,
        .mods = 0,
        .keycode_start = KC_1,
        .keycode_end = KC_0,
        .sat_idle = VIAL_RGB_SAT_PURE,
        .h_idle = 30,                         // ~120 degrees (green)
        .sat_pressed = VIAL_RGB_SAT_OFF,
        .h_pressed = 0,
    },
    // Rule 3: Function keys (F1-F12)
    {
        .layer_enable = 0,
        .caps_enable = 0,
        .layer = 0,
        .mods = 0,
        .keycode_start = KC_F1,
        .keycode_end = KC_F12,
        .sat_idle = VIAL_RGB_SAT_PURE,
        .h_idle = 18,                         // ~72 degrees (yellow-green)
        .sat_pressed = VIAL_RGB_SAT_OFF,
        .h_pressed = 0,
    },
    // Rule 4: Layer change keys (TO(0)-0x527f)
    {
        .layer_enable = 0,
        .caps_enable = 0,
        .layer = 0,
        .mods = 0,
        .keycode_start = 0x5100,              // TO(0)
        .keycode_end = 0x527f,
        .sat_idle = VIAL_RGB_SAT_PURE,
        .h_idle = 0,                          // 0 degrees (red)
        .sat_pressed = VIAL_RGB_SAT_OFF,
        .h_pressed = 0,
    },
    // Rule 5: Special keys (0x87-0x777f)
    {
        .layer_enable = 0,
        .caps_enable = 0,
        .layer = 0,
        .mods = 0,
        .keycode_start = 0x0087,              // KC_RO
        .keycode_end = 0x777f,
        .sat_idle = VIAL_RGB_SAT_PURE,
        .h_idle = 11,                         // ~44 degrees (orange)
        .sat_pressed = VIAL_RGB_SAT_OFF,
        .h_pressed = 0,
    },
    // Rule 6-11: Empty slots
    {}, {}, {}, {}, {}, {},
    // Rule 12: Layer 1 - all keys pastel purple
    {
        .layer_enable = 1,
        .caps_enable = 0,
        .layer = 1,
        .mods = 0,
        .keycode_start = 0x0000,
        .keycode_end = 0xffff,
        .sat_idle = VIAL_RGB_SAT_PASTEL,      // 2 = pastel
        .h_idle = 59,                         // ~236 degrees (purple)
        .sat_pressed = VIAL_RGB_SAT_OFF,
        .h_pressed = 0,
    },
    // Rule 13: Layer 2 - all keys pastel cyan
    {
        .layer_enable = 1,
        .caps_enable = 0,
        .layer = 2,
        .mods = 0,
        .keycode_start = 0x0000,
        .keycode_end = 0xffff,
        .sat_idle = VIAL_RGB_SAT_PASTEL,
        .h_idle = 47,                         // ~188 degrees (cyan)
        .sat_pressed = VIAL_RGB_SAT_OFF,
        .h_pressed = 0,
    },
    // Rule 14: Layer 3 - all keys pastel orange
    {
        .layer_enable = 1,
        .caps_enable = 0,
        .layer = 3,
        .mods = 0,
        .keycode_start = 0x0000,
        .keycode_end = 0xffff,
        .sat_idle = VIAL_RGB_SAT_PASTEL,
        .h_idle = 11,                         // ~44 degrees (orange)
        .sat_pressed = VIAL_RGB_SAT_OFF,
        .h_pressed = 0,
    },
    // Rule 15: Default fallback - all other keys white
    {
        .layer_enable = 0,
        .caps_enable = 0,
        .layer = 0,
        .mods = 0,
        .keycode_start = 0x0000,
        .keycode_end = 0xffff,
        .sat_idle = VIAL_RGB_SAT_WHITE,       // 1 = white
        .h_idle = 0,
        .sat_pressed = VIAL_RGB_SAT_OFF,
        .h_pressed = 0,
    },
};
#endif

#ifdef CONSOLE_ENABLE
static uint32_t last_debug_print = 0;

void debug_rule_lighting(void) {
    if (timer_elapsed32(last_debug_print) > 5000) {
        last_debug_print = timer_read32();

        uprintf("=== Rule Lighting Debug ===\n");
        uprintf("RGB Matrix Mode: %d\n", rgb_matrix_get_mode());
        uprintf("RGB Matrix Enabled: %d\n", rgb_matrix_is_enabled());

        #ifdef RULE_LIGHTING_ENABLE
        const rule_lighting_entry_t *rules = rule_lighting_get_rules();
        uint8_t rule_count = 0;

        // Count non-empty rules
        for (uint8_t i = 0; i < RULE_LIGHTING_ENTRIES; i++) {
            if (VIAL_RGB_SAT_IS_ON(rules[i].sat_idle) || VIAL_RGB_SAT_IS_ON(rules[i].sat_pressed)) {
                rule_count++;
            }
        }

        uprintf("Active rules: %d\n", rule_count);

        for (uint8_t i = 0; i < RULE_LIGHTING_ENTRIES; i++) {
            const rule_lighting_entry_t *rule = &rules[i];
            // Skip empty entries
            if (!VIAL_RGB_SAT_IS_ON(rule->sat_idle) && !VIAL_RGB_SAT_IS_ON(rule->sat_pressed)) {
                continue;
            }
            uprintf("Rule %d: layer_en=%d layer=%d caps_en=%d mods=0x%02X kc=%04X-%04X sat_idle=%d h_idle=%d sat_pr=%d h_pr=%d\n",
                i, rule->layer_enable, rule->layer, rule->caps_enable, rule->mods,
                rule->keycode_start, rule->keycode_end,
                rule->sat_idle, rule->h_idle, rule->sat_pressed, rule->h_pressed);
        }
        #endif
    }
}
#endif

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    #ifdef CONSOLE_ENABLE
        uprintf("KL: kc: 0x%04X, col: %2u, row: %2u, pressed: %u, time: %5u, int: %u, count: %u\n", keycode, record->event.key.col, record->event.key.row, record->event.pressed, record->event.time, record->tap.interrupted, record->tap.count);
    #endif
    return true;
}

void housekeeping_task_user() {
    #ifdef RGB_MATRIX_ENABLE
        int val = rgb_matrix_get_val();
        if (val > RGB_MATRIX_MAXIMUM_BRIGHTNESS) {
            rgb_matrix_decrease_val();
        }
    #endif

    #ifdef CONSOLE_ENABLE
        debug_rule_lighting();
    #endif
}
