#ifndef EVAS_ENGINE_H
# define EVAS_ENGINE_H

# include "config.h"
# include "evas_common_private.h"
# include "evas_private.h"
# include "Evas.h"
# include "Evas_Engine_Wayland_Egl.h"

# include "www.h"

/* NB: This already includes wayland-client.h */
# include <wayland-egl.h>

# define GL_GLEXT_PROTOTYPES

# if !defined(HAVE_ECORE_X_XLIB) && !defined(MESA_EGL_NO_X11_HEADERS)
#  define MESA_EGL_NO_X11_HEADERS
# endif

# include <EGL/egl.h>
# include <GLES2/gl2.h>
# include <GLES2/gl2ext.h>

# include "../gl_generic/Evas_Engine_GL_Generic.h"

extern int _evas_engine_wl_egl_log_dom;

# ifdef ERR
#  undef ERR
# endif
# define ERR(...) EINA_LOG_DOM_ERR(_evas_engine_wl_egl_log_dom, __VA_ARGS__)

# ifdef DBG
#  undef DBG
# endif
# define DBG(...) EINA_LOG_DOM_DBG(_evas_engine_wl_egl_log_dom, __VA_ARGS__)

# ifdef INF
#  undef INF
# endif
# define INF(...) EINA_LOG_DOM_INFO(_evas_engine_wl_egl_log_dom, __VA_ARGS__)

# ifdef WRN
#  undef WRN
# endif
# define WRN(...) EINA_LOG_DOM_WARN(_evas_engine_wl_egl_log_dom, __VA_ARGS__)

# ifdef CRI
#  undef CRI
# endif
# define CRI(...) EINA_LOG_DOM_CRIT(_evas_engine_wl_egl_log_dom, __VA_ARGS__)

# ifndef EGL_BUFFER_AGE_EXT
#  define EGL_BUFFER_AGE_EXT 0x313d
# endif

struct _Outbuf
{
   struct wl_display *disp;
   struct wl_egl_window *win;
   struct wl_surface *surface;
   int w, h;
   int depth, screen, rot, alpha;

   Evas *evas;
   Evas_Engine_Info_Wayland_Egl *info;
   Evas_Engine_GL_Context *gl_context;

   Render_Engine_Swap_Mode swap_mode;
   int prev_age, vsync;
   int frame_cnt;

   struct 
     {
        Eina_Bool drew : 1;
     } draw;

   EGLContext egl_context[1];
   EGLSurface egl_surface[1];
   EGLConfig egl_config;
   EGLDisplay egl_disp;

   Eina_Bool lost_back : 1;
   Eina_Bool surf : 1;

   Model *model;

   float offset_x, offset_y;

   Evas_GL_Redirect *redirect;
};

struct _Context_3D
{
   EGLDisplay display;
   EGLContext context;
   EGLSurface surface;
};

extern Eina_Bool extn_have_buffer_age;
extern Eina_Bool extn_have_y_inverted;

extern Evas_GL_Common_Context_New glsym_evas_gl_common_context_new;
extern Evas_GL_Common_Context_Call glsym_evas_gl_common_context_flush;
extern Evas_GL_Common_Context_Call glsym_evas_gl_common_context_free;
extern Evas_GL_Common_Context_Call glsym_evas_gl_common_context_use;
extern Evas_GL_Common_Context_Call glsym_evas_gl_common_context_newframe;
extern Evas_GL_Common_Context_Call glsym_evas_gl_common_context_done;
extern Evas_GL_Common_Context_Resize_Call glsym_evas_gl_common_context_resize;
extern Evas_GL_Common_Buffer_Dump_Call glsym_evas_gl_common_buffer_dump;
extern Evas_GL_Preload_Render_Call glsym_evas_gl_preload_render_lock;
extern Evas_GL_Preload_Render_Call glsym_evas_gl_preload_render_unlock;

extern Evas_GL_Redirect *(*glsym_evas_gl_common_context_redirect) (Evas_Engine_GL_Context *gc);
extern void              (*glsym_evas_gl_common_context_unredirect) (Evas_GL_Redirect *re);
extern GLuint            (*glsym_evas_gl_common_context_redirect_texture_get) (Evas_GL_Redirect *re);
extern void              (*glsym_evas_gl_common_context_redirect_bind) (Evas_GL_Redirect *re);
extern void              (*glsym_evas_gl_common_context_redirect_unbind) (Evas_GL_Redirect *re);

extern unsigned int (*glsym_eglSwapBuffersWithDamage) (EGLDisplay a, void *b, const EGLint *d, EGLint c);
extern unsigned int (*glsym_eglSetDamageRegionKHR) (EGLDisplay a, EGLSurface b, EGLint *c, EGLint d);

extern GLuint       (*glsym_glCreateShader) (GLenum a);
extern void         (*glsym_glShaderSource) (GLuint a, GLsizei b, const GLchar **c, const GLint *d);
extern void         (*glsym_glCompileShader) (GLuint a);
extern void         (*glsym_glGetShaderiv) (GLuint a, GLenum b, GLint *c);
extern void         (*glsym_glGetShaderInfoLog) (GLuint a, GLsizei b, GLsizei *c, GLchar *d);
extern GLuint       (*glsym_glCreateProgram) (void);
extern void         (*glsym_glAttachShader) (GLuint a, GLuint b);
extern void         (*glsym_glBindAttribLocation) (GLuint a, GLuint b, const GLchar *c);
extern void         (*glsym_glLinkProgram) (GLuint a);
extern void         (*glsym_glGetProgramiv) (GLuint a, GLenum b, GLint *c);
extern void         (*glsym_glGetProgramInfoLog) (GLuint a, GLsizei b, GLsizei *c, GLchar *d);


Outbuf *eng_window_new(Evas *evas, Evas_Engine_Info_Wayland_Egl *einfo, int w, int h, Render_Engine_Swap_Mode swap_mode);
void eng_window_free(Outbuf *gw);
void eng_window_use(Outbuf *gw);
void eng_window_unsurf(Outbuf *gw);
void eng_window_resurf(Outbuf *gw);

void eng_outbuf_reconfigure(Outbuf *ob, int w, int h, int rot, Outbuf_Depth depth);
int eng_outbuf_rotation_get(Outbuf *ob);
Render_Engine_Swap_Mode eng_outbuf_swap_mode_get(Outbuf *ob);
Eina_Bool eng_outbuf_region_first_rect(Outbuf *ob);
void *eng_outbuf_update_region_new(Outbuf *ob, int x, int y, int w, int h, int *cx, int *cy, int *cw, int *ch);
void eng_outbuf_update_region_free(Outbuf *ob, RGBA_Image *update);
void eng_outbuf_update_region_push(Outbuf *ob, RGBA_Image *update, int x, int y, int w, int h);
void eng_outbuf_flush(Outbuf *ob, Tilebuf_Rect *rects, Evas_Render_Mode render_mode);

Evas_Engine_GL_Context *eng_outbuf_gl_context_get(Outbuf *ob);
void *eng_outbuf_egl_display_get(Outbuf *ob);
Eina_Bool eng_preload_make_current(void *data, void *doit);

Context_3D *eng_gl_context_new(Outbuf *win);
void eng_gl_context_free(Context_3D *context);
void eng_gl_context_use(Context_3D *context);


void *eng_display_get(void);

static inline int 
_re_wincheck(Outbuf *ob)
{
   if (ob->surf) return 1;
   eng_window_resurf(ob);
   ob->lost_back = EINA_TRUE;
   if (!ob->surf)
     ERR("Wayland EGL Engine cannot recreate window surface");
   return 0;
}

#endif
