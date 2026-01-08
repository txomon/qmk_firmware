# Rule Lighting Implementation in the `rl` Branch

This document provides detailed documentation of the Rule Lighting feature implementation in the `rl` branch of this QMK firmware fork. The documentation is comprehensive enough to allow reimplementation from scratch.

## Table of Contents

1. [Overview](#overview)
2. [Feature Architecture](#feature-architecture)
3. [Core Files](#core-files)
   - [rule_lighting.h](#rule_lightingh---quantum)
   - [rule_lighting.c](#rule_lightingc---quantum)
   - [rule_lighting_anim.h](#rule_lighting_animh---rgb-matrix-animation)
4. [EEPROM Storage](#eeprom-storage)
   - [nvm_dynamic_keymap.c](#nvm_dynamic_keymapc---eeprom-storage)
5. [Vial Protocol Integration](#vial-protocol-integration)
   - [vial.h](#vialh---protocol-definitions)
   - [vial.c](#vialc---protocol-handlers)
6. [Build System Integration](#build-system-integration)
   - [build_vial.mk](#build_vialmk)
   - [rgb_matrix_effects.inc](#rgb_matrix_effectsinc)
   - [vialrgb_effects.inc](#vialrgb_effectsinc)
7. [RGB Matrix Integration](#rgb-matrix-integration)
   - [rgb_matrix.c](#rgb_matrixc---initialization-hooks)
8. [Split Keyboard Support](#split-keyboard-support)
   - [Configuration](#split-keyboard-configuration)
   - [hooks_base.c](#hooks_basec---split-sync-implementation)
9. [Data Structures](#data-structures)
10. [Algorithm Details](#algorithm-details)
11. [How to Enable](#how-to-enable)

---

## Overview

Rule Lighting is a QMK RGB Matrix effect that colors keys based on configurable rules. It provides a powerful way to create layer-aware, modifier-aware, and keycode-aware RGB lighting configurations that persist across reboots via EEPROM storage.

### Key Features

- **Rule-Based Coloring**: Keys are colored based on rules that match layer state, modifier state, caps lock state, and keycode ranges
- **EEPROM Persistence**: Rules are stored in EEPROM and survive power cycles
- **Vial Configuration**: Rules can be configured via the Vial protocol (USB HID)
- **Split Keyboard Support**: Full support for split keyboards with rule and keymap synchronization between halves
- **Press Animation**: Each rule defines both idle and pressed colors with smooth transitions
- **Compact Storage**: Each rule entry is only 8 bytes, with configurable entry count based on EEPROM size

### Rule Matching Criteria

Each rule can match on:
- **Layer State**: Which layer is currently active (0-15)
- **Modifier State**: Which modifiers are currently held (8-bit modifier mask)
- **Caps Lock State**: Whether caps lock is enabled
- **Keycode Range**: A range of keycodes (start to end, inclusive)

---

## Feature Architecture

```
+-------------------+     +------------------+     +-------------------+
|   Vial Protocol   |---->|   rule_lighting  |---->|   RGB Matrix      |
|   (vial.c)        |     |   .c/.h          |     |   Animation       |
+-------------------+     +------------------+     +-------------------+
         |                        |                        |
         v                        v                        v
+-------------------+     +------------------+     +-------------------+
|   EEPROM Storage  |     |   Split Sync     |     |   LED Output      |
|   (nvm_*.c)       |     |   (hooks_base.c) |     |                   |
+-------------------+     +------------------+     +-------------------+
```

---

## Core Files

### rule_lighting.h - Quantum

**Location**: `quantum/rule_lighting.h`

This header file defines all data structures, constants, and function prototypes for the rule lighting system.

#### Entry Count Configuration

```c
#ifndef RULE_LIGHTING_ENTRIES
    #if TOTAL_EEPROM_BYTE_COUNT > 4000
        #define RULE_LIGHTING_ENTRIES 32
    #elif TOTAL_EEPROM_BYTE_COUNT > 2000
        #define RULE_LIGHTING_ENTRIES 16
    #elif TOTAL_EEPROM_BYTE_COUNT > 1000
        #define RULE_LIGHTING_ENTRIES 8
    #else
        #define RULE_LIGHTING_ENTRIES 4
    #endif
#endif
```

**Purpose**: Automatically scales the number of rule entries based on available EEPROM size. Keyboards can override this by defining `RULE_LIGHTING_ENTRIES` in their config.h.

#### Saturation Modes Enum

```c
typedef enum {
    VIAL_RGB_SAT_OFF    = 0b00,  // LED off (RGB 0,0,0)
    VIAL_RGB_SAT_WHITE  = 0b01,  // S=0 (white/grayscale)
    VIAL_RGB_SAT_PASTEL = 0b10,  // S=128 (soft color)
    VIAL_RGB_SAT_PURE   = 0b11,  // S=255 (vivid color)
} vial_rgb_saturation_t;
```

**Purpose**: Defines 4 saturation levels that can be encoded in just 2 bits. This compact encoding allows storing both hue and saturation in a single byte.

- `VIAL_RGB_SAT_OFF` (0b00): LED is completely off
- `VIAL_RGB_SAT_WHITE` (0b01): No saturation (white/grayscale based on value)
- `VIAL_RGB_SAT_PASTEL` (0b10): Half saturation (soft, pastel colors)
- `VIAL_RGB_SAT_PURE` (0b11): Full saturation (vivid, pure colors)

#### Helper Macro

```c
#define VIAL_RGB_SAT_IS_ON(sat) ((sat) != VIAL_RGB_SAT_OFF)
```

**Purpose**: Quick check if a saturation mode means the LED should be lit.

#### Rule Entry Structure (8 bytes)

```c
typedef struct {
    /* Condition (16 bits) */
    uint16_t layer_enable   : 1;   // Bit 0: Enable layer matching
    uint16_t caps_enable    : 1;   // Bit 1: Enable caps lock matching
    uint16_t reserved       : 2;   // Bits 2-3: Reserved for future use
    uint16_t layer          : 4;   // Bits 4-7: Layer to match (0-15)
    uint16_t mods           : 8;   // Bits 8-15: Modifier mask

    /* Keycode range (32 bits) */
    uint16_t keycode_start;        // First keycode in range
    uint16_t keycode_end;          // Last keycode in range (inclusive)

    /* Colors (16 bits) */
    uint8_t  sat_idle       : 2;   // Idle saturation mode
    uint8_t  h_idle         : 6;   // Idle hue (6-bit, multiply by 4 for 8-bit)
    uint8_t  sat_pressed    : 2;   // Pressed saturation mode
    uint8_t  h_pressed      : 6;   // Pressed hue (6-bit)

} __attribute__((packed)) rule_lighting_entry_t;
```

**Bit Layout Explanation**:

- **Bytes 0-1 (Condition)**:
  - Bit 0 (`layer_enable`): When 1, the rule only matches if `layer` field matches current layer
  - Bit 1 (`caps_enable`): When 1, the rule only matches if caps lock is ON
  - Bits 2-3 (`reserved`): Reserved for future condition flags
  - Bits 4-7 (`layer`): The layer number to match (0-15)
  - Bits 8-15 (`mods`): Modifier mask - rule matches if ANY of these mods are held

- **Bytes 2-3 (`keycode_start`)**: First keycode in the matching range

- **Bytes 4-5 (`keycode_end`)**: Last keycode in the matching range (inclusive)

- **Byte 6 (Idle Color)**:
  - Bits 0-1 (`sat_idle`): Saturation mode for idle state
  - Bits 2-7 (`h_idle`): 6-bit hue for idle state (shift left 2 for full 8-bit hue)

- **Byte 7 (Pressed Color)**:
  - Bits 0-1 (`sat_pressed`): Saturation mode for pressed state
  - Bits 2-7 (`h_pressed`): 6-bit hue for pressed state

**Design Rationale**: The 6-bit hue encoding sacrifices some color precision (64 hues instead of 256) to fit both hue and saturation mode in a single byte, keeping the total structure at exactly 8 bytes.

#### Config Structure (1 byte)

```c
typedef struct {
    uint8_t entry_count;    // Number of active rules (0-255)
} __attribute__((packed)) rule_lighting_config_t;
```

**Purpose**: Stores global configuration. Currently only tracks how many rules are active, but the structure allows for future expansion.

#### Sync Structure (for Split Keyboards)

```c
typedef struct {
    rule_lighting_config_t config;
    rule_lighting_entry_t entries[RULE_LIGHTING_ENTRIES];
} __attribute__((packed)) rule_lighting_sync_t;
```

**Purpose**: Aggregates all rule lighting data into a single structure for efficient split keyboard synchronization. The master half sends this to the slave periodically.

#### Function Prototypes

```c
void rule_lighting_init(void);                    // Initialize system
void rule_lighting_reset(void);                   // Reset to defaults
void rule_lighting_load(void);                    // Load from EEPROM
void rule_lighting_save(void);                    // Save to EEPROM

rule_lighting_entry_t* rule_lighting_get_entry(uint8_t index);
void rule_lighting_set_entry(uint8_t index, const rule_lighting_entry_t *entry);

rule_lighting_config_t* rule_lighting_get_config(void);
void rule_lighting_set_config(const rule_lighting_config_t *config);

const rule_lighting_sync_t* rule_lighting_get_sync_data(void);
void rule_lighting_apply_sync_data(const rule_lighting_sync_t *sync_data);

uint16_t get_synced_keycode(uint8_t layer, uint8_t row, uint8_t col);  // Weak
```

#### Utility Macros and Functions

```c
#define VIAL_RGB_HUE_6TO8(h6) ((h6) << 2)

static inline uint8_t vial_rgb_sat_to_value(uint8_t sat_mode) {
    switch (sat_mode) {
        case VIAL_RGB_SAT_OFF:    return 0;
        case VIAL_RGB_SAT_WHITE:  return 0;
        case VIAL_RGB_SAT_PASTEL: return 128;
        case VIAL_RGB_SAT_PURE:   return 255;
        default:                   return 0;
    }
}
```

**Purpose**:
- `VIAL_RGB_HUE_6TO8`: Converts 6-bit hue to 8-bit by left-shifting 2 bits
- `vial_rgb_sat_to_value`: Converts saturation mode enum to actual saturation value (0-255)

---

### rule_lighting.c - Quantum

**Location**: `quantum/rule_lighting.c`

This file implements the rule lighting system's core logic.

#### RAM Cache

```c
static rule_lighting_config_t indicator_config;
static rule_lighting_entry_t indicator_entries[RULE_LIGHTING_ENTRIES];
```

**Purpose**: All rules are cached in RAM for fast access during rendering. EEPROM is only accessed during load/save operations.

#### rule_lighting_clear_ram()

```c
static void rule_lighting_clear_ram(void) {
    indicator_config.entry_count = 0;

    for (uint8_t i = 0; i < RULE_LIGHTING_ENTRIES; i++) {
        indicator_entries[i] = (rule_lighting_entry_t){
            .layer_enable = 0,
            .caps_enable = 0,
            .layer = 0,
            .mods = 0,
            .keycode_start = 0,
            .keycode_end = 0,
            .sat_idle = VIAL_RGB_SAT_OFF,
            .h_idle = 0,
            .sat_pressed = VIAL_RGB_SAT_OFF,
            .h_pressed = 0,
        };
    }
}
```

**Purpose**: Clears all rules from RAM cache. Sets entry count to 0 and all entries to a blank "off" state.

#### rule_lighting_reset()

```c
void rule_lighting_reset(void) {
    rule_lighting_clear_ram();
    nvm_dynamic_keymap_set_rgb_indicator_config(&indicator_config);
    for (uint8_t i = 0; i < RULE_LIGHTING_ENTRIES; i++) {
        nvm_dynamic_keymap_set_rgb_indicator_entry(i, &indicator_entries[i]);
    }
}
```

**Purpose**: Resets rule lighting to factory defaults and persists to EEPROM. Called when RGB Matrix configuration is uninitialized.

#### rule_lighting_load()

```c
void rule_lighting_load(void) {
#ifdef SPLIT_KEYBOARD
    if (!is_keyboard_master()) {
        rule_lighting_clear_ram();
        return;
    }
#endif

    nvm_dynamic_keymap_get_rgb_indicator_config(&indicator_config);

    if (indicator_config.entry_count > RULE_LIGHTING_ENTRIES) {
        indicator_config.entry_count = 0;
    }

    for (uint8_t i = 0; i < RULE_LIGHTING_ENTRIES; i++) {
        nvm_dynamic_keymap_get_rgb_indicator_entry(i, &indicator_entries[i]);
    }
}
```

**Purpose**: Loads configuration and all rules from EEPROM into RAM cache.

**Split Keyboard Behavior**: On split keyboards, only the master half has EEPROM access. The slave half starts with blank RAM and receives data via split transport sync.

**Validation**: If entry_count is corrupted (greater than max entries), it's reset to 0.

#### rule_lighting_init()

```c
void rule_lighting_init(void) {
    rule_lighting_load();
}
```

**Purpose**: Initialization entry point. Simply loads from EEPROM. The reset case is handled separately by `eeconfig_update_rgb_matrix_default()`.

#### rule_lighting_get_entry() / rule_lighting_set_entry()

```c
rule_lighting_entry_t* rule_lighting_get_entry(uint8_t index) {
    if (index >= RULE_LIGHTING_ENTRIES) {
        return NULL;
    }
    return &indicator_entries[index];
}

void rule_lighting_set_entry(uint8_t index, const rule_lighting_entry_t *entry) {
    if (index >= RULE_LIGHTING_ENTRIES) {
        return;
    }
    indicator_entries[index] = *entry;
}
```

**Purpose**: Accessor functions for rule entries. These modify only RAM - call `rule_lighting_save()` to persist.

#### rule_lighting_get_config() / rule_lighting_set_config()

```c
rule_lighting_config_t* rule_lighting_get_config(void) {
    return &indicator_config;
}

void rule_lighting_set_config(const rule_lighting_config_t *config) {
    indicator_config = *config;
}
```

**Purpose**: Accessor functions for global config. These modify only RAM.

#### rule_lighting_save()

```c
void rule_lighting_save(void) {
    nvm_dynamic_keymap_set_rgb_indicator_config(&indicator_config);
    for (uint8_t i = 0; i < RULE_LIGHTING_ENTRIES; i++) {
        nvm_dynamic_keymap_set_rgb_indicator_entry(i, &indicator_entries[i]);
    }
}
```

**Purpose**: Persists all RAM data to EEPROM.

#### rule_lighting_get_sync_data()

```c
const rule_lighting_sync_t* rule_lighting_get_sync_data(void) {
    static rule_lighting_sync_t sync_data;
    sync_data.config = indicator_config;
    for (uint8_t i = 0; i < RULE_LIGHTING_ENTRIES; i++) {
        sync_data.entries[i] = indicator_entries[i];
    }
    return &sync_data;
}
```

**Purpose**: Packages all rule lighting data into a single structure for split keyboard transport. The master calls this to get data to send to the slave.

#### rule_lighting_apply_sync_data()

```c
void rule_lighting_apply_sync_data(const rule_lighting_sync_t *sync_data) {
    indicator_config = sync_data->config;
    for (uint8_t i = 0; i < RULE_LIGHTING_ENTRIES; i++) {
        indicator_entries[i] = sync_data->entries[i];
    }
}
```

**Purpose**: Applies received sync data to RAM cache. The slave calls this when it receives data from the master. No EEPROM write - the slave only keeps data in RAM.

#### get_synced_keycode() (Weak Default)

```c
__attribute__((weak)) uint16_t get_synced_keycode(uint8_t layer, uint8_t row, uint8_t col) {
    return keycode_at_keymap_location_raw(layer, row, col);
}
```

**Purpose**: Returns the keycode for a given position. This is a weak function that keyboards can override to provide synced keymap support for split keyboards.

**Default Behavior**: Uses the compiled keymap from flash via `keycode_at_keymap_location_raw()`.

**Override**: Split keyboards override this to use synchronized keymap data on the slave half.

---

### rule_lighting_anim.h - RGB Matrix Animation

**Location**: `quantum/rgb_matrix/animations/rule_lighting_anim.h`

This file implements the actual RGB Matrix effect that renders the rule-based lighting.

#### Effect Registration

```c
#ifdef RULE_LIGHTING_ENABLE
#define RGB_MATRIX_EFFECT_RULE_LIGHTING
RGB_MATRIX_EFFECT(RULE_LIGHTING)
#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS
// ... implementation ...
#endif
#endif
```

**Purpose**: Registers `RULE_LIGHTING` as an RGB Matrix effect. The effect only exists when `RULE_LIGHTING_ENABLE` is defined.

#### rule_lighting_find_match()

```c
static int8_t rule_lighting_find_match(uint16_t keycode,
                                        uint8_t current_layer,
                                        uint8_t current_mods,
                                        bool caps_on) {
    rule_lighting_config_t *config = rule_lighting_get_config();

    for (uint8_t i = 0; i < config->entry_count && i < RULE_LIGHTING_ENTRIES; i++) {
        const rule_lighting_entry_t *rule = rule_lighting_get_entry(i);
        if (!rule) continue;

        /* Layer check */
        if (rule->layer_enable && current_layer != rule->layer) {
            continue;
        }

        /* Mods check (OR logic) */
        if (rule->mods != 0 && (current_mods & rule->mods) == 0) {
            continue;
        }

        /* Caps check */
        if (rule->caps_enable && !caps_on) {
            continue;
        }

        /* Keycode range check */
        if (keycode >= rule->keycode_start && keycode <= rule->keycode_end) {
            return i;
        }
    }
    return -1;
}
```

**Purpose**: Finds the first rule that matches a given keycode under current keyboard state.

**Matching Logic**:
1. **Layer Check**: If `layer_enable` is set, `current_layer` must equal `rule->layer`
2. **Mods Check**: If `rule->mods` is non-zero, at least one of the specified mods must be held (OR logic via bitwise AND)
3. **Caps Check**: If `caps_enable` is set, caps lock must be ON
4. **Keycode Range**: The keycode must be within `[keycode_start, keycode_end]` inclusive

**Return Value**: Index of matching rule, or -1 if no match.

**First-Match-Wins**: Rules are evaluated in order (0 to entry_count-1). The first matching rule is used, allowing for rule priority through ordering.

#### rule_lighting_get_tick()

```c
static uint16_t rule_lighting_get_tick(uint8_t led_index, uint8_t speed) {
#ifdef RGB_MATRIX_KEYREACTIVE_ENABLED
    uint16_t max_tick = 65535 / qadd8(speed, 1);

    for (int8_t j = g_last_hit_tracker.count - 1; j >= 0; j--) {
        if (g_last_hit_tracker.index[j] == led_index) {
            return g_last_hit_tracker.tick[j];
        }
    }
    return max_tick;
#else
    return 65535;
#endif
}
```

**Purpose**: Gets the time since the last key press for an LED, used for press-to-idle animation.

**Implementation**: Searches the global `g_last_hit_tracker` (QMK's key reactive tracking) for the LED index. Returns the tick count if found, or max_tick (fully idle) if not found.

**Without Reactive Support**: If `RGB_MATRIX_KEYREACTIVE_ENABLED` is not defined, always returns 65535 (always idle state).

#### rule_lighting_calc_color()

```c
static void rule_lighting_calc_color(const rule_lighting_entry_t *rule,
                                      uint16_t tick,
                                      uint8_t speed,
                                      uint8_t *h, uint8_t *s, uint8_t *v) {
    uint16_t scaled_time = scale16by8(tick, qadd8(speed, 1));
    uint8_t blend = (scaled_time > 255) ? 255 : (uint8_t)scaled_time;

    uint8_t brightness = rgb_matrix_get_val();

    uint8_t h_idle = VIAL_RGB_HUE_6TO8(rule->h_idle);
    uint8_t h_pressed = VIAL_RGB_HUE_6TO8(rule->h_pressed);
    uint8_t s_idle = vial_rgb_sat_to_value(rule->sat_idle);
    uint8_t s_pressed = vial_rgb_sat_to_value(rule->sat_pressed);

    bool pressed_off = !VIAL_RGB_SAT_IS_ON(rule->sat_pressed);
    bool idle_off = !VIAL_RGB_SAT_IS_ON(rule->sat_idle);

    if (blend < 255) {
        if (pressed_off) {
            *h = h_idle;
            *s = s_idle;
            *v = scale8(brightness, blend);
        } else if (idle_off) {
            *h = h_pressed;
            *s = s_pressed;
            *v = scale8(brightness, 255 - blend);
        } else {
            *h = lerp8by8(h_pressed, h_idle, blend);
            *s = lerp8by8(s_pressed, s_idle, blend);
            *v = brightness;
        }
    } else {
        if (idle_off) {
            *h = 0;
            *s = 0;
            *v = 0;
        } else {
            *h = h_idle;
            *s = s_idle;
            *v = brightness;
        }
    }
}
```

**Purpose**: Calculates the HSV color for a key based on its rule and how recently it was pressed.

**Parameters**:
- `rule`: The matching rule entry
- `tick`: Time since last press (from `g_last_hit_tracker`)
- `speed`: RGB matrix speed setting
- `h`, `s`, `v`: Output HSV values

**Blend Calculation**:
1. `scaled_time = scale16by8(tick, speed + 1)` - Scales tick by speed
2. `blend` - Clamped to 0-255, where 0 = just pressed, 255 = fully idle

**Color Transitions**:
- **Pressed OFF, Idle ON**: Fade in from black to idle color
- **Pressed ON, Idle OFF**: Fade out from pressed color to black
- **Both ON**: Interpolate from pressed color to idle color
- **Both OFF**: LED stays off

**Brightness**: The `v` component uses `rgb_matrix_get_val()` for global brightness control.

#### RULE_LIGHTING() - Main Effect Function

```c
bool RULE_LIGHTING(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    /* Get current keyboard state */
    uint8_t current_layer = get_highest_layer(layer_state);
    uint8_t current_mods = get_mods() | get_weak_mods();
#ifndef NO_ACTION_ONESHOT
    current_mods |= get_oneshot_mods();
#endif
    bool caps_on = host_keyboard_led_state().caps_lock;

    /* Iterate over the key matrix */
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            uint8_t led_index = g_led_config.matrix_co[row][col];

            if (led_index == NO_LED || led_index < led_min || led_index >= led_max) {
                continue;
            }

            if (!HAS_ANY_FLAGS(g_led_config.flags[led_index], params->flags)) {
                continue;
            }

            uint16_t keycode = get_synced_keycode(current_layer, row, col);

            int8_t rule_idx = rule_lighting_find_match(keycode, current_layer, current_mods, caps_on);

            if (rule_idx >= 0) {
                const rule_lighting_entry_t *rule = rule_lighting_get_entry(rule_idx);
                uint8_t speed = rgb_matrix_get_speed();
                uint16_t tick = rule_lighting_get_tick(led_index, speed);

                uint8_t h, s, v;
                rule_lighting_calc_color(rule, tick, speed, &h, &s, &v);

                hsv_t hsv = {.h = h, .s = s, .v = v};
                rgb_t rgb = rgb_matrix_hsv_to_rgb(hsv);
                rgb_matrix_set_color(led_index, rgb.r, rgb.g, rgb.b);
            } else {
                rgb_matrix_set_color(led_index, 0, 0, 0);
            }
        }
    }

    return rgb_matrix_check_finished_leds(led_max);
}
```

**Purpose**: The main RGB Matrix effect function that renders rule-based lighting.

**Execution Flow**:
1. **Get Limits**: Uses `RGB_MATRIX_USE_LIMITS` for split rendering support
2. **Get State**: Captures current layer, mods (including oneshot), and caps lock
3. **Iterate Matrix**: Loops through all row/col positions
4. **Filter LEDs**: Skips positions without LEDs, outside range, or wrong flags
5. **Get Keycode**: Uses `get_synced_keycode()` for split keyboard support
6. **Find Rule**: Searches for matching rule
7. **Calculate Color**: If rule found, calculates animated HSV color
8. **Set Color**: Converts to RGB and sets the LED color
9. **No Match**: Sets unmatched keys to black (off)

**Return Value**: `rgb_matrix_check_finished_leds(led_max)` for proper animation continuation.

---

## EEPROM Storage

### nvm_dynamic_keymap.c - EEPROM Storage

**Location**: `quantum/nvm/eeprom/nvm_dynamic_keymap.c`

This file handles all EEPROM storage for dynamic keymap features, including rule lighting.

#### EEPROM Layout

The EEPROM is organized sequentially:

```
DYNAMIC_KEYMAP_EEPROM_ADDR    -> Keymaps
VIAL_ENCODERS_EEPROM_ADDR     -> Encoders
VIAL_QMK_SETTINGS_EEPROM_ADDR -> QMK Settings
VIAL_TAP_DANCE_EEPROM_ADDR    -> Tap Dance
VIAL_COMBO_EEPROM_ADDR        -> Combos
VIAL_KEY_OVERRIDE_EEPROM_ADDR -> Key Overrides
VIAL_ALT_REPEAT_KEY_EEPROM_ADDR -> Alt Repeat Key
RULE_LIGHTING_EEPROM_ADDR     -> Rule Lighting Config + Entries
DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR -> Macros
```

#### Rule Lighting Address Calculation

```c
#define RULE_LIGHTING_EEPROM_ADDR (VIAL_ALT_REPEAT_KEY_EEPROM_ADDR + VIAL_ALT_REPEAT_KEY_SIZE)

#ifdef RULE_LIGHTING_ENABLE
#include "rule_lighting.h"
#define RULE_LIGHTING_CONFIG_SIZE (sizeof(rule_lighting_config_t))
#define RULE_LIGHTING_ENTRIES_SIZE (sizeof(rule_lighting_entry_t) * RULE_LIGHTING_ENTRIES)
#define RULE_LIGHTING_SIZE (RULE_LIGHTING_CONFIG_SIZE + RULE_LIGHTING_ENTRIES_SIZE)
#else
#define RULE_LIGHTING_SIZE 0
#endif
```

**Storage Size**: 1 byte (config) + 8 bytes * RULE_LIGHTING_ENTRIES

#### EEPROM Access Functions

```c
int nvm_dynamic_keymap_get_rgb_indicator_config(rule_lighting_config_t *config) {
    void *address = (void*)(RULE_LIGHTING_EEPROM_ADDR);
    eeprom_read_block(config, address, sizeof(rule_lighting_config_t));
    return 0;
}

int nvm_dynamic_keymap_set_rgb_indicator_config(const rule_lighting_config_t *config) {
    void *address = (void*)(RULE_LIGHTING_EEPROM_ADDR);
    eeprom_write_block(config, address, sizeof(rule_lighting_config_t));
    return 0;
}

int nvm_dynamic_keymap_get_rgb_indicator_entry(uint8_t index, rule_lighting_entry_t *entry) {
    if (index >= RULE_LIGHTING_ENTRIES)
        return -1;

    void *address = (void*)(RULE_LIGHTING_EEPROM_ADDR + RULE_LIGHTING_CONFIG_SIZE
                            + index * sizeof(rule_lighting_entry_t));
    eeprom_read_block(entry, address, sizeof(rule_lighting_entry_t));
    return 0;
}

int nvm_dynamic_keymap_set_rgb_indicator_entry(uint8_t index, const rule_lighting_entry_t *entry) {
    if (index >= RULE_LIGHTING_ENTRIES)
        return -1;

    void *address = (void*)(RULE_LIGHTING_EEPROM_ADDR + RULE_LIGHTING_CONFIG_SIZE
                            + index * sizeof(rule_lighting_entry_t));
    eeprom_write_block(entry, address, sizeof(rule_lighting_entry_t));
    return 0;
}
```

**Purpose**: Low-level EEPROM read/write functions for rule lighting data.

**Layout in EEPROM**:
```
Offset 0:                        rule_lighting_config_t (1 byte)
Offset 1:                        rule_lighting_entry_t[0] (8 bytes)
Offset 9:                        rule_lighting_entry_t[1] (8 bytes)
...
Offset 1 + (N-1)*8:              rule_lighting_entry_t[N-1] (8 bytes)
```

---

## Vial Protocol Integration

### vial.h - Protocol Definitions

**Location**: `quantum/vial.h`

#### Command IDs

```c
enum {
    // ... other commands ...
    vial_dynamic_entry_op = 0x0D,
};

enum {
    // ... other operations ...
    dynamic_rule_lighting_get_config = 0x09,
    dynamic_rule_lighting_set_config = 0x0A,
    dynamic_rule_lighting_get_entry = 0x0B,
    dynamic_rule_lighting_set_entry = 0x0C,
};
```

**Protocol**: Rule lighting uses the `vial_dynamic_entry_op` (0x0D) command with sub-operations 0x09-0x0C.

#### Entry Count Reporting

In `vial.h`, the RULE_LIGHTING_ENTRIES is exposed for Vial GUI:

```c
#if defined(RGB_MATRIX_ENABLE) && defined(RULE_LIGHTING_ENABLE)
#include "rule_lighting.h"
#else
#undef RULE_LIGHTING_ENTRIES
#define RULE_LIGHTING_ENTRIES 0
#endif
```

### vial.c - Protocol Handlers

**Location**: `quantum/vial.c`

#### Initialization

```c
void vial_init(void) {
    // ... other init ...
    // Note: rule_lighting_init() is called from rgb_matrix_init() instead,
    // because is_keyboard_master() is not valid until split_pre_init() runs
}
```

**Important**: Rule lighting is NOT initialized in `vial_init()`. It's initialized from `rgb_matrix_init()` to ensure proper split keyboard detection.

#### Entry Count Query

```c
case dynamic_vial_get_number_of_entries: {
    memset(msg, 0, length);
    msg[0] = VIAL_TAP_DANCE_ENTRIES;
    msg[1] = VIAL_COMBO_ENTRIES;
    msg[2] = VIAL_KEY_OVERRIDE_ENTRIES;
    msg[3] = VIAL_ALT_REPEAT_KEY_ENTRIES;
    msg[4] = RULE_LIGHTING_ENTRIES;       // <-- Rule lighting entries
    // ...
    break;
}
```

**Purpose**: Reports available entry counts to Vial GUI, including rule lighting slots.

#### Get Config Handler

```c
case dynamic_rule_lighting_get_config: {
    rule_lighting_config_t *config = rule_lighting_get_config();
    memset(msg, 0, length);
    if (config) {
        msg[0] = 0;  /* success */
        memcpy(&msg[1], config, sizeof(rule_lighting_config_t));
    } else {
        msg[0] = 1;  /* error */
    }
    break;
}
```

**Protocol**:
- Request: `[0xFE, 0x0D, 0x09, ...]`
- Response: `[status, config_data...]`

#### Set Config Handler

```c
case dynamic_rule_lighting_set_config: {
    rule_lighting_config_t config;
    memcpy(&config, &msg[3], sizeof(config));
    rule_lighting_set_config(&config);
    msg[0] = 0;  /* success */
    break;
}
```

**Protocol**:
- Request: `[0xFE, 0x0D, 0x0A, config_data...]`
- Response: `[status]`

#### Get Entry Handler

```c
case dynamic_rule_lighting_get_entry: {
    uint8_t idx = msg[3];
    rule_lighting_entry_t *entry = rule_lighting_get_entry(idx);
    memset(msg, 0, length);
    if (entry) {
        memcpy(&msg[1], entry, sizeof(rule_lighting_entry_t));
        msg[0] = 0;  /* success */
    } else {
        msg[0] = 1;  /* error */
    }
    break;
}
```

**Protocol**:
- Request: `[0xFE, 0x0D, 0x0B, index, ...]`
- Response: `[status, entry_data...]`

#### Set Entry Handler

```c
case dynamic_rule_lighting_set_entry: {
    uint8_t idx = msg[3];
    rule_lighting_entry_t entry;
    memcpy(&entry, &msg[4], sizeof(entry));
    rule_lighting_set_entry(idx, &entry);
    msg[0] = 0;  /* success */
    break;
}
```

**Protocol**:
- Request: `[0xFE, 0x0D, 0x0C, index, entry_data...]`
- Response: `[status]`

**Note**: These handlers only update RAM. The GUI must trigger EEPROM save separately (via RGB matrix save command).

---

## Build System Integration

### build_vial.mk

**Location**: `builddefs/build_vial.mk`

```makefile
ifeq ($(strip $(RULE_LIGHTING_ENABLE)), yes)
    SRC += $(QUANTUM_DIR)/rule_lighting.c
    OPT_DEFS += -DRULE_LIGHTING_ENABLE
endif
```

**Purpose**: When `RULE_LIGHTING_ENABLE = yes` in rules.mk:
1. Compiles `rule_lighting.c`
2. Defines `RULE_LIGHTING_ENABLE` preprocessor macro

### rgb_matrix_effects.inc

**Location**: `quantum/rgb_matrix/animations/rgb_matrix_effects.inc`

```c
// ... other effects ...
#include "rule_lighting_anim.h"
```

**Purpose**: Includes the rule lighting animation header so it's compiled as part of RGB Matrix effects.

### vialrgb_effects.inc

**Location**: `quantum/vialrgb_effects.inc`

```c
enum {
    // ... other effects ...
    VIALRGB_EFFECT_RULE_LIGHTING = 0xFE,
};

static const PROGMEM vialrgb_supported_mode_t supported_modes[] = {
    // ... other modes ...
#ifdef RGB_MATRIX_EFFECT_RULE_LIGHTING
    { VIALRGB_EFFECT_RULE_LIGHTING, RGB_MATRIX_RULE_LIGHTING },
#endif
};
```

**Purpose**: Maps the VialRGB effect ID (0xFE) to the QMK RGB Matrix effect enum value. This allows Vial GUI to select the rule lighting effect.

---

## RGB Matrix Integration

### rgb_matrix.c - Initialization Hooks

**Location**: `quantum/rgb_matrix/rgb_matrix.c`

#### Include

```c
#ifdef RULE_LIGHTING_ENABLE
#include "rule_lighting.h"
#endif
```

#### Force Flush Hook

```c
void eeconfig_force_flush_rgb_matrix(void) {
    uprintf("eeconfig_force_flush_rgb_matrix called\n");
#ifdef RULE_LIGHTING_ENABLE
    rule_lighting_save();
#endif
    eeconfig_flush_rgb_matrix(true);
}
```

**Purpose**: When RGB Matrix config is saved to EEPROM, also save rule lighting data. This ensures both are persisted together.

#### Reset Hook

```c
void eeconfig_update_rgb_matrix_default(void) {
    dprintf("eeconfig_update_rgb_matrix_default\n");
    rgb_matrix_config.enable = RGB_MATRIX_DEFAULT_ON;
    rgb_matrix_config.mode   = RGB_MATRIX_DEFAULT_MODE;
    rgb_matrix_config.hsv    = (hsv_t){RGB_MATRIX_DEFAULT_HUE, RGB_MATRIX_DEFAULT_SAT, RGB_MATRIX_DEFAULT_VAL};
    rgb_matrix_config.speed  = RGB_MATRIX_DEFAULT_SPD;
    rgb_matrix_config.flags  = RGB_MATRIX_DEFAULT_FLAGS;
#ifdef RULE_LIGHTING_ENABLE
    rule_lighting_reset();
#endif
    eeconfig_flush_rgb_matrix(true);
}
```

**Purpose**: When RGB Matrix is reset to defaults (uninitialized EEPROM), also reset rule lighting to defaults.

---

## Split Keyboard Support

Split keyboard support requires additional configuration and a custom `hooks_base.c` implementation.

### Split Keyboard Configuration

**Location**: Keyboard's `config.h`

```c
// Define custom split transaction IDs
#define SPLIT_TRANSACTION_IDS_KB SPLIT_RULE_LIGHTING_SYNC_ID, SPLIT_KEYMAP_SYNC_ID

#ifdef RULE_LIGHTING_ENABLE
    // Number of rule entries (optional, uses default based on EEPROM)
    #define RULE_LIGHTING_ENTRIES 16

    // Enable state syncing for rule lighting
    #define SPLIT_LAYER_STATE_ENABLE
    #define SPLIT_LED_STATE_ENABLE
    #define SPLIT_MODS_ENABLE

    // IMPORTANT: Do NOT use SPLIT_TRANSPORT_MIRROR
    // Each half calculates its own RGB using synced data
#endif
```

**Key Points**:
1. **Custom Transaction IDs**: Two custom IDs for rule lighting sync and keymap sync
2. **State Syncing**: Layer, LED (caps lock), and mods state must be synced
3. **No Transport Mirror**: Each half renders its own LEDs independently

### hooks_base.c - Split Sync Implementation

**Location**: Keyboard's `keymaps/hooks_base.c`

This file implements the split keyboard synchronization for rule lighting.

#### Includes

```c
#include "transactions.h"
#include "rule_lighting.h"
#include "dynamic_keymap.h"
#include "keymap_introspection.h"
#include "action_layer.h"
#include <string.h>
```

#### Rule Lighting Slave Callback

```c
void rule_lighting_slave_callback(uint8_t m2s_size, const void *m2s_buffer,
                                   uint8_t s2m_size, void *s2m_buffer) {
    if (m2s_size == sizeof(rule_lighting_sync_t)) {
        rule_lighting_apply_sync_data((const rule_lighting_sync_t *)m2s_buffer);
    }
}
```

**Purpose**: Slave callback that receives rule lighting data from master and applies it to RAM.

#### Keymap Sync Data Structure

```c
typedef struct {
    uint8_t counter;
    uint8_t layer;
    uint16_t keycodes[MATRIX_ROWS][MATRIX_COLS];
} __attribute__((packed)) keymap_layer_sync_t;
```

**Purpose**: Holds one layer of keymap data plus a change counter for sync integrity.

#### Slave Keymap Storage

```c
static uint16_t synced_keymap[DYNAMIC_KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
static uint8_t synced_counter = 0xFF;       // Counter for completed sync
static uint8_t pending_counter = 0xFF;      // Counter currently being synced
static uint8_t pending_mask = 0;            // Layers received for pending_counter
```

**Purpose**: RAM storage on slave for the synchronized keymap.

- `synced_keymap`: The complete keymap (all layers)
- `synced_counter`: Counter value of last successfully completed sync
- `pending_counter`: Counter value of sync in progress
- `pending_mask`: Bitmask of which layers have been received

#### Keymap Slave Callback

```c
void keymap_slave_callback(uint8_t m2s_size, const void *m2s_buffer,
                           uint8_t s2m_size, void *s2m_buffer) {
    // Master is querying our counter
    if (m2s_size == 0 && s2m_size == 1) {
        *(uint8_t *)s2m_buffer = synced_counter;
        return;
    }

    // Master is sending layer data
    if (m2s_size == sizeof(keymap_layer_sync_t)) {
        const keymap_layer_sync_t *data = (const keymap_layer_sync_t *)m2s_buffer;

        if (data->layer >= DYNAMIC_KEYMAP_LAYER_COUNT) return;

        // New counter? Reset pending state
        if (data->counter != pending_counter) {
            pending_counter = data->counter;
            pending_mask = 0;
        }

        // Store layer data
        memcpy(synced_keymap[data->layer], data->keycodes, sizeof(data->keycodes));
        pending_mask |= (1 << data->layer);

        // All layers received? Commit the sync
        uint8_t all_layers = (1 << DYNAMIC_KEYMAP_LAYER_COUNT) - 1;
        if (pending_mask == all_layers) {
            synced_counter = pending_counter;
        }
    }
}
```

**Purpose**: Handles two types of transactions:
1. **Counter Query**: Master sends 0 bytes, slave returns its current synced_counter
2. **Layer Data**: Master sends layer data, slave stores it and checks for completion

**Sync Integrity**: The counter ensures that if the keymap changes mid-sync, the old partial data is discarded.

#### KC_TRNS Resolution

```c
static uint16_t resolve_keycode(uint8_t layer, uint8_t row, uint8_t col, bool use_synced) {
    if (use_synced && synced_counter == 0xFF) {
        return KC_NO;  // No valid sync yet
    }

    for (int8_t l = layer; l >= 0; l--) {
        uint16_t kc = use_synced ? synced_keymap[l][row][col]
                                 : dynamic_keymap_get_keycode(l, row, col);
        if (kc != KC_TRNS) {
            return kc;
        }
    }
    return KC_NO;
}
```

**Purpose**: Resolves `KC_TRNS` by walking down through lower layers until a non-transparent keycode is found.

#### get_synced_keycode() Override

```c
uint16_t get_synced_keycode(uint8_t layer, uint8_t row, uint8_t col) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KC_NO;
    }
    return resolve_keycode(layer, row, col, !is_keyboard_master());
}
```

**Purpose**: Overrides the weak default in `rule_lighting.c`.
- **Master**: Uses `dynamic_keymap_get_keycode()` (EEPROM/RAM)
- **Slave**: Uses `synced_keymap[]` (synced RAM)

#### Callback Registration

```c
void keyboard_post_init_user(void) {
#if defined(SPLIT_KEYBOARD) && defined(RGB_MATRIX_ENABLE) && defined(RULE_LIGHTING_ENABLE)
    transaction_register_rpc(SPLIT_RULE_LIGHTING_SYNC_ID, rule_lighting_slave_callback);
    transaction_register_rpc(SPLIT_KEYMAP_SYNC_ID, keymap_slave_callback);
#endif
}
```

**Purpose**: Registers the slave callbacks during keyboard initialization.

#### Master Sync Task

```c
void hooks_housekeeping_task_user() {
    // ... brightness limiting code ...

#if defined(SPLIT_KEYBOARD) && defined(RGB_MATRIX_ENABLE) && defined(RULE_LIGHTING_ENABLE)
    if (is_keyboard_master()) {
        static uint32_t last_check = 0;
        static uint8_t sync_layer = 0xFF;  // 0xFF = idle, 0-3 = syncing layer N
        static uint8_t sync_counter = 0;

        uint32_t interval = (sync_layer == 0xFF) ? 5000 : 100;

        if (timer_elapsed32(last_check) > interval) {
            last_check = timer_read32();

            if (sync_layer == 0xFF) {
                // Idle: query slave counter and send rule lighting
                uint8_t slave_counter = 0xFF;
                bool ok = transaction_rpc_exec(SPLIT_KEYMAP_SYNC_ID, 0, NULL, 1, &slave_counter);
                uint8_t master_counter = dynamic_keymap_get_change_counter();

                if (ok && slave_counter != master_counter) {
                    sync_layer = 0;
                    sync_counter = master_counter;
                }

                // Always sync rule lighting
                const rule_lighting_sync_t *sync_data = rule_lighting_get_sync_data();
                transaction_rpc_send(SPLIT_RULE_LIGHTING_SYNC_ID,
                                     sizeof(rule_lighting_sync_t), sync_data);
            } else {
                // Syncing: send one layer
                keymap_layer_sync_t layer_data;
                layer_data.counter = sync_counter;
                layer_data.layer = sync_layer;
                for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                        layer_data.keycodes[row][col] =
                            dynamic_keymap_get_keycode(sync_layer, row, col);
                    }
                }
                transaction_rpc_send(SPLIT_KEYMAP_SYNC_ID,
                                     sizeof(keymap_layer_sync_t), &layer_data);

                sync_layer++;
                if (sync_layer >= DYNAMIC_KEYMAP_LAYER_COUNT) {
                    sync_layer = 0xFF;  // Done
                }
            }
        }
    }
#endif
}
```

**Purpose**: Master-side sync logic that runs in `housekeeping_task_user`.

**Sync State Machine**:
1. **Idle State** (`sync_layer == 0xFF`):
   - Every 5 seconds: Query slave's synced_counter
   - If counters don't match: Start keymap sync (set `sync_layer = 0`)
   - Always: Send rule lighting sync data

2. **Syncing State** (`sync_layer == 0..N`):
   - Every 100ms: Send one layer of keymap data
   - Increment `sync_layer` after each send
   - When all layers sent: Return to idle state

---

## Data Structures

### Summary of All Structures

| Structure | Size | Purpose |
|-----------|------|---------|
| `rule_lighting_entry_t` | 8 bytes | Single rule definition |
| `rule_lighting_config_t` | 1 byte | Global configuration |
| `rule_lighting_sync_t` | 1 + 8*N bytes | Split sync container |
| `keymap_layer_sync_t` | Variable | One layer of keymap for sync |

### rule_lighting_entry_t Bit Layout

```
Byte 0:  [layer_enable:1][caps_enable:1][reserved:2][layer:4]
Byte 1:  [mods:8]
Bytes 2-3: keycode_start (uint16_t, little-endian)
Bytes 4-5: keycode_end (uint16_t, little-endian)
Byte 6:  [sat_idle:2][h_idle:6]
Byte 7:  [sat_pressed:2][h_pressed:6]
```

---

## Algorithm Details

### Rule Matching Algorithm

```
FOR each rule from 0 to entry_count-1:
    IF layer_enable AND current_layer != rule.layer:
        CONTINUE (skip rule)

    IF rule.mods != 0 AND (current_mods & rule.mods) == 0:
        CONTINUE (skip rule)

    IF caps_enable AND NOT caps_on:
        CONTINUE (skip rule)

    IF keycode >= rule.keycode_start AND keycode <= rule.keycode_end:
        RETURN rule index (match found)

RETURN -1 (no match)
```

### Color Animation Algorithm

```
scaled_time = tick * (speed + 1) / 256
blend = MIN(scaled_time, 255)

IF blend == 255 (fully idle):
    IF idle_off:
        color = BLACK
    ELSE:
        color = (h_idle, s_idle, brightness)
ELSE (transitioning):
    IF pressed_off:
        // Fade in from black
        color = (h_idle, s_idle, brightness * blend / 255)
    ELSE IF idle_off:
        // Fade out to black
        color = (h_pressed, s_pressed, brightness * (255 - blend) / 255)
    ELSE:
        // Blend between colors
        color = (lerp(h_pressed, h_idle, blend),
                 lerp(s_pressed, s_idle, blend),
                 brightness)
```

### Split Sync Algorithm

**Master Side**:
```
Every 5 seconds (when idle):
    Query slave counter
    IF slave_counter != master_counter:
        Start full keymap sync

    Send rule_lighting_sync_t to slave

Every 100ms (when syncing):
    Send keymap_layer_sync_t for current layer
    Increment layer
    IF all layers sent:
        Return to idle
```

**Slave Side**:
```
On rule lighting sync receive:
    Apply data to RAM

On keymap counter query:
    Return synced_counter

On keymap layer receive:
    IF new counter:
        Reset pending state

    Store layer data
    Mark layer as received

    IF all layers received:
        Commit sync (synced_counter = pending_counter)
```

---

## How to Enable

### 1. Keyboard rules.mk

```makefile
# Enable required features
VIA_ENABLE = yes
VIAL_ENABLE = yes
RGB_MATRIX_ENABLE = yes
VIALRGB_ENABLE = yes
RULE_LIGHTING_ENABLE = yes
```

### 2. Keyboard config.h (Non-Split)

```c
// Optional: Override default entry count
#define RULE_LIGHTING_ENTRIES 16
```

### 3. Keyboard config.h (Split)

```c
// Required: Define custom transaction IDs
#define SPLIT_TRANSACTION_IDS_KB SPLIT_RULE_LIGHTING_SYNC_ID, SPLIT_KEYMAP_SYNC_ID

#ifdef RULE_LIGHTING_ENABLE
    // Optional: Override default entry count
    #define RULE_LIGHTING_ENTRIES 16

    // Required: Enable state syncing
    #define SPLIT_LAYER_STATE_ENABLE
    #define SPLIT_LED_STATE_ENABLE
    #define SPLIT_MODS_ENABLE

    // Important: Do NOT define SPLIT_TRANSPORT_MIRROR
#endif
```

### 4. Split Keyboard: hooks_base.c

Create a `hooks_base.c` file in the keymap directory with the sync implementation as shown in the [Split Keyboard Support](#hooks_basec---split-sync-implementation) section.

### 5. Include hooks_base.c in keymap.c

```c
#include "../hooks_base.c"

void housekeeping_task_user(void) {
    hooks_housekeeping_task_user();
}
```

---

## File Summary

| File | Role |
|------|------|
| `quantum/rule_lighting.h` | Data structures, constants, API declarations |
| `quantum/rule_lighting.c` | Core logic, RAM cache, EEPROM interface |
| `quantum/rgb_matrix/animations/rule_lighting_anim.h` | RGB Matrix effect implementation |
| `quantum/nvm/eeprom/nvm_dynamic_keymap.c` | EEPROM storage functions |
| `quantum/vial.h` | Protocol command IDs |
| `quantum/vial.c` | Protocol handlers |
| `quantum/vialrgb_effects.inc` | VialRGB effect ID mapping |
| `quantum/rgb_matrix/animations/rgb_matrix_effects.inc` | Effect include list |
| `quantum/rgb_matrix/rgb_matrix.c` | Init/save hooks |
| `builddefs/build_vial.mk` | Build system integration |
| `keymaps/hooks_base.c` (keyboard) | Split keyboard sync implementation |

---

## Version Information

- Branch: `rl`
- Base: `merge-2025-12-28`
- Documentation Date: 2026-01-06
