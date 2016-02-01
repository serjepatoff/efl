#include "ecore_drm2_private.h"

static Ecore_Drm2_Interface *_ifaces[] =
{
#ifdef HAVE_SYSTEMD
   &_logind_iface,
#endif
   NULL, /* &_launch_iface, */
   NULL, /* &_direct_iface, */
   NULL,
};

EAPI Ecore_Drm2_Launcher *
ecore_drm2_launcher_connect(const char *seat, unsigned int tty, Eina_Bool sync)
{
   Ecore_Drm2_Interface **it;

   for (it = _ifaces; *it != NULL; it++)
     {
        Ecore_Drm2_Interface *iface;
        Ecore_Drm2_Launcher *l;

        iface = *it;
        if (iface->connect(&l, seat, tty, sync))
          return l;
     }

   return NULL;
}

EAPI void
ecore_drm2_launcher_disconnect(Ecore_Drm2_Launcher *launcher)
{
   EINA_SAFETY_ON_NULL_RETURN(launcher);
   EINA_SAFETY_ON_NULL_RETURN(launcher->iface);

   if (launcher->iface->disconnect)
     launcher->iface->disconnect(launcher);
}
