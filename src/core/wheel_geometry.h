#ifndef OPENGT4_WHEEL_GEOMETRY_H
#define OPENGT4_WHEEL_GEOMETRY_H
#include <math.h>
typedef struct {float minimum,radial,bead,axial;} gt4_tire_fit;
/* 00458AB8 +64..+D0: width/nominal rim radius and sidewall/rim.
   00450F50 passes (width, total radius, total radius - rim radius).
   Preserve the operation order of the original, including its reciprocal. */
static inline gt4_tire_fit gt4_tire_fit_prepare(float width,float radius,float sidewall,const float bounds[3])
{
    float reciprocal=1.f/(radius-sidewall);
    sidewall=sidewall*reciprocal;width=width*reciprocal;radius=radius*reciprocal;
    gt4_tire_fit fit={bounds[0],sidewall/(bounds[1]-bounds[0]),radius-sidewall,width/bounds[2]};return fit;
}
/* 00458AB8 +144..+18C; the original keeps the authored normals.
   CarModel callback 00453848..00453868 subsequently scales XYZ by rim radius. */
static inline void gt4_tire_fit_position(float p[3],gt4_tire_fit fit)
{
    float r=sqrtf(p[1]*p[1]+p[2]*p[2]);
    p[0]=p[0]*fit.axial;
    float scale=((r-fit.minimum)*fit.radial+fit.bead)/r;
    p[2]=p[2]*scale;p[1]=p[1]*scale;
}
#endif
