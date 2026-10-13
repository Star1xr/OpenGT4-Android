/* DualShock configuration at the native pad boundary. */
#ifndef OPENGT4_PAD_CONFIG_H
#define OPENGT4_PAD_CONFIG_H
#include "adhoc/adhoc_vm.h"
#include "platform/platform.h"
enum { GT4_PAD_LSTEER,GT4_PAD_RSTEER,GT4_PAD_SHIFTDOWN,GT4_PAD_SHIFTUP,
 GT4_PAD_ACCEL,GT4_PAD_BRAKE,GT4_PAD_SIDE,GT4_PAD_REVERSE,GT4_PAD_SPECIAL,
 GT4_PAD_PLAYVIEW,GT4_PAD_BACKVIEW,GT4_PAD_FRONTVIEW,GT4_PAD_REARVIEW,
 GT4_PAD_LEFTVIEW,GT4_PAD_RIGHTVIEW,GT4_PAD_BEST_LINE,GT4_PAD_GHOST,
 GT4_PAD_PAUSE,GT4_PAD_ACTIONS };
/* Keys follow the PS2 button bits; NONE is not a physical key. */
enum { GT4_PAD_SELECT,GT4_PAD_L3,GT4_PAD_R3,GT4_PAD_START,
 GT4_PAD_UP,GT4_PAD_RIGHT,GT4_PAD_DOWN,GT4_PAD_LEFT,
 GT4_PAD_L2,GT4_PAD_R2,GT4_PAD_L1,GT4_PAD_R1,
 GT4_PAD_TRIANGLE,GT4_PAD_CIRCLE,GT4_PAD_CROSS,GT4_PAD_SQUARE };
enum { GT4_PAD_LX,GT4_PAD_RX,GT4_PAD_LYF,GT4_PAD_LYL,GT4_PAD_RYF,GT4_PAD_RYL };
typedef struct { int button[16],analog[6]; } gt4_pad_config;
typedef struct { float action[GT4_PAD_ACTIONS],steer;unsigned analog; } gt4_pad_actions;
void gt4_pad_defaults(gt4_pad_config *out,int port);
void gt4_pad_bind_option(ah_vm *vm,ah_obj *option);
void gt4_pad_option(ah_vm *vm,ah_obj *option,int port,gt4_pad_config *out);
void gt4_pad_evaluate(const gt4_pad_config *config,const plat_pad_state *pad,gt4_pad_actions *out);
#endif
