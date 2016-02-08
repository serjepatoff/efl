#include "ecore_drm2_private.h"

static void
_device_calibration_set(Ecore_Drm2_Input_Device *dev)
{
   float cal[6];
   const char *vals;
   const char *sysname;
   const char *devices;
   Eina_List *devices;
   int w = 0, h = 0;
   enum libinput_config_status status;

   if (!dev->output) return;

   w = dev->output->current_mode->width;
   h = dev->output->current_mode->height;

   if ((!libinput_device_config_calibration_has_matrix(dev->device)) ||
       (libinput_device_config_calibration_get_default_matrix(dev->device, cal) != 0))
     return;

   sysname = libinput_device_get_sysname(dev->device);

   devices = eeze_udev_find_by_subsystem_sysname("input", sysname);
   EINA_LIST_FREE(devices, device)
     {
        vals = eeze_udev_syspath_get_property(device, "WL_CALIBRATION");
        if ((!vals) ||
            (sscanf(vals, "%f %f %f %f %f %f",
                    &cal[0], &cal[1], &cal[2], &cal[3], &cal[4], &cal[5]) != 6))
          goto cont;

        cal[2] /= w;
        cal[5] /= h;

        status =
          libinput_device_config_calibration_set_matrix(dev->device, cal);
        if (status != LIBINPUT_CONFIG_STATUS_SUCCESS)
          WRN("Failed to apply device calibration");

cont:
        eina_stringshare_del(device);
     }
}

static void
_device_configure(Ecore_Drm2_Input_Device *dev)
{
   if (libinput_device_config_tap_get_finger_count(dev->device) > 0)
     {
        Eina_Bool enable = EINA_FALSE;

        enable = libinput_device_config_tap_get_default_enabled(dev->device);
        libinput_device_config_tap_set_enabled(dev->device, enable);
     }

   _device_calibration_set(dev);
}

Ecore_Drm2_Input_Device *
_ecore_drm2_input_device_create(Ecore_Drm2_Seat *seat, struct libinput_device *device)
{
   Ecore_Drm2_Input_Device *dev;

   dev = calloc(1, sizeof(Ecore_Drm2_Input_Device));
   if (!dev) return NULL;

   dev->seat = seat;
   dev->device = device;

   if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_KEYBOARD))
     {
        dev->caps |= EVDEV_SEAT_KEYBOARD;
     }

   if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_POINTER))
     {
        dev->caps |= EVDEV_SEAT_POINTER;
     }

   if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_TOUCH))
     {
        dev->caps |= EVDEV_SEAT_TOUCH;
     }

   libinput_device_set_user_data(device, dev);
   libinput_device_ref(device);

   _device_configure(dev);

   return dev;
}

void
_ecore_drm2_input_device_destroy(Ecore_Drm2_Input_Device *device)
{
   if (!device) return;

   /* TODO: release pointer, keyboard, touch from seat */

   libinput_device_unref(device->device);
   eina_stringshare_del(device->output_name);
   free(device);
}

int
_ecore_drm2_input_device_event_process(struct libinput_event *event)
{
   int ret = 1;

   switch (libinput_event_get_type(event))
     {
      case LIBINPUT_EVENT_KEYBOARD_KEY:
        break;
      case LIBINPUT_EVENT_POINTER_MOTION:
        break;
      case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
        break;
      case LIBINPUT_EVENT_POINTER_BUTTON:
        break;
      case LIBINPUT_EVENT_POINTER_AXIS:
        break;
      case LIBINPUT_EVENT_TOUCH_DOWN:
        break;
      case LIBINPUT_EVENT_TOUCH_MOTION:
        break;
      case LIBINPUT_EVENT_TOUCH_UP:
        break;
      case LIBINPUT_EVENT_TOUCH_FRAME:
        break;
      default:
        ret = 0;
        break;
     }

   return ret;
}
