#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#include <Elementary.h>
#include "elm_priv.h"
#include "efl_ui_focus_object.eo.h"
#include "efl_ui_focus_manager.eo.h"


#define DIM_EFL_UI_FOCUS_DIRECTION(dim,neg) dim*2+neg
#define NODE_DIRECTIONS_COUNT 4
#define MY_CLASS EFL_UI_FOCUS_MANAGER_CLASS
#define FOCUS_DATA(obj) Efl_Ui_Focus_Manager_Data *pd = eo_data_scope_get(obj, MY_CLASS);


typedef struct {
    Eina_Bool positive;
    Efl_Ui_Focus_Object *anchor;
} Anchor;

typedef enum {
    DIMENSION_X = 0,
    DIMENSION_Y = 1,
} Dimension;

typedef struct _Node Node;
struct _Node {
    Efl_Ui_Focus_Object *focusable;
    Node *directions[NODE_DIRECTIONS_COUNT];
    Efl_Ui_Focus_Manager *manager;
};

typedef struct {
    Eina_List *focus_stack;
    Eina_Hash *node_hash;
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

/**
 * Create a new node
 */
static Node*
node_new(Efl_Ui_Focus_Object *focusable, Efl_Ui_Focus_Manager *manager)
{
    Node *node;

    node = calloc(1, sizeof(Node));

    node->focusable = focusable;
    node->manager = manager;

    return node;
}

static Node*
node_get(Efl_Ui_Focus_Manager_Data *pd, Efl_Ui_Focus_Object *focusable)
{
   return eina_hash_find(pd->node_hash, &focusable);
}

/**
 * Unlink a node from a special direction
 * If node is already linked in the given direction
 * the referenced node will also be unlinked
 * in the complement direction
 *--
 * @param item the node to unlink
 * @param direction the direction to unlink the item at
 */
static void
node_unlink(Node *item, Efl_Ui_Focus_Direction direction)
{
   if (!item->directions[direction]) return;

   //delete where item is linked
   item->directions[direction]->directions[_complement(direction)] = NULL;
   //delete our link to direction
   item->directions[direction] = NULL;
}

/**
 * link a in direction dir to b and b in the complement direction to a
 *
 * @param direction the direction to link the item to the new item
 * @param leaking a array of 2 fields where the old relations of a and b are saved
 *         the 0 index is the leak of a
 *         the 1 index is the leak of b
 *
 */
static void
node_relink(Node *a, Node *b, Efl_Ui_Focus_Direction dir, Node* leaking[2])
{
    Node *old_a = NULL, *old_b = NULL;

    if (leaking)
      {
         leaking[0] = NULL;
         leaking[1] = NULL;
      }

    if (a)
      old_a = a->directions[dir];
    if (b)
      old_b = b->directions[_complement(dir)];

    //null out the old nodes
    if (old_a)
      old_a->directions[_complement(dir)] = NULL;
    if (old_b)
      old_b->directions[dir] = NULL;

    if (leaking)
      {
         if (old_a && old_a != b)
             leaking[0] = old_a;
         if (old_b && old_b != a)
             leaking[1] = old_b;
      }

    if (a)
      a->directions[dir] = b;
    if (b)
      b->directions[_complement(dir)] = a;
}

/**
 * Place a child in the direction, next to the anchor node.
 *
 * The passed dimension and the positive flag of the anchor defines a direction.
 * The function will place the child in the direction next to the anchor node.
 * Linking is transphorms from
 *
 * if the relation partner is NULL or the new positive relation partner of child will also be NULL
 *
 * ------------------(dim)------------------------->
 *  |node_anchor|-(positive)->|relation_partner|
 *
 * to
 *
 * ------------------(dim)------------------------->
 *  |node_anchor|-(positive)->|child|-(positive)->|relation_partner|
 *
 * the relation is defined with the anchor positive flag and the dimension
 *
 * @param child the item to place
 * @param anchor the struct which defines the node and the direction to go for, the
 * @param dim the dimension to perform this operation on
 * @param leaking the leaked nodes from the child
 */
static void
node_relink_middle(Efl_Ui_Focus_Manager_Data *pd, Node *child, Anchor anchor, Dimension dim, Node* leaking[2])
{
    Node *node_anchor, *relation_partner;
    Node *leaks[2];
    Efl_Ui_Focus_Direction supported, complement;

    //we have a anchor which tells us that we are having the passed item in the positive or negative direction of the passed dimension
    supported = DIM_EFL_UI_FOCUS_DIRECTION(dim, !anchor.positive);
    complement = DIM_EFL_UI_FOCUS_DIRECTION(dim, anchor.positive);

    //we are fitting the new child between two existing nodes with a relation
    node_anchor = node_get(pd, anchor.anchor);
    relation_partner = node_anchor->directions[supported];

    if (relation_partner == child)
      {
         if (leaking)
           {
              leaks[0] = NULL;
              leaking[1] = NULL;
           }
         return;
      }

    //relink the child to the relation partner in the
    node_relink(child, relation_partner, supported, leaks);
    if (leaking) leaking[0] = leaks[0];
    node_relink(child, node_anchor, complement, leaks);
    if (leaking) leaking[1] = leaks[1];
}

/**
 * Free a node item and unlink this item from all direction
 */
static void
node_item_free(Node *item)
{
    for(int i = 0;i < NODE_DIRECTIONS_COUNT; i++){
        node_unlink(item, i);
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

static inline Eina_Bool
check_intersection(Efl_Ui_Focus_Manager_Data *pd)
{
   Eina_Iterator *one, *two;
   Efl_Ui_Focus_Object **elm1;
   Eina_Rectangle elm1_xy, elm2_xy;

   one = eina_hash_iterator_key_new(pd->node_hash);


   EINA_ITERATOR_FOREACH(one, elm1)
    {
       Efl_Ui_Focus_Object **elm2;

       evas_object_geometry_get(*elm1, &elm1_xy.x, &elm1_xy.y, &elm1_xy.w, &elm1_xy.h);
       two = eina_hash_iterator_key_new(pd->node_hash);

       EINA_ITERATOR_FOREACH(two, elm2)
         {
            if (*elm1 == *elm2) continue;

            evas_object_geometry_get(*elm2, &elm2_xy.x, &elm2_xy.y, &elm2_xy.w, &elm2_xy.h);

            if (eina_rectangle_intersection(&elm2_xy, &elm1_xy))
              {
                 ERR("Object %p and %p are intersecting! Focus experience will be poor\n", elm1, elm2);
              }
         }
    }
    return EINA_FALSE;
}

static inline void
_calculate_node(Anchor *anchor, Efl_Ui_Focus_Manager_Data *pd, Efl_Ui_Focus_Object *node, Dimension dim)
{
   Eina_Rectangle rect = EINA_RECTANGLE_INIT;
   Efl_Ui_Focus_Object *op;
   Efl_Ui_Focus_Object **focus_key;
   int distance = 0;
   int dim_min, dim_max;
   Eina_Iterator *nodes;


   nodes = eina_hash_iterator_key_new(pd->node_hash);
   evas_object_geometry_get(node, &rect.x, &rect.y, &rect.w, &rect.h);

   //null out the anchor
   anchor->anchor = NULL;
   anchor->positive = EINA_FALSE;

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
              (dim_min <= dim_max && dim_max <= min && min <= max)))
          {
             //this thing hits horizontal
             int tmp_dis;

             tmp_dis = _distance(rect, op_rect, dim);
#if 0
             printf("LOOKING AT IT DISTANCE %d \n", tmp_dis);
#endif
             if (anchor->anchor == NULL || abs(tmp_dis) < abs(distance)) //init case
               {
                  distance = tmp_dis;
                  anchor->positive = tmp_dis > 0 ? EINA_FALSE : EINA_TRUE;
                  anchor->anchor = op;
                  //Helper for debugging wrong calculations
                  #if 0
                  printf("CORRECTION FOR %p found anchor %s (%d,%d,%d,%d) (%d,%d,%d,%d)\n", node, eo_id_get(op),
                   op_rect.x, op_rect.y, op_rect.w, op_rect.h,
                   rect.x, rect.y, rect.w, rect.h);
                  #endif
               }
         }

     }
}


static inline void
relation_fix(Efl_Ui_Focus_Manager_Data *pd, Dimension dim, Node *node, Anchor anchor)
{
   if (anchor.anchor)
     {
        Node *leaks[2] = {NULL};

        node_relink_middle(pd, node, anchor, dim, leaks);
        /*
         * If only one node would have leaked,
         * the node would have only one other part linked.
         * so this was a node at the border before.
         */
        if (leaks[0] && leaks[1])
          node_relink(leaks[0], leaks[1], DIM_EFL_UI_FOCUS_DIRECTION(dim, !anchor.positive), NULL);
     }
   else
     {
        Node *pos = node->directions[DIM_EFL_UI_FOCUS_DIRECTION(dim, 1)];
        Node *neg = node->directions[DIM_EFL_UI_FOCUS_DIRECTION(dim, 0)];
        //we know the leaks here since node is leaked at both ends
        if (pos && neg)
          node_relink(pos, neg, DIM_EFL_UI_FOCUS_DIRECTION(dim, 0), NULL);
     }
}

static void
dirty_flush(Efl_Ui_Focus_Manager_Data *pd)
{
   Node *node;

   check_intersection(pd);

   EINA_LIST_FREE(pd->dirty, node)
     {
        Anchor horizontal, vertical;

        _calculate_node(&horizontal, pd, node->focusable, DIMENSION_X);
        relation_fix(pd, DIMENSION_X, node, horizontal);

        _calculate_node(&vertical, pd, node->focusable, DIMENSION_Y);
        relation_fix(pd, DIMENSION_Y, node, vertical);
     }
}
static void
dirty_add(Efl_Ui_Focus_Manager_Data *pd, Node *dirty)
{
   if (eina_list_data_find(pd->dirty, dirty)) return;

   pd->dirty = eina_list_append(pd->dirty, dirty);
}


static Eina_Bool
_node_new_geometery_cb(void *data, const Eo_Event *event)
{
   Node *node;
   FOCUS_DATA(data)

   node = node_get(pd, event->object);

   dirty_add(pd, node);

   return EO_CALLBACK_CONTINUE;
}

static Eina_Bool
_focus_in_cb(void *data, const Eo_Event *event)
{
   Node *node;
   Node *old_focus;
   FOCUS_DATA(data)

   node = node_get(pd, event->object);

   old_focus = eina_list_last_data_get(pd->focus_stack);

   if (old_focus && old_focus->focusable == event->object)
     {
        //dont do anything here, this means another node unfocused while at the top of stack
        return EO_CALLBACK_CONTINUE;
     }

   //remove the object from the list and add it again
   pd->focus_stack = eina_list_remove(pd->focus_stack, node);
   pd->focus_stack = eina_list_append(pd->focus_stack, node);

   //remove focus from the old
   if (old_focus)
     efl_ui_focus_object_focus_set(old_focus->focusable, EINA_FALSE);

   return EO_CALLBACK_CONTINUE;
}

static Eina_Bool
_focus_out_cb(void *data, const Eo_Event *event)
{
   Node *old_focus;
   FOCUS_DATA(data)

   old_focus = eina_list_last_data_get(pd->focus_stack);

   if (old_focus->focusable == event->object)
     {
        Node *new_focus;

        //the object with the current focus doesnt want anymore, remove it from the stack
        pd->focus_stack = eina_list_remove(pd->focus_stack, old_focus);
        //set focus on the new upper focus element
        new_focus = eina_list_last_data_get(pd->focus_stack);
        if (new_focus)
          efl_ui_focus_object_focus_set(new_focus->focusable, EINA_TRUE);
     }
   else
     {
        //its not the top element, it doesnt give up focus explicit, so keep it on the stack, for the case a upper focus element drops
     }

   return EO_CALLBACK_CONTINUE;
}

EO_CALLBACKS_ARRAY_DEFINE(focusable_node,
    {EVAS_OBJECT_EVENT_RESIZE, _node_new_geometery_cb},
    {EVAS_OBJECT_EVENT_MOVE, _node_new_geometery_cb},
    {EFL_UI_FOCUS_OBJECT_EVENT_FOCUS_IN, _focus_in_cb},
    {EFL_UI_FOCUS_OBJECT_EVENT_FOCUS_OUT, _focus_out_cb}
);

//=============================


EOLIAN static Eina_Bool
_efl_ui_focus_manager_register(Eo *obj, Efl_Ui_Focus_Manager_Data *pd, Evas_Object *child)
{
   Node *node;

   if (node_get(pd, child))
     {
        ERR("Child %p is already registered in the graph", child);
        return EINA_FALSE;
     }

   node = node_new(child, obj);
   eina_hash_add(pd->node_hash, &child, node);

   //mark dirty
   dirty_add(pd, node);

   //listen to events
   eo_event_callback_array_add(child, focusable_node(), obj);

   return EINA_TRUE;
}


EOLIAN static void
_efl_ui_focus_manager_unregister(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Data *pd, Evas_Object *child)
{
   Node *node;

   node = node_get(pd, child);

   if (!node) return;

   //remove the object from the stack if it hasnt dont that until now
   //after this its not at the top anymore
   efl_ui_focus_object_focus_set(node->focusable, EINA_FALSE);

   //delete again from the list, for the case it was not at the top
   pd->focus_stack = eina_list_remove(pd->focus_stack, node);

   //remove from the dirty parts
   pd->dirty = eina_list_remove(pd->dirty, node);

   eina_hash_del_by_key(pd->node_hash, &child);
}

EOLIAN static Efl_Ui_Focus_Object*
_efl_ui_focus_manager_move(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Data *pd, Efl_Ui_Focus_Direction direction)
{
   Node *upper, *dir;

   dirty_flush(pd);

   if (pd->redirect)
     return efl_ui_focus_manager_move(pd->redirect, direction);

   upper = eina_list_last_data_get(pd->focus_stack);
   if (!upper) return NULL;

   dir = upper->directions[direction];
   if (!dir) return NULL;

   efl_ui_focus_object_focus_set(dir->focusable, EINA_TRUE);

   return dir->focusable;
}

EOLIAN static void
_efl_ui_focus_manager_redirect_set(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Data *pd, Efl_Ui_Focus_Manager *redirect)
{
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
             if (!node->directions[i])
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

   dirty_flush(pd);

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



#include "efl_ui_focus_manager.eo.c"