/*
 * Copyright (C) 2009 by
 *
 *      Sean Cier            <scier@7b5labs.com>
 *      Michael Sherman      <msherman@7b5labs.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef GLUTIL_H
#define GLUTIL_H


#include "GLHeaders.h"
#include "FrameworkDefines.h"

extern GLuint nullRGBA;
extern GLuint blackRGBA;
extern GLuint whiteRGBA;
extern GLuint blueRGBA;
extern GLuint redRGBA;
extern GLuint greenRGBA;
extern GLuint yellowRGBA;

BEGIN_EXTERN_C
void draw_quad_short(short x0, short y0, short x1, short y1, short x2, short y2, short x3, short y3);
void draw_quad_float(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3);
void draw_textured_quad_short(
    short x0, short y0, float s0, float t0,
    short x1, short y1, float s1, float t1,
    short x2, short y2, float s2, float t2,
    short x3, short y3, float s3, float t3);
void draw_textured_quad_float(
    float x0, float y0, float s0, float t0,
    float x1, float y1, float s1, float t1,
    float x2, float y2, float s2, float t2,
    float x3, float y3, float s3, float t3);
void draw_quad_outline_short(short x0, short y0, short x1, short y1, short x2, short y2, short x3, short y3);
void draw_quad_outline_float(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3);
void draw_triangle_short(short x0, short y0, short x1, short y1, short x2, short y2);
void draw_triangle_float(float x0, float y0, float x1, float y1, float x2, float y2);
void draw_triangle_outline_short(short x0, short y0, short x1, short y1, short x2, short y2);
void draw_triangle_outline_float(float x0, float y0, float x1, float y1, float x2, float y2);
void draw_segment_short(short x0, short y0, short x1, short y1);
void draw_segment_float(float x0, float y0, float x1, float y1);
void draw_point_short(short x, short y);
void draw_point_float(float x, float y);

void draw_circle(float x, float y,
	         float radius, int filled,
                 int resolution);
void draw_ring(float x, float y,
               float innerRadius, float outerRadius,
               int resolution);


typedef enum { STIPPLE_CLAMP_AS_LOOP, STIPPLE_CLAMP_AS_LINE, STIPPLE_CLAMP_NONE } stipple_clamp_t;

extern GLuint stippleTexture;

GLuint stipple_texture_init();
void stipple_texture_cleanup();
float stipple_texture_length(float lineLength, float scaleFactor, stipple_clamp_t clamp);
GLfloat *get_stipple_tex_coords(GLfloat *vertices, int numVertices, int stride, float stippleScale, stipple_clamp_t clamp, GLfloat *texCoords);
void draw_stippled_segment_short(short x0, short y0, short x1, short y1, float scaleFactor);
void draw_stippled_segment_float(float x0, float y0, float x1, float y1, float scaleFactor);

END_EXTERN_C

#ifdef __cplusplus
template <typename V> GLfloat *get_stipple_tex_coords(V *vertices, int numVertices, float stippleScale, stipple_clamp_t clamp, GLfloat *texCoords)
{
    float length = 0;
    if (numVertices == 0) {
        return texCoords;
    }
    texCoords[0] = texCoords[1] = 0;
    for (int i = 1; i < numVertices; i++) {
        float dx = vertices[i].v[0] - vertices[i-1].v[0];
        float dy = vertices[i].v[1] - vertices[i-1].v[1];
        length += sqrtf(dx*dx + dy*dy);
        texCoords[2*i  ] = length;
        texCoords[2*i+1] = 0;
    }
    if ((length == 0) || (clamp == STIPPLE_CLAMP_NONE)) {
        return texCoords;
    }
    float correctionFactor = stipple_texture_length(length, stippleScale, clamp) / length;
    for (int i = 1; i < numVertices; i++) {
        texCoords[2*i] *= correctionFactor;
    }
    return texCoords;
}

static inline GLfloat *get_stipple_tex_coords(const GLfloat *vertices, int numVertices, float stippleScale, stipple_clamp_t clamp, GLfloat *texCoords)
{
    float length = 0;
    if (numVertices == 0) {
        return texCoords;
    }
    texCoords[0] = texCoords[1] = 0;
    for (int i = 1; i < numVertices; i++) {
        float dx = vertices[2*i  ] - vertices[2*i-2];
        float dy = vertices[2*i+1] - vertices[2*i-1];
        length += sqrtf(dx*dx + dy*dy);
        texCoords[2*i  ] = length;
        texCoords[2*i+1] = 0;
    }
    if ((length == 0) || (clamp == STIPPLE_CLAMP_NONE)) {
        return texCoords;
    }
    float correctionFactor = stipple_texture_length(length, stippleScale, clamp) / length;
    for (int i = 1; i < numVertices; i++) {
        texCoords[2*i] *= correctionFactor;
    }
    return texCoords;
}
#endif

#endif