#ifndef SIMPLE_TRIANGULATOR_H_
#define SIMPLE_TRIANGULATOR_H_

#include <Efl.h>

typedef struct _Simple_Triangulator Simple_Triangulator;
struct _Simple_Triangulator
{
   Eina_Inarray *vertices;
   Eina_Inarray *stops;  //list of contours need to be drawn as separate triangle fan.
   float minx;
   float miny;
   float maxx;
   float maxy;
};

/**
 * Creates a new simple triangulator.
 *
 */
Simple_Triangulator * simple_triangulator_new(void);

/**
 * Frees the given triangulator and any associated resource.
 *
 * st The given triangulator.
 */
void simple_triangulator_free(Simple_Triangulator *st);

/**
 * Process the command list to generate triangle fans.
 * The alogrithm handles multiple contours by providing the list of stops.
 *
 * cmds :        commnad list
 * pts  :        point list.
 * convex :      shape is convex or not.
 *
 * output: If the shape is convex then, the triangle fan will exactly fill the shape. but if its not convex, it will overflow
 *         to outside shape, in that case user has to use stencil method (2 pass drawing) to fill the shape.
 */
void simple_triangulator_process(Simple_Triangulator *st, const Efl_Gfx_Path_Command *cmds, const double *pts, Eina_Bool convex);

#endif // #endif // SIMPLE_TRIANGULATOR_H_