#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ecore_wl2_private.h"

static void
_pointer_cb_enter(void *data, struct wl_pointer *pointer EINA_UNUSED, unsigned int serial, struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy)
{
   Ecore_Wl2_Input *input;
   Ecore_Wl2_Window *window;

   input = data;
   if (!input) return;

   /* trap for a surface that was just destroyed */
   if (!surface) return;

   input->pointer.enter_serial = serial;
   input->pointer.sx = wl_fixed_to_double(sx);
   input->pointer.sy = wl_fixed_to_double(sy);

   /* find the window which this surface belongs to */
   window = _ecore_wl2_display_window_surface_find(input->display, surface);
   if (!window) return;

   input->focus.pointer = window;

   /* TODO: send mouse in event */
}

static void
_pointer_cb_leave(void *data, struct wl_pointer *pointer EINA_UNUSED, unsigned int serial, struct wl_surface *surface)
{
   Ecore_Wl2_Input *input;
   Ecore_Wl2_Window *window;

   input = data;
   if (!input) return;

   input->focus.pointer = NULL;

   /* trap for a surface that was just destroyed */
   if (!surface) return;

   /* find the window which this surface belongs to */
   window = _ecore_wl2_display_window_surface_find(input->display, surface);
   if (!window) return;

   /* TODO: send mouse out event */
}

static void
_pointer_cb_motion(void *data, struct wl_pointer *pointer EINA_UNUSED, unsigned int timestamp, wl_fixed_t sx, wl_fixed_t sy)
{
   Ecore_Wl2_Input *input;
   Ecore_Wl2_Window *window;

   input = data;
   if (!input) return;

   /* get currently focused window */
   window = input->focus.pointer;
   if (!window) return;

   input->pointer.sx = wl_fixed_to_double(sx);
   input->pointer.sy = wl_fixed_to_double(sy);

   /* NB: Unsure if we need this just yet, so commented out for now */
   /* if ((input->pointer.sx > window->geometry.w) || */
   /*     (input->pointer.sy > window->geometry.h)) */
   /*   return; */

   /* TODO: send mouse move event */
}

static void
_pointer_cb_button(void *data, struct wl_pointer *pointer EINA_UNUSED, unsigned int serial, unsigned int timestamp, unsigned int button, unsigned int state)
{
   Ecore_Wl2_Input *input;

   input = data;
   if (!input) return;

   if (state == WL_POINTER_BUTTON_STATE_PRESSED)
     {
        if ((input->focus.pointer) && (!input->grab.window))
          {
             /* TODO: issue input grab */
             input->grab.timestamp = timestamp;
          }

        if (input->focus.pointer)
          {
             /* TODO: send mouse down event */
          }
     }
   else
     {
        if (input->focus.pointer)
          {
             /* TODO: send mouse up event */
          }

        if ((input->grab.window) && (input->grab.button == button))
          {
             /* TODO: issue input ungrab */
          }
     }
}

static void
_pointer_cb_axis(void *data, struct wl_pointer *pointer EINA_UNUSED, unsigned int timestamp, unsigned int axis, wl_fixed_t value)
{
   Ecore_Wl2_Input *input;

   input = data;
   if (!input) return;

   /* TODO: send mouse wheel event */
}

static const struct wl_pointer_listener _pointer_listener =
{
   _pointer_cb_enter,
   _pointer_cb_leave,
   _pointer_cb_motion,
   _pointer_cb_button,
   _pointer_cb_axis,
};

static const struct wl_keyboard_listener _keyboard_listener =
{
   NULL, // keyboard keymap
   NULL, // keyboard enter
   NULL, // keyboard leave
   NULL, // keyboard key
   NULL, // keyboard modifiers
   NULL, // keyboard repeat setup
};

static const struct wl_touch_listener _touch_listener =
{
   NULL, // touch down
   NULL, // touch up
   NULL, // touch motion
   NULL, // touch frame
   NULL, // touch cancel
};

static const struct wl_data_device_listener _data_listener =
{
   NULL, // data offer
   NULL, // data enter
   NULL, // data leave
   NULL, // data motion
   NULL, // data drop
   NULL, // data selection
};

static void
_seat_cb_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
   Ecore_Wl2_Input *input;

   input = data;
   if (!input) return;

   if ((caps & WL_SEAT_CAPABILITY_POINTER) && (!input->wl.pointer))
     {
        input->wl.pointer = wl_seat_get_pointer(seat);
        wl_pointer_set_user_data(input->wl.pointer, input);
        wl_pointer_add_listener(input->wl.pointer, &_pointer_listener, input);

        if (!input->cursor.surface)
          {
             input->cursor.surface =
               wl_compositor_create_surface(input->display->wl.compositor);
          }
     }
   else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && (input->wl.pointer))
     {
        if (input->cursor.surface) wl_surface_destroy(input->cursor.surface);
        input->cursor.surface = NULL;

        wl_pointer_destroy(input->wl.pointer);
        input->wl.pointer = NULL;
     }
   else if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && (!input->wl.keyboard))
     {
        input->wl.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_set_user_data(input->wl.keyboard, input);
        wl_keyboard_add_listener(input->wl.keyboard, &_keyboard_listener, input);
     }
   else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && (input->wl.keyboard))
     {
        wl_keyboard_destroy(input->wl.keyboard);
        input->wl.keyboard = NULL;
     }
   else if ((caps & WL_SEAT_CAPABILITY_TOUCH) && (!input->wl.touch))
     {
        input->wl.touch = wl_seat_get_touch(seat);
        wl_touch_set_user_data(input->wl.touch, input);
        wl_touch_add_listener(input->wl.touch, &_touch_listener, input);
     }
   else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && (input->wl.touch))
     {
        wl_touch_destroy(input->wl.touch);
        input->wl.touch = NULL;
     }
}

static const struct wl_seat_listener _seat_listener =
{
   _seat_cb_capabilities,
   NULL
};

static void
_ecore_wl2_input_cursor_setup(Ecore_Wl2_Input *input)
{
   char *tmp;

   input->cursor.size = 32;
   tmp = getenv("ECORE_WL_CURSOR_SIZE");
   if (tmp) input->cursor.size = atoi(tmp);

   tmp = getenv("ECORE_WL_CURSOR_THEME_NAME");
   eina_stringshare_replace(&input->cursor.theme_name, tmp);

   if (!input->cursor.name)
     input->cursor.name = eina_stringshare_add("left_ptr");

   if (input->display->wl.shm)
     {
        input->cursor.theme =
          wl_cursor_theme_load(input->cursor.theme_name, input->cursor.size,
                               input->display->wl.shm);
     }
}

void
_ecore_wl2_input_add(Ecore_Wl2_Display *display, unsigned int id)
{
   Ecore_Wl2_Input *input;

   input = calloc(1, sizeof(Ecore_Wl2_Input));
   if (!input) return;

   input->display = display;

   input->repeat.rate = 0.025;
   input->repeat.delay = 0.4;
   input->repeat.enabled = EINA_TRUE;

   wl_array_init(&input->data.types);

   /* setup cursor size and theme */
   _ecore_wl2_input_cursor_setup(input);

   input->wl.seat =
     wl_registry_bind(wl_display_get_registry(display->wl.display),
                      id, &wl_seat_interface, 1);

   display->inputs =
     eina_inlist_append(display->inputs, EINA_INLIST_GET(input));

   wl_seat_add_listener(input->wl.seat, &_seat_listener, input);
   wl_seat_set_user_data(input->wl.seat, input);

   if (!display->wl.data_device_manager) return;

   input->data.device =
     wl_data_device_manager_get_data_device(display->wl.data_device_manager,
                                            input->wl.seat);
   wl_data_device_add_listener(input->data.device, &_data_listener, input);
}

void
_ecore_wl2_input_del(Ecore_Wl2_Input *input)
{
   Ecore_Wl2_Display *display;

   if (!input) return;

   display = input->display;

   if (input->repeat.timer) ecore_timer_del(input->repeat.timer);

   if (input->cursor.theme) wl_cursor_theme_destroy(input->cursor.theme);
   if (input->cursor.surface) wl_surface_destroy(input->cursor.surface);
   if (input->cursor.name) eina_stringshare_del(input->cursor.name);
   if (input->cursor.theme_name)
     eina_stringshare_del(input->cursor.theme_name);

   if (input->data.types.data)
     {
        char **t;

        wl_array_for_each(t, &input->data.types)
          free(*t);

        wl_array_release(&input->data.types);
     }

   if (input->data.source) wl_data_source_destroy(input->data.source);
   /* TODO: cleanup dnd */
   /* TODO: cleanup selection */
   if (input->data.device) wl_data_device_destroy(input->data.device);

   if (input->xkb.state) xkb_state_unref(input->xkb.state);
   if (input->xkb.keymap) xkb_map_unref(input->xkb.keymap);

   if (input->wl.seat) wl_seat_destroy(input->wl.seat);

   display->inputs =
     eina_inlist_remove(display->inputs, EINA_INLIST_GET(input));

   free(input);
}