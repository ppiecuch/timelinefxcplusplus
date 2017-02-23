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

#include "gl_util.h"

#define RES			128
#define BLOCK_SZ	35
#define TABLE_SIZE	RES

float tbl_sin[TABLE_SIZE] = { 0.000000,0.049068,0.098017,0.146730,0.195090,0.242980,0.290285,0.336890,0.382683,0.427555,0.471397,0.514103,0.555570,0.595699,0.634393,0.671559,0.707107,0.740951,0.773010,0.803208,0.831470,0.857729,0.881921,0.903989,0.923880,0.941544,0.956940,0.970031,0.980785,0.989177,0.995185,0.998795,1.000000,0.998795,0.995185,0.989177,0.980785,0.970031,0.956940,0.941544,0.923880,0.903989,0.881921,0.857729,0.831470,0.803207,0.773010,0.740951,0.707107,0.671559,0.634393,0.595699,0.555570,0.514103,0.471397,0.427555,0.382684,0.336890,0.290285,0.242980,0.195090,0.146731,0.098017,0.049068,-0.000000,-0.049068,-0.098017,-0.146730,-0.195090,-0.242980,-0.290285,-0.336890,-0.382683,-0.427555,-0.471397,-0.514103,-0.555570,-0.595699,-0.634393,-0.671559,-0.707107,-0.740951,-0.773010,-0.803208,-0.831469,-0.857729,-0.881921,-0.903989,-0.923879,-0.941544,-0.956940,-0.970031,-0.980785,-0.989177,-0.995185,-0.998795,-1.000000,-0.998795,-0.995185,-0.989177,-0.980785,-0.970031,-0.956940,-0.941544,-0.923879,-0.903989,-0.881921,-0.857729,-0.831470,-0.803208,-0.773010,-0.740951,-0.707107,-0.671559,-0.634393,-0.595699,-0.555570,-0.514103,-0.471397,-0.427555,-0.382683,-0.336890,-0.290285,-0.242980,-0.195090,-0.146730,-0.098017,-0.049068 };
float tbl_cos[TABLE_SIZE] = { 1.000000,0.998795,0.995185,0.989177,0.980785,0.970031,0.956940,0.941544,0.923880,0.903989,0.881921,0.857729,0.831470,0.803208,0.773010,0.740951,0.707107,0.671559,0.634393,0.595699,0.555570,0.514103,0.471397,0.427555,0.382683,0.336890,0.290285,0.242980,0.195090,0.146730,0.098017,0.049068,-0.000000,-0.049068,-0.098017,-0.146730,-0.195090,-0.242980,-0.290285,-0.336890,-0.382683,-0.427555,-0.471397,-0.514103,-0.555570,-0.595699,-0.634393,-0.671559,-0.707107,-0.740951,-0.773010,-0.803208,-0.831470,-0.857729,-0.881921,-0.903989,-0.923880,-0.941544,-0.956940,-0.970031,-0.980785,-0.989177,-0.995185,-0.998795,-1.000000,-0.998795,-0.995185,-0.989177,-0.980785,-0.970031,-0.956940,-0.941544,-0.923880,-0.903989,-0.881921,-0.857729,-0.831470,-0.803208,-0.773010,-0.740951,-0.707107,-0.671559,-0.634393,-0.595699,-0.555570,-0.514103,-0.471397,-0.427555,-0.382684,-0.336890,-0.290285,-0.242980,-0.195090,-0.146730,-0.098017,-0.049068,0.000000,0.049068,0.098017,0.146730,0.195090,0.242980,0.290285,0.336890,0.382684,0.427555,0.471397,0.514103,0.555570,0.595699,0.634393,0.671559,0.707107,0.740951,0.773011,0.803207,0.831470,0.857729,0.881921,0.903989,0.923880,0.941544,0.956940,0.970031,0.980785,0.989177,0.995185,0.998795 };

#ifdef DEBUG
/* Range checking: find out where the table size is exceeded. */
# define CHK2(x, m)	((MOD2(x, m) != x) ? (printf("MOD %s:%d:%s\n", __FILE__, __LINE__, #x), MOD2(x, m)) : (x))
#else
/* No range checking. */
# define CHK2(x, m)	(x)
#endif

/* New table lookup with optional range checking and no extra calculations. */
# define tsin(x)	(tbl_sin[CHK2(x, TABLE_SIZE)])
# define tcos(x)	(tbl_cos[CHK2(x, TABLE_SIZE)])

#ifndef MAX
	#define MAX(x, y) ((x)>(y) ? (x) : (y))
#endif

GLuint nullRGBA     = 0x00000000;
GLuint blackRGBA    = 0x000000ff;
GLuint whiteRGBA    = 0xffffffff;
GLuint blueRGBA     = 0x0000ffff;
GLuint redRGBA	     = 0xff0000ff;
GLuint greenRGBA    = 0x00ff00ff;
GLuint yellowRGBA   = 0xffff00ff;

GLuint stippleTexture = 0;

void draw_quad_short(short x0, short y0, short x1, short y1, short x2, short y2, short x3, short y3)
{
    GLshort vertices[] = { x0, y0, x1, y1, x3, y3, x2, y2 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_SHORT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_quad_float(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
{
    GLfloat vertices[] = { x0, y0, x1, y1, x3, y3, x2, y2 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_SHORT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_textured_quad_short(
    short x0, short y0, float s0, float t0,
    short x1, short y1, float s1, float t1,
    short x2, short y2, float s2, float t2,
    short x3, short y3, float s3, float t3)
{
    GLshort vertices[]  = { x0, y0, x1, y1, x3, y3, x2, y2 };
    GLfloat texCoords[] = { s0, t0, s1, t1, s3, t3, s2, t2 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_SHORT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_textured_quad_float(
    float x0, float y0, float s0, float t0,
    float x1, float y1, float s1, float t1,
    float x2, float y2, float s2, float t2,
    float x3, float y3, float s3, float t3)
{
    GLfloat vertices[]  = { x0, y0, x1, y1, x3, y3, x2, y2 };
    GLfloat texCoords[] = { s0, t0, s1, t1, s3, t3, s2, t2 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_quad_outline_short(short x0, short y0, short x1, short y1, short x2, short y2, short x3, short y3)
{
    GLshort vertices[] = { x0, y0, x1, y1, x2, y2, x3, y3 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_SHORT, 0, vertices);
    glDrawArrays(GL_LINE_LOOP, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_quad_outline_float(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
{
    GLfloat vertices[] = { x0, y0, x1, y1, x2, y2, x3, y3 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_LINE_LOOP, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_triangle_short(short x0, short y0, short x1, short y1, short x2, short y2)
{
    GLshort vertices[] = { x0, y0, x1, y1, x2, y2 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_SHORT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_triangle_float(float x0, float y0, float x1, float y1, float x2, float y2)
{
    GLfloat vertices[] = { x0, y0, x1, y1, x2, y2 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_SHORT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_triangle_outline_short(short x0, short y0, short x1, short y1, short x2, short y2)
{
    GLshort vertices[] = { x0, y0, x1, y1, x2, y2 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_SHORT, 0, vertices);
    glDrawArrays(GL_LINE_LOOP, 0, 3);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_triangle_outline_float(float x0, float y0, float x1, float y1, float x2, float y2)
{
    GLfloat vertices[] = { x0, y0, x1, y1, x2, y2 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_LINE_LOOP, 0, 3);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_segment_short(short x0, short y0, short x1, short y1)
{
    GLshort vertices[] = { x0, y0, x1, y1 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_SHORT, 0, vertices);
    glDrawArrays(GL_LINES, 0, 2);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_segment_float(float x0, float y0, float x1, float y1)
{
    GLfloat vertices[] = { x0, y0, x1, y1 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_LINES, 0, 2);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_point_short(short x, short y)
{
    GLshort vertices[] = { x, y };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_SHORT, 0, vertices);
    glDrawArrays(GL_POINTS, 0, 1);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_point_float(float x, float y)
{
    GLfloat vertices[] = { x, y };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_POINTS, 0, 1);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_circle(float x, float y,
	         float radius, int filled,
                 int resolution)
{
    GLfloat vertices[2*resolution];
    if (resolution <= TABLE_SIZE) {
        for (int i = 0; i < resolution; i++) {
            int tableIndex = i*TABLE_SIZE/resolution;
            vertices[2*i  ] = x + tcos(tableIndex)*radius;
            vertices[2*i+1] = y + tsin(tableIndex)*radius;
        }
    } else {
        for (int i = 0; i < resolution; i++) {
            float angle = i*2*M_PI/resolution;
            vertices[2*i  ] = x + cos(angle)*radius;
            vertices[2*i+1] = y + sin(angle)*radius;
        }
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(filled ? GL_TRIANGLE_FAN : GL_LINE_LOOP, 0, resolution);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_ring(float x, float y,
               float innerRadius, float outerRadius,
               int resolution)
{
    GLfloat vertices[4*(resolution+1)];
    if (resolution <= TABLE_SIZE) {
        for (int i = 0; i <= resolution; i++) {
            int tableIndex = (i*TABLE_SIZE/resolution) % TABLE_SIZE;
            vertices[4*i  ] = x + tcos(tableIndex)*outerRadius;
            vertices[4*i+1] = y + tsin(tableIndex)*outerRadius;
            vertices[4*i+2] = x + tcos(tableIndex)*innerRadius;
            vertices[4*i+3] = y + tsin(tableIndex)*innerRadius;
        }
    } else {
        for (int i = 0; i <= resolution; i++) {
            float angle = i*2*M_PI/resolution;
            vertices[4*i  ] = x + cos(angle)*outerRadius;
            vertices[4*i+1] = y + sin(angle)*outerRadius;
            vertices[4*i+2] = x + cos(angle)*innerRadius;
            vertices[4*i+3] = y + sin(angle)*innerRadius;
        }
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (resolution+1)*2);
    glDisableClientState(GL_VERTEX_ARRAY);
}



GLuint stipple_texture_init()
{
    if (stippleTexture)
        return stippleTexture;

    glGenTextures(1, &stippleTexture);
    
    glBindTexture(GL_TEXTURE_2D, stippleTexture);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);    
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    GLubyte data[] = { 255, 0 };
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 2, 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, data);
    
    return stippleTexture;
}

void stipple_texture_cleanup()
{
    if (stippleTexture) {
        glDeleteTextures(1, &stippleTexture);
        stippleTexture = 0;
    }
}

float stipple_texture_length(float lineLength, float scaleFactor, stipple_clamp_t clamp)
{
    float s1 = 0.5 * lineLength / scaleFactor;
    switch (clamp) {
        case STIPPLE_CLAMP_AS_LOOP:
            s1 = round(s1);
            s1 = MAX(s1, 1);
            break;
        case STIPPLE_CLAMP_AS_LINE:
            s1 = ((int)s1) + 0.5;
            s1 = MAX(s1, 1.5f);
            break;
    }
    return s1;
}

#define VV(vertex) ((GLfloat*)(vertex))
GLfloat *get_stipple_tex_coords(GLfloat *vertices, int numVertices, int stride, float stippleScale, stipple_clamp_t clamp, GLfloat *texCoords)
{
    float length = 0;
    if (numVertices == 0) {
        return texCoords;
    }
    texCoords[0] = texCoords[1] = 0;
    GLubyte *v = (GLubyte*)vertices;
    for (int i = 1; i < numVertices; i++) {
        float dx = VV(v+i*stride)[0] - VV(v+(i-1)*stride)[0];
        float dy = VV(v+i*stride)[1] - VV(v+(i-1)*stride)[1];
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


void draw_stippled_segment_short(short x0, short y0, short x1, short y1, float scaleFactor)
{
    short dx = (x1-x0);
    short dy = (y1-y0);
    float lineLength = sqrtf(dx*dx + dy*dy);
    
    GLshort vertices[] = { x0, y0, x1, y1 };
    GLfloat texCoords[] = { 0, 0, stipple_texture_length(lineLength, scaleFactor, STIPPLE_CLAMP_AS_LINE), 0 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, stippleTexture);
    glVertexPointer(2, GL_SHORT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    glDrawArrays(GL_LINES, 0, 2);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_stippled_segment_float(float x0, float y0, float x1, float y1, float scaleFactor)
{
    float dx = (x1-x0);
    float dy = (y1-y0);
    float lineLength = sqrtf(dx*dx + dy*dy);
    
    GLfloat vertices[] = { x0, y0, x1, y1 };
    GLfloat texCoords[] = { 0, 0, stipple_texture_length(lineLength, scaleFactor, STIPPLE_CLAMP_AS_LINE), 0 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, stippleTexture);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    glDrawArrays(GL_LINES, 0, 2);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}
