// 66.74 % of time
static void
FUNC_NAME(RGBA_Image *src, RGBA_Image *dst,
          int clip_x, int clip_y, int clip_w, int clip_h,
          DATA32 mul_col, int render_op,
          RGBA_Map_Point *p,
          int smooth, int anti_alias, int level EINA_UNUSED) // level unused for now - for future use
{
   int i;
   int cx, cy, cw, ch;
   int ytop, ybottom, ystart, yend, y, sw, shp, swp, direct;
   Line *spans;
   DATA32 *buf = NULL, *sp;
   RGBA_Gfx_Func func = NULL;
   Eina_Bool havea = EINA_FALSE;
   int havecol = 4;

   cx = clip_x;
   cy = clip_y;
   cw = clip_w;
   ch = clip_h;

   // find y top line and collect point color info
   ytop = p[0].y;
   if ((p[0].col >> 24) < 0xff) havea = EINA_TRUE;
   if (p[0].col == 0xffffffff) havecol--;
   for (i = 1; i < 4; i++)
     {
        if (p[i].y < ytop) ytop = p[i].y;
        if ((p[i].col >> 24) < 0xff) havea = EINA_TRUE;
        if (p[i].col == 0xffffffff) havecol--;
     }

   // find y bottom line
   ybottom = p[0].y;
   for (i = 1; i < 4; i++)
     {
        if (p[i].y > ybottom) ybottom = p[i].y;
     }

   // convert to screen space from fixed point
   ytop = ytop >> FP;
   ybottom = ybottom >> FP;

   // if its outside the clip vertical bounds - don't bother
   if ((ytop >= (cy + ch)) || (ybottom < cy)) return;

   // limit to the clip vertical bounds
   if (ytop < cy) ystart = cy;
   else ystart = ytop;
   if (ybottom >= (cy + ch)) yend = (cy + ch) - 1;
   else yend = ybottom;

   // get some source image information
   sp = src->image.data;
   sw = src->cache_entry.w;
   swp = sw << (FP + FPI);
   shp = src->cache_entry.h << (FP + FPI);

   // limit u,v coords of points to be within the source image
   for (i = 0; i < 4; i++)
     {
        if (p[i].u < 0) p[i].u = 0;
        else if (p[i].u > (int)(src->cache_entry.w << FP))
          p[i].u = src->cache_entry.w << FP;

        if (p[i].v < 0) p[i].v = 0;
        else if (p[i].v > (int)(src->cache_entry.h << FP))
          p[i].v = src->cache_entry.h << FP;
     }

   // allocate some spans to hold out span list
   spans = alloca((yend - ystart + 1) * sizeof(Line));
   memset(spans, 0, (yend - ystart + 1) * sizeof(Line));

   // calculate the spans list
   _calc_spans(p, spans, ystart, yend, cx, cy, cw, ch);

   // calculate anti alias edges
   if (anti_alias) _calc_aa_edges(spans, ystart, yend);

   // walk through spans and render

   // if operation is solid, bypass buf and draw func and draw direct to dst
   direct = 0;


   /* FIXME: even if anti-alias is enabled, only edges may require the
      pixels composition. we can optimize it. */

   if ((!src->cache_entry.flags.alpha) && (!dst->cache_entry.flags.alpha) &&
       (mul_col == 0xffffffff) && (!havea) && (!anti_alias))
     {
        direct = 1;
     }
   else
     {
        int pa;

        buf = alloca(cw * sizeof(DATA32));
        pa = src->cache_entry.flags.alpha;
        if (havea) src->cache_entry.flags.alpha = 1;
        if (mul_col != 0xffffffff)
          func = evas_common_gfx_func_composite_pixel_color_span_get(src->cache_entry.flags.alpha, src->cache_entry.flags.alpha_sparse, mul_col, dst->cache_entry.flags.alpha, cw, render_op);
        else
          func = evas_common_gfx_func_composite_pixel_span_get(src->cache_entry.flags.alpha, src->cache_entry.flags.alpha_sparse, dst->cache_entry.flags.alpha, cw, render_op);

        if (anti_alias) src->cache_entry.flags.alpha = EINA_TRUE;
        else src->cache_entry.flags.alpha = pa;
     }
   if (havecol == 0)
     {
#undef COLMUL
#include "evas_map_image_core.c"
     }
   else
     {
#define COLMUL 1
#include "evas_map_image_core.c"
     }
}

static void
FUNC_NAME_DO(RGBA_Image *src, RGBA_Image *dst,
             RGBA_Draw_Context *dc,
             const RGBA_Map_Spans *ms,
             int smooth, int anti_alias EINA_UNUSED, int level EINA_UNUSED) // level unused for now - for future use
{
   Line *spans;
   DATA32 *buf = NULL, *sp;
   RGBA_Gfx_Func func = NULL;
   int cx, cy, cw, ch;
   DATA32 mul_col;
   int ystart, yend, y, sw, shp, swp, direct;
   int havecol;
   int i;

   cx = dc->clip.x;
   cy = dc->clip.y;
   cw = dc->clip.w;
   ch = dc->clip.h;

   mul_col = dc->mul.use ? dc->mul.col : 0xffffffff;

   if (ms->ystart < cy) ystart = cy;
   else ystart = ms->ystart;
   if (ms->yend >= (cy + ch)) yend = (cy + ch) - 1;
   else yend = ms->yend;

   // get some source image information
   sp = src->image.data;
   sw = src->cache_entry.w;
   swp = sw << (FP + FPI);
   shp = src->cache_entry.h << (FP + FPI);
   havecol = ms->havecol;
   direct = ms->direct;
   // allocate some s to hold out span list
   spans = alloca((yend - ystart + 1) * sizeof(Line));
   memcpy(spans, &ms->spans[ystart - ms->ystart],
          (yend - ystart + 1) * sizeof(Line));
   _clip_spans(spans, ystart, yend, cx, cw, EINA_TRUE);

   // if operation is solid, bypass buf and draw func and draw direct to dst
   if (!direct)
     {
        int pa;

        buf = alloca(cw * sizeof(DATA32));
        pa = src->cache_entry.flags.alpha;
        if (ms->havea) src->cache_entry.flags.alpha = 1;
        if (dc->mul.use)
          func = evas_common_gfx_func_composite_pixel_color_span_get(src->cache_entry.flags.alpha, src->cache_entry.flags.alpha_sparse, dc->mul.col, dst->cache_entry.flags.alpha, cw, dc->render_op);
        else
          func = evas_common_gfx_func_composite_pixel_span_get(src->cache_entry.flags.alpha, src->cache_entry.flags.alpha_sparse, dst->cache_entry.flags.alpha, cw, dc->render_op);
        src->cache_entry.flags.alpha = pa;
     }

   if (havecol == 0)
     {
#undef COLMUL
#include "evas_map_image_core.c"
     }
   else
     {
#define COLMUL 1
#include "evas_map_image_core.c"
     }
}
