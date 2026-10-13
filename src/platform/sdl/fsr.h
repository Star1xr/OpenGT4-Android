/* FSR 1 scene reconstruction; needs an OpenGL 4.3 context. */
#ifndef OPENGT4_FSR_H
#define OPENGT4_FSR_H

#include <stdint.h>
#include "gl.h"

int    fsr_available(void);
/* GPU input and cached GPU output, owned by FSR. No readback. */
GLuint fsr_resolve(GLuint input,int w,int h,int output_width,int output_height);

#endif
