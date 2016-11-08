#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "evas_common_private.h"
#include "evas_private.h"
#include "evas_vg_private.h"

static Evas_Cache_Evg* evg_cache = NULL;

// evg file load

struct ext_loader_s
{
   unsigned int length;
   const char *extension;
   const char *loader;
};

#define MATCHING(Ext, Module) { sizeof(Ext)-1, Ext, Module }

static const struct ext_loader_s loaders[] =
{ /* map extensions to loaders to use for good first-guess tries */
   MATCHING(".eet", "eet"),
   MATCHING(".edj", "eet"),
   MATCHING(".svg", "svg"),
   MATCHING(".svgz", "svg"),
   MATCHING(".svg.gz", "svg")
};

static Evas_Module *
_find_loader_module(const char *file)
{
   const char           *loader = NULL, *end;
   Evas_Module          *em = NULL;
   unsigned int          i;
   int                   len, len2;
   len = strlen(file);
   end = file + len;
   for (i = 0; i < (sizeof (loaders) / sizeof(struct ext_loader_s)); i++)
     {
        len2 = loaders[i].length;
        if (len2 > len) continue;
        if (!strcasecmp(end - len2, loaders[i].extension))
          {
             loader = loaders[i].loader;
             break;
          }
     }
   if (loader)
     em = evas_module_find_type(EVAS_MODULE_TYPE_VG_LOADER, loader);
   return em;
}

Evg_Data *
_evg_load_from_file(const char *file, const char *key)
{
   Evas_Module       *em;
   Evas_Vg_Load_Func *loader;
   int                error = EVAS_LOAD_ERROR_GENERIC;
   Evg_Data          *evg_data = NULL;

   em = _find_loader_module(file);
   if (em)
     {
        loader = em->functions;
        evg_data = loader->file_data(file, key, &error);
     }
   return evg_data;
}


// evg file save
struct ext_saver_s
{
   unsigned int length;
   const char *extension;
   const char *saver;
};

static const struct ext_saver_s savers[] =
{ /* map extensions to loaders to use for good first-guess tries */
   MATCHING(".eet", "eet"),
   MATCHING(".edj", "eet")
};

static Evas_Module *
_find_saver_module(const char *file)
{
   const char           *saver = NULL, *end;
   Evas_Module          *em = NULL;
   unsigned int          i;
   int                   len, len2;
   len = strlen(file);
   end = file + len;
   for (i = 0; i < (sizeof (savers) / sizeof(struct ext_saver_s)); i++)
     {
        len2 = savers[i].length;
        if (len2 > len) continue;
        if (!strcasecmp(end - len2, savers[i].extension))
          {
             saver = savers[i].saver;
             break;
          }
     }
   if (saver)
     em = evas_module_find_type(EVAS_MODULE_TYPE_VG_SAVER, saver);
   return em;
}

Eina_Bool
evas_evg_save_to_file(Evg_Data *evg_data, const char *file, const char *key, const char *flags)
{
   Evas_Module       *em;
   Evas_Vg_Save_Func *saver;
   int                error = EVAS_LOAD_ERROR_GENERIC;
   int                compress = 9;

   if (flags)
     {
        char *p, *pp;
        char *tflags;

        tflags = alloca(strlen(flags) + 1);
        strcpy(tflags, flags);
        p = tflags;
        while (p)
          {
             pp = strchr(p, ' ');
             if (pp) *pp = 0;
             sscanf(p, "compress=%i", &compress);
             if (pp) p = pp + 1;
             else break;
          }
     }

   em = _find_saver_module(file);
   if (em)
     {
        saver = em->functions;
        error = saver->vg_save(evg_data, file, key, compress);
     }

   if (error)
     return EINA_FALSE;

   return EINA_TRUE;
}



static void
_evas_cache_vg_data_free_cb(void *data)
{
   Evg_Data *val = data;

   efl_del(val->root);
   free(val);
}

static void
_evas_cache_svg_entry_free_cb(void *data)
{
   Evg_Entry *entry = data;

   eina_stringshare_del(entry->src_file);
   eina_stringshare_del(entry->dest_file);
   eina_stringshare_del(entry->src_key);
   eina_stringshare_del(entry->dest_key);
   free(entry->hash_key);
   efl_del(entry->root);
   free(entry);
}

void
evas_evg_cache_init(void)
{
   if (evg_cache)
     {
        evg_cache->ref++;
        return;
     }
   evg_cache =  calloc(1, sizeof(Evas_Cache_Evg));
   evg_cache->vg_hash = eina_hash_string_superfast_new(_evas_cache_vg_data_free_cb);
   evg_cache->active = eina_hash_string_superfast_new(_evas_cache_svg_entry_free_cb);
   evg_cache->ref++;
}

void
evas_evg_cache_shutdown(void)
{
   if (!evg_cache) return;
   evg_cache->ref--;
   if (evg_cache->ref) return;
   eina_hash_free(evg_cache->vg_hash);
   eina_hash_free(evg_cache->active);
   evg_cache = NULL;
}

Evg_Data *
evas_evg_cache_file_info(const char *file, const char *key)
{
   Evg_Data *vd;
   Eina_Strbuf *hash_key;

   hash_key = eina_strbuf_new();
   eina_strbuf_append_printf(hash_key, "%s/%s", file, key);
   vd = eina_hash_find(evg_cache->vg_hash, eina_strbuf_string_get(hash_key));
   if (!vd)
     {
        vd = _evg_load_from_file(file, key);
        eina_hash_add(evg_cache->vg_hash, eina_strbuf_string_get(hash_key), vd);
     }
   eina_strbuf_free(hash_key);
   return vd;
}

static void
_apply_transformation(Efl_VG *root, double w, double h, Evg_Data *vg_data)
{
   double sx, sy, scale;
   Eina_Matrix3 m;

   sx = w/vg_data->view_box.w;
   sy = h/vg_data->view_box.h;
   scale = sx < sy ? sx: sy;
   eina_matrix3_identity(&m);
   // allign hcenter and vcenter
   //@TODO take care of the preserveaspectratio attribute
   if (vg_data->preserve_aspect)
     {
        eina_matrix3_translate(&m, (w - vg_data->view_box.w * scale)/2.0, (h - vg_data->view_box.h * scale)/2.0);
        eina_matrix3_scale(&m, scale, scale);
        eina_matrix3_translate(&m, -vg_data->view_box.x, -vg_data->view_box.y);
     }
   else
     {
        eina_matrix3_scale(&m, sx, sy);
        eina_matrix3_translate(&m, -vg_data->view_box.x, -vg_data->view_box.y);
     }
   evas_vg_node_transformation_set(root, &m);
}

static Efl_VG *
_evas_vg_dup_vg_tree(Evg_Data *src, Evg_Data *dest, float pos, double w, double h)
{

   Efl_VG *root;
   Eina_Matrix3 m;

   if (!src) return NULL;
   if (w==0 || h ==0 ) return NULL;
   if (!dest)
     {
        root = evas_vg_container_add(NULL);
        evas_vg_node_dup(root, src->root);
        _apply_transformation(root, w, h, src);
     }
   else
     {
        root = evas_vg_container_add(NULL);
        evas_vg_node_dup(root, src->root);

        // for start vector
        _apply_transformation(src->root, w, h, src);

        // for end vector
        _apply_transformation(dest->root, w, h, dest);

        // do the interpolation
        if (!evas_vg_node_interpolate(root, src->root, dest->root, pos))
          {
             ;//ERR(" Can't interpolate check the svg file \n");
          }
        // performance hack
        // instead of duplicating the tree and applying the transformation
        // i just updated the transformation matrix and reset it back to null.
        // assumption is that the root vg will never have a transformation
        eina_matrix3_identity(&m);
        evas_vg_node_transformation_set(src->root, &m);
        evas_vg_node_transformation_set(dest->root, &m);
     }
   return root;
}

static void
_evas_cache_svg_vg_tree_update(Evg_Entry *entry)
{
   Evg_Data *src_vg = NULL, *dst_vg = NULL;
   if(!entry) return;

   if (!entry->src_file)
     {
        entry->root = NULL;
        return;
     } 

   src_vg = evas_evg_cache_file_info(entry->src_file, entry->src_key);
   if (entry->dest_file)
     dst_vg = evas_evg_cache_file_info(entry->dest_file, entry->dest_key);

   entry->root = _evas_vg_dup_vg_tree(src_vg, dst_vg, entry->key_frame, entry->w, entry->h);
   eina_stringshare_del(entry->src_file);
   eina_stringshare_del(entry->dest_file);
   eina_stringshare_del(entry->src_key);
   eina_stringshare_del(entry->dest_key);
   entry->src_file = NULL;
   entry->dest_file = NULL;
   entry->src_key = NULL;
   entry->dest_key = NULL;
}

Evg_Entry*
evas_evg_cache_find(const char *src_file, const char *src_key,
                    const char *dest_file, const char *dest_key,
                    float key_frame, int w, int h)
{
   Evg_Entry* se;
   Eina_Strbuf *hash_key;

   if (!evg_cache) return NULL;

   hash_key = eina_strbuf_new();
   eina_strbuf_append_printf(hash_key, "%s/%s/%s/%s/%.2f/%d/%d",
                             src_file, src_key, dest_file, dest_key,key_frame, w, h);
   se = eina_hash_find(evg_cache->active, eina_strbuf_string_get(hash_key));
   if (!se)
     {
        se = calloc(1, sizeof(Evg_Entry));
        se->src_file = eina_stringshare_add(src_file);
        se->src_key = eina_stringshare_add(src_key);
        se->dest_file = eina_stringshare_add(dest_file);
        se->dest_key = eina_stringshare_add(dest_key);
        se->key_frame = key_frame;
        se->w = w;
        se->h = h;
        se->hash_key = eina_strbuf_string_steal(hash_key);
        eina_hash_direct_add(evg_cache->active, se->hash_key, se);
     }
   eina_strbuf_free(hash_key);
   se->ref++;
   return se;
}

Efl_VG*
evas_evg_cache_vg_tree_get(Evg_Entry *entry)
{
   if (entry->root) return entry->root;

   if (entry->src_file)
     {
        _evas_cache_svg_vg_tree_update(entry);
     }
   return entry->root;
}

void
evas_evg_cache_entry_del(Evg_Entry *svg_entry)
{
   if (!svg_entry) return;

   svg_entry->ref--;
}

