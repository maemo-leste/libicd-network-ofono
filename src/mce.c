#include <icd/support/icd_dbus.h>
#include <mce/dbus-names.h>
#include <mce/mode-names.h>
#include <libofono/log.h>

#include <string.h>


#include "mce.h"

static gboolean inited = FALSE;
static enum ofono_mce_device_mode device_mode = OFONO_MCE_DEVICE_MODE_INVALID;
static GSList *notifiers = NULL;

static void
ofono_mce_device_mode_change(const char *mode)
{
  if (!strcmp(mode, MCE_NORMAL_MODE))
    device_mode = OFONO_MCE_DEVICE_MODE_NORMAL;
  else if (!strcmp(mode, MCE_FLIGHT_MODE) || !strcmp(mode, MCE_OFFLINE_MODE))
    device_mode = OFONO_MCE_DEVICE_MODE_OFFLINE;
  else
    device_mode = OFONO_MCE_DEVICE_MODE_INVALID;

  ofono_notifier_notify(notifiers, &device_mode);
}

static void
ofono_mce_get_device_mode_cb(DBusPendingCall *pending, void *user_data)
{
  DBusMessage *reply;

  if (pending)
  {
    reply = dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);
  }

  if (pending && reply)
  {
    if (dbus_message_get_type(reply) != DBUS_MESSAGE_TYPE_ERROR)
    {
      const char * mode;
      if (!dbus_message_get_args(reply, NULL,
                                 DBUS_TYPE_STRING, &mode,
                                 DBUS_TYPE_INVALID))
      {
        OFONO_WARN("could not get args from D-Bus message");
      }
      else
        ofono_mce_device_mode_change(mode);
    }
    else
    {
      OFONO_WARN("ask device_state returned '%s'",
                 dbus_message_get_error_name(reply));
    }

    dbus_message_unref(reply);
  }
}

static gboolean
ofono_mce_get_device_mode_init()
{
  gboolean rv = FALSE;
  DBusMessage *message;
  DBusPendingCall *mcall;

  OFONO_ENTER

  message = dbus_message_new_method_call(MCE_SERVICE,
                                         MCE_REQUEST_PATH,
                                         MCE_REQUEST_IF,
                                         MCE_DEVICE_MODE_GET);

  if (message)
  {
    mcall = icd_dbus_send_system_mcall(message, 10000,
                                       ofono_mce_get_device_mode_cb, NULL);

    if (mcall)
    {
      OFONO_DEBUG("sent mcall");
      rv = TRUE;
    }
    else
      OFONO_ERR("could not send '" MCE_DEVICE_MODE_GET "' mcall");

    dbus_message_unref(message);
  }
  else
    OFONO_ERR("could not create '" MCE_DEVICE_MODE_GET "' method call");

  OFONO_EXIT

  return rv;
}

static DBusHandlerResult
ofono_mce_dbus_filter(DBusConnection *connection, DBusMessage *message,
                      void *user_data)
{
  if (dbus_message_is_signal(message, MCE_SIGNAL_IF, MCE_DEVICE_MODE_SIG))
  {
    DBusError error;
    const char *mode;

    dbus_error_init(&error);

    if (dbus_message_get_args(message, &error,
                              DBUS_TYPE_STRING, &mode,
                              DBUS_TYPE_INVALID))
    {
      ofono_mce_device_mode_change(mode);
    }
    else
    {
      OFONO_ERR("could not get args from sig, '%s'", error.message);
      dbus_error_free(&error);
    }
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
static gboolean
ofono_mce_device_mode_add_dbus_filter()
{
  return
      icd_dbus_connect_system_bcast_signal(MCE_REQUEST_IF,
                                           ofono_mce_dbus_filter, NULL,
                                           "member='" MCE_DEVICE_MODE_SIG "'");
}

static void
ofono_mce_device_mode_remove_dbus_filter()
{
  icd_dbus_disconnect_system_bcast_signal(MCE_REQUEST_IF,
                                          ofono_mce_dbus_filter, NULL,
                                          "member='" MCE_DEVICE_MODE_SIG "'");
}

gboolean
ofono_mce_device_mode_register(ofono_notify_fn cb, gpointer user_data)
{
  gboolean rv = TRUE;

  if (!inited)
  {
    if ((rv = ofono_mce_get_device_mode_init()))
        rv = ofono_mce_device_mode_add_dbus_filter();
    inited = TRUE;
  }

  if (rv)
    ofono_notifier_register(&notifiers, cb, user_data);

  return rv;
}

void
ofono_mce_device_mode_close(ofono_notify_fn cb, gpointer user_data)
{
  ofono_notifier_close(&notifiers, cb, user_data);

  if (!notifiers)
  {
    ofono_mce_device_mode_remove_dbus_filter();
    device_mode = OFONO_MCE_DEVICE_MODE_INVALID;
    inited = FALSE;
  }
}

enum ofono_mce_device_mode
ofono_mce_get_device_mode(void)
{
  return device_mode;
}
