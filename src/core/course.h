/*
 * GT4 course files (/crs/<name>): a header of offsets to the model sets and
 * data blocks of one track. Read here: the world model set (MDLS, converted
 * by model.c) and the runway (RNW4): length, grid start positions and the
 * road surface triangles. Layouts cross-checked with Nenkai's MIT PDTools
 * (CourseDataFileGT4, RunwayData) and GT4Runway.bt; see docs/COURSES.md.
 */
#ifndef OPENGT4_COURSE_H
#define OPENGT4_COURSE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *data;       /* whole course file, owned by the caller */
    size_t         size;
    size_t         world;      /* world model set (MDLS) offset */
    size_t         runway;     /* runway (RNW4) offset */
    float          length;     /* course length, metres */
    int            grid_count;
    int            road_vertex_count, road_tri_count;
    size_t         grids, road_vertices, road_tris;   /* absolute offsets */
} gt4_course;

typedef struct { float pos[3]; float heading; } gt4_grid;   /* heading in radians */

/* Validates the header and the runway tables. 1 on success. */
int gt4_course_open(gt4_course *c, const uint8_t *data, size_t size);
gt4_grid gt4_course_grid(const gt4_course *c, int index);
/* Road surface height under (x, z): the highest road triangle at or below
   y_limit. 1 if the point is over the road. */
int gt4_course_height(const gt4_course *c, float x, float z, float y_limit, float *y);
/* Immutable road spatial index for frequent surface queries (shadows).
   Borrows the validated course buffer; destroy before freeing that buffer. */
typedef struct gt4_course_surface gt4_course_surface;
gt4_course_surface *gt4_course_surface_create(const gt4_course *c);
void gt4_course_surface_free(gt4_course_surface *s);
int gt4_course_surface_at(const gt4_course_surface *s,float x,float z,float ceiling,float *y,float normal[3]);
/* Same immutable query, retaining the winning RNW4 triangle for presentation. */
int gt4_course_surface_triangle(const gt4_course_surface *s,float x,float z,float ceiling,float *y,float normal[3],int *triangle);

/* Original Vls0: grid of candidate Voronoi nodes, each selecting an authored
   u16-count/u16-model-index list. Buffer belongs to the course. */
typedef struct {const uint8_t *data;size_t size;unsigned models;} gt4_course_vision;
int gt4_course_vision_open(gt4_course_vision *v,const uint8_t *data,size_t size,unsigned models);
/* Returns the stored list and its node index; NULL on invalid position/grid. */
const uint8_t *gt4_course_vision_select(const gt4_course_vision *v,const float position[3],unsigned *node);
#endif
