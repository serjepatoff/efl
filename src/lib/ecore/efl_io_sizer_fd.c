#define EFL_IO_SIZER_FD_PROTECTED 1

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <Ecore.h>
#include "ecore_private.h"

#define MY_CLASS EFL_IO_SIZER_FD_CLASS

/* TODO: FIXME, add to eina_error.h */
#define eina_error_from_errno(x) x

typedef struct _Efl_Io_Sizer_Fd_Data
{
   int fd;
} Efl_Io_Sizer_Fd_Data;

EOLIAN static void
_efl_io_sizer_fd_sizer_fd_set(Eo *o EINA_UNUSED, Efl_Io_Sizer_Fd_Data *pd, int fd)
{
   pd->fd = fd;
}

EOLIAN static int
_efl_io_sizer_fd_sizer_fd_get(Eo *o EINA_UNUSED, Efl_Io_Sizer_Fd_Data *pd)
{
   return pd->fd;
}

EOLIAN static Eina_Error
_efl_io_sizer_fd_efl_io_sizer_resize(Eo *o, Efl_Io_Sizer_Fd_Data *pd EINA_UNUSED, uint64_t size)
{
   int fd = efl_io_sizer_fd_sizer_fd_get(o);
   if (ftruncate(fd, size) < 0) return eina_error_from_errno(errno);
   efl_event_callback_call(o, EFL_IO_SIZER_EVENT_CHANGED, NULL);
   return 0;
}

EOLIAN static uint64_t
_efl_io_sizer_fd_efl_io_sizer_size_get(Eo *o, Efl_Io_Sizer_Fd_Data *pd EINA_UNUSED)
{
   int fd = efl_io_sizer_fd_sizer_fd_get(o);
   struct stat st;
   int r;

   EINA_SAFETY_ON_TRUE_RETURN_VAL(fd < 0, 0);

   r = fstat(fd, &st);
   EINA_SAFETY_ON_TRUE_RETURN_VAL(r < 0, 0);

   return st.st_size;
}

#include "efl_io_sizer_fd.eo.c"
