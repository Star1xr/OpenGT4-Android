#ifndef OPENGT4_RACE_BUILD_H
#define OPENGT4_RACE_BUILD_H
#include "ee.h"

#include <stddef.h>
#include <stdint.h>

/* Transitional: a Time Trial race in guest memory without a snapshot (the
   executable's image plus the objects the setup reads). `elf`: the disc
   image (its CORE.GT4 unpacked here) or an ELF (extracted/core.elf). Then
   phys_race_setup puts a course and a car in it. */
int phys_race_build(uint8_t *ram, const char *elf, uint32_t *conductor, char *msg, size_t len);
/* The same for a race against the AI (the Arcade's), `cars` of them (1..6,
   the player's first) */
int phys_race_build_arcade(uint8_t *ram, const char *elf, int cars, uint32_t *conductor, char *msg, size_t len);

#endif
