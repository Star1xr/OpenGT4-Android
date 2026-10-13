# Embeds the AMD FidelityFX FSR 1 headers (MIT, see LICENSE.txt) into a C
# source as byte arrays, so the GLSL shaders need no files at run time.
# Usage: cmake -DOUT=<file.c> -DDIR=<this folder> -P embed.cmake
set(content "/* Generated from third_party/fsr1 (AMD FidelityFX FSR 1, MIT). */\n#include <stddef.h>\n")
foreach(name ffx_a ffx_fsr1)
  file(READ "${DIR}/${name}.h" hex HEX)
  string(LENGTH "${hex}" len)
  math(EXPR bytes "${len} / 2")
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex "${hex}")
  string(REGEX REPLACE "(0x[0-9a-f][0-9a-f],){32}" "\\0\n" hex "${hex}")
  string(APPEND content "const char fsr_${name}[] = {\n${hex}0x00};\nconst size_t fsr_${name}_size = ${bytes};\n")
endforeach()
file(WRITE "${OUT}" "${content}")
