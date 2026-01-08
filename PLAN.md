# Plan: Rule Lighting Implementation Documentation

## Goal
Create a comprehensive document that describes how rule_lighting is implemented in each branch (rl and HEAD) by listing all differences against the common base branch (merge-2025-12-28). The documentation should be detailed enough to reimplement the feature from scratch.

## Approach
Document each branch SEPARATELY. For each branch:
1. List all files that differ from the base branch (filtered to rule_lighting relevant files)
2. For each file, show the FULL content or diff
3. Describe what each piece of code does and its responsibility
4. Be verbose - explain the purpose, not just show the code

## Structure of Documentation

### Part 1: RL Branch Implementation
For each changed file (vs merge-2025-12-28):
- File path
- Status (new file / modified)
- Full content (for new files) or diff (for modified files)
- Line-by-line explanation of what the code does
- Responsibility summary

Files to document for RL:
1. quantum/rule_lighting.h - Core header with types and API
2. quantum/rule_lighting.c - Core implementation
3. quantum/rgb_matrix/animations/rule_lighting_anim.h - RGB effect
4. quantum/rgb_matrix/animations/rgb_matrix_effects.inc - Effect registration
5. quantum/rgb_matrix/rgb_matrix.c - Integration with RGB matrix
6. quantum/vial.c - Vial protocol commands
7. quantum/vial.h - Vial protocol definitions
8. quantum/vialrgb.c - VialRGB integration
9. quantum/vialrgb_effects.inc - Effect list for Vial
10. quantum/dynamic_keymap.h - EEPROM layout declarations
11. quantum/dynamic_keymap.c - Change counter for sync
12. quantum/nvm/eeprom/nvm_dynamic_keymap.c - EEPROM storage functions
13. keyboards/splitted_space/lea_choc/v1/config.h - Keyboard config
14. keyboards/splitted_space/lea_choc/v1/rules.mk - Build rules
15. keyboards/splitted_space/lea_choc/v1/keymaps/hooks_base.c - Split sync code

### Part 2: HEAD Branch Implementation
Same structure as Part 1, but for HEAD branch.

Additional files that HEAD modifies (that rl doesn't):
- quantum/split_common/transaction_id_define.h
- quantum/split_common/transactions.c
- quantum/split_common/transport.h
- quantum/rgb_matrix/rgb_matrix.h

### Part 3: Summary Tables
- Table of all files and which branch modifies them
- Table of responsibilities and where they're handled in each branch

## Output
Single markdown file: RULE_LIGHTING_IMPLEMENTATION.md

## Execution Steps
1. Get list of rule_lighting relevant files changed in rl vs base
2. For each file in rl: get full content, write detailed documentation
3. Get list of rule_lighting relevant files changed in HEAD vs base
4. For each file in HEAD: get full content, write detailed documentation
5. Create summary tables
