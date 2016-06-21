#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define ELM_INTERFACE_ATSPI_ACCESSIBLE_PROTECTED
#define ELM_INTERFACE_ATSPI_COMPONENT_PROTECTED
#define ELM_WIDGET_PROTECTED
#define ELM_WIDGET_ITEM_PROTECTED
#include <Elementary.h>

#include "elm_priv.h"
#include "elm_widget_container.h"

#define MY_SELF_REGISTERER EFL_UI_WIDGET_FOCUS_MIXIN
#define MY_USER EFL_UI_WIDGET_FOCUS_USER_MIXIN

#define REREGISTER_DATA_GET(o,d) Efl_Ui_Widget_Focus_Data *d = eo_data_scope_get(o, MY_SELF_REGISTERER);
#define USER_DATA_GET(o,d) Efl_Ui_Widget_Focus_User_Data *d = eo_data_scope_get(o, MY_USER);

typedef struct {
  Efl_Ui_Focus_Manager *manager;
} Efl_Ui_Widget_Focus_User_Data;

typedef struct {
  Eina_Bool registered;
  Efl_Ui_Focus_Manager *old_manager;
} Efl_Ui_Widget_Focus_Data;

static void
_parent_eval(Eo *obj)
{
   REREGISTER_DATA_GET(obj, pd);


   if (pd->old_manager != efl_ui_focus_object_manager_get(obj))
     {
        if (pd->registered && pd->old_manager)
          efl_ui_focus_manager_unregister(pd->old_manager, obj);

        pd->old_manager = efl_ui_focus_object_manager_get(obj);

        if (pd->registered && pd->old_manager)
          {
             efl_ui_focus_manager_register(pd->old_manager, obj);
          }
     }

}


static void
_reeval(Eo *obj, Efl_Ui_Widget_Focus_Data *pd)
{
   if (elm_widget_can_focus_get(obj) && !elm_widget_disabled_get(obj))
     {
        if (!pd->registered)
          {
             efl_ui_focus_manager_register(pd->old_manager, obj);
             pd->registered = EINA_TRUE;
          }
     }
   else
     {
        if (pd->registered)
          {
             efl_ui_focus_manager_unregister(pd->old_manager, obj);
             pd->registered = EINA_FALSE;
          }

     }
}


static void
_parent_changed(void *data EINA_UNUSED, const Eo_Event *event)
{
   _parent_eval(event->object);
}


EOLIAN static Eo_Base *
_efl_ui_widget_focus_eo_base_constructor(Eo *obj, Efl_Ui_Widget_Focus_Data *pd)
{
   Eo *ret;
   eo_event_callback_add(obj, ELM_WIDGET_EVENT_PARENT_CHANGED, _parent_changed, NULL);

   ret = eo_constructor(eo_super(obj, MY_SELF_REGISTERER));

   _parent_eval(obj);

   return ret;
}

EOLIAN static void
_efl_ui_widget_focus_elm_widget_can_focus_set(Eo *obj, Efl_Ui_Widget_Focus_Data *pd, Eina_Bool can_focus)
{
   elm_obj_widget_can_focus_set(eo_super(obj, MY_SELF_REGISTERER), can_focus);
   _reeval(obj,pd);
}

EOLIAN static void
_efl_ui_widget_focus_elm_widget_disabled_set(Eo *obj, Efl_Ui_Widget_Focus_Data *pd, Eina_Bool disabled)
{
   elm_obj_widget_disabled_set(eo_super(obj, MY_SELF_REGISTERER), disabled);
   _reeval(obj,pd);
}

/* Implementation of focus_user */


static Eo*
_find_focus_manager(Eo *obj)
{
   Eo *cantidate;

   cantidate  = obj;

   while (elm_widget_parent_get(cantidate))
     {
        cantidate = elm_widget_parent_get(cantidate);
        if (eo_isa(cantidate, EFL_UI_FOCUS_MANAGER_CLASS))
          {
             return cantidate;
          }
     }
   return NULL;
}

static void
_parent_changed_focus_user(void *data EINA_UNUSED, const Eo_Event *info)
{
   USER_DATA_GET(info->object, pd);
   Efl_Ui_Focus_Manager *manager;

   manager = _find_focus_manager(info->object);

   if (manager == pd->manager) return;
   if (!manager)
     {
        ERR("Failed to find a manager");
     }

   //search the next higher focus manager
   pd->manager = manager;
}

EOLIAN static Efl_Ui_Focus_Manager *
_efl_ui_widget_focus_user_efl_ui_focus_object_manager_get(Eo *obj EINA_UNUSED, Efl_Ui_Widget_Focus_User_Data *pd)
{
   return pd->manager;
}

EOLIAN static Eo_Base *
_efl_ui_widget_focus_user_eo_base_constructor(Eo *obj, Efl_Ui_Widget_Focus_User_Data *pd EINA_UNUSED)
{
   eo_event_callback_add(obj, ELM_WIDGET_EVENT_PARENT_CHANGED, _parent_changed_focus_user, NULL);
   return eo_constructor(eo_super(obj, EFL_UI_WIDGET_FOCUS_USER_MIXIN));
}

#include "efl_ui_focus_object.eo.c"
#include "efl_ui_widget_focus.eo.c"
#include "efl_ui_widget_focus_user.eo.c"