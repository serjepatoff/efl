#define EFL_CANVAS_OBJECT_BETA
#define EVAS_CANVAS_BETA

#include "evas_common_private.h"
#include "evas_private.h"

#define EFL_INTERNAL_UNSTABLE
#include "interfaces/efl_common_internal.h"

EVAS_MEMPOOL(_mp_pc);

extern Eina_Hash* signals_hash_table;

/* Legacy events, do not use anywhere */
static const Efl_Event_Description _EVAS_OBJECT_EVENT_FREE = EFL_EVENT_DESCRIPTION("free");
static const Efl_Event_Description _EVAS_OBJECT_EVENT_DEL = EFL_EVENT_DESCRIPTION("del");
#define EVAS_OBJECT_EVENT_FREE (&(_EVAS_OBJECT_EVENT_FREE))
#define EVAS_OBJECT_EVENT_DEL (&(_EVAS_OBJECT_EVENT_DEL))

/**
 * Evas events descriptions for Eo.
 */
#define DEFINE_EVAS_CALLBACKS(FUNC, LAST, ...)                          \
  static const Efl_Event_Description *FUNC(unsigned int index)          \
  {                                                                     \
     static const Efl_Event_Description *internals[LAST] = { NULL };    \
                                                                        \
     if (index >= LAST) return NULL;                                    \
     if (internals[0] == NULL)                                          \
       {                                                                \
          memcpy(internals,                                             \
                 ((const Efl_Event_Description*[]) { __VA_ARGS__ }),    \
                 sizeof ((const Efl_Event_Description *[]) { __VA_ARGS__ })); \
       }                                                                \
     return internals[index];                                           \
  }

DEFINE_EVAS_CALLBACKS(_legacy_evas_callback_table, EVAS_CALLBACK_LAST,
                      EFL_EVENT_POINTER_IN,
                      EFL_EVENT_POINTER_OUT,
                      EFL_EVENT_POINTER_DOWN,
                      EFL_EVENT_POINTER_UP,
                      EFL_EVENT_POINTER_MOVE,
                      EFL_EVENT_POINTER_WHEEL,
                      EFL_EVENT_FINGER_DOWN,
                      EFL_EVENT_FINGER_UP,
                      EFL_EVENT_FINGER_MOVE,
                      EVAS_OBJECT_EVENT_FREE,
                      EFL_EVENT_KEY_DOWN,
                      EFL_EVENT_KEY_UP,
                      EFL_CANVAS_OBJECT_EVENT_FOCUS_IN,
                      EFL_CANVAS_OBJECT_EVENT_FOCUS_OUT,
                      EFL_GFX_EVENT_SHOW,
                      EFL_GFX_EVENT_HIDE,
                      EFL_GFX_EVENT_MOVE,
                      EFL_GFX_EVENT_RESIZE,
                      EFL_GFX_EVENT_RESTACK,
                      EVAS_OBJECT_EVENT_DEL,
                      EFL_EVENT_HOLD,
                      EFL_GFX_EVENT_CHANGE_SIZE_HINTS,
                      EFL_IMAGE_EVENT_PRELOAD,
                      EFL_CANVAS_EVENT_FOCUS_IN,
                      EFL_CANVAS_EVENT_FOCUS_OUT,
                      EVAS_CANVAS_EVENT_RENDER_FLUSH_PRE,
                      EVAS_CANVAS_EVENT_RENDER_FLUSH_POST,
                      EFL_CANVAS_EVENT_OBJECT_FOCUS_IN,
                      EFL_CANVAS_EVENT_OBJECT_FOCUS_OUT,
                      EFL_IMAGE_EVENT_UNLOAD,
                      EFL_CANVAS_EVENT_RENDER_PRE,
                      EFL_CANVAS_EVENT_RENDER_POST,
                      EFL_IMAGE_EVENT_RESIZE,
                      EFL_CANVAS_EVENT_DEVICE_CHANGED,
                      EFL_EVENT_POINTER_AXIS,
                      EVAS_CANVAS_EVENT_VIEWPORT_RESIZE );

static inline Evas_Callback_Type
_legacy_evas_callback_type(const Efl_Event_Description *desc)
{
   Evas_Callback_Type type;

   for (type = 0; type < EVAS_CALLBACK_LAST; type++)
     {
        if (_legacy_evas_callback_table(type) == desc)
          return type;
     }

   return EVAS_CALLBACK_LAST;
}

typedef enum {
   EFL_EVENT_TYPE_NULL,
   EFL_EVENT_TYPE_OBJECT,
   EFL_EVENT_TYPE_STRUCT,
   EFL_EVENT_TYPE_POINTER,
   EFL_EVENT_TYPE_KEY,
   EFL_EVENT_TYPE_HOLD
} Efl_Event_Info_Type;

typedef struct
{
   EINA_INLIST;
   Evas_Object_Event_Cb func;
   void *data;
   Evas_Callback_Type type;
   Efl_Event_Info_Type efl_event_type;
} _eo_evas_object_cb_info;

typedef struct
{
   EINA_INLIST;
   Evas_Event_Cb func;
   void *data;
   Evas_Callback_Type type;
} _eo_evas_cb_info;

static int
_evas_event_efl_event_info_type(Evas_Callback_Type type)
{
   switch (type)
     {
      case EVAS_CALLBACK_MOUSE_IN:
      case EVAS_CALLBACK_MOUSE_OUT:
      case EVAS_CALLBACK_MOUSE_DOWN:
      case EVAS_CALLBACK_MOUSE_UP:
      case EVAS_CALLBACK_MOUSE_MOVE:
      case EVAS_CALLBACK_MOUSE_WHEEL:
      case EVAS_CALLBACK_MULTI_DOWN:
      case EVAS_CALLBACK_MULTI_UP:
      case EVAS_CALLBACK_MULTI_MOVE:
      case EVAS_CALLBACK_AXIS_UPDATE:
        return EFL_EVENT_TYPE_POINTER;

      case EVAS_CALLBACK_KEY_DOWN:
      case EVAS_CALLBACK_KEY_UP:
        return EFL_EVENT_TYPE_KEY;

      case EVAS_CALLBACK_HOLD:
        return EFL_EVENT_TYPE_HOLD;

      case EVAS_CALLBACK_CANVAS_OBJECT_FOCUS_IN:
      case EVAS_CALLBACK_CANVAS_OBJECT_FOCUS_OUT: /* Efl.Canvas.Object */
        return EFL_EVENT_TYPE_OBJECT;

      case EVAS_CALLBACK_RENDER_POST: /* Efl_Gfx_Event_Render_Post */
        return EFL_EVENT_TYPE_STRUCT;

      case EVAS_CALLBACK_DEVICE_CHANGED: /* Efl.Input.Device */
        return EFL_EVENT_TYPE_OBJECT;

      default:
        return EFL_EVENT_TYPE_NULL;
     }
}

static void
_eo_evas_object_cb(void *data, const Efl_Event *event)
{
   Evas_Event_Flags *event_flags = NULL, evflags = EVAS_EVENT_FLAG_NONE;
   Efl_Input_Event *efl_event_info = event->info;
   _eo_evas_object_cb_info *info = data;
   void *event_info;
   Evas *evas;

   if (!info->func) return;
   evas = evas_object_evas_get(event->object);

   switch (info->efl_event_type)
     {
      case EFL_EVENT_TYPE_POINTER:
        event_info = efl_input_pointer_legacy_info_fill(evas, efl_event_info, info->type, &event_flags);
        break;

      case EFL_EVENT_TYPE_KEY:
        event_info = efl_input_key_legacy_info_fill(efl_event_info, &event_flags);
        break;

      case EFL_EVENT_TYPE_HOLD:
        event_info = efl_input_hold_legacy_info_fill(efl_event_info, &event_flags);
        break;

      case EFL_EVENT_TYPE_NULL:
      case EFL_EVENT_TYPE_STRUCT:
      case EFL_EVENT_TYPE_OBJECT:
        info->func(info->data, evas, event->object, event->info);
        return;

      default: return;
     }

   if (!event_info) return;
   if (event_flags) evflags = *event_flags;
   info->func(info->data, evas, event->object, event_info);
   if (event_flags && (evflags != *event_flags))
     efl_input_event_flags_set(efl_event_info, *event_flags);
}

static void
_eo_evas_cb(void *data, const Efl_Event *event)
{
   _eo_evas_cb_info *info = data;
   if (info->func) info->func(info->data, event->object, event->info);
}

void
_evas_post_event_callback_call(Evas *eo_e, Evas_Public_Data *e)
{
   Evas_Post_Callback *pc;
   int skip = 0;
   static int first_run = 1; // FIXME: This is a workaround to prevent this
                             // function from being called recursively.

   if (e->delete_me || (!first_run)) return;
   _evas_walk(e);
   first_run = 0;
   EINA_LIST_FREE(e->post_events, pc)
     {
        if ((!skip) && (!e->delete_me) && (!pc->delete_me))
          {
             if (!pc->func((void*)pc->data, eo_e)) skip = 1;
          }
        EVAS_MEMPOOL_FREE(_mp_pc, pc);
     }
   first_run = 1;
   _evas_unwalk(e);
}

void
_evas_post_event_callback_free(Evas *eo_e)
{
   Evas_Public_Data *e = efl_data_scope_get(eo_e, EVAS_CANVAS_CLASS);
   Evas_Post_Callback *pc;

   EINA_LIST_FREE(e->post_events, pc)
     {
        EVAS_MEMPOOL_FREE(_mp_pc, pc);
     }
}

void
evas_object_event_callback_all_del(Evas_Object *eo_obj)
{
   _eo_evas_object_cb_info *info;
   Eina_Inlist *itr;
   Evas_Object_Protected_Data *obj = efl_data_scope_get(eo_obj, EFL_CANVAS_OBJECT_CLASS);

   if (!obj) return;
   if (!obj->callbacks) return;
   EINA_INLIST_FOREACH_SAFE(obj->callbacks, itr, info)
     {
        efl_event_callback_del(eo_obj, _legacy_evas_callback_table(info->type), _eo_evas_object_cb, info);

        obj->callbacks =
           eina_inlist_remove(obj->callbacks, EINA_INLIST_GET(info));
        free(info);
     }
}

void
evas_object_event_callback_cleanup(Evas_Object *eo_obj)
{
   evas_object_event_callback_all_del(eo_obj);
}

void
evas_event_callback_all_del(Evas *eo_e)
{
   _eo_evas_object_cb_info *info;
   Eina_Inlist *itr;
   Evas_Public_Data *e = efl_data_scope_get(eo_e, EVAS_CANVAS_CLASS);

   if (!e) return;
   if (!e->callbacks) return;

   EINA_INLIST_FOREACH_SAFE(e->callbacks, itr, info)
     {
        efl_event_callback_del(eo_e, _legacy_evas_callback_table(info->type), _eo_evas_cb, info);

        e->callbacks =
           eina_inlist_remove(e->callbacks, EINA_INLIST_GET(info));
        free(info);
     }
}

void
evas_event_callback_cleanup(Evas *eo_e)
{
   evas_event_callback_all_del(eo_e);
}

void
evas_event_callback_call(Evas *eo_e, Evas_Callback_Type type, void *event_info)
{
   efl_event_callback_legacy_call(eo_e, _legacy_evas_callback_table(type), event_info);
}

void
evas_object_event_callback_call(Evas_Object *eo_obj, Evas_Object_Protected_Data *obj,
                                Evas_Callback_Type type, void *event_info, int event_id,
                                const Efl_Event_Description *efl_event_desc)
{
   /* MEM OK */
   const Evas_Button_Flags CLICK_MASK = EVAS_BUTTON_DOUBLE_CLICK | EVAS_BUTTON_TRIPLE_CLICK;
   Evas_Button_Flags flags = EVAS_BUTTON_NONE;
   Evas_Public_Data *e;

   if (!obj) return;
   if ((obj->delete_me) || (!obj->layer)) return;
   if ((obj->last_event == event_id) &&
       (obj->last_event_type == type)) return;
   if (obj->last_event > event_id)
     {
        if ((obj->last_event_type == EVAS_CALLBACK_MOUSE_OUT) &&
            ((type >= EVAS_CALLBACK_MOUSE_DOWN) &&
             (type <= EVAS_CALLBACK_MULTI_MOVE)))
          {
             return;
          }
     }
   obj->last_event = event_id;
   obj->last_event_type = type;
   if (!(e = obj->layer->evas)) return;

   _evas_walk(e);

   if (!_evas_object_callback_has_by_type(obj, type))
     goto nothing_here;

   if ((type == EVAS_CALLBACK_MOUSE_DOWN) || (type == EVAS_CALLBACK_MOUSE_UP))
     {
        flags = efl_input_pointer_button_flags_get(event_info);
        if (flags & CLICK_MASK)
          {
             if (obj->last_mouse_down_counter < (e->last_mouse_down_counter - 1))
               efl_input_pointer_button_flags_set(event_info, flags & ~CLICK_MASK);
          }
        obj->last_mouse_down_counter = e->last_mouse_down_counter;
     }

   if (!efl_event_desc)
     {
        /* This can happen for DEL and FREE which are defined only in here */
        efl_event_desc = _legacy_evas_callback_table(type);
     }

   efl_event_callback_legacy_call(eo_obj, efl_event_desc, event_info);

   /* multi events with finger 0 - only for eo callbacks */
   if (type == EVAS_CALLBACK_MOUSE_DOWN)
     {
        if (_evas_object_callback_has_by_type(obj, EVAS_CALLBACK_MULTI_DOWN))
          efl_event_callback_call(eo_obj, EFL_EVENT_FINGER_DOWN, event_info);
        efl_input_pointer_button_flags_set(event_info, flags);
     }
   else if (type == EVAS_CALLBACK_MOUSE_UP)
     {
        if (_evas_object_callback_has_by_type(obj, EVAS_CALLBACK_MULTI_UP))
          efl_event_callback_call(eo_obj, EFL_EVENT_FINGER_UP, event_info);
        efl_input_pointer_button_flags_set(event_info, flags);
     }
   else if (type == EVAS_CALLBACK_MOUSE_MOVE)
     {
        if (_evas_object_callback_has_by_type(obj, EVAS_CALLBACK_MULTI_MOVE))
          efl_event_callback_call(eo_obj, EFL_EVENT_FINGER_MOVE, event_info);
     }

nothing_here:
   if (!obj->no_propagate)
     {
        if ((obj->smart.parent) && (type != EVAS_CALLBACK_FREE) &&
              (type <= EVAS_CALLBACK_KEY_UP))
          {
             Evas_Object_Protected_Data *smart_parent = efl_data_scope_get(obj->smart.parent, EFL_CANVAS_OBJECT_CLASS);
             evas_object_event_callback_call(obj->smart.parent, smart_parent, type, event_info, event_id, efl_event_desc);
          }
     }
   _evas_unwalk(e);
}

EAPI void
evas_object_event_callback_add(Evas_Object *eo_obj, Evas_Callback_Type type, Evas_Object_Event_Cb func, const void *data)
{
   evas_object_event_callback_priority_add(eo_obj, type,
                                           EVAS_CALLBACK_PRIORITY_DEFAULT, func, data);
}

EAPI void
evas_object_event_callback_priority_add(Evas_Object *eo_obj, Evas_Callback_Type type, Evas_Callback_Priority priority, Evas_Object_Event_Cb func, const void *data)
{
   if(!eo_obj) return;
   EINA_SAFETY_ON_FALSE_RETURN(efl_isa(eo_obj, EFL_CANVAS_OBJECT_CLASS));
   Evas_Object_Protected_Data *obj = efl_data_scope_get(eo_obj, EFL_CANVAS_OBJECT_CLASS);

   if (!obj) return;
   if (!func) return;

   _eo_evas_object_cb_info *cb_info = calloc(1, sizeof(*cb_info));
   cb_info->func = func;
   cb_info->data = (void *)data;
   cb_info->type = type;
   cb_info->efl_event_type = _evas_event_efl_event_info_type(type);

   const Efl_Event_Description *desc = _legacy_evas_callback_table(type);
   efl_event_callback_priority_add(eo_obj, desc, priority, _eo_evas_object_cb, cb_info);

   obj->callbacks =
      eina_inlist_append(obj->callbacks, EINA_INLIST_GET(cb_info));
}

EAPI void *
evas_object_event_callback_del(Evas_Object *eo_obj, Evas_Callback_Type type, Evas_Object_Event_Cb func)
{
   if(!eo_obj) return NULL;
   EINA_SAFETY_ON_FALSE_RETURN_VAL(efl_isa(eo_obj, EFL_CANVAS_OBJECT_CLASS), NULL);
   Evas_Object_Protected_Data *obj = efl_data_scope_get(eo_obj, EFL_CANVAS_OBJECT_CLASS);
   _eo_evas_object_cb_info *info;

   if (!obj) return NULL;
   if (!func) return NULL;

   if (!obj->callbacks) return NULL;

   EINA_INLIST_REVERSE_FOREACH(obj->callbacks, info)
     {
        if ((info->func == func) && (info->type == type))
          {
             void *tmp = info->data;
             efl_event_callback_del(eo_obj, _legacy_evas_callback_table(type), _eo_evas_object_cb, info);

             obj->callbacks =
                eina_inlist_remove(obj->callbacks, EINA_INLIST_GET(info));
             free(info);
             return tmp;
          }
     }
   return NULL;
}

EAPI void *
evas_object_event_callback_del_full(Evas_Object *eo_obj, Evas_Callback_Type type, Evas_Object_Event_Cb func, const void *data)
{
   if(!eo_obj) return NULL;
   EINA_SAFETY_ON_FALSE_RETURN_VAL(efl_isa(eo_obj, EFL_CANVAS_OBJECT_CLASS), NULL);
   Evas_Object_Protected_Data *obj = efl_data_scope_get(eo_obj, EFL_CANVAS_OBJECT_CLASS);
   _eo_evas_object_cb_info *info;

   if (!obj) return NULL;
   if (!func) return NULL;

   if (!obj->callbacks) return NULL;

   EINA_INLIST_FOREACH(obj->callbacks, info)
     {
        if ((info->func == func) && (info->type == type) && info->data == data)
          {
             void *tmp = info->data;
             efl_event_callback_del(eo_obj, _legacy_evas_callback_table(type), _eo_evas_object_cb, info);

             obj->callbacks =
                eina_inlist_remove(obj->callbacks, EINA_INLIST_GET(info));
             free(info);
             return tmp;
          }
     }
   return NULL;
}

EAPI void
evas_event_callback_add(Evas *eo_e, Evas_Callback_Type type, Evas_Event_Cb func, const void *data)
{
   evas_event_callback_priority_add(eo_e, type, EVAS_CALLBACK_PRIORITY_DEFAULT,
                                    func, data);
}

EAPI void
evas_event_callback_priority_add(Evas *eo_e, Evas_Callback_Type type, Evas_Callback_Priority priority, Evas_Event_Cb func, const void *data)
{
   if(!eo_e) return;
   EINA_SAFETY_ON_FALSE_RETURN(efl_isa(eo_e, EVAS_CANVAS_CLASS));
   Evas_Public_Data *e = efl_data_scope_get(eo_e, EVAS_CANVAS_CLASS);

   if (!func) return;

   _eo_evas_cb_info *cb_info = calloc(1, sizeof(*cb_info));
   cb_info->func = func;
   cb_info->data = (void *)data;
   cb_info->type = type;

   const Efl_Event_Description *desc = _legacy_evas_callback_table(type);
   efl_event_callback_priority_add(eo_e, desc, priority, _eo_evas_cb, cb_info);

   e->callbacks = eina_inlist_append(e->callbacks, EINA_INLIST_GET(cb_info));
}

EAPI void *
evas_event_callback_del(Evas *eo_e, Evas_Callback_Type type, Evas_Event_Cb func)
{
   if(!eo_e) return NULL;
   EINA_SAFETY_ON_FALSE_RETURN_VAL(efl_isa(eo_e, EVAS_CANVAS_CLASS), NULL);
   Evas_Public_Data *e = efl_data_scope_get(eo_e, EVAS_CANVAS_CLASS);
   _eo_evas_cb_info *info;

   if (!e) return NULL;
   if (!func) return NULL;

   if (!e->callbacks) return NULL;

   EINA_INLIST_REVERSE_FOREACH(e->callbacks, info)
     {
        if ((info->func == func) && (info->type == type))
          {
             void *tmp = info->data;
             efl_event_callback_del(eo_e, _legacy_evas_callback_table(type), _eo_evas_cb, info);

             e->callbacks =
                eina_inlist_remove(e->callbacks, EINA_INLIST_GET(info));
             free(info);
             return tmp;
          }
     }
   return NULL;
}

EAPI void *
evas_event_callback_del_full(Evas *eo_e, Evas_Callback_Type type, Evas_Event_Cb func, const void *data)
{
   if(!eo_e) return NULL;
   EINA_SAFETY_ON_FALSE_RETURN_VAL(efl_isa(eo_e, EVAS_CANVAS_CLASS), NULL);
   Evas_Public_Data *e = efl_data_scope_get(eo_e, EVAS_CANVAS_CLASS);
   _eo_evas_cb_info *info;

   if (!e) return NULL;
   if (!func) return NULL;

   if (!e->callbacks) return NULL;

   EINA_INLIST_FOREACH(e->callbacks, info)
     {
        if ((info->func == func) && (info->type == type) && (info->data == data))
          {
             void *tmp = info->data;
             efl_event_callback_del(eo_e, _legacy_evas_callback_table(type), _eo_evas_cb, info);

             e->callbacks =
                eina_inlist_remove(e->callbacks, EINA_INLIST_GET(info));
             free(info);
             return tmp;
          }
     }
   return NULL;
}

EAPI void
evas_post_event_callback_push(Evas *eo_e, Evas_Object_Event_Post_Cb func, const void *data)
{
   if(!eo_e) return;
   EINA_SAFETY_ON_FALSE_RETURN(efl_isa(eo_e, EVAS_CANVAS_CLASS));
   Evas_Public_Data *e = efl_data_scope_get(eo_e, EVAS_CANVAS_CLASS);
   Evas_Post_Callback *pc;

   if (!e) return;
   EVAS_MEMPOOL_INIT(_mp_pc, "evas_post_callback", Evas_Post_Callback, 64, );
   pc = EVAS_MEMPOOL_ALLOC(_mp_pc, Evas_Post_Callback);
   if (!pc) return;
   EVAS_MEMPOOL_PREP(_mp_pc, pc, Evas_Post_Callback);
   if (e->delete_me) return;

   pc->func = func;
   pc->data = data;
   e->post_events = eina_list_prepend(e->post_events, pc);
}

EAPI void
evas_post_event_callback_remove(Evas *eo_e, Evas_Object_Event_Post_Cb func)
{
   if(!eo_e) return;
   EINA_SAFETY_ON_FALSE_RETURN(efl_isa(eo_e, EVAS_CANVAS_CLASS));
   Evas_Public_Data *e = efl_data_scope_get(eo_e, EVAS_CANVAS_CLASS);
   Evas_Post_Callback *pc;
   Eina_List *l;

   if (!e) return;
   EINA_LIST_FOREACH(e->post_events, l, pc)
     {
        if (pc->func == func)
          {
             pc->delete_me = 1;
             return;
          }
     }
}

EAPI void
evas_post_event_callback_remove_full(Evas *eo_e, Evas_Object_Event_Post_Cb func, const void *data)
{
   if(!eo_e) return;
   EINA_SAFETY_ON_FALSE_RETURN(efl_isa(eo_e, EVAS_CANVAS_CLASS));
   Evas_Public_Data *e = efl_data_scope_get(eo_e, EVAS_CANVAS_CLASS);
   Evas_Post_Callback *pc;
   Eina_List *l;

   if (!e) return;
   EINA_LIST_FOREACH(e->post_events, l, pc)
     {
        if ((pc->func == func) && (pc->data == data))
          {
             pc->delete_me = 1;
             return;
          }
     }
}

static void
_animator_repeater(void *data, const Efl_Event *event)
{
   Evas_Object_Protected_Data *obj = data;

   efl_event_callback_legacy_call(obj->object, EFL_EVENT_ANIMATOR_TICK, event->info);
   DBG("Emitting animator tick on %p.", obj->object);
}

static void
_check_event_catcher_add(void *data, const Efl_Event *event)
{
   const Efl_Callback_Array_Item *array = event->info;
   Evas_Object_Protected_Data *obj = data;
   Evas_Callback_Type type = EVAS_CALLBACK_LAST;
   int i;

   for (i = 0; array[i].desc != NULL; i++)
     {
        if (array[i].desc == EFL_EVENT_ANIMATOR_TICK)
          {
             if (obj->animator_ref++ > 0) break;

             efl_event_callback_add(obj->layer->evas->evas, EFL_EVENT_ANIMATOR_TICK, _animator_repeater, obj);
             INF("Registering an animator tick on canvas %p for object %p.",
                 obj->layer->evas->evas, obj->object);
          }
        else if ((type = _legacy_evas_callback_type(array[i].desc)) != EVAS_CALLBACK_LAST)
          {
             obj->callback_mask |= (1 << type);
          }
     }
}

static void
_check_event_catcher_del(void *data, const Efl_Event *event)
{
   const Efl_Callback_Array_Item *array = event->info;
   Evas_Object_Protected_Data *obj = data;
   int i;

   for (i = 0; array[i].desc != NULL; i++)
     {
        if (array[i].desc == EFL_EVENT_ANIMATOR_TICK)
          {
             if ((--obj->animator_ref) > 0) break;

             efl_event_callback_del(obj->layer->evas->evas, EFL_EVENT_ANIMATOR_TICK, _animator_repeater, obj);
             INF("Unregistering an animator tick on canvas %p for object %p.",
                 obj->layer->evas->evas, obj->object);
          }
     }
}

EFL_CALLBACKS_ARRAY_DEFINE(event_catcher_watch,
                          { EFL_EVENT_CALLBACK_ADD, _check_event_catcher_add },
                          { EFL_EVENT_CALLBACK_DEL, _check_event_catcher_del });

void
evas_object_callback_init(Efl_Canvas_Object *eo_obj, Evas_Object_Protected_Data *obj)
{
   efl_event_callback_array_add(eo_obj, event_catcher_watch(), obj);
}
