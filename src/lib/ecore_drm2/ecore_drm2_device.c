#include "ecore_drm2_private.h"

#ifndef DRM_CAP_CURSOR_WIDTH
# define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
# define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

static const char *
_drm2_device_find(const char *seat)
{
   Eina_List *devs, *l;
   const char *dev, *ret = NULL;
   Eina_Bool found = EINA_FALSE;
   Eina_Bool platform = EINA_FALSE;

   devs = eeze_udev_find_by_subsystem_sysname("drm", "card[0-9]*");
   if (!devs) return NULL;

   EINA_LIST_FOREACH(devs, l, dev)
     {
        const char *dpath, *dseat, *dparent;

        dpath = eeze_udev_syspath_get_devpath(dev);
        if (!dpath) continue;

        dseat = eeze_udev_syspath_get_property(dev, "ID_SEAT");
        if (!dseat) dseat = eina_stringshare_add("seat0");

        if ((seat) && (strcmp(seat, dseat)))
          goto cont;
        else if (strcmp(dseat, "seat0"))
          goto cont;

        dparent = eeze_udev_syspath_get_parent_filtered(dev, "pci", NULL);
        if (!dparent)
          {
             dparent =
               eeze_udev_syspath_get_parent_filtered(dev, "platform", NULL);
             platform = EINA_TRUE;
          }

        if (dparent)
          {
             if (!platform)
               {
                  const char *id;

                  id = eeze_udev_syspath_get_sysattr(dparent, "boot_vga");
                  if (id)
                    {
                       if (!strcmp(id, "1")) found = EINA_TRUE;
                       eina_stringshare_del(id);
                    }
               }
             else
               found = EINA_TRUE;

             eina_stringshare_del(dparent);
          }

cont:
        eina_stringshare_del(dpath);
        eina_stringshare_del(dseat);
        if (found) break;
     }

   if (!found) goto out;

   ret = eeze_udev_syspath_get_devpath(dev);

out:
   EINA_LIST_FREE(devs, dev)
     eina_stringshare_del(dev);

   return ret;
}

EAPI Ecore_Drm2_Device *
ecore_drm2_device_find(const char *seat, unsigned int tty, Eina_Bool sync)
{
   Ecore_Drm2_Device *dev;

   dev = calloc(1, sizeof(Ecore_Drm2_Device));
   if (!dev) return NULL;

   dev->em = elput_manager_connect(seat, tty, sync);
   if (!dev->em)
     {
        ERR("Could not connect to input manager");
        goto man_err;
     }

   dev->path = _drm2_device_find(seat);
   if (!dev->path)
     {
        ERR("Could not find drm device on seat %s", seat);
        goto path_err;
     }

   return dev;

path_err:
   elput_manager_disconnect(dev->em);
man_err:
   free(dev);
   return NULL;
}

EAPI int
ecore_drm2_device_open(Ecore_Drm2_Device *device)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(device, -1);

   device->fd = elput_manager_open(device->em, device->path, -1);

   DBG("Device Path: %s", device->path);
   DBG("Device Fd: %d", device->fd);

   /* NB: Not going to enable planes if we don't support atomic */
   /* if (drmSetClientCap(device->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) < 0) */
   /*   ERR("Could not set Universal Plane support: %m"); */

   return device->fd;
}

EAPI void
ecore_drm2_device_close(Ecore_Drm2_Device *device)
{
   EINA_SAFETY_ON_NULL_RETURN(device);
   EINA_SAFETY_ON_TRUE_RETURN(device->fd < 0);

   elput_manager_close(device->em, device->fd);
}

EAPI void
ecore_drm2_device_free(Ecore_Drm2_Device *device)
{
   EINA_SAFETY_ON_NULL_RETURN(device);

   eina_stringshare_del(device->path);
   free(device);
}

EAPI int
ecore_drm2_device_clock_id_get(Ecore_Drm2_Device *device)
{
   uint64_t caps;
   int ret;

   EINA_SAFETY_ON_NULL_RETURN_VAL(device, -1);
   EINA_SAFETY_ON_TRUE_RETURN_VAL((device->fd < 0), -1);

   ret = drmGetCap(device->fd, DRM_CAP_TIMESTAMP_MONOTONIC, &caps);
   if ((ret == 0) && (caps == 1))
     return CLOCK_MONOTONIC;
   else
     return CLOCK_REALTIME;
}

EAPI void
ecore_drm2_device_cursor_size_get(Ecore_Drm2_Device *device, int *width, int *height)
{
   uint64_t caps;
   int ret;

   EINA_SAFETY_ON_NULL_RETURN(device);
   EINA_SAFETY_ON_TRUE_RETURN((device->fd < 0));

   if (width)
     {
        *width = 64;
        ret = drmGetCap(device->fd, DRM_CAP_CURSOR_WIDTH, &caps);
        if (ret == 0) *width = caps;
     }
   if (height)
     {
        *height = 64;
        ret = drmGetCap(device->fd, DRM_CAP_CURSOR_HEIGHT, &caps);
        if (ret == 0) *height = caps;
     }
}
