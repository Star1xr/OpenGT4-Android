/* Controller-selection test input, not an edit to physics coefficients.
   func_00362E00 +0..8 stores the requested low byte at block+466 and
   clears block+467. The block is car+104; the oracle checks both writes. */
#ifndef GT4_TEST_DRIVER_FIXTURE_H
#define GT4_TEST_DRIVER_FIXTURE_H
static void fixture_controller_kind(uint32_t car,uint32_t kind)
{
    GT4_U8(car+0x104+0x466)=(uint8_t)kind;
    GT4_U8(car+0x104+0x467)=0;
}
#endif
