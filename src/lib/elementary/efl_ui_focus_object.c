#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#include <Elementary.h>
#include "elm_priv.h"

typedef struct {
     Eina_Bool focus;
     Eina_Bool focusable;
     Eo *manager;
} Efl_Ui_Focus_Object_Data;

static Eina_Bool
_efl_ui_focus_object_focus_get(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Object_Data *pd)
{
   return pd->focus;
}

static void
_efl_ui_focus_object_focus_set(Eo *obj, Efl_Ui_Focus_Object_Data *pd, Eina_Bool focus)
{
   //do not set states twice
   if (pd->focus == focus) return;

   if (pd->focusable)
     {
        pd->focus = focus;
        if (pd->focus)
          eo_event_callback_call(obj, EFL_UI_FOCUS_OBJECT_EVENT_FOCUS_IN, NULL);
        else
          eo_event_callback_call(obj, EFL_UI_FOCUS_OBJECT_EVENT_FOCUS_OUT, NULL);
     }
}

static Eina_Bool
_efl_ui_focus_object_focusable_get(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Object_Data *pd)
{
   return pd->focusable;
}

static Efl_Ui_Focus_Manager*
_search_focus_manager(Eo *obj)
{
   Eo *search = obj;

   while (search)
     {
        if (eo_isa(search, EFL_UI_FOCUS_MANAGER_CLASS))
          {
             return search;
          }
        search = eo_parent_get(search);
     }
   return NULL;
}

static void
_efl_ui_focus_object_focusable_set(Eo *obj, Efl_Ui_Focus_Object_Data *pd, Eina_Bool focusable)
{
   if (pd->focusable == focusable) return;

   if (!focusable)
     {
        if (pd->focus)
          efl_ui_focus_object_focus_set(obj, EINA_FALSE);

        efl_ui_focus_manager_unregister(pd->manager, obj);
        pd->manager = NULL;
        pd->focusable = EINA_FALSE;
     }
   else
     {
        pd->manager = eo_provider_find(obj, EFL_UI_FOCUS_MANAGER_CLASS);
        if (!pd->manager)
          {
             ERR("Unable to find focus manager!");
             return;
          }
        efl_ui_focus_manager_register(pd->manager, obj);
        pd->focusable = EINA_TRUE;
     }
}


#include "efl_ui_focus_object.eo.c"