# Integer multiply/divide for original replay assets

`ps2float.c` and `ps2float.h` are from libretro/ps2, commit
`cac1c32c7d4c2ad00edd6f4ba756340cefb39dd3`, paths `pcsx2/ps2float.*`:
https://github.com/libretro/ps2

Copyright PCSX2 Dev Team, LGPL-3.0-or-later. See COPYING.LESSER and COPYING.
The files are unmodified apart from LF line endings.

OpenGT4 uses only multiply/divide to decode original Base replay assets
(GT4 func_00579E00). Its feedback cipher depends on the PS2's floating-point
bit patterns; ordinary host floating point does not decode those assets.
This library does not run game code and is not used for race simulation.
