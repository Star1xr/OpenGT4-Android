/* A made-up driver for comparisons: the player's car (the conductor's
   first) gets a digital pad whose buttons follow the frame (throttle, now
   and then the brake, steering left and right in turns); the other cars
   keep their own controllers. The same in physics_run (PHYS_PAD_FIXED) and
   race_run_test, so both runs can be compared. */
#ifndef OPENGT4_PAD_FIXED_H
#define OPENGT4_PAD_FIXED_H
#include <stdint.h>
#include <string.h>

static uint32_t pad_fixed_car, pad_fixed_frame;

static int pad_fixed_read(void *ctx, uint32_t car, uint8_t *cmd, const uint8_t *ev)
{
    uint32_t f = pad_fixed_frame++;
    int16_t steer = (f / 120) % 4 == 1 ? 4096 : (f / 120) % 4 == 3 ? -4096 : 0, one = 1, zero = 0;
    int brake = f % 300 >= 240 && f % 300 < 270;
    (void)ctx;
    if (car != pad_fixed_car) {
        pad_fixed_frame--;
        return 0;
    }
    cmd[0] = 0;          /* digital */
    cmd[1] = 1;          /* a digital pad */
    cmd[2] = 0;
    cmd[3] = ev[3];
    memcpy(cmd + 4, &steer, 2);
    memcpy(cmd + 8, brake ? &zero : &one, 2);
    memcpy(cmd + 0xa, brake ? &one : &zero, 2);
    return 1;
}

static int pad_fixed_wheel(void *ctx, uint32_t car, int16_t state[2])
{
    (void)ctx;
    (void)state;
    return car == pad_fixed_car;
}

static int pad_fixed_write(void *ctx, uint32_t car, const uint8_t *out)
{
    (void)ctx;
    (void)out;
    return car == pad_fixed_car;
}
#endif
