#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define ELM_INTERFACE_ATSPI_ACCESSIBLE_PROTECTED
#define ELM_INTERNAL_API_ARGESFSDFEFC
#include <Elementary.h>
#include "elm_suite.h"
#include "elm_widget.h"

typedef struct {
    Eina_Rectangle rect;
    Eina_Bool focus;
} Focus_Test_Data;

#include "focus_test.eo.h"

static void
_focus_test_efl_gfx_position_set(Eo *obj, Focus_Test_Data *pd, int x, int y)
{
    pd->rect.x = x;
    pd->rect.y = y;
    eo_event_callback_call(obj, EVAS_OBJECT_EVENT_RESIZE, NULL);
}

static void
_focus_test_efl_gfx_position_get(Eo *obj, Focus_Test_Data *pd, int *x, int *y)
{
   if (x) *x = pd->rect.x;
   if (y) *y = pd->rect.y;
}

static void
_focus_test_efl_gfx_size_set(Eo *obj, Focus_Test_Data *pd, int w, int h)
{
    pd->rect.w = w;
    pd->rect.h = h;
    eo_event_callback_call(obj, EVAS_OBJECT_EVENT_RESIZE, NULL);
}

static void
_focus_test_efl_gfx_size_get(Eo *obj, Focus_Test_Data *pd, int *w, int *h)
{
    if (w) *w = pd->rect.w;
    if (h) *h = pd->rect.h;
}

static Eo_Base*
_focus_test_eo_base_constructor(Eo *obj, Focus_Test_Data *pd)
{
   Eo *eo;

   eo = eo_constructor(eo_super(obj, FOCUS_TEST_CLASS));
   eina_rectangle_coords_from(&pd->rect, 0, 0, 0, 0);
   return eo;
}

static void
_focus_test_efl_ui_focus_object_focus_set(Eo *obj, Focus_Test_Data *pd, Eina_Bool focus)
{
   pd->focus = focus;

   if (pd->focus)
     eo_event_callback_call(obj, EFL_UI_FOCUS_OBJECT_EVENT_FOCUSED, NULL);
   else
     eo_event_callback_call(obj, EFL_UI_FOCUS_OBJECT_EVENT_UNFOCUSED, NULL);
}

static Eina_Bool
_focus_test_efl_ui_focus_object_focus_get(Eo *obj, Focus_Test_Data *pd)
{
   return pd->focus;
}

static const char *
_focus_test_elm_widget_part_text_get(Eo *obj, Focus_Test_Data *pd, const char * part EINA_UNUSED)
{
   return eo_name_get(obj);
}

#include "focus_test.eo.c"

#define Q(o,x,y,w,h) \
   efl_gfx_position_set(o, x, y); \
   efl_gfx_size_set(o, w, h);

START_TEST(focus_unregister_twice)
{
   elm_init(1, NULL);
   Efl_Ui_Focus_Manager *m = eo_add(EFL_UI_FOCUS_MANAGER_CLASS, NULL);
   Efl_Ui_Focus_Object *r1 = eo_add(FOCUS_TEST_CLASS, m);
   Efl_Ui_Focus_Object *r2 = eo_add(FOCUS_TEST_CLASS, m);

   fail_if(!efl_ui_focus_manager_register(m, r1));
   fail_if(!efl_ui_focus_manager_register(m, r2));

   efl_ui_focus_manager_unregister(m, r1);
   efl_ui_focus_manager_unregister(m, r1);
   efl_ui_focus_manager_unregister(m, r1);

   eo_unref(r2);
   eo_unref(r1);
   eo_unref(m);

   elm_shutdown();
}
END_TEST

START_TEST(focus_register_twice)
{
   elm_init(1, NULL);

   Efl_Ui_Focus_Manager *m = eo_add(EFL_UI_FOCUS_MANAGER_CLASS, NULL);
   Efl_Ui_Focus_Object *r1 = eo_add(FOCUS_TEST_CLASS, m);

   fail_if(!efl_ui_focus_manager_register(m, r1));
   fail_if(efl_ui_focus_manager_register(m, r1));

   eo_unref(r1);
   eo_unref(m);

   elm_shutdown();
}
END_TEST

static void
_setup_cross(Efl_Ui_Focus_Object **middle, Efl_Ui_Focus_Object **south,
  Efl_Ui_Focus_Object **north, Efl_Ui_Focus_Object **east, Efl_Ui_Focus_Object **west,
  Efl_Ui_Focus_Manager *m)
  {


   *middle = eo_add(FOCUS_TEST_CLASS, m);
   eo_name_set(*middle, "middle");
   Q(*middle, 40, 40, 20, 20)

   *south = eo_add(FOCUS_TEST_CLASS, m);
   eo_name_set(*south, "south");
   Q(*south, 40, 80, 20, 20)

   *north = eo_add(FOCUS_TEST_CLASS, m);
   eo_name_set(*north, "north");
   Q(*north, 40, 0, 20, 20)

   *east = eo_add(FOCUS_TEST_CLASS, m);
   eo_name_set(*east, "east");
   Q(*east, 80, 40, 20, 20)

   *west = eo_add(FOCUS_TEST_CLASS, m);
   eo_name_set(*west, "west");
   Q(*west, 0, 40, 20, 20)

   efl_ui_focus_manager_register(m, *middle);
   efl_ui_focus_manager_register(m, *south);
   efl_ui_focus_manager_register(m, *north);
   efl_ui_focus_manager_register(m, *east);
   efl_ui_focus_manager_register(m, *west);
  }

START_TEST(pos_check)
{
   Efl_Ui_Focus_Manager *m;
   Efl_Ui_Focus_Object *middle, *east, *west, *north, *south;

   elm_init(1, NULL);

   m = eo_add(EFL_UI_FOCUS_MANAGER_CLASS, NULL);

   _setup_cross(&middle, &south, &north, &east, &west, m);

#define CHECK(obj, r,l,u,d) \
   efl_ui_focus_object_focus_set(obj, EINA_TRUE); \
   ck_assert_ptr_eq(efl_ui_focus_manager_move(m, EFL_UI_FOCUS_DIRECTION_RIGHT), r); \
   efl_ui_focus_object_focus_set(obj, EINA_TRUE); \
   ck_assert_ptr_eq(efl_ui_focus_manager_move(m, EFL_UI_FOCUS_DIRECTION_LEFT), l); \
   efl_ui_focus_object_focus_set(obj, EINA_TRUE); \
   ck_assert_ptr_eq(efl_ui_focus_manager_move(m, EFL_UI_FOCUS_DIRECTION_UP), u); \
   efl_ui_focus_object_focus_set(obj, EINA_TRUE); \
   ck_assert_ptr_eq(efl_ui_focus_manager_move(m, EFL_UI_FOCUS_DIRECTION_DOWN), d); \
   efl_ui_focus_object_focus_set(obj, EINA_TRUE);

   printf("middle check\n");
   CHECK(middle, east, west, north, south)
   printf("east check\n");
   CHECK(east, NULL, middle, NULL, NULL)
   printf("west check\n");
   CHECK(west, middle, NULL, NULL, NULL)
   printf("north check\n");
   CHECK(north, NULL, NULL, NULL, middle)
   printf("south check\n");
   CHECK(south, NULL, NULL, middle, NULL)

   eo_unref(middle);
   eo_unref(south);
   eo_unref(north);
   eo_unref(east);
   eo_unref(west);

   elm_shutdown();
}
END_TEST

START_TEST(redirect)
{
   elm_init(1, NULL);

   Efl_Ui_Focus_Manager *m = eo_add(EFL_UI_FOCUS_MANAGER_CLASS, NULL);
   Efl_Ui_Focus_Manager *m2 = eo_add(EFL_UI_FOCUS_MANAGER_CLASS, NULL);

   printf("ONE\n");
   Efl_Ui_Focus_Object *one = eo_add(FOCUS_TEST_CLASS, m2);
   eo_name_set(one, "one");
   Q(one, 0, 0, 20, 20)
   efl_ui_focus_manager_register(m2, one);

   printf("TWO\n");
   Efl_Ui_Focus_Object *two = eo_add(FOCUS_TEST_CLASS, m2);
   eo_name_set(two, "two");
   Q(two, 20, 0, 20, 20)
   efl_ui_focus_manager_register(m2, two);

   efl_ui_focus_object_focus_set(one, EINA_TRUE);
   efl_ui_focus_manager_redirect_set(m, m2);

   ck_assert_ptr_eq(efl_ui_focus_manager_move(m, EFL_UI_FOCUS_DIRECTION_RIGHT), two);

   elm_shutdown();
}
END_TEST

START_TEST(border_check)
{
   Efl_Ui_Focus_Manager *m;
   Efl_Ui_Focus_Object *middle, *east, *west, *north, *south;
   Eina_List *list = NULL;
   Eina_Iterator *iter;
   Efl_Ui_Focus_Object *obj;

   elm_init(1, NULL);

   m = eo_add(EFL_UI_FOCUS_MANAGER_CLASS, NULL);

   _setup_cross(&middle, &south, &north, &east, &west, m);

   iter = efl_ui_focus_manager_border_elements_get(m);

   EINA_ITERATOR_FOREACH(iter, obj)
     {
        list = eina_list_append(list, obj);
     }

   ck_assert(eina_list_data_find(list, east) == east);
   ck_assert(eina_list_data_find(list, north) == north);
   ck_assert(eina_list_data_find(list, west) == west);
   ck_assert(eina_list_data_find(list, east) == east);
   ck_assert(eina_list_data_find(list, middle) == NULL);
   ck_assert(eina_list_count(list) == 4);

   elm_shutdown();
}
END_TEST

void elm_test_focus(TCase *tc)
{
    tcase_add_test(tc, focus_register_twice);
    tcase_add_test(tc, focus_unregister_twice);
    tcase_add_test(tc, pos_check);
    tcase_add_test(tc, redirect);
    tcase_add_test(tc, border_check);
}
