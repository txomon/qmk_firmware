# QMK Firmware - Lea Choc Keyboard

## Keyboard Info
- **Path**: `keyboards/splitted_space/lea_choc/v1`
- **MCU**: STM32F072 (16KB RAM - at limit!)
- **Type**: Split keyboard (master/slave via TRRS)
- **Features**: RGB Matrix, Vial, Rule Lighting (custom)

## Build Commands

```bash
# Compile
qmk compile -kb splitted_space/lea_choc/v1 -km base

# Output file
splitted_space_lea_choc_v1_base.bin
```

## Flashing (Both Halves)

1. Put both halves in DFU boot mode (hold boot button while plugging USB)

2. Check devices are detected:
```bash
dfu-util -l | grep "Found DFU"
# Should see two devices with different paths (e.g., 3-3 and 3-4)
```

3. Flash both halves in parallel using separate background Bash tool calls:
```bash
# First tool call (run_in_background: true)
dfu-util -d 0483:df11 -a 0 -s 0x08000000 -p 3-3 -D splitted_space_lea_choc_v1_base.bin

# Second tool call (run_in_background: true)
dfu-util -d 0483:df11 -a 0 -s 0x08000000 -p 3-4 -D splitted_space_lea_choc_v1_base.bin
```

Then use TaskOutput to wait for both to complete.

**Notes**:
- Flashing takes ~7 minutes per half
- Use TWO separate Bash tool calls with run_in_background for parallelism + error isolation
- If one fails, only reflash that one (not both)

## Key Files

- `config.h` - Hardware config, split transaction IDs, RGB settings
- `keymaps/keymaps_base.c` - Keymap layers, LED config
- `keymaps/hooks_base.c` - Split sync callbacks, housekeeping task
- `quantum/rule_lighting.c` - Rule lighting core logic
- `quantum/rule_lighting.h` - Rule lighting data structures
- `quantum/rgb_matrix/animations/rule_lighting_anim.h` - RGB effect implementation
- `quantum/dynamic_keymap.c` - Added change counter for efficient sync

## Rule Lighting

Custom RGB effect that colors keys based on rules matching:
- Layer
- Modifiers
- Caps lock state
- Keycode ranges

Synced to slave via split transport with change-counter optimization.

## Split Sync Architecture

- **Rule lighting**: Synced every 1 second
- **Keymap**: Counter sent every 500ms, full sync only when Vial changes keymap
- **State**: `SPLIT_LAYER_STATE_ENABLE`, `SPLIT_MODS_ENABLE`, `SPLIT_LED_STATE_ENABLE`

## RAM Constraints

STM32F072 has 16KB RAM - currently at 100% usage:
- `synced_keymap`: 480 bytes (4 layers x 10 rows x 6 cols x 2 bytes)
- `ws2812_frame_buffer`: ~2KB
- `WordBuf` (Vial): 4KB

No room for additional features without optimization.

## Vial GUI

```bash
cd /home/javier/projects/txomonltd/keyboard/vial-gui
source venv/bin/activate
python src/main/python/main.py
```

## Console Debugging

```bash
# View firmware debug output
qmk console
```
