#pragma once

#define DYNAMIC_KEYMAP_LAYER_COUNT 4


#define NO_ACTION_ONESHOT //Save 244 bytes
#define NO_RESET //Save 40 bytes


#ifdef ENCODER_ENABLE
    #ifdef ENCODER_RESOLUTION
        #undef ENCODER_RESOLUTION
        #define ENCODER_RESOLUTION 4
    #endif
#endif