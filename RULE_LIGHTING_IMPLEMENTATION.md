# Rule Lighting Feature Implementation Documentation

**Base branch**: `merge-2025-12-28`
**Working branch**: `rl`
**Current branch**: `HEAD` / `splitted-space-rule-lighting`

This document describes how rule_lighting is implemented in each branch, listing all differences against the common base branch. The goal is to provide enough detail to reimplement the feature from scratch.

---

# BRANCH: RL (Working Implementation)

## Overview

The rl branch implements rule_lighting with split keyboard support by:
1. Adding core rule_lighting logic in quantum/rule_lighting.c and .h
2. Adding RGB matrix effect in quantum/rgb_matrix/animations/rule_lighting_anim.h
3. Adding Vial protocol support for configuration
4. Placing split keyboard sync code in the keyboard's hooks_base.c file
5. Using QMK's standard SPLIT_TRANSACTION_IDS_KB mechanism for transaction IDs

---

## RL: File Changes (Quantum Level)

