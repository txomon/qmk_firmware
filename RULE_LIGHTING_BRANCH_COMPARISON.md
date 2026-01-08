# Rule Lighting Branch Comparison

**Base branch**: `merge-2025-12-28`
**Working branch**: `rl` (slave LEDs work)
**Broken branch**: `HEAD` / `splitted-space-rule-lighting` (slave LEDs off)

**Problem**: Slave half LEDs are off when using rule_lighting RGB effect. Master works. Issue follows the slave side.

---

# PART 1: RL BRANCH (Working)

This section describes all files in the `rl` branch that differ from `merge-2025-12-28`.

---

## RL: keyboards/splitted_space/lea_choc/v1/config.h

**Status**: New file (not in base)
**Lines**: 120

### Full Content Description:

**Lines 1-2**: Standard `#pragma once` header guard.

**Lines 3-11**: Serial communication configuration:
```c
#define SPLIT_HAND_PIN B5
#define SERIAL_USART_FULL_DUPLEX
#define SERIAL_USART_TX_PIN A9
#define SERIAL_USART_RX_PIN A10
#define SERIAL_USART_DRIVER SD1
#define SERIAL_USART_TX_PAL_MODE 1
#define SERIAL_USART_RX_PAL_MODE 1
```
This configures the split keyboard communication using full-duplex USART on pins A9/A10.

**Lines 13-15**: CRITICAL split transaction configuration:
```c
// Split transaction IDs for rule lighting and keymap sync
#define SPLIT_TRANSACTION_IDS_KB SPLIT_RULE_LIGHTING_SYNC_ID, SPLIT_KEYMAP_SYNC_ID
```
This uses the standard QMK mechanism for adding custom transaction IDs. The macro expands in the `transaction_id_define.h` enum, placing these IDs AFTER `GET_RPC_RESP_DATA` in the enum.

**Lines 17-18**: CRITICAL RPC buffer size:
```c
// Buffer size for rule lighting sync (config 2 bytes + 16 entries * 8 bytes = 130 bytes)
#define RPC_M2S_BUFFER_SIZE 140
```
The rule_lighting_sync_t structure requires ~130 bytes. Default QMK buffer is only 32 bytes. This MUST be defined before `transport.h` is included.

**Lines 20-22**: Dynamic keymap and Vial configuration:
```c
#define DYNAMIC_KEYMAP_LAYER_COUNT 4
#define VIAL_KEYBOARD_UID { 0x05, 0xCD, 0x9F, 0x8A, 0xF4, 0xDF, 0xDE, 0xC3 }
```

**Lines 24-25**: Feature disables:
```c
#define NO_ACTION_ONESHOT
#define NO_RESET
```

**Lines 27-35**: Timing configuration:
```c
#define DEBOUNCE 5
#define TAP_CODE_DELAY 10
#define TAPPING_TOGGLE 2
#define TAPPING_TERM 200
#define QUICK_TAP_TERM 160
#define RETRO_TAPPING
#undef FLOW_TAP_TERM
#define FLOW_TAP_TERM 500
```

**Lines 37-42**: I2C configuration:
```c
#define I2C_DRIVER I2CD1
#define I2C1_SCL_PIN B6
#define I2C1_SDA_PIN B7
#define I2C1_SCL_PAL_MODE 1
#define I2C1_SDA_PAL_MODE 1
```

**Lines 44-49**: ChibiOS timer and WS2812 PWM configuration:
```c
#define CH_CFG_ST_RESOLUTION 16
#define WS2812_PWM_DRIVER PWMD2
#define WS2812_PWM_CHANNEL 4
#define WS2812_PWM_PAL_MODE 2
#define WS2812_DMA_STREAM STM32_DMA1_STREAM2
#define WS2812_DMA_CHANNEL 2
```

**Lines 51-55**: Combo configuration (conditional):
```c
#ifdef COMBO_ENABLE
    #define VIAL_COMBO_ENTRIES 4
    #define COMBO_TERM 400
#endif
```

**Lines 57-61**: OLED configuration (conditional):
```c
#ifdef OLED_ENABLE
    #define OLED_DISPLAY_ADDRESS 0x3C
    #define OLED_TIMEOUT 180000
#endif
```

**Lines 63-84**: Quantum Painter (LCD) configuration (conditional):
```c
#ifdef QUANTUM_PAINTER_ENABLE
    #define SPI_DRIVER SPID1
    // ... SPI pins ...
    #define LCD_RST_PIN A1
    #define LCD_CS_PIN A2
    #define LCD_DC_PIN A0
    #define LCD_WIDTH 80
    #define LCD_HEIGHT 160
#endif
```

**Lines 86-120**: RGB Matrix configuration (conditional):
```c
#ifdef RGB_MATRIX_ENABLE
    #define RULE_LIGHTING_ENTRIES 16

    // WS2812 timing
    #define WS2812_TRST_US 80
    #define WS2812_T0H 300
    #define WS2812_T1H 950

    // Brightness
    #define RGB_MATRIX_MAXIMUM_BRIGHTNESS 170
    #define RGB_MATRIX_DEFAULT_VAL 110

    // LED count
    #define RGBLED_NUM 58
    #define DRIVER_LED_TOTAL RGBLED_NUM
    #define RGB_MATRIX_LED_COUNT RGBLED_NUM

    // Enabled effects
    #define RGB_MATRIX_KEYPRESSES
    #define ENABLE_RGB_MATRIX_SOLID_REACTIVE_SIMPLE
    // ... more effects ...

    #define RGB_TRIGGER_ON_KEYDOWN

    // CRITICAL: Enable split state syncing
    #define SPLIT_LAYER_STATE_ENABLE
    #define SPLIT_LED_STATE_ENABLE
    #define SPLIT_MODS_ENABLE
#endif
```

---

## RL: keyboards/splitted_space/lea_choc/v1/keymaps/hooks_base.c

**Status**: New file (not in base)
**Lines**: 130

### Full Content Description:

**Lines 1-7**: Includes:
```c
#include "transactions.h"
#include "rule_lighting.h"
#include "dynamic_keymap.h"
#include "keymap_introspection.h"
#include "action_layer.h"
#include <string.h>
```
These includes bring in the split transaction API, rule lighting API, and keymap access functions.

**Line 9**: Forward declaration:
```c
void hooks_housekeeping_task_user(void);
```

**Lines 11-17**: Conditional block for split+RGB+rule_lighting:
```c
#if defined(SPLIT_KEYBOARD) && defined(RGB_MATRIX_ENABLE) && defined(RULE_LIGHTING_ENABLE)
```
All split sync code is wrapped in this condition.

**Lines 13-17**: Rule lighting slave callback:
```c
void rule_lighting_slave_callback(uint8_t m2s_size, const void *m2s_buffer, uint8_t s2m_size, void *s2m_buffer) {
    if (m2s_size == sizeof(rule_lighting_sync_t)) {
        rule_lighting_apply_sync_data((const rule_lighting_sync_t *)m2s_buffer);
    }
}
```
This callback is invoked on the slave when master sends rule lighting config. It applies the received data to the slave's RAM cache.

**Lines 19-25**: Keymap sync structure:
```c
typedef struct {
    uint8_t counter;
    uint8_t layer;
    uint16_t keycodes[MATRIX_ROWS][MATRIX_COLS];
} __attribute__((packed)) keymap_layer_sync_t;
```
This structure carries one layer of keymap data. The counter tracks sync versions.

**Lines 27-30**: Slave-side synced keymap storage:
```c
static uint16_t synced_keymap[DYNAMIC_KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
static uint8_t synced_counter = 0xFF;       // Counter for completed sync
static uint8_t pending_counter = 0xFF;      // Counter currently being synced
static uint8_t pending_mask = 0;            // Layers received for pending_counter
```
The slave stores the received keymap here. The counter system ensures atomic updates - all layers must be received before the sync is "committed".

**Lines 32-56**: Keymap slave callback:
```c
void keymap_slave_callback(uint8_t m2s_size, const void *m2s_buffer, uint8_t s2m_size, void *s2m_buffer) {
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
This callback handles two operations:
1. When m2s_size=0: Master is querying slave's current counter (to check if sync needed)
2. When m2s_size=sizeof(keymap_layer_sync_t): Master is sending layer data

**Lines 58-69**: Keycode resolver:
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
This handles KC_TRNS (transparent) keycodes by searching lower layers.

**Lines 71-77**: get_synced_keycode implementation:
```c
uint16_t get_synced_keycode(uint8_t layer, uint8_t row, uint8_t col) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KC_NO;
    }
    return resolve_keycode(layer, row, col, !is_keyboard_master());
}
```
Master uses dynamic_keymap (EEPROM), slave uses synced_keymap (RAM from sync).

**Lines 79-85**: keyboard_post_init_user - CRITICAL:
```c
void keyboard_post_init_user(void) {
#if defined(SPLIT_KEYBOARD) && defined(RGB_MATRIX_ENABLE) && defined(RULE_LIGHTING_ENABLE)
    // Register slave callbacks
    transaction_register_rpc(SPLIT_RULE_LIGHTING_SYNC_ID, rule_lighting_slave_callback);
    transaction_register_rpc(SPLIT_KEYMAP_SYNC_ID, keymap_slave_callback);
#endif
}
```
THIS IS WHERE THE RPC HANDLERS ARE REGISTERED. This is called during keyboard initialization.

**Lines 87-91**: process_record_user stub:
```c
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    return true;
}
```

**Lines 93-130**: hooks_housekeeping_task_user with master sync logic:
```c
void hooks_housekeeping_task_user() {
    #ifdef RGB_MATRIX_ENABLE
        // Brightness limiter
        int val = rgb_matrix_get_val();
        if (val > RGB_MATRIX_MAXIMUM_BRIGHTNESS) {
            rgb_matrix_decrease_val();
        }
    #endif

#if defined(SPLIT_KEYBOARD) && defined(RGB_MATRIX_ENABLE) && defined(RULE_LIGHTING_ENABLE)
    if (is_keyboard_master()) {
        static uint32_t last_check = 0;
        static uint8_t sync_layer = 0xFF;  // 0xFF = idle, 0-3 = syncing layer N
        static uint8_t sync_counter = 0;   // Counter we're syncing

        // Query slave every 5 seconds when idle, or send layers quickly when syncing
        uint32_t interval = (sync_layer == 0xFF) ? 5000 : 100;

        if (timer_elapsed32(last_check) > interval) {
            last_check = timer_read32();

            if (sync_layer == 0xFF) {
                // Idle mode: query slave's counter and send rule lighting
                uint8_t slave_counter = 0xFF;
                bool ok = transaction_rpc_exec(SPLIT_KEYMAP_SYNC_ID, 0, NULL, 1, &slave_counter);
                uint8_t master_counter = dynamic_keymap_get_change_counter();

                if (ok && slave_counter != master_counter) {
                    sync_layer = 0;  // Start syncing layers
                    sync_counter = master_counter;
                }

                // Always sync rule lighting
                const rule_lighting_sync_t *sync_data = rule_lighting_get_sync_data();
                transaction_rpc_send(SPLIT_RULE_LIGHTING_SYNC_ID, sizeof(rule_lighting_sync_t), sync_data);
            } else {
                // Syncing mode: send one layer with counter
                keymap_layer_sync_t layer_data;
                layer_data.counter = sync_counter;
                layer_data.layer = sync_layer;
                for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                        layer_data.keycodes[row][col] = dynamic_keymap_get_keycode(sync_layer, row, col);
                    }
                }
                transaction_rpc_send(SPLIT_KEYMAP_SYNC_ID, sizeof(keymap_layer_sync_t), &layer_data);

                sync_layer++;
                if (sync_layer >= DYNAMIC_KEYMAP_LAYER_COUNT) {
                    sync_layer = 0xFF;  // Done, back to idle
                }
            }
        }
    }
#endif
}
```
This runs on master every housekeeping cycle:
1. Every 5 seconds in idle mode: query slave counter, send rule lighting
2. Every 100ms in sync mode: send one layer at a time until all done

---

## RL: quantum/rule_lighting.c

**Status**: New file (not in base)
**Lines**: ~175

### Key Functions:

```c
void rule_lighting_init(void) {
    rule_lighting_load();
}
```
Just loads from EEPROM. NO split registration here - that's in hooks_base.c.

```c
__attribute__((weak)) uint16_t get_synced_keycode(uint8_t layer, uint8_t row, uint8_t col) {
    return keycode_at_keymap_location_raw(layer, row, col);
}
```
This is a WEAK function - overridden by the strong implementation in hooks_base.c.

Other functions: rule_lighting_reset(), rule_lighting_load(), rule_lighting_save(), rule_lighting_get_entry(), rule_lighting_set_entry(), rule_lighting_get_config(), rule_lighting_set_config(), rule_lighting_get_sync_data(), rule_lighting_apply_sync_data().

---

## RL: quantum/rule_lighting.h

**Status**: New file (not in base)
**Lines**: ~120

Contains:
- RULE_LIGHTING_ENTRIES default calculation based on EEPROM size
- Type definitions: rule_lighting_entry_t (8 bytes), rule_lighting_config_t (1 byte)
- Function declarations
- rule_lighting_sync_t structure definition
- NO split auto-configuration

---

## RL: quantum/split_common/transaction_id_define.h

**Status**: UNCHANGED from base branch

The rl branch does NOT modify this file. It relies on `SPLIT_TRANSACTION_IDS_KB` being defined in config.h to expand the IDs.

---

## RL: quantum/rgb_matrix/rgb_matrix.c

**Status**: Modified from base

Key changes in rl branch:
```c
#ifdef RULE_LIGHTING_ENABLE
    // In eeconfig_force_flush_rgb_matrix:
    rule_lighting_save();

    // In eeconfig_update_rgb_matrix_default:
    rule_lighting_reset();

    // In rgb_matrix_init_drivers:
    if (rgb_matrix_config.mode == 0) {
        eeconfig_update_rgb_matrix_default();
    }
    else {
        rule_lighting_init();  // ONLY in else block!
    }
#endif
```

NO call to rule_lighting_task() - the sync is handled in hooks_housekeeping_task_user() instead.

---

# PART 2: HEAD BRANCH (Broken)

This section describes all files in the `HEAD` branch that differ from `merge-2025-12-28`.

---

## HEAD: keyboards/splitted_space/lea_choc/v1/config.h

**Status**: New file (not in base)
**Lines**: 114

### Key Differences from RL:

**MISSING** (lines that exist in rl but not in HEAD):
```c
// Split transaction IDs for rule lighting and keymap sync
#define SPLIT_TRANSACTION_IDS_KB SPLIT_RULE_LIGHTING_SYNC_ID, SPLIT_KEYMAP_SYNC_ID

// Buffer size for rule lighting sync (config 2 bytes + 16 entries * 8 bytes = 130 bytes)
#define RPC_M2S_BUFFER_SIZE 140
```

The rest of config.h is essentially the same as rl. It still has:
```c
#ifdef RGB_MATRIX_ENABLE
    #define SPLIT_LAYER_STATE_ENABLE
    #define SPLIT_LED_STATE_ENABLE
    #define SPLIT_MODS_ENABLE
#endif
```

But without `SPLIT_TRANSACTION_IDS_KB` and `RPC_M2S_BUFFER_SIZE`, the auto-configuration in rule_lighting.h won't work due to include order.

---

## HEAD: keyboards/splitted_space/lea_choc/v1/keymaps/hooks_base.c

**Status**: New file (not in base)
**Lines**: 15

### Full Content:
```c
void hooks_housekeeping_task_user(void);

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    return true;
}

void hooks_housekeeping_task_user() {
#ifdef RGB_MATRIX_ENABLE
    int val = rgb_matrix_get_val();
    if (val > RGB_MATRIX_MAXIMUM_BRIGHTNESS) {
        rgb_matrix_decrease_val();
    }
#endif
}
```

### What's Missing:
- ALL split sync code removed
- NO keyboard_post_init_user() - transaction_register_rpc is NOT called here
- NO master sync task

The sync code was moved to quantum/rule_lighting.c in HEAD.

---

## HEAD: quantum/rule_lighting.c

**Status**: New file (not in base)
**Lines**: ~330

### Full Content Description:

**Lines 1-26**: Includes and forward declarations:
```c
#include "rule_lighting.h"
#include "keymap_introspection.h"
#include "dynamic_keymap.h"
#include <stddef.h>
#include <string.h>

#ifdef SPLIT_KEYBOARD
#include "keyboard.h"
#include "transactions.h"
#include "timer.h"
#endif
```
HEAD adds dynamic_keymap.h, string.h, and conditional split includes.

**Lines 28-95**: Core rule lighting functions (same as rl):
- rule_lighting_clear_ram()
- rule_lighting_reset()
- rule_lighting_load()
- rule_lighting_get_entry()
- rule_lighting_set_entry()
- rule_lighting_get_config()
- rule_lighting_set_config()
- rule_lighting_save()
- rule_lighting_get_sync_data()
- rule_lighting_apply_sync_data()

**Lines 97-180**: Split keyboard sync code (moved from hooks_base.c):

```c
#ifdef SPLIT_KEYBOARD

/* Keymap sync data structure (one layer at a time, includes counter) */
typedef struct {
    uint8_t counter;
    uint8_t layer;
    uint16_t keycodes[MATRIX_ROWS][MATRIX_COLS];
} __attribute__((packed)) keymap_layer_sync_t;

/* Synced keymap storage on slave (RAM) */
static uint16_t synced_keymap[DYNAMIC_KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
static uint8_t synced_counter = 0xFF;
static uint8_t pending_counter = 0xFF;
static uint8_t pending_mask = 0;

static void rule_lighting_slave_handler(uint8_t m2s_size, const void *m2s_buffer, uint8_t s2m_size, void *s2m_buffer) {
    if (m2s_size == sizeof(rule_lighting_sync_t)) {
        rule_lighting_apply_sync_data((const rule_lighting_sync_t *)m2s_buffer);
    }
}

static void keymap_slave_handler(uint8_t m2s_size, const void *m2s_buffer, uint8_t s2m_size, void *s2m_buffer) {
    // Same logic as rl's keymap_slave_callback
}

static uint16_t resolve_keycode(uint8_t layer, uint8_t row, uint8_t col, bool use_synced) {
    // Same as rl
}

uint16_t get_synced_keycode(uint8_t layer, uint8_t row, uint8_t col) {
    // Strong implementation (not weak)
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KC_NO;
    }
    return resolve_keycode(layer, row, col, !is_keyboard_master());
}

static void rule_lighting_split_init(void) {
    transaction_register_rpc(SPLIT_RULE_LIGHTING_SYNC_ID, rule_lighting_slave_handler);
    transaction_register_rpc(SPLIT_KEYMAP_SYNC_ID, keymap_slave_handler);
}

static void rule_lighting_master_sync(void) {
    // Same master sync logic as rl's hooks_housekeeping_task_user
}

#else /* !SPLIT_KEYBOARD */

uint16_t get_synced_keycode(uint8_t layer, uint8_t row, uint8_t col) {
    return keycode_at_keymap_location_raw(layer, row, col);
}

#endif /* SPLIT_KEYBOARD */
```

**Lines 182-200**: Init and task functions:
```c
void rule_lighting_init(void) {
    rule_lighting_load();
#ifdef SPLIT_KEYBOARD
    rule_lighting_split_init();  // <-- RPC registration happens here now
#endif
}

void rule_lighting_task(void) {
#ifdef SPLIT_KEYBOARD
    if (is_keyboard_master()) {
        rule_lighting_master_sync();  // <-- Master sync happens here now
    }
#endif
}
```

### Key Architectural Change:
In HEAD, the RPC registration and master sync task are moved into rule_lighting.c:
- `rule_lighting_init()` calls `rule_lighting_split_init()` to register handlers
- `rule_lighting_task()` calls `rule_lighting_master_sync()` for master sync

This is called from rgb_matrix.c instead of from user hooks.

---

## HEAD: quantum/rule_lighting.h

**Status**: New file (not in base)
**Lines**: ~185

### Key Addition - Auto-configuration block:

```c
#if defined(SPLIT_KEYBOARD) && defined(RULE_LIGHTING_ENABLE)

/*
 * RPC buffer size calculation
 * Buffer must fit the larger of:
 *   - rule_lighting_sync_t: 1 + 8 * RULE_LIGHTING_ENTRIES
 *   - keymap_layer_sync_t:  2 + MATRIX_ROWS * MATRIX_COLS * 2
 */
#define _RL_SYNC_SIZE (1 + 8 * RULE_LIGHTING_ENTRIES)
#define _KM_SYNC_SIZE (2 + MATRIX_ROWS * MATRIX_COLS * 2)
#define _RL_RPC_BUFFER_SIZE ((_RL_SYNC_SIZE > _KM_SYNC_SIZE) ? _RL_SYNC_SIZE : _KM_SYNC_SIZE)

#ifndef RPC_M2S_BUFFER_SIZE
    #define RPC_M2S_BUFFER_SIZE _RL_RPC_BUFFER_SIZE
#elif RPC_M2S_BUFFER_SIZE < _RL_RPC_BUFFER_SIZE
    #undef RPC_M2S_BUFFER_SIZE
    #define RPC_M2S_BUFFER_SIZE _RL_RPC_BUFFER_SIZE
#endif

/* Enable required split state syncing */
#ifndef SPLIT_LAYER_STATE_ENABLE
    #define SPLIT_LAYER_STATE_ENABLE
#endif
#ifndef SPLIT_LED_STATE_ENABLE
    #define SPLIT_LED_STATE_ENABLE
#endif
#ifndef SPLIT_MODS_ENABLE
    #define SPLIT_MODS_ENABLE
#endif

#endif /* SPLIT_KEYBOARD && RULE_LIGHTING_ENABLE */
```

### The Problem:
This auto-configuration attempts to set `RPC_M2S_BUFFER_SIZE`, but it's **TOO LATE**:
1. transport.h is included first in transactions.c
2. transport.h sets `RPC_M2S_BUFFER_SIZE 32` as default
3. rule_lighting.h is included later
4. By then, the buffer struct is already sized at 32 bytes

The `#undef` trick doesn't work because the struct definition in transport.h has already been processed.

### New function declaration:
```c
void rule_lighting_task(void);  // Called from rgb_matrix_task
```

---

## HEAD: quantum/split_common/transaction_id_define.h

**Status**: Modified from base
**Lines**: 134

### Changes from base/rl:

**Lines 102-107**: Added RULE_LIGHTING_ENABLE to RPC infrastructure condition:
```c
#if defined(SPLIT_TRANSACTION_IDS_KB) || defined(SPLIT_TRANSACTION_IDS_USER) || (defined(RULE_LIGHTING_ENABLE) && defined(SPLIT_KEYBOARD))
    PUT_RPC_INFO,
    PUT_RPC_REQ_DATA,
    EXECUTE_RPC,
    GET_RPC_RESP_DATA,
#endif
```

**Lines 109-113**: Added rule lighting transaction IDs:
```c
// Rule lighting transaction IDs (must come AFTER GET_RPC_RESP_DATA for validation)
#if defined(RULE_LIGHTING_ENABLE) && defined(SPLIT_KEYBOARD)
    SPLIT_RULE_LIGHTING_SYNC_ID,
    SPLIT_KEYMAP_SYNC_ID,
#endif
```

### Analysis:
The enum ordering is correct - the rule lighting IDs come AFTER GET_RPC_RESP_DATA. This satisfies the validation in transaction_register_rpc():
```c
if (transaction_id <= GET_RPC_RESP_DATA) return;
```

So the transaction ID ordering is NOT the bug.

---

## HEAD: quantum/rgb_matrix/rgb_matrix.c

**Status**: Modified from base
**Lines**: Same as base + rule_lighting additions

### Changes:

**New call in rgb_matrix_task():**
```c
#ifdef RULE_LIGHTING_ENABLE
    rule_lighting_task();  // <-- NEW: master sync
#endif
```

**In rgb_matrix_init_drivers (same structure as rl):**
```c
#ifdef RULE_LIGHTING_ENABLE
    if (rgb_matrix_config.mode == 0) {
        eeconfig_update_rgb_matrix_default();
    }
    else {
        rule_lighting_init();  // RPC registration happens here
    }
#endif
```

---

# PART 3: ROOT CAUSE ANALYSIS

## The Bug

The issue is **RPC_M2S_BUFFER_SIZE** being too small.

### Include Order Problem:

1. **transactions.c** includes:
   - transport.h (sets `RPC_M2S_BUFFER_SIZE 32`)
   - transaction_id_define.h (sets up enum)

2. **rgb_matrix.c** includes:
   - rule_lighting.h (tries to set `RPC_M2S_BUFFER_SIZE` but TOO LATE)

3. When master calls `transaction_rpc_send()` with ~130 bytes of data:
   - transactions.c checks: `if (initiator2target_buffer_size > RPC_M2S_BUFFER_SIZE) return false;`
   - 130 > 32, so it returns false
   - The sync silently fails

### Why RL Works:

RL defines `RPC_M2S_BUFFER_SIZE 140` in config.h, which is included VERY EARLY via:
```
mcuconf.h -> config.h (keyboard config)
```

This happens BEFORE transport.h, so when transport.h checks:
```c
#ifndef RPC_M2S_BUFFER_SIZE
#    define RPC_M2S_BUFFER_SIZE 32
#endif
```

The define is already set to 140, so the default 32 is not used.

---

# PART 4: SOLUTION

Add these lines to `keyboards/splitted_space/lea_choc/v1/config.h` at the TOP (before other includes):

```c
// Buffer size for rule lighting sync - MUST be defined before transport.h
#define RPC_M2S_BUFFER_SIZE 140
```

Optionally also add (for consistency with standard QMK patterns):
```c
#define SPLIT_TRANSACTION_IDS_KB SPLIT_RULE_LIGHTING_SYNC_ID, SPLIT_KEYMAP_SYNC_ID
```

But this second line is not strictly necessary since HEAD's transaction_id_define.h handles the IDs directly.
