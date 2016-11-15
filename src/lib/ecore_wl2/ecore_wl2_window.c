#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ecore_wl2_private.h"

static void
_session_recovery_create_uuid(void *data, struct zwp_e_session_recovery *session_recovery EINA_UNUSED, struct wl_surface *surface EINA_UNUSED, const char *uuid)
{
   Ecore_Wl2_Window *win = data;

   eina_stringshare_replace(&win->uuid, uuid);
}

static const struct zwp_e_session_recovery_listener _session_listener =
{
   _session_recovery_create_uuid,
};

static void
_ecore_wl2_window_configure_send(Ecore_Wl2_Window *window, int w, int h, unsigned int edges, Eina_Bool fs, Eina_Bool max)
{
   Ecore_Wl2_Event_Window_Configure *ev;

   ev = calloc(1, sizeof(Ecore_Wl2_Event_Window_Configure));
   if (!ev) return;

   ev->win = window->id;
   ev->event_win = window->id;
   ev->w = w;
   ev->h = h;
   ev->edges = edges;
   if (fs)
     ev->states |= ECORE_WL2_WINDOW_STATE_FULLSCREEN;
   if (max)
     ev->states |= ECORE_WL2_WINDOW_STATE_MAXIMIZED;

   ecore_event_add(ECORE_WL2_EVENT_WINDOW_CONFIGURE, ev, NULL, NULL);
}

static void
_wl_shell_surface_cb_ping(void *data EINA_UNUSED, struct wl_shell_surface *shell_surface, unsigned int serial)
{
   wl_shell_surface_pong(shell_surface, serial);
}

static void
_wl_shell_surface_cb_configure(void *data, struct wl_shell_surface *shell_surface EINA_UNUSED, unsigned int edges, int w, int h)
{
   Ecore_Wl2_Window *win = data;

   _ecore_wl2_window_configure_send(win, w, h, edges, win->fullscreen, win->maximized);
}

static void
_wl_shell_surface_cb_popup_done(void *data EINA_UNUSED, struct wl_shell_surface *shell_surface EINA_UNUSED)
{
   Ecore_Wl2_Window *win;

   win = data;
   if (!win) return;

   _ecore_wl2_input_ungrab(win->input);
}

static const struct wl_shell_surface_listener _wl_shell_surface_listener =
{
   _wl_shell_surface_cb_ping,
   _wl_shell_surface_cb_configure,
   _wl_shell_surface_cb_popup_done
};

static void
_xdg_popup_cb_done(void *data, struct xdg_popup *xdg_popup EINA_UNUSED)
{
   Ecore_Wl2_Window *win;

   win = data;
   if (!win) return;

   _ecore_wl2_input_ungrab(win->input);
}

static const struct xdg_popup_listener _xdg_popup_listener =
{
   _xdg_popup_cb_done,
};

static void
_xdg_surface_cb_configure(void *data, struct xdg_surface *xdg_surface EINA_UNUSED, int32_t w, int32_t h, struct wl_array *states, uint32_t serial)
{
   Ecore_Wl2_Window *win = data;
   uint32_t *s;
   Eina_Bool fs, max;

   if ((!win->maximized) && (!win->fullscreen))
     win->saved = win->geometry;

   fs = win->fullscreen;
   max = win->maximized;

   win->minimized = EINA_FALSE;
   win->maximized = EINA_FALSE;
   win->fullscreen = EINA_FALSE;
   win->focused = EINA_FALSE;
   win->resizing = EINA_FALSE;

   wl_array_for_each(s, states)
     {
        switch (*s)
          {
           case XDG_SURFACE_STATE_MAXIMIZED:
             win->maximized = EINA_TRUE;
             break;
           case XDG_SURFACE_STATE_FULLSCREEN:
             win->fullscreen = EINA_TRUE;
             break;
           case XDG_SURFACE_STATE_RESIZING:
             win->resizing = EINA_TRUE;
             break;
           case XDG_SURFACE_STATE_ACTIVATED:
             win->focused = EINA_TRUE;
             win->minimized = EINA_FALSE;
           default:
             break;
          }
     }

   win->configure_serial = serial;
   if ((win->geometry.w == w) && (win->geometry.h == h))
     w = h = 0;
   else if ((!w) && (!h) && (!win->fullscreen) && (!win->maximized) &&
            ((win->fullscreen != fs) || (win->maximized != max)))
     w = win->saved.w, h = win->saved.h;

   _ecore_wl2_window_configure_send(win, w, h, !!win->resizing,
                                    win->fullscreen, win->maximized);
}

static void
_xdg_surface_cb_delete(void *data, struct xdg_surface *xdg_surface EINA_UNUSED)
{
   Ecore_Wl2_Window *win;

   win = data;
   if (!win) return;

   ecore_wl2_window_free(win);
}

static const struct xdg_surface_listener _xdg_surface_listener =
{
   _xdg_surface_cb_configure,
   _xdg_surface_cb_delete,
};

static void
_ecore_wl2_window_type_set(Ecore_Wl2_Window *win)
{
   switch (win->type)
     {
      case ECORE_WL2_WINDOW_TYPE_MENU:
          {
             Ecore_Wl2_Input *input;

             input = win->input;
             if ((!input) && (win->parent))
               {
                  input = win->parent->input;
               }

             if ((!input) || (!input->wl.seat)) return;

             if (win->xdg_surface)
               {
                  win->xdg_popup =
                    xdg_shell_get_xdg_popup(win->display->wl.xdg_shell,
                                            win->surface, win->parent->surface,
                                            input->wl.seat,
                                            win->display->serial,
                                            win->geometry.x, win->geometry.y);
                  if (!win->xdg_popup)
                    {
                       ERR("Could not create xdg popup");
                       return;
                    }

                  xdg_popup_set_user_data(win->xdg_popup, win);
                  xdg_popup_add_listener(win->xdg_popup,
                                         &_xdg_popup_listener, win);
               }
             else if (win->wl_shell_surface)
               {
                  wl_shell_surface_set_popup(win->wl_shell_surface,
                                             input->wl.seat,
                                             win->display->serial,
                                             win->parent->surface,
                                             win->geometry.x,
                                             win->geometry.y, 0);
               }
          }
        break;
      case ECORE_WL2_WINDOW_TYPE_TOPLEVEL:
        if (win->xdg_surface)
          xdg_surface_set_parent(win->xdg_surface, NULL);
        else if (win->wl_shell_surface)
          wl_shell_surface_set_toplevel(win->wl_shell_surface);
        break;
      default:
        break;
     }
}

static void
_www_surface_end_drag(void *data, struct www_surface *www_surface EINA_UNUSED)
{
   Ecore_Wl2_Window *window = data;
   Ecore_Wl2_Event_Window_WWW_Drag *ev;

   ev = malloc(sizeof(Ecore_Wl2_Event_Window_WWW_Drag));
   EINA_SAFETY_ON_NULL_RETURN(ev);
   ev->window = window->id;
   ev->dragging = 0;

   ecore_event_add(_ecore_wl2_event_window_www_drag, ev, NULL, NULL);
}

static void
_www_surface_start_drag(void *data, struct www_surface *www_surface EINA_UNUSED)
{
   Ecore_Wl2_Window *window = data;
   Ecore_Wl2_Event_Window_WWW_Drag *ev;

   ev = malloc(sizeof(Ecore_Wl2_Event_Window_WWW_Drag));
   EINA_SAFETY_ON_NULL_RETURN(ev);
   ev->window = window->id;
   ev->dragging = 1;

   ecore_event_add(_ecore_wl2_event_window_www_drag, ev, NULL, NULL);
}

static void
_www_surface_status(void *data, struct www_surface *www_surface EINA_UNUSED, int32_t x_rel, int32_t y_rel, uint32_t timestamp)
{
   Ecore_Wl2_Window *window = data;
   Ecore_Wl2_Event_Window_WWW *ev;

   ev = malloc(sizeof(Ecore_Wl2_Event_Window_WWW));
   EINA_SAFETY_ON_NULL_RETURN(ev);
   ev->window = window->id;
   ev->x_rel = x_rel;
   ev->y_rel = y_rel;
   ev->timestamp = timestamp;

   ecore_event_add(_ecore_wl2_event_window_www, ev, NULL, NULL);
}

static struct www_surface_listener _www_surface_listener =
{
   .status = _www_surface_status,
   .start_drag = _www_surface_start_drag,
   .end_drag = _www_surface_end_drag,
};

void
_ecore_wl2_window_www_surface_init(Ecore_Wl2_Window *window)
{
   if (!window->surface) return;
   if (!window->display->wl.www) return;
   if (window->www_surface) return;
   window->www_surface = www_create(window->display->wl.www, window->surface);
   www_surface_set_user_data(window->www_surface, window);
   www_surface_add_listener(window->www_surface, &_www_surface_listener, window);
}

void
_ecore_wl2_window_shell_surface_init(Ecore_Wl2_Window *window)
{
   if (!window->surface) return;
   if ((window->display->wl.xdg_shell) && (!window->xdg_surface))
     {
        window->xdg_surface =
          xdg_shell_get_xdg_surface(window->display->wl.xdg_shell,
                                    window->surface);
        if (!window->xdg_surface) goto surf_err;

        if (window->title)
          xdg_surface_set_title(window->xdg_surface, window->title);
        if (window->class)
          xdg_surface_set_app_id(window->xdg_surface, window->class);

        xdg_surface_set_user_data(window->xdg_surface, window);
        xdg_surface_add_listener(window->xdg_surface,
                                 &_xdg_surface_listener, window);

        window->configure_ack = xdg_surface_ack_configure;
        _ecore_wl2_window_type_set(window);
        if (window->display->wl.session_recovery)
          {
             if (window->uuid)
               {
                  zwp_e_session_recovery_set_uuid(window->display->wl.session_recovery,
                    window->surface, window->uuid);
                  xdg_surface_set_window_geometry(window->xdg_surface,
                    window->geometry.x, window->geometry.y,
                    window->geometry.w, window->geometry.h);
                  ecore_wl2_window_opaque_region_set(window,
                    window->opaque.x, window->opaque.y,
                    window->opaque.w, window->opaque.h);
               }
             else
               zwp_e_session_recovery_get_uuid(window->display->wl.session_recovery, window->surface);
          }
     }
   else if ((window->display->wl.wl_shell) && (!window->wl_shell_surface))
     {
        window->wl_shell_surface =
          wl_shell_get_shell_surface(window->display->wl.wl_shell,
                                     window->surface);
        if (!window->wl_shell_surface) goto surf_err;

        if (window->title)
          wl_shell_surface_set_title(window->wl_shell_surface, window->title);
        if (window->class)
          wl_shell_surface_set_class(window->wl_shell_surface, window->class);

        wl_shell_surface_add_listener(window->wl_shell_surface,
                                      &_wl_shell_surface_listener, window);
        _ecore_wl2_window_type_set(window);
     }

   return;

surf_err:
   ERR("Failed to create surface for window");
}

static void
_ecore_wl2_window_surface_create(Ecore_Wl2_Window *window)
{
   if (window->surface) return;

   EINA_SAFETY_ON_NULL_RETURN(window->display->wl.compositor);

   window->surface =
     wl_compositor_create_surface(window->display->wl.compositor);
   if (!window->surface)
     {
        ERR("Failed to create surface for window");
        return;
     }

   window->surface_id =
     wl_proxy_get_id((struct wl_proxy *)window->surface);

   if (window->display->wl.session_recovery)
     zwp_e_session_recovery_add_listener(window->display->wl.session_recovery,
                                         &_session_listener, window);
}

EAPI Ecore_Wl2_Window *
ecore_wl2_window_new(Ecore_Wl2_Display *display, Ecore_Wl2_Window *parent, int x, int y, int w, int h)
{
   Ecore_Wl2_Window *win;
   static int _win_id = 1;

   EINA_SAFETY_ON_NULL_RETURN_VAL(display, NULL);

   /* try to allocate space for window structure */
   win = calloc(1, sizeof(Ecore_Wl2_Window));
   if (!win) return NULL;

   win->display = display;
   win->parent = parent;
   win->id = _win_id++;

   win->geometry.x = x;
   win->geometry.y = y;
   win->geometry.w = w;
   win->geometry.h = h;

   win->opaque.x = x;
   win->opaque.y = y;
   win->opaque.w = w;
   win->opaque.h = h;

   win->type = ECORE_WL2_WINDOW_TYPE_TOPLEVEL;

   display->windows =
     eina_inlist_append(display->windows, EINA_INLIST_GET(win));

   return win;
}

EAPI int
ecore_wl2_window_id_get(Ecore_Wl2_Window *window)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, -1);
   return window->id;
}

EAPI struct wl_surface *
ecore_wl2_window_surface_get(Ecore_Wl2_Window *window)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, NULL);

   _ecore_wl2_window_surface_create(window);

   return window->surface;
}

EAPI int
ecore_wl2_window_surface_id_get(Ecore_Wl2_Window *window)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, -1);
   return window->surface_id;
}

EAPI void
ecore_wl2_window_show(Ecore_Wl2_Window *window)
{
   EINA_SAFETY_ON_NULL_RETURN(window);

   _ecore_wl2_window_surface_create(window);

   if (window->input_set)
     ecore_wl2_window_input_region_set(window, window->input_rect.x, window->input_rect.y,
                                               window->input_rect.w, window->input_rect.h);
   if (window->opaque_set)
     ecore_wl2_window_opaque_region_set(window, window->opaque.x, window->opaque.y,
                                               window->opaque.w, window->opaque.h);

   if ((window->type != ECORE_WL2_WINDOW_TYPE_DND) &&
       (window->type != ECORE_WL2_WINDOW_TYPE_NONE))
     {
        _ecore_wl2_window_shell_surface_init(window);
        _ecore_wl2_window_www_surface_init(window);
     }
}

EAPI void
ecore_wl2_window_hide(Ecore_Wl2_Window *window)
{
   Ecore_Wl2_Subsurface *subsurf;
   Eina_Inlist *tmp;
   EINA_SAFETY_ON_NULL_RETURN(window);

   if (window->xdg_surface) xdg_surface_destroy(window->xdg_surface);
   window->xdg_surface = NULL;

   if (window->xdg_popup) xdg_popup_destroy(window->xdg_popup);
   window->xdg_popup = NULL;

   if (window->wl_shell_surface)
     wl_shell_surface_destroy(window->wl_shell_surface);
   window->wl_shell_surface = NULL;

   if (window->www_surface)
     www_surface_destroy(window->www_surface);
   window->www_surface = NULL;

   EINA_INLIST_FOREACH_SAFE(window->subsurfs, tmp, subsurf)
     _ecore_wl2_subsurf_unmap(subsurf);

   if (window->uuid && window->surface && window->display->wl.session_recovery)
     zwp_e_session_recovery_destroy_uuid(window->display->wl.session_recovery,
       window->surface, window->uuid);

   if (window->surface) wl_surface_destroy(window->surface);
   window->surface = NULL;

   window->configure_serial = 0;
   window->configure_ack = NULL;

   window->surface_id = -1;
}

EAPI void
ecore_wl2_window_free(Ecore_Wl2_Window *window)
{
   Ecore_Wl2_Display *display;
   Ecore_Wl2_Input *input;
   Ecore_Wl2_Subsurface *subsurf;
   Eina_Inlist *tmp;

   EINA_SAFETY_ON_NULL_RETURN(window);

   display = window->display;

   EINA_INLIST_FOREACH(display->inputs, input)
      _ecore_wl2_input_window_remove(input, window);

   EINA_INLIST_FOREACH_SAFE(window->subsurfs, tmp, subsurf)
     _ecore_wl2_subsurf_free(subsurf);

   ecore_wl2_window_hide(window);
   eina_stringshare_replace(&window->uuid, NULL);

   if (window->title) eina_stringshare_del(window->title);
   if (window->class) eina_stringshare_del(window->class);

   display->windows =
     eina_inlist_remove(display->windows, EINA_INLIST_GET(window));

   free(window);
}

EAPI void
ecore_wl2_window_move(Ecore_Wl2_Window *window, int x EINA_UNUSED, int y EINA_UNUSED)
{
   Ecore_Wl2_Input *input;

   EINA_SAFETY_ON_NULL_RETURN(window);

   input = window->input;
   if ((!input) && (window->parent))
     {
        input = window->parent->input;
     }

   if ((!input) || (!input->wl.seat)) return;

   window->moving = EINA_TRUE;

   if (window->xdg_surface)
     xdg_surface_move(window->xdg_surface, input->wl.seat,
                      window->display->serial);
   else if (window->wl_shell_surface)
     wl_shell_surface_move(window->wl_shell_surface, input->wl.seat,
                           window->display->serial);
}

EAPI void
ecore_wl2_window_resize(Ecore_Wl2_Window *window, int w EINA_UNUSED, int h EINA_UNUSED, int location)
{
   Ecore_Wl2_Input *input;

   EINA_SAFETY_ON_NULL_RETURN(window);

   input = window->input;
   if ((!input) && (window->parent))
     {
        input = window->parent->input;
     }

   if ((!input) || (!input->wl.seat)) return;

   if (window->xdg_surface)
     xdg_surface_resize(window->xdg_surface, input->wl.seat,
                        input->display->serial, location);
   else if (window->wl_shell_surface)
     wl_shell_surface_resize(window->wl_shell_surface, input->wl.seat,
                             input->display->serial, location);
}

EAPI void
ecore_wl2_window_raise(Ecore_Wl2_Window *window)
{
   EINA_SAFETY_ON_NULL_RETURN(window);

   if (window->xdg_surface)
     {
        struct wl_array states;
        uint32_t *s;

        wl_array_init(&states);
        s = wl_array_add(&states, sizeof(*s));
        *s = XDG_SURFACE_STATE_ACTIVATED;
        _xdg_surface_cb_configure(window, window->xdg_surface,
                                  window->geometry.w, window->geometry.h,
                                  &states, 0);
        wl_array_release(&states);
     }
   else if (window->wl_shell_surface)
     wl_shell_surface_set_toplevel(window->wl_shell_surface);
}

EAPI Eina_Bool
ecore_wl2_window_alpha_get(Ecore_Wl2_Window *window)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EINA_FALSE);

   return window->alpha;
}

EAPI void
ecore_wl2_window_alpha_set(Ecore_Wl2_Window *window, Eina_Bool alpha)
{
   EINA_SAFETY_ON_NULL_RETURN(window);

   if (window->alpha == alpha) return;

   window->alpha = alpha;

   if (!window->alpha)
     ecore_wl2_window_opaque_region_set(window, window->opaque.x,
                                        window->opaque.y, window->opaque.w,
                                        window->opaque.h);
   else
     ecore_wl2_window_opaque_region_set(window, 0, 0, 0, 0);
}

EAPI void
ecore_wl2_window_transparent_set(Ecore_Wl2_Window *window, Eina_Bool transparent)
{
   EINA_SAFETY_ON_NULL_RETURN(window);

   if (window->transparent == transparent) return;

   window->transparent = transparent;

   if (!window->transparent)
     ecore_wl2_window_opaque_region_set(window, window->opaque.x,
                                        window->opaque.y, window->opaque.w,
                                        window->opaque.h);
   else
     ecore_wl2_window_opaque_region_set(window, 0, 0, 0, 0);
}

EAPI void
ecore_wl2_window_opaque_region_set(Ecore_Wl2_Window *window, int x, int y, int w, int h)
{
   struct wl_region *region;

   EINA_SAFETY_ON_NULL_RETURN(window);

   window->opaque.x = x;
   window->opaque.y = y;
   window->opaque.w = w;
   window->opaque.h = h;
   window->opaque_set = 1;

   if ((window->transparent) || (window->alpha)) return;
   if (!window->surface) return; //surface not created yet

   region = wl_compositor_create_region(window->display->wl.compositor);
   if (!region)
     {
        ERR("Failed to create opaque region");
        return;
     }

   switch (window->rotation)
     {
      case 0:
        wl_region_add(region, x, y, w, h);
        break;
      case 180:
        wl_region_add(region, x, x + y, w, h);
        break;
      case 90:
        wl_region_add(region, y, x, h, w);
        break;
      case 270:
        wl_region_add(region, x + y, x, h, w);
        break;
     }

   wl_surface_set_opaque_region(window->surface, region);
   wl_region_destroy(region);
}

EAPI void
ecore_wl2_window_input_region_set(Ecore_Wl2_Window *window, int x, int y, int w, int h)
{
   struct wl_region *region;

   EINA_SAFETY_ON_NULL_RETURN(window);

   window->input_rect.x = x;
   window->input_rect.y = y;
   window->input_rect.w = w;
   window->input_rect.h = h;
   window->input_set = 1;

   if (window->type == ECORE_WL2_WINDOW_TYPE_DND) return;
   if (!window->surface) return; //surface not created yet

   region = wl_compositor_create_region(window->display->wl.compositor);
   if (!region)
     {
        ERR("Failed to create opaque region");
        return;
     }

   switch (window->rotation)
     {
      case 0:
        wl_region_add(region, x, y, w, h);
        break;
      case 180:
        wl_region_add(region, x, x + y, w, h);
        break;
      case 90:
        wl_region_add(region, y, x, h, w);
        break;
      case 270:
        wl_region_add(region, x + y, x, h, w);
        break;
     }

   wl_surface_set_input_region(window->surface, region);
   wl_region_destroy(region);
}

EAPI Eina_Bool
ecore_wl2_window_maximized_get(Ecore_Wl2_Window *window)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EINA_FALSE);

   return window->maximized;
}

EAPI void
ecore_wl2_window_maximized_set(Ecore_Wl2_Window *window, Eina_Bool maximized)
{
   Eina_Bool prev;

   EINA_SAFETY_ON_NULL_RETURN(window);

   prev = window->maximized;
   maximized = !!maximized;
   if (prev == maximized) return;

   if (window->wl_shell_surface)
     window->maximized = maximized;

   if (maximized)
     {
        window->saved = window->geometry;

        if (window->xdg_surface)
          xdg_surface_set_maximized(window->xdg_surface);
        else if (window->wl_shell_surface)
          wl_shell_surface_set_maximized(window->wl_shell_surface, NULL);
     }
   else
     {
        if (window->xdg_surface)
          xdg_surface_unset_maximized(window->xdg_surface);
        else if (window->wl_shell_surface)
          {
             wl_shell_surface_set_toplevel(window->wl_shell_surface);

             _ecore_wl2_window_configure_send(window, window->saved.w,
                                              window->saved.h, 0, window->fullscreen, window->maximized);
          }
     }
}

EAPI Eina_Bool
ecore_wl2_window_fullscreen_get(Ecore_Wl2_Window *window)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EINA_FALSE);

   return window->fullscreen;
}

EAPI void
ecore_wl2_window_fullscreen_set(Ecore_Wl2_Window *window, Eina_Bool fullscreen)
{
   Eina_Bool prev;

   EINA_SAFETY_ON_NULL_RETURN(window);

   prev = window->fullscreen;
   fullscreen = !!fullscreen;
   if (prev == fullscreen) return;

   if (window->wl_shell_surface)
     window->fullscreen = fullscreen;

   if (fullscreen)
     {
        window->saved = window->geometry;

        if (window->xdg_surface)
          xdg_surface_set_fullscreen(window->xdg_surface, NULL);
        else if (window->wl_shell_surface)
          wl_shell_surface_set_fullscreen(window->wl_shell_surface,
                                          WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                          0, NULL);
     }
   else
     {
        if (window->xdg_surface)
          xdg_surface_unset_fullscreen(window->xdg_surface);
        else if (window->wl_shell_surface)
          {
             wl_shell_surface_set_toplevel(window->wl_shell_surface);

             _ecore_wl2_window_configure_send(window, window->saved.w,
                                              window->saved.h, 0, window->fullscreen, window->maximized);
          }
     }
}

EAPI int
ecore_wl2_window_rotation_get(Ecore_Wl2_Window *window)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, -1);

   return window->rotation;
}

EAPI void
ecore_wl2_window_rotation_set(Ecore_Wl2_Window *window, int rotation)
{
   EINA_SAFETY_ON_NULL_RETURN(window);

   window->rotation = rotation;
}

EAPI void
ecore_wl2_window_title_set(Ecore_Wl2_Window *window, const char *title)
{
   EINA_SAFETY_ON_NULL_RETURN(window);

   eina_stringshare_replace(&window->title, title);
   if (!window->title) return;

   if (window->xdg_surface)
     xdg_surface_set_title(window->xdg_surface, window->title);
   else if (window->wl_shell_surface)
     wl_shell_surface_set_title(window->wl_shell_surface, window->title);
}

EAPI void
ecore_wl2_window_class_set(Ecore_Wl2_Window *window, const char *clas)
{
   EINA_SAFETY_ON_NULL_RETURN(window);

   eina_stringshare_replace(&window->class, clas);
   if (!window->class) return;

   if (window->xdg_surface)
     xdg_surface_set_app_id(window->xdg_surface, window->class);
   else if (window->wl_shell_surface)
     wl_shell_surface_set_class(window->wl_shell_surface, window->class);
}

EAPI void
ecore_wl2_window_geometry_get(Ecore_Wl2_Window *window, int *x, int *y, int *w, int *h)
{
   EINA_SAFETY_ON_NULL_RETURN(window);

   if (x) *x = window->geometry.x;
   if (y) *y = window->geometry.y;
   if (w) *w = window->geometry.w;
   if (h) *h = window->geometry.h;
}

EAPI void
ecore_wl2_window_geometry_set(Ecore_Wl2_Window *window, int x, int y, int w, int h)
{
   EINA_SAFETY_ON_NULL_RETURN(window);

   if ((window->geometry.x == x) && (window->geometry.y == y) &&
       (window->geometry.w == w) && (window->geometry.h == h))
     return;

   window->geometry.x = x;
   window->geometry.y = y;
   window->geometry.w = w;
   window->geometry.h = h;

   if (window->xdg_surface)
     xdg_surface_set_window_geometry(window->xdg_surface, x, y, w, h);
}

EAPI Eina_Bool
ecore_wl2_window_iconified_get(Ecore_Wl2_Window *window)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EINA_FALSE);

   return window->minimized;
}

EAPI void
ecore_wl2_window_iconified_set(Ecore_Wl2_Window *window, Eina_Bool iconified)
{
   Eina_Bool prev;

   EINA_SAFETY_ON_NULL_RETURN(window);

   prev = window->minimized;
   iconified = !!iconified;
   if (prev == iconified) return;

   window->minimized = iconified;

   if (iconified)
     {
        if (window->xdg_surface)
          xdg_surface_set_minimized(window->xdg_surface);
     }
   else
     {
        if (window->xdg_surface)
          {
             struct wl_array states;
             uint32_t *s;

             wl_array_init(&states);
             s = wl_array_add(&states, sizeof(*s));
             *s = XDG_SURFACE_STATE_ACTIVATED;
             _xdg_surface_cb_configure(window, window->xdg_surface,
                                       window->geometry.w, window->geometry.h,
                                       &states, 0);
             wl_array_release(&states);
          }
     }
}

EAPI void
ecore_wl2_window_pointer_xy_get(Ecore_Wl2_Window *window, int *x, int *y)
{
   Ecore_Wl2_Input *input;

   EINA_SAFETY_ON_NULL_RETURN(window);

   if (x) *x = 0;
   if (y) *y = 0;

   input = ecore_wl2_window_input_get(window);
   if (!input) return;

   if (x) *x = input->pointer.sx;
   if (y) *y = input->pointer.sy;
}

EAPI void
ecore_wl2_window_pointer_set(Ecore_Wl2_Window *window, struct wl_surface *surface, int hot_x, int hot_y)
{
   Ecore_Wl2_Input *input;

   EINA_SAFETY_ON_NULL_RETURN(window);

   input = ecore_wl2_window_input_get(window);
   if (!input) return;

   _ecore_wl2_input_cursor_update_stop(input);

   input->cursor.surface = surface;
   input->cursor.hot_x = hot_x;
   input->cursor.hot_y = hot_y;

   _ecore_wl2_input_cursor_update(input);
}

EAPI void
ecore_wl2_window_cursor_from_name_set(Ecore_Wl2_Window *window, const char *cursor)
{
   Ecore_Wl2_Input *input;

   EINA_SAFETY_ON_NULL_RETURN(window);

   input = ecore_wl2_window_input_get(window);
   if (!input) return;

   _ecore_wl2_input_cursor_set(input, cursor);
}

EAPI void
ecore_wl2_window_type_set(Ecore_Wl2_Window *window, Ecore_Wl2_Window_Type type)
{
   EINA_SAFETY_ON_NULL_RETURN(window);
   window->type = type;
}

EAPI Ecore_Wl2_Display *
ecore_wl2_window_display_get(const Ecore_Wl2_Window *window)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, NULL);
   return window->display;
}

EAPI Ecore_Wl2_Input *
ecore_wl2_window_input_get(Ecore_Wl2_Window *window)
{
   Ecore_Wl2_Input *input;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(window->display, NULL);

   if (window->input) return window->input;

   EINA_INLIST_FOREACH(window->display->inputs, input)
     {
        if (input->focus.pointer) return input;
     }

   return NULL;
}

EAPI Eina_Iterator *
ecore_wl2_display_inputs_get(Ecore_Wl2_Display *display)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(display, NULL);
   return eina_inlist_iterator_new(display->inputs);
}

EAPI Eina_Bool
ecore_wl2_window_has_shell_surface(Ecore_Wl2_Window *window)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EINA_FALSE);

   return (window->xdg_surface || window->wl_shell_surface);
}
