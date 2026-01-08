# Rule Lighting Implementation Documentation (HEAD Branch)

This document provides a comprehensive description of the Rule Lighting feature implementation in the HEAD branch (`splitted-space-rule-lighting`). The documentation is detailed enough to enable reimplementation from scratch.

## Table of Contents

1. [Overview](#overview)
2. [Feature Architecture](#feature-architecture)
3. [File-by-File Documentation](#file-by-file-documentation)
   - [quantum/rule_lighting.h](#quantumrule_lightingh)
   - [quantum/rule_lighting.c](#quantumrule_lightingc)
   - [quantum/rgb_matrix/animations/rule_lighting_anim.h](#quantumrgb_matrixanimationsrule_lighting_animh)
   - [quantum/vial.h](#quantumvialh)
   - [quantum/vial.c](#quantumvialc)
   - [quantum/nvm/eeprom/nvm_dynamic_keymap.c](#quantumnvmeepromnvm_dynamic_keymapc)
   - [quantum/split_common/transaction_id_define.h](#quantumsplit_commontransaction_id_defineh)
   - [quantum/split_common/transport.h](#quantumsplit_commontransporth)
   - [quantum/rgb_matrix/rgb_matrix.c](#quantumrgb_matrixrgb_matrixc)
   - [quantum/keyboard.c](#quantumkeyboardc)
   - [quantum/dynamic_keymap.h and .c](#quantumdynamic_keymaph-and-c)
   - [quantum/rgb_matrix/animations/rgb_matrix_effects.inc](#quantumrgb_matrixanimationsrgb_matrix_effectsinc)
4. [Data Structures](#data-structures)
5. [Split Keyboard Synchronization](#split-keyboard-synchronization)
6. [Enabling the Feature](#enabling-the-feature)
7. [EEPROM Layout](#eeprom-layout)

---

## Overview

Rule Lighting is a QMK RGB Matrix effect that colors individual keys based on configurable rules. Each rule can match:

- **Layer state**: Which layer is currently active (0-15)
- **Modifier state**: Which modifiers are held (8-bit modifier mask)
- **Caps lock state**: Whether caps lock is enabled
- **Keycode ranges**: Which keycodes should be colored (16-bit start/end range)

Each rule specifies two colors:
- **Idle color**: Displayed when the key is not pressed
- **Pressed color**: Displayed when the key is pressed (with smooth transition)

Rules are stored in EEPROM and can be configured via the Vial protocol. For split keyboards, the feature automatically syncs rule configuration and keymap data from master to slave so both halves can render correctly.

---

## Feature Architecture

The feature consists of several interconnected components:

```
+------------------+     +-----------------+     +-------------------+
|   rule_lighting  |---->| rule_lighting   |---->|  RGB Matrix       |
|       .h         |     |      .c         |     |  Animation Effect |
+------------------+     +-----------------+     +-------------------+
        |                        |                        |
        v                        v                        v
+------------------+     +-----------------+     +-------------------+
|   vial.h/c       |     | nvm_dynamic_    |     | rule_lighting_    |
|  (Vial protocol) |     | keymap.c        |     |    anim.h         |
+------------------+     | (EEPROM)        |     +-------------------+
                         +-----------------+
                                 |
                                 v
                         +-----------------+
                         | Split Keyboard  |
                         | Transactions    |
                         +-----------------+
```

---

## File-by-File Documentation

### quantum/rule_lighting.h

**Responsibility**: Defines all data structures, configuration macros, and function prototypes for the rule lighting system.

#### Configuration Macros

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

**Purpose**: Automatically determines the number of rule entries based on available EEPROM size. Can be overridden in `config.h`. Each entry is 8 bytes, plus 1 byte for config.

#### Split Keyboard Auto-Configuration

When `SPLIT_KEYBOARD` and `RULE_LIGHTING_ENABLE` are both defined:

```c
/* RPC buffer size calculation */
#define _RL_SYNC_SIZE (1 + 8 * RULE_LIGHTING_ENTRIES)
#define _KM_SYNC_SIZE (2 + MATRIX_ROWS * MATRIX_COLS * 2)
#define _RL_RPC_BUFFER_SIZE ((_RL_SYNC_SIZE > _KM_SYNC_SIZE) ? _RL_SYNC_SIZE : _KM_SYNC_SIZE)
```

**Purpose**: Calculates the maximum buffer size needed for either syncing rule lighting data OR keymap data, and sets `RPC_M2S_BUFFER_SIZE` to the larger value.

Also auto-enables required split state syncing:
- `SPLIT_LAYER_STATE_ENABLE` - for layer state sync
- `SPLIT_LED_STATE_ENABLE` - for caps lock sync
- `SPLIT_MODS_ENABLE` - for modifier state sync

#### Saturation Modes Enum

```c
typedef enum {
    VIAL_RGB_SAT_OFF    = 0b00,  // LED off (RGB 0,0,0)
    VIAL_RGB_SAT_WHITE  = 0b01,  // S=0 (white/grayscale)
    VIAL_RGB_SAT_PASTEL = 0b10,  // S=128 (soft color)
    VIAL_RGB_SAT_PURE   = 0b11,  // S=255 (vivid color)
} vial_rgb_saturation_t;
```

**Purpose**: Encodes saturation in 2 bits to save EEPROM space. The `VIAL_RGB_SAT_OFF` value means the LED should be turned off for that state.

#### Rule Entry Structure (8 bytes)

```c
typedef struct {
    /* Condition (16 bits) */
    uint16_t layer_enable   : 1;   // 0 = any layer, 1 = match layer field
    uint16_t caps_enable    : 1;   // 0 = any caps state, 1 = caps must be ON
    uint16_t reserved       : 2;   // Reserved for future use
    uint16_t layer          : 4;   // Layer to match (0-15)
    uint16_t mods           : 8;   // Modifier mask (0 = any)

    /* Keycode range (32 bits) */
    uint16_t keycode_start;        // Start of keycode range (inclusive)
    uint16_t keycode_end;          // End of keycode range (inclusive)

    /* Colors (16 bits) */
    uint8_t  sat_idle       : 2;   // Saturation mode for idle
    uint8_t  h_idle         : 6;   // Hue for idle (6-bit, use << 2 for 8-bit)
    uint8_t  sat_pressed    : 2;   // Saturation mode for pressed
    uint8_t  h_pressed      : 6;   // Hue for pressed (6-bit)

} __attribute__((packed)) rule_lighting_entry_t;
```

**Purpose**: Compact 8-byte representation of a single rule. The packed structure ensures exact memory layout for EEPROM storage and split sync.

**Condition Logic**:
- If `layer_enable=1`, the rule only matches when `current_layer == layer`
- If `mods != 0`, the rule only matches when at least one matching modifier is held (OR logic)
- If `caps_enable=1`, the rule only matches when caps lock is ON
- The keycode range check is always performed: `keycode_start <= keycode <= keycode_end`

#### Config Structure (1 byte)

```c
typedef struct {
    uint8_t entry_count;    // Number of active rules (0-255)
} __attribute__((packed)) rule_lighting_config_t;
```

**Purpose**: Stores the count of active rules. Rules are evaluated in order (first-match-wins), so only the first `entry_count` rules are checked.

#### Sync Data Structure

```c
typedef struct {
    rule_lighting_config_t config;
    rule_lighting_entry_t entries[RULE_LIGHTING_ENTRIES];
} __attribute__((packed)) rule_lighting_sync_t;
```

**Purpose**: Complete rule lighting state for split keyboard transport. Contains config + all entries.

#### Function Prototypes

| Function | Purpose |
|----------|---------|
| `void rule_lighting_init(void)` | Early init - load from EEPROM (called from `rgb_matrix_init()`) |
| `void rule_lighting_post_init(void)` | Register split RPC handlers (called from `keyboard_init()` after `split_post_init()`) |
| `void rule_lighting_task(void)` | Housekeeping task, handles split sync on master |
| `void rule_lighting_reset(void)` | Reset to defaults, save blank state to EEPROM |
| `void rule_lighting_load(void)` | Load config and rules from EEPROM |
| `void rule_lighting_save(void)` | Save all data to EEPROM |
| `rule_lighting_entry_t* rule_lighting_get_entry(uint8_t index)` | Get pointer to a rule entry |
| `void rule_lighting_set_entry(uint8_t index, const rule_lighting_entry_t *entry)` | Set a rule entry (RAM only) |
| `rule_lighting_config_t* rule_lighting_get_config(void)` | Get pointer to config |
| `void rule_lighting_set_config(const rule_lighting_config_t *config)` | Set config (RAM only) |
| `const rule_lighting_sync_t* rule_lighting_get_sync_data(void)` | Get sync data for split transport |
| `void rule_lighting_apply_sync_data(const rule_lighting_sync_t *sync_data)` | Apply received sync data on slave |
| `uint16_t get_synced_keycode(uint8_t layer, uint8_t row, uint8_t col)` | Get keycode with split awareness |

#### Helper Macros/Functions

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

---

### quantum/rule_lighting.c

**Responsibility**: Implements the core rule lighting logic, EEPROM persistence, and split keyboard synchronization.

#### RAM Cache

```c
static rule_lighting_config_t indicator_config;
static rule_lighting_entry_t indicator_entries[RULE_LIGHTING_ENTRIES];
```

**Purpose**: All operations work on RAM cache. Changes are persisted to EEPROM only when `rule_lighting_save()` is called.

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

**Purpose**: Clears RAM cache to a blank/disabled state. Used during reset and slave initialization.

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

**Purpose**: Resets to defaults and persists the blank state to EEPROM. Called from `eeconfig_update_rgb_matrix_default()` when RGB Matrix is uninitialized.

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

**Purpose**: Loads config and rules from EEPROM. On split keyboards, only the master loads from EEPROM; the slave starts blank and receives data via split transport.

#### rule_lighting_save()

```c
void rule_lighting_save(void) {
    nvm_dynamic_keymap_set_rgb_indicator_config(&indicator_config);
    for (uint8_t i = 0; i < RULE_LIGHTING_ENTRIES; i++) {
        nvm_dynamic_keymap_set_rgb_indicator_entry(i, &indicator_entries[i]);
    }
}
```

**Purpose**: Persists all RAM cache data to EEPROM. Called from `eeconfig_force_flush_rgb_matrix()`.

#### Getter/Setter Functions

Simple accessors that operate on the RAM cache:
- `rule_lighting_get_entry()` / `rule_lighting_set_entry()` - Access individual rules
- `rule_lighting_get_config()` / `rule_lighting_set_config()` - Access global config

#### Split Keyboard Support (SPLIT_KEYBOARD section)

##### Keymap Sync Data Structure

```c
typedef struct {
    uint8_t counter;
    uint8_t layer;
    uint16_t keycodes[MATRIX_ROWS][MATRIX_COLS];
} __attribute__((packed)) keymap_layer_sync_t;
```

**Purpose**: Syncs one layer of the keymap at a time, with a counter to detect changes.

##### Slave Keymap Storage

```c
static uint16_t synced_keymap[DYNAMIC_KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
static uint8_t synced_counter = 0xFF;
static uint8_t pending_counter = 0xFF;
static uint8_t pending_mask = 0;
```

**Purpose**:
- `synced_keymap`: Complete keymap copy on slave
- `synced_counter`: Counter of last successfully synced keymap
- `pending_counter`: Counter currently being synced
- `pending_mask`: Bitmask of layers received for current sync

##### rule_lighting_slave_handler()

```c
static void rule_lighting_slave_handler(uint8_t m2s_size, const void *m2s_buffer,
                                         uint8_t s2m_size, void *s2m_buffer) {
    if (m2s_size == sizeof(rule_lighting_sync_t)) {
        rule_lighting_apply_sync_data((const rule_lighting_sync_t *)m2s_buffer);
    }
}
```

**Purpose**: RPC handler on slave for receiving rule lighting configuration.

##### keymap_slave_handler()

```c
static void keymap_slave_handler(uint8_t m2s_size, const void *m2s_buffer,
                                  uint8_t s2m_size, void *s2m_buffer) {
    // Query mode: return current counter
    if (m2s_size == 0 && s2m_size == 1) {
        *(uint8_t *)s2m_buffer = synced_counter;
        return;
    }

    // Data mode: receive layer data
    if (m2s_size == sizeof(keymap_layer_sync_t)) {
        const keymap_layer_sync_t *data = (const keymap_layer_sync_t *)m2s_buffer;
        // Handle counter change, store layer, update pending_mask
        // When all layers received, commit synced_counter
    }
}
```

**Purpose**: RPC handler on slave for receiving keymap data and responding to counter queries. Uses a counter-based protocol:
1. Master queries slave's counter
2. If counters differ, master sends all layers one by one
3. Each layer includes the counter value
4. Slave tracks which layers received
5. When all layers received with same counter, commit the sync

##### resolve_keycode()

```c
static uint16_t resolve_keycode(uint8_t layer, uint8_t row, uint8_t col, bool use_synced) {
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

**Purpose**: Resolves `KC_TRNS` (transparent) keycodes by checking lower layers until a non-transparent keycode is found. Uses synced keymap on slave, dynamic keymap on master.

##### get_synced_keycode()

```c
uint16_t get_synced_keycode(uint8_t layer, uint8_t row, uint8_t col) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KC_NO;
    }
    return resolve_keycode(layer, row, col, !is_keyboard_master());
}
```

**Purpose**: Public API for getting keycodes with split awareness. Master uses dynamic_keymap, slave uses synced_keymap.

##### rule_lighting_master_sync()

```c
static void rule_lighting_master_sync(void) {
    static uint32_t last_check = 0;
    static uint8_t sync_layer = 0xFF;  // 0xFF = idle, 0-N = syncing layer N
    static uint8_t sync_counter = 0;

    uint32_t interval = (sync_layer == 0xFF) ? 5000 : 100;

    if (timer_elapsed32(last_check) > interval) {
        last_check = timer_read32();

        if (sync_layer == 0xFF) {
            // Idle: query slave counter, send rule lighting
            // If counters differ, start keymap sync
        } else {
            // Syncing: send one layer, advance to next
        }
    }
}
```

**Purpose**: Master-side sync task. Runs two modes:
1. **Idle mode** (every 5 seconds): Query slave's counter, always sync rule lighting config, start keymap sync if counters differ
2. **Syncing mode** (every 100ms): Send one layer at a time until all layers sent

#### rule_lighting_init()

```c
void rule_lighting_init(void) {
    rule_lighting_load();
    /* Split RPC registration moved to rule_lighting_post_init()
     * which is called from keyboard_init() after split_post_init() */
}
```

**Purpose**: Early initialization - loads config and rules from EEPROM. Called from `rgb_matrix_init()` during `keyboard_init()`.

**Important**: This function does NOT register split RPC handlers. That happens later in `rule_lighting_post_init()`.

#### rule_lighting_post_init()

```c
void rule_lighting_post_init(void) {
    transaction_register_rpc(SPLIT_RULE_LIGHTING_SYNC_ID, rule_lighting_slave_handler);
    transaction_register_rpc(SPLIT_KEYMAP_SYNC_ID, keymap_slave_handler);
}
```

**Purpose**: Registers split keyboard RPC handlers. Called from `keyboard_init()` AFTER `split_post_init()` completes.

**Critical Timing**: This must be called after `split_post_init()` because:
1. `split_post_init()` initializes the slave transport layer
2. RPC handlers registered before transport initialization are ignored
3. The slave half would not receive sync data if handlers are registered too early

The call chain in `keyboard_init()` is:
```
split_pre_init()       // Determines master/slave
rgb_matrix_init()      // Calls rule_lighting_init() - loads EEPROM
split_post_init()      // Initializes transport
rule_lighting_post_init()  // Registers RPC handlers (NEW)
keyboard_post_init_kb()
```

#### rule_lighting_task()

```c
void rule_lighting_task(void) {
#ifdef SPLIT_KEYBOARD
    if (is_keyboard_master()) {
        rule_lighting_master_sync();
    }
#endif
}
```

**Purpose**: Housekeeping task called from `rgb_matrix_task()`. On master, runs the sync logic.

---

### quantum/rgb_matrix/animations/rule_lighting_anim.h

**Responsibility**: Implements the actual RGB Matrix effect that renders colors based on rules.

#### Guard Macros

```c
#ifdef RULE_LIGHTING_ENABLE
#define RGB_MATRIX_EFFECT_RULE_LIGHTING
RGB_MATRIX_EFFECT(RULE_LIGHTING)
#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS
// ... implementation ...
#endif
#endif
```

**Purpose**: Standard QMK RGB Matrix effect registration pattern. Creates the `RULE_LIGHTING` effect in the effect enum.

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

        // Layer check
        if (rule->layer_enable && current_layer != rule->layer) continue;

        // Mods check (OR logic)
        if (rule->mods != 0 && (current_mods & rule->mods) == 0) continue;

        // Caps check
        if (rule->caps_enable && !caps_on) continue;

        // Keycode range check
        if (keycode >= rule->keycode_start && keycode <= rule->keycode_end) {
            return i;
        }
    }
    return -1;
}
```

**Purpose**: Finds the first matching rule for a given keycode and keyboard state. Returns rule index or -1 if no match.

**Matching Logic**:
1. Rules are evaluated in order (first-match-wins)
2. Only the first `entry_count` rules are checked
3. All enabled conditions must match for a rule to match
4. Layer match: if `layer_enable=1`, current layer must equal rule's layer
5. Mods match: if `mods != 0`, at least one modifier in the mask must be held (OR)
6. Caps match: if `caps_enable=1`, caps lock must be on
7. Keycode match: keycode must be within the inclusive range

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

**Purpose**: Gets the time since last keypress for an LED. Uses QMK's `g_last_hit_tracker` which is populated when `RGB_MATRIX_KEYPRESSES` is enabled. Returns maximum tick if key reactive is disabled.

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
        // Key was recently pressed - blend between pressed and idle
        if (pressed_off) {
            // Pressed color is off - fade in from black to idle
            *h = h_idle;
            *s = s_idle;
            *v = scale8(brightness, blend);
        } else if (idle_off) {
            // Idle color is off - fade out from pressed to black
            *h = h_pressed;
            *s = s_pressed;
            *v = scale8(brightness, 255 - blend);
        } else {
            // Both colors on - blend between them
            *h = lerp8by8(h_pressed, h_idle, blend);
            *s = lerp8by8(s_pressed, s_idle, blend);
            *v = brightness;
        }
    } else {
        // Key is idle - show idle color
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

**Purpose**: Calculates the HSV color for a key based on its rule and press state.

**Color Blending Logic**:
- `blend` is 0 immediately after keypress, increases toward 255 over time
- Speed from RGB Matrix settings controls how fast blend increases
- When `blend < 255`, we're transitioning from pressed to idle color
- Uses QMK's `lerp8by8` for smooth interpolation
- Special handling for OFF states (fade in/out instead of color blend)

#### RULE_LIGHTING() Effect Function

```c
bool RULE_LIGHTING(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    uint8_t current_layer = get_highest_layer(layer_state);
    uint8_t current_mods = get_mods() | get_weak_mods();
#ifndef NO_ACTION_ONESHOT
    current_mods |= get_oneshot_mods();
#endif
    bool caps_on = host_keyboard_led_state().caps_lock;

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

**Purpose**: Main effect function called by RGB Matrix subsystem.

**Processing Steps**:
1. Get current keyboard state (layer, mods, caps lock)
2. Iterate over entire key matrix
3. For each position with an LED in the current processing range:
   - Get the keycode using `get_synced_keycode()` (split-aware)
   - Find matching rule
   - If match found: calculate color based on rule and press state
   - If no match: set to black (0,0,0)
4. Return whether all LEDs are processed

**Important**: Uses `get_synced_keycode()` which returns the correct keycode on both master and slave halves of split keyboards.

---

### quantum/vial.h

**Responsibility**: Defines Vial protocol constants and structures, including rule lighting commands.

#### Rule Lighting Command IDs

```c
enum {
    // ... other commands ...
    dynamic_rule_lighting_get_config = 0x09,
    dynamic_rule_lighting_set_config = 0x0A,
    dynamic_rule_lighting_get_entry = 0x0B,
    dynamic_rule_lighting_set_entry = 0x0C,
};
```

**Purpose**: Defines the Vial protocol sub-commands for rule lighting under `vial_dynamic_entry_op` (0x0D).

#### Entry Count Reporting

```c
case dynamic_vial_get_number_of_entries: {
    memset(msg, 0, length);
    msg[0] = VIAL_TAP_DANCE_ENTRIES;
    msg[1] = VIAL_COMBO_ENTRIES;
    msg[2] = VIAL_KEY_OVERRIDE_ENTRIES;
    msg[3] = VIAL_ALT_REPEAT_KEY_ENTRIES;
    msg[4] = RULE_LIGHTING_ENTRIES;  // <-- Added for rule lighting
    // ...
}
```

**Purpose**: Reports the number of available rule lighting entries to the Vial GUI.

#### Feature Enable Check

```c
#if defined(RGB_MATRIX_ENABLE) && defined(RULE_LIGHTING_ENABLE)
#include "rule_lighting.h"
#else
#undef RULE_LIGHTING_ENTRIES
#define RULE_LIGHTING_ENTRIES 0
#endif
```

**Purpose**: Only includes rule_lighting.h when both RGB_MATRIX_ENABLE and RULE_LIGHTING_ENABLE are defined. Otherwise sets entries to 0.

---

### quantum/vial.c

**Responsibility**: Implements Vial protocol handlers including rule lighting get/set commands.

#### Rule Lighting Handlers

```c
#ifdef RULE_LIGHTING_ENABLE
case dynamic_rule_lighting_get_config: {
    rule_lighting_config_t *config = rule_lighting_get_config();
    memset(msg, 0, length);
    if (config) {
        msg[0] = 0;  // success
        memcpy(&msg[1], config, sizeof(rule_lighting_config_t));
    } else {
        msg[0] = 1;  // error
    }
    break;
}
case dynamic_rule_lighting_set_config: {
    rule_lighting_config_t config;
    memcpy(&config, &msg[3], sizeof(config));
    rule_lighting_set_config(&config);
    msg[0] = 0;  // success
    break;
}
case dynamic_rule_lighting_get_entry: {
    uint8_t idx = msg[3];
    rule_lighting_entry_t *entry = rule_lighting_get_entry(idx);
    memset(msg, 0, length);
    if (entry) {
        memcpy(&msg[1], entry, sizeof(rule_lighting_entry_t));
        msg[0] = 0;  // success
    } else {
        msg[0] = 1;  // error
    }
    break;
}
case dynamic_rule_lighting_set_entry: {
    uint8_t idx = msg[3];
    rule_lighting_entry_t entry;
    memcpy(&entry, &msg[4], sizeof(entry));
    rule_lighting_set_entry(idx, &entry);
    msg[0] = 0;  // success
    break;
}
#endif
```

**Purpose**: Handles USB HID messages from Vial GUI:
- **get_config**: Returns the current config (entry_count)
- **set_config**: Sets new config (updates RAM, EEPROM written via flush)
- **get_entry**: Returns a rule entry by index
- **set_entry**: Sets a rule entry by index

**Note**: These handlers only modify RAM. EEPROM persistence happens when `eeconfig_force_flush_rgb_matrix()` is called.

#### Initialization Note

```c
void vial_init(void) {
    // ... other init ...
    // Note: rule_lighting_init() is called from rgb_matrix_init() instead,
    // because is_keyboard_master() is not valid until split_pre_init() runs
}
```

**Purpose**: Documents why rule_lighting_init() is not called from vial_init() - the split keyboard master detection isn't ready yet.

---

### quantum/nvm/eeprom/nvm_dynamic_keymap.c

**Responsibility**: Provides EEPROM storage for rule lighting configuration.

#### EEPROM Address Calculation

```c
// RGB Indicator
#define RULE_LIGHTING_EEPROM_ADDR (VIAL_ALT_REPEAT_KEY_EEPROM_ADDR + VIAL_ALT_REPEAT_KEY_SIZE)

#ifdef RULE_LIGHTING_ENABLE
#include "rule_lighting.h"
#define RULE_LIGHTING_CONFIG_SIZE (sizeof(rule_lighting_config_t))
#define RULE_LIGHTING_ENTRIES_SIZE (sizeof(rule_lighting_entry_t) * RULE_LIGHTING_ENTRIES)
#define RULE_LIGHTING_SIZE (RULE_LIGHTING_CONFIG_SIZE + RULE_LIGHTING_ENTRIES_SIZE)
#else
#define RULE_LIGHTING_SIZE 0
#endif

// Dynamic macro starts after rule lighting
#define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (RULE_LIGHTING_EEPROM_ADDR + RULE_LIGHTING_SIZE)
```

**Purpose**: Calculates EEPROM addresses. Rule lighting is stored after alt repeat key entries, before dynamic macros.

**Layout**: `[config: 1 byte][entry 0: 8 bytes][entry 1: 8 bytes]...[entry N-1: 8 bytes]`

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
    if (index >= RULE_LIGHTING_ENTRIES) return -1;
    void *address = (void*)(RULE_LIGHTING_EEPROM_ADDR + RULE_LIGHTING_CONFIG_SIZE
                            + index * sizeof(rule_lighting_entry_t));
    eeprom_read_block(entry, address, sizeof(rule_lighting_entry_t));
    return 0;
}

int nvm_dynamic_keymap_set_rgb_indicator_entry(uint8_t index, const rule_lighting_entry_t *entry) {
    if (index >= RULE_LIGHTING_ENTRIES) return -1;
    void *address = (void*)(RULE_LIGHTING_EEPROM_ADDR + RULE_LIGHTING_CONFIG_SIZE
                            + index * sizeof(rule_lighting_entry_t));
    eeprom_write_block(entry, address, sizeof(rule_lighting_entry_t));
    return 0;
}
```

**Purpose**: Low-level EEPROM read/write functions for config and entries. Uses `eeprom_read_block`/`eeprom_write_block` for atomic multi-byte operations.

---

### quantum/split_common/transaction_id_define.h

**Responsibility**: Defines transaction IDs for split keyboard communication.

#### Rule Lighting Transaction IDs

```c
#if defined(SPLIT_TRANSACTION_IDS_KB) || defined(SPLIT_TRANSACTION_IDS_USER) || (defined(RULE_LIGHTING_ENABLE) && defined(SPLIT_KEYBOARD))
    PUT_RPC_INFO,
    PUT_RPC_REQ_DATA,
    EXECUTE_RPC,
    GET_RPC_RESP_DATA,
#endif

// Rule lighting transaction IDs (must come AFTER GET_RPC_RESP_DATA for validation)
#if defined(RULE_LIGHTING_ENABLE) && defined(SPLIT_KEYBOARD)
    SPLIT_RULE_LIGHTING_SYNC_ID,
    SPLIT_KEYMAP_SYNC_ID,
#endif
```

**Purpose**: Adds two new transaction IDs:
- `SPLIT_RULE_LIGHTING_SYNC_ID`: For syncing rule lighting config and entries
- `SPLIT_KEYMAP_SYNC_ID`: For syncing keymap data

**Critical**: The rule lighting transaction IDs must come AFTER `GET_RPC_RESP_DATA` in the enum. The RPC system validates that custom transaction IDs are >= `GET_RPC_RESP_DATA`. If placed before, the transactions would fail validation.

Also enables the RPC (Remote Procedure Call) infrastructure when rule lighting is enabled on split keyboards.

### quantum/split_common/transport.h

**Responsibility**: Defines split transport configuration including buffer sizes.

#### Rule Lighting Include

```c
// Allow rule_lighting to set buffer size before defaults
#if defined(RULE_LIGHTING_ENABLE) && defined(SPLIT_KEYBOARD)
#    include "rule_lighting.h"
#endif

#ifndef RPC_M2S_BUFFER_SIZE
#    define RPC_M2S_BUFFER_SIZE 32
#endif
```

**Purpose**: Includes `rule_lighting.h` early in transport.h so that the buffer size calculation in `rule_lighting.h` can set `RPC_M2S_BUFFER_SIZE` before the default value is applied. This ensures the buffer is large enough for rule lighting sync data.

---

### quantum/rgb_matrix/rgb_matrix.c

**Responsibility**: RGB Matrix core, modified to integrate rule lighting.

#### Include and Init

```c
#ifdef RULE_LIGHTING_ENABLE
#include "rule_lighting.h"
#endif

// In rgb_matrix_init():
    if (!rgb_matrix_config.mode) {
        dprintf("rgb_matrix_init_drivers rgb_matrix_config.mode = 0. Write default values to EEPROM.\n");
        eeconfig_update_rgb_matrix_default();
    }
#ifdef RULE_LIGHTING_ENABLE
    // Always initialize rule_lighting - loads config from EEPROM
    // (rule_lighting_reset was called above if mode==0, but init still needed)
    rule_lighting_init();
#endif
```

**Purpose**: Includes rule_lighting.h and calls `rule_lighting_init()` from `rgb_matrix_init()`. The init is always called (not conditionally) because even after a reset, the init function needs to run to load the (now blank) config into RAM.

### quantum/keyboard.c

**Responsibility**: Core keyboard initialization, modified to call rule lighting post-init.

#### Include and Post-Init Call

```c
#if defined(RULE_LIGHTING_ENABLE) && defined(SPLIT_KEYBOARD)
#    include "rule_lighting.h"
#endif

// In keyboard_init():
#ifdef SPLIT_KEYBOARD
    split_post_init();
#endif
#if defined(RULE_LIGHTING_ENABLE) && defined(SPLIT_KEYBOARD)
    rule_lighting_post_init();
#endif
```

**Purpose**: Calls `rule_lighting_post_init()` immediately after `split_post_init()` to register the RPC handlers at the correct time. This ensures the slave transport is initialized before handlers are registered.

#### Task Integration

```c
void rgb_matrix_task(void) {
    rgb_task_timers();

#ifdef RULE_LIGHTING_ENABLE
    rule_lighting_task();
#endif
    // ... rest of task ...
}
```

**Purpose**: Calls `rule_lighting_task()` on every RGB Matrix task cycle to handle split sync.

#### EEPROM Flush

```c
void eeconfig_force_flush_rgb_matrix(void) {
#ifdef RULE_LIGHTING_ENABLE
    rule_lighting_save();
#endif
    eeconfig_flush_rgb_matrix(true);
}
```

**Purpose**: Saves rule lighting data when EEPROM flush is requested.

#### Reset Integration

```c
void eeconfig_update_rgb_matrix_default(void) {
    // ... set defaults ...
#ifdef RULE_LIGHTING_ENABLE
    rule_lighting_reset();
#endif
    eeconfig_flush_rgb_matrix(true);
}
```

**Purpose**: Resets rule lighting when RGB Matrix defaults are reset.

---

### quantum/dynamic_keymap.h and .c

**Responsibility**: Dynamic keymap with change counter for split sync.

#### Change Counter

```c
// In dynamic_keymap.h:
uint8_t dynamic_keymap_get_change_counter(void);
void    dynamic_keymap_set_change_counter(uint8_t value);

// In dynamic_keymap.c:
static uint8_t keymap_change_counter = 0;

uint8_t dynamic_keymap_get_change_counter(void) {
    return keymap_change_counter;
}

void dynamic_keymap_set_change_counter(uint8_t value) {
    keymap_change_counter = value;
}

void dynamic_keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t column, uint16_t keycode) {
    nvm_dynamic_keymap_update_keycode(layer, row, column, keycode);
    keymap_change_counter++;  // Increment on every change
}
```

**Purpose**: The change counter increments whenever a keycode is modified. The split sync logic uses this to detect when the keymap has changed and needs to be re-synced to the slave.

---

### quantum/rgb_matrix/animations/rgb_matrix_effects.inc

**Responsibility**: Lists all RGB Matrix effects to be compiled.

```c
// ... other effects ...
#include "rule_lighting_anim.h"
```

**Purpose**: Includes the rule lighting animation header, which registers the RULE_LIGHTING effect.

---

## Data Structures

### Rule Entry (8 bytes)

| Byte | Bits | Field | Description |
|------|------|-------|-------------|
| 0-1 | 0 | layer_enable | 1 = match layer field |
| 0-1 | 1 | caps_enable | 1 = require caps lock ON |
| 0-1 | 2-3 | reserved | For future use |
| 0-1 | 4-7 | layer | Layer number (0-15) |
| 0-1 | 8-15 | mods | Modifier mask (8-bit) |
| 2-3 | all | keycode_start | Start of keycode range |
| 4-5 | all | keycode_end | End of keycode range (inclusive) |
| 6 | 0-1 | sat_idle | Saturation mode for idle |
| 6 | 2-7 | h_idle | Hue for idle (6-bit) |
| 7 | 0-1 | sat_pressed | Saturation mode for pressed |
| 7 | 2-7 | h_pressed | Hue for pressed (6-bit) |

### Config (1 byte)

| Byte | Field | Description |
|------|-------|-------------|
| 0 | entry_count | Number of active rules |

---

## Split Keyboard Synchronization

The split sync uses two mechanisms:

### 1. Rule Lighting Sync (periodic)

- Master sends complete `rule_lighting_sync_t` every 5 seconds
- Slave applies data directly to RAM (no EEPROM write)
- Always sent, regardless of changes

### 2. Keymap Sync (change-based)

The keymap sync is more complex because it can be large:

1. **Master queries slave counter**: Sends empty request, receives 1-byte counter
2. **If counters differ**: Master enters sync mode
3. **Layer-by-layer sync**: Master sends one layer at a time (every 100ms)
4. **Each layer packet includes**:
   - Counter value being synced
   - Layer number
   - Full keycode matrix for that layer
5. **Slave tracks progress**: Uses `pending_mask` bitmask
6. **Commit on completion**: When all layers received, slave updates `synced_counter`

This approach:
- Minimizes sync traffic (only syncs when changed)
- Handles partial sync failures gracefully
- Ensures atomic updates (all layers received before commit)

---

## Enabling the Feature

### In rules.mk (keymap level):

```makefile
RULE_LIGHTING_ENABLE = yes
```

### In config.h (optional):

```c
// Override default entry count
#define RULE_LIGHTING_ENTRIES 16
```

### Requirements:

- `RGB_MATRIX_ENABLE = yes` must be set
- For split keyboards, feature auto-configures required settings

### Effect Selection:

The effect appears as "RULE_LIGHTING" in the RGB Matrix effect list. Select it via:
- Vial GUI RGB settings
- `RGB_MOD` key cycling
- Programmatically: `rgb_matrix_mode(RGB_MATRIX_RULE_LIGHTING)`

---

## EEPROM Layout

```
DYNAMIC_KEYMAP_EEPROM_ADDR
  |
  +-- Keymap data
  |
VIAL_ENCODERS_EEPROM_ADDR
  |
  +-- Encoder maps
  |
VIAL_QMK_SETTINGS_EEPROM_ADDR
  |
  +-- QMK Settings
  |
VIAL_TAP_DANCE_EEPROM_ADDR
  |
  +-- Tap Dance entries
  |
VIAL_COMBO_EEPROM_ADDR
  |
  +-- Combo entries
  |
VIAL_KEY_OVERRIDE_EEPROM_ADDR
  |
  +-- Key Override entries
  |
VIAL_ALT_REPEAT_KEY_EEPROM_ADDR
  |
  +-- Alt Repeat Key entries
  |
RULE_LIGHTING_EEPROM_ADDR
  |
  +-- rule_lighting_config_t (1 byte)
  +-- rule_lighting_entry_t[0] (8 bytes)
  +-- rule_lighting_entry_t[1] (8 bytes)
  +-- ...
  +-- rule_lighting_entry_t[N-1] (8 bytes)
  |
DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR
  |
  +-- Dynamic macros (remaining space)
```

Total EEPROM for rule lighting: `1 + (8 * RULE_LIGHTING_ENTRIES)` bytes

---

## Implementation Checklist for Reimplementation

1. **Data Structures**: Define `rule_lighting_entry_t` and `rule_lighting_config_t` with exact bit layouts
2. **RAM Cache**: Maintain static arrays for config and entries
3. **EEPROM Storage**: Calculate addresses, implement get/set functions
4. **Core API**: Implement init, load, save, reset, get/set functions
5. **RGB Matrix Effect**: Register effect, implement matching logic, implement color blending
6. **Vial Protocol**: Add command handlers for get/set config/entry
7. **Split Sync**: Register transactions, implement master sync task, implement slave handlers
8. **Keymap Sync**: Implement counter-based layer-by-layer sync protocol
9. **Integration**:
   - Call `rule_lighting_init()` from `rgb_matrix_init()` (early init, loads EEPROM)
   - Call `rule_lighting_post_init()` from `keyboard_init()` AFTER `split_post_init()` (registers RPC handlers)
   - Call `rule_lighting_task()` from `rgb_matrix_task()`
   - Call `rule_lighting_save()` from `eeconfig_force_flush_rgb_matrix()`
10. **Transaction IDs**: Place custom transaction IDs AFTER `GET_RPC_RESP_DATA` in enum
11. **Buffer Sizing**: Include `rule_lighting.h` from `transport.h` before default buffer size is set
