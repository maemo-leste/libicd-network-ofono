#include <libofono/log.h>
#include <libofono/ofono-manager.h>

#include "ofono-private.h"
#include "ofono-gconf.h"
#include "search.h"
#include "link.h"
#include "mce.h"

gboolean
icd_nw_init(struct icd_nw_api *network_api, icd_nw_watch_pid_fn watch_fn,
            gpointer watch_fn_token, icd_nw_close_fn close_fn,
            icd_nw_status_change_fn status_change_fn, icd_nw_renew_fn renew_fn);

static void
mce_device_mode_change_cb(gpointer data, gpointer user_data)
{
  enum ofono_mce_device_mode mode = *((enum ofono_mce_device_mode *)data);

  OFONO_ENTER

  OFONO_INFO("Device mode changed: %d", mode);

  //    close_all_contexts(priv, "Entered offline mode", NULL);

  OFONO_EXIT
}

static void
ofono_manager_modem_change_cb(gpointer data, gpointer user_data)
{
  modem_changed *mc = data;
  const gchar *path = mc->modem->path;
  ofono_private *priv = user_data;

  OFONO_ENTER

  if (mc->type == OFONO_MANAGER_MODEM_ADD)
  {
    OFONO_INFO("Added modem, path %s", path);
  }
  else if (mc->type == OFONO_MANAGER_MODEM_CHANGE)
  {
    OFONO_INFO("Change modem, path %s", path);
    pending_operation_group_list_execute(priv->operation_groups);
  }
  else
  {
    OFONO_INFO("Removed modem, path %s", path);
    pending_operation_group_list_remove(priv->operation_groups, path);
  }

  OFONO_EXIT
}

static gboolean
ofono_network_init(ofono_private *priv)
{
  gboolean rv = FALSE;

  OFONO_ENTER

  rv = ofono_mce_device_mode_register(mce_device_mode_change_cb, priv);

  if (rv)
    ofono_manager_modems_register(ofono_manager_modem_change_cb, priv);

  if (rv)
    priv->operation_groups = pending_operation_group_list_create();

  if (rv)
    rv = ofono_gconf_init(priv);

  OFONO_EXIT

  return rv;
}

static void
ofono_network_exit(ofono_private *priv)
{
  OFONO_ENTER

  ofono_gconf_exit(priv);

  if (priv)
  {
    pending_operation_group_list_destroy(priv->operation_groups);
    ofono_manager_modems_close(ofono_manager_modem_change_cb, priv);
    ofono_mce_device_mode_close(mce_device_mode_change_cb, priv);
  }

  OFONO_EXIT
}

static void
ofono_network_destruct(gpointer *_priv)
{
  ofono_private *priv = *_priv;

  OFONO_ENTER

  ofono_network_exit(priv);

  g_free(*_priv);
  *_priv = NULL;

  OFONO_EXIT
}

gboolean
icd_nw_init(struct icd_nw_api *network_api, icd_nw_watch_pid_fn watch_fn,
            gpointer watch_fn_token, icd_nw_close_fn close_fn,
            icd_nw_status_change_fn status_change_fn, icd_nw_renew_fn renew_fn)
{
  ofono_private *priv = g_new0(ofono_private, 1);
  gboolean rv = FALSE;

  OFONO_ENTER

  OFONO_DEBUG("new priv: %p", priv);

  network_api->version = ICD_NW_MODULE_VERSION;
  network_api->link_up = ofono_link_up;
  network_api->link_down = ofono_link_down;
  //network_api->link_post_up = ofono_link_post_up;
  /*network_api->ip_up = gprs_ip_up;
  network_api->ip_down = gprs_ip_down;*/
  network_api->start_search = ofono_start_search;
  network_api->stop_search = ofono_stop_search;
  /*network_api->ip_addr_info = gprs_ip_addr_info;
  network_api->ip_stats = gprs_ip_stats;*/
  network_api->search_lifetime = 30;
  network_api->search_interval = 20;
  network_api->network_destruct = ofono_network_destruct;
/*  network_api->child_exit = gprs_child_exit;
*/
  priv->close_fn = close_fn;
  priv->watch_fn_token = watch_fn_token;
  priv->watch_fn = watch_fn;

  rv = ofono_network_init(priv);

  if (!rv)
  {
    ofono_network_exit(priv);
    g_free(priv);
    priv = NULL;
  }

  network_api->private = priv;

  OFONO_EXIT

  return rv;
}
