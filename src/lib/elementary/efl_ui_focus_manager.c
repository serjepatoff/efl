#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#include <Elementary.h>
#include "elm_priv.h"

#define DIM_EFL_UI_FOCUS_DIRECTION(dim,neg) dim*2+neg
#define NODE_DIRECTIONS_COUNT 4
#define MY_CLASS EFL_UI_FOCUS_MANAGER_CLASS
#define FOCUS_DATA(obj) Efl_Ui_Focus_Manager_Data *pd = eo_data_scope_get(obj, MY_CLASS);


typedef struct {
    Eina_Bool positive;
    Elm_Widget *anchor;
} Anchor;

typedef enum {
    DIMENSION_X = 0,
    DIMENSION_Y = 1,
} Dimension;

typedef struct _Border Border;
struct _Border {
  Eina_List *partners;
};

typedef struct _Node Node;
struct _Node {
    Elm_Widget *focusable;
    Border directions[NODE_DIRECTIONS_COUNT];
    Efl_Ui_Focus_Manager *manager;
};

typedef struct {
    Eina_List *focus_stack;
    Eina_Hash *node_hash;
    Eina_Hash *listener_hash;
    Efl_Ui_Focus_Manager *redirect;

    Eina_List *dirty;
} Efl_Ui_Focus_Manager_Data;

static Efl_Ui_Focus_Direction
_complement(Efl_Ui_Focus_Direction dir)
{
    #define COMP(a,b) \
        if (dir == a) return b; \
        if (dir == b) return a;

    COMP(EFL_UI_FOCUS_DIRECTION_RIGHT, EFL_UI_FOCUS_DIRECTION_LEFT)
    COMP(EFL_UI_FOCUS_DIRECTION_UP, EFL_UI_FOCUS_DIRECTION_DOWN)
    COMP(EFL_UI_FOCUS_DIRECTION_PREV, EFL_UI_FOCUS_DIRECTION_NEXT)

    #undef COMP

    return EFL_UI_FOCUS_DIRECTION_LAST;
}

/*
 * Set this new list of partners to the border.
 * All old partners will be deleted
 */
static void
border_partners_set(Node *node, Efl_Ui_Focus_Direction direction, Eina_List *list)
{
   Node *partner;
   Eina_List *lnode;
   Border *border = &node->directions[direction];

   EINA_LIST_FREE(border->partners, partner)
     {
        Border *comp_border = &partner->directions[_complement(direction)];

        comp_border->partners = eina_list_remove(comp_border->partners, node);
     }

   border->partners = list;

   EINA_LIST_FOREACH(border->partners, lnode, partner)
     {
        Border *comp_border = &partner->directions[_complement(direction)];

        comp_border->partners = eina_list_append(comp_border->partners, node);
     }
}

/**
 * Create a new node
 */
static Node*
node_new(Elm_Widget *focusable, Efl_Ui_Focus_Manager *manager)
{
    Node *node;

    node = calloc(1, sizeof(Node));

    node->focusable = focusable;
    node->manager = manager;

    return node;
}

static Node*
node_get(Efl_Ui_Focus_Manager_Data *pd, Elm_Widget *focusable)
{
   return eina_hash_find(pd->node_hash, &focusable);
}

/**
 * Free a node item and unlink this item from all direction
 */
static void
node_item_free(Node *item)
{
    for(int i = 0;i < NODE_DIRECTIONS_COUNT; i++){
        border_partners_set(item, i, NULL);
    }

    free(item);
}


//CALCULATING STUFF

static inline int
_distance(Eina_Rectangle node, Eina_Rectangle op, Dimension dim)
{
    int min, max, point;
    int v1, v2;

    if (dim == DIMENSION_X)
      {
         min = op.x;
         max = eina_rectangle_max_x(&op);
         point = node.x + node.w/2;
      }
    else
      {
         min = op.y;
         max = eina_rectangle_max_y(&op);
         point = node.y + node.h/2;
      }

    v1 = min - point;
    v2 = max - point;

    if (abs(v1) < abs(v2))
      return v1;
    else
      return v2;
}

static inline void
_calculate_node(Efl_Ui_Focus_Manager_Data *pd, Elm_Widget *node, Dimension dim, Eina_List **pos, Eina_List **neg)
{
   Eina_Rectangle rect = EINA_RECTANGLE_INIT;
   Elm_Widget *op;
   Elm_Widget **focus_key;
   int dim_min, dim_max;
   Eina_Iterator *nodes;
   int cur_pos_min = 0, cur_neg_min = 0;

   nodes = eina_hash_iterator_key_new(pd->node_hash);
   evas_object_geometry_get(node, &rect.x, &rect.y, &rect.w, &rect.h);

   *pos = NULL;
   *neg = NULL;

   if (dim == DIMENSION_X)
     {
        dim_min = rect.y;
        dim_max = rect.y + rect.h;
     }
   else
     {
        dim_min = rect.x;
        dim_max = rect.x + rect.w;
     }

   EINA_ITERATOR_FOREACH(nodes, focus_key)
     {
        Eina_Rectangle op_rect = EINA_RECTANGLE_INIT;
        int min, max;

        op = *focus_key;
        if (op == node) continue;

        evas_object_geometry_get(op, &op_rect.x, &op_rect.y, &op_rect.w, &op_rect.h);

        if (dim == DIMENSION_X)
          {
             min = op_rect.y;
             max = eina_rectangle_max_y(&op_rect);
          }
        else
          {
             min = op_rect.x;
             max = eina_rectangle_max_x(&op_rect);
          }


        /* two only way the calculation does make sense is if the two number
         * lines are not disconnected.
         * If they are connected one point of the 4 lies between the min and max of the other line
         */
        if (!((min <= max && max <= dim_min && dim_min <= dim_max) ||
              (dim_min <= dim_max && dim_max <= min && min <= max)) &&
              !eina_rectangle_intersection(&op_rect, &rect))
          {
             //this thing hits horizontal
             int tmp_dis;

             tmp_dis = _distance(rect, op_rect, dim);

             if (tmp_dis < 0)
               {
                  if (tmp_dis == cur_neg_min)
                    {
                       //add it
                       *neg = eina_list_append(*neg, op);
                    }
                  else if (tmp_dis > cur_neg_min
                    || cur_neg_min == 0) //init case
                    {
                       //nuke the old and add
#ifdef DEBUG
                       printf("CORRECTION FOR %s\n found anchor %s in distance %d\n (%d,%d,%d,%d)\n (%d,%d,%d,%d)\n\n", elm_widget_part_text_get(node, NULL), elm_widget_part_text_get(op, NULL),
                         tmp_dis,
                         op_rect.x, op_rect.y, op_rect.w, op_rect.h,
                         rect.x, rect.y, rect.w, rect.h);
#endif
                       *neg = eina_list_free(*neg);
                       *neg = eina_list_append(NULL, op);
                       cur_neg_min = tmp_dis;
                    }
               }
             else
               {
                  if (tmp_dis == cur_pos_min)
                    {
                       //add it
                       *pos = eina_list_append(*pos, op);
                    }
                  else if (tmp_dis < cur_pos_min
                    || cur_pos_min == 0) //init case
                    {
                       //nuke the old and add
#ifdef DEBUG
                       printf("CORRECTION FOR %s\n found anchor %s in distance %d\n (%d,%d,%d,%d)\n (%d,%d,%d,%d)\n\n", elm_widget_part_text_get(node, NULL), elm_widget_part_text_get(op, NULL),
                         tmp_dis,
                         op_rect.x, op_rect.y, op_rect.w, op_rect.h,
                         rect.x, rect.y, rect.w, rect.h);
#endif
                       *pos = eina_list_free(*pos);
                       *pos = eina_list_append(NULL, op);
                       cur_pos_min = tmp_dis;
                    }
               }


#if 0
             printf("(%d,%d,%d,%d)%s vs(%d,%d,%d,%d)%s\n", rect.x, rect.y, rect.w, rect.h, elm_widget_part_text_get(node, NULL), op_rect.x, op_rect.y, op_rect.w, op_rect.h, elm_widget_part_text_get(op, NULL));
             printf("(%d,%d,%d,%d)\n", min, max, dim_min, dim_max);
             printf("Candidate %d\n", tmp_dis);
             if (anchor->anchor == NULL || abs(tmp_dis) < abs(distance)) //init case
               {
                  distance = tmp_dis;
                  anchor->positive = tmp_dis > 0 ? EINA_FALSE : EINA_TRUE;
                  anchor->anchor = op;
                  //Helper for debugging wrong calculations

               }
#endif
         }

     }
}

#ifdef DEBUG
static void
_debug_node(Node *node)
{
   Eina_List *tmp = NULL;

   if (!node) return;

   printf("NODE %s\n", elm_widget_part_text_get(node->focusable, NULL));

#define DIR_LIST(dir) node->directions[dir].partners

#define DIR_OUT(dir)\
   tmp = DIR_LIST(dir); \
   { \
      Eina_List *list_node; \
      Node *partner; \
      printf("-"#dir"-> ("); \
      EINA_LIST_FOREACH(tmp, list_node, partner) \
        printf("%s,", elm_widget_part_text_get(partner->focusable, NULL)); \
      printf(")\n"); \
   }

   DIR_OUT(EFL_UI_FOCUS_DIRECTION_RIGHT)
   DIR_OUT(EFL_UI_FOCUS_DIRECTION_LEFT)
   DIR_OUT(EFL_UI_FOCUS_DIRECTION_UP)
   DIR_OUT(EFL_UI_FOCUS_DIRECTION_DOWN)

}
#endif

static void
convert_border_set(Efl_Ui_Focus_Manager_Data *pd, Node *node, Eina_List *focusable_list, Efl_Ui_Focus_Direction dir)
{
   Eina_List *partners = NULL;
   Elm_Widget *obj;

   EINA_LIST_FREE(focusable_list, obj)
     {
        Node *entry;

        entry = node_get(pd, obj);
        if (!entry)
          {
             CRI("Found a obj in graph without node-entry!");
             return;
          }
        partners = eina_list_append(partners, entry);
     }

   border_partners_set(node, dir, partners);
}

static void
dirty_flush(Efl_Ui_Focus_Manager *obj, Efl_Ui_Focus_Manager_Data *pd)
{
   Node *node;

   eo_event_callback_call(obj, EFL_UI_FOCUS_MANAGER_EVENT_PRE_FLUSH, NULL);

   EINA_LIST_FREE(pd->dirty, node)
     {
        Eina_List *x_partners_pos, *x_partners_neg;
        Eina_List *y_partners_pos, *y_partners_neg;

        _calculate_node(pd, node->focusable, DIMENSION_X, &x_partners_pos, &x_partners_neg);
        _calculate_node(pd, node->focusable, DIMENSION_Y, &y_partners_pos, &y_partners_neg);

        convert_border_set(pd, node, x_partners_pos, EFL_UI_FOCUS_DIRECTION_RIGHT);
        convert_border_set(pd, node, x_partners_neg, EFL_UI_FOCUS_DIRECTION_LEFT);
        convert_border_set(pd, node, y_partners_neg, EFL_UI_FOCUS_DIRECTION_UP);
        convert_border_set(pd, node, y_partners_pos, EFL_UI_FOCUS_DIRECTION_DOWN);

#ifdef DEBUG
        _debug_node(node);
#endif
     }
}
static void
dirty_add(Efl_Ui_Focus_Manager_Data *pd, Node *dirty)
{
   //if (eina_list_data_find(pd->dirty, dirty)) return;
   pd->dirty = eina_list_remove(pd->dirty, dirty);
   pd->dirty = eina_list_append(pd->dirty, dirty);
}


static void
_node_new_geometery_cb(void *data, const Eo_Event *event)
{
   Node *node;
   FOCUS_DATA(data)

   node = node_get(pd, event->object);

   dirty_add(pd, node);

   return;
}

static void
_focus_in_cb(void *data, const Eo_Event *event)
{
   Node *node;
   Node *old_focus;
   FOCUS_DATA(data)

   node = node_get(pd, event->object);

   if (!node) return;

   old_focus = eina_list_last_data_get(pd->focus_stack);

   if (old_focus && old_focus->focusable == event->object)
     {
        //dont do anything here, this means another node unfocused while at the top of stack
        return;
     }

   //remove the object from the list and add it again
   pd->focus_stack = eina_list_remove(pd->focus_stack, node);
   pd->focus_stack = eina_list_append(pd->focus_stack, node);
}

static void
_child_del(void *data, const Eo_Event *event)
{
   WRN("The manager itself catched a deletion of a child. BAD");
   efl_ui_focus_manager_unregister(data, event->object);
}

EO_CALLBACKS_ARRAY_DEFINE(focusable_node,
    {EO_EVENT_DEL, _child_del},
    {EFL_GFX_EVENT_RESIZE, _node_new_geometery_cb},
    {EFL_GFX_EVENT_MOVE, _node_new_geometery_cb},
    {ELM_WIDGET_EVENT_FOCUSED, _focus_in_cb}
);

//=============================

static Node*
_register(Eo *obj, Efl_Ui_Focus_Manager_Data *pd, Eo *child)
{
   Node *node;
   if (node_get(pd, child))
     {
        ERR("Child %p is already registered in the graph", child);
        return NULL;
     }

   node = node_new(child, obj);
   eina_hash_add(pd->node_hash, &child, node);


   return node;
}


EOLIAN static Eina_Bool
_efl_ui_focus_manager_register(Eo *obj, Efl_Ui_Focus_Manager_Data *pd, Evas_Object *child)
{
   Node *node = _register(obj, pd, child);

   if (!node) return EINA_FALSE;

   //mark dirty
   dirty_add(pd, node);

   //listen to events
   eo_event_callback_array_add(child, focusable_node(), obj);

   return EINA_TRUE;
}

static void
_listener_focus(void *data, const Eo_Event *info)
{
   Efl_Ui_Focus_Manager *manager;
   FOCUS_DATA(data);

   manager = eina_hash_find(pd->listener_hash, &info->object);

   efl_ui_focus_manager_redirect_set(data, manager);
}

EOLIAN static Eina_Bool
_efl_ui_focus_manager_listener(Eo *obj, Efl_Ui_Focus_Manager_Data *pd, Eo_Base *child, Efl_Ui_Focus_Manager *redirect)
{
   Node *node = _register(obj, pd, child);
   Efl_Ui_Focus_Manager *manager;

   if (!node) return EINA_FALSE;

   //mark dirty
   dirty_add(pd, node);

   manager = eina_hash_find(pd->listener_hash, &child);
   eina_hash_add(pd->listener_hash, &child, redirect);

   if (!manager)
     eo_event_callback_add(child, ELM_WIDGET_EVENT_FOCUSED, _listener_focus, obj);

   return EINA_TRUE;
}


EOLIAN static void
_efl_ui_focus_manager_unregister(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Data *pd, Evas_Object *child)
{
   Node *node;
   Efl_Ui_Focus_Manager *manager;

   node = node_get(pd, child);

   if (!node) return;

   //remove the object from the stack if it hasnt dont that until now
   //after this its not at the top anymore
   //elm_widget_focus_set(node->focusable, EINA_FALSE);
   //delete again from the list, for the case it was not at the top
   pd->focus_stack = eina_list_remove(pd->focus_stack, node);

   //add all neighboors of the node to the dirty list
   for(int i = 0; i < 4; i++)
     {
        Node *partner;
        Eina_List *n;

        EINA_LIST_FOREACH(node->directions[i].partners, n, partner)
          {
             dirty_add(pd, partner);
          }
     }

   //remove from the dirty parts
   pd->dirty = eina_list_remove(pd->dirty, node);

   manager = eina_hash_find(pd->listener_hash, &child);

   if (manager)
     {
        //this is a listener element
        eo_event_callback_add(child, ELM_WIDGET_EVENT_FOCUSED, _listener_focus, obj);

        //remove from known listeners
        eina_hash_del_by_key(pd->listener_hash, &child);
     }

   eina_hash_del_by_key(pd->node_hash, &child);
}

EOLIAN static Elm_Widget*
_efl_ui_focus_manager_move(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Data *pd, Efl_Ui_Focus_Direction direction)
{
   Node *dir, *upper;
   Elm_Widget *candidate;

   candidate = efl_ui_focus_manager_request_move(obj, direction);
   dir = node_get(pd, candidate);

   if (!dir) return NULL;

   _elm_widget_focus_auto_show(dir->focusable);
   _elm_widget_focus_highlight_start(dir->focusable);

   //unfocus the old one for now ...
   upper = eina_list_last_data_get(pd->focus_stack);
   if (upper)
     elm_obj_widget_focused_object_clear(upper->focusable);

   elm_widget_focus_set(dir->focusable, EINA_TRUE);

   //sometimes widgets are swallowing the events or not calling them, workarround that by setting this on the stack
   pd->focus_stack = eina_list_remove(pd->focus_stack, dir);
   pd->focus_stack = eina_list_append(pd->focus_stack, dir);

#ifdef DEBUG
   printf("Focus, MOVE %s %s\n", elm_widget_part_text_get(dir->focusable, NULL), elm_widget_type_get(dir->focusable));
#endif
   return dir->focusable;
}

EOLIAN static void
_efl_ui_focus_manager_redirect_set(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Data *pd, Efl_Ui_Focus_Manager *redirect)
{
   if (pd->redirect == redirect) return;

   if (pd->redirect)
     eo_unref(pd->redirect);

   pd->redirect = redirect;

   if (pd->redirect)
     eo_ref(pd->redirect);
}

EOLIAN static Efl_Ui_Focus_Manager *
_efl_ui_focus_manager_redirect_get(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Data *pd)
{
   return pd->redirect;
}

static void
_free_node(void *data)
{
   Node *node = data;
   eo_event_callback_array_del(node->focusable, focusable_node(), node->manager);

   node_item_free(node);
}

EOLIAN static Eo_Base *
_efl_ui_focus_manager_eo_base_constructor(Eo *obj, Efl_Ui_Focus_Manager_Data *pd)
{
   pd->node_hash = eina_hash_pointer_new(_free_node);
   pd->listener_hash = eina_hash_pointer_new(NULL);
   return eo_constructor(eo_super(obj, MY_CLASS));
}

EOLIAN static Eo_Base *
_efl_ui_focus_manager_eo_base_provider_find(Eo *obj, Efl_Ui_Focus_Manager_Data *pd EINA_UNUSED, const Eo_Base *klass)
{
   if (klass == MY_CLASS)
     return obj;

   return eo_provider_find(eo_super(obj, MY_CLASS), klass);
}

EOLIAN static void
_efl_ui_focus_manager_eo_base_destructor(Eo *obj, Efl_Ui_Focus_Manager_Data *pd)
{
   eina_list_free(pd->focus_stack);
   eina_list_free(pd->dirty);

   eina_hash_free(pd->node_hash);

   eo_destructor(eo_super(obj, MY_CLASS));
}

typedef struct {
   Eina_Iterator iterator;
   Eina_Iterator *real_iterator;
   Efl_Ui_Focus_Manager *object;
} Border_Elements_Iterator;

static Eina_Bool
_iterator_next(Border_Elements_Iterator *it, void **data)
{
   Node *node;

   while(eina_iterator_next(it->real_iterator, (void**)&node))
     {
        for(int i = 0 ;i < NODE_DIRECTIONS_COUNT; i++)
          {
             if (!node->directions[i].partners)
               {
                  *data = node->focusable;
                  return EINA_TRUE;
               }
          }
     }
   return EINA_FALSE;
}

static Elm_Layout *
_iterator_get_container(Border_Elements_Iterator *it)
{
   return it->object;
}

static void
_iterator_free(Border_Elements_Iterator *it)
{
   eina_iterator_free(it->real_iterator);
   free(it);
}

EOLIAN static Eina_Iterator*
_efl_ui_focus_manager_border_elements_get(Eo *obj, Efl_Ui_Focus_Manager_Data *pd)
{
   Border_Elements_Iterator *it;

   dirty_flush(obj, pd);

   it = calloc(1, sizeof(Border_Elements_Iterator));

   EINA_MAGIC_SET(&it->iterator, EINA_MAGIC_ITERATOR);

   it->real_iterator = eina_hash_iterator_data_new(pd->node_hash);
   it->iterator.version = EINA_ITERATOR_VERSION;
   it->iterator.next = FUNC_ITERATOR_NEXT(_iterator_next);
   it->iterator.get_container = FUNC_ITERATOR_GET_CONTAINER(_iterator_get_container);
   it->iterator.free = FUNC_ITERATOR_FREE(_iterator_free);
   it->object = obj;

   return (Eina_Iterator*) it;
}

EOLIAN static Eina_Iterator*
_efl_ui_focus_manager_request(Eo *obj, Efl_Ui_Focus_Manager_Data *pd, Eo_Base *child, Efl_Ui_Focus_Direction direction)
{
   Node *node, *partners;
   Eina_List *raw = NULL, *n;
   if (!child)
     node = eina_list_last_data_get(pd->focus_stack);
   else
     node = node_get(pd, child);

   if (!node) return NULL;

   EINA_LIST_FOREACH(node->directions[direction].partners, n, partners)
     raw = eina_list_append(raw, partners->focusable);


   return eina_list_iterator_new(raw);
}

EOLIAN static Elm_Widget*
_efl_ui_focus_manager_request_move(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Data *pd, Efl_Ui_Focus_Direction direction)
{
   Node *upper = NULL, *candidate, *dir = NULL;
   Eina_List *node;

   dirty_flush(obj, pd);

   if (pd->redirect)
     return efl_ui_focus_manager_move(pd->redirect, direction);

   upper = eina_list_last_data_get(pd->focus_stack);

   if (!upper)
     {
        //select a item from the graph
        Eina_Iterator *iter;

        iter = eina_hash_iterator_data_new(pd->node_hash);

        if (!eina_iterator_next(iter, (void**)&upper))
          return NULL;

        eina_iterator_free(iter);
     }

#ifdef DEBUG
   _debug_node(upper);
#endif

   //we are searcing which of the partners is lower to the history
   EINA_LIST_REVERSE_FOREACH(pd->focus_stack, node, candidate)
     {
        if (eina_list_data_find(upper->directions[direction].partners, candidate))
          {
             //this is the next accessable part
             dir = candidate;
             break;
          }
     }

   if (!dir)
     {
        dir = eina_list_data_get(upper->directions[direction].partners);
        if (!dir) return NULL;
     }

   return dir->focusable;
}


#include "efl_ui_focus_manager.eo.c"