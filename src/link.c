#include <icd/icd_gconf.h>

#include "ofono-private.h"
#include "ofono-modem.h"

#include "log.h"
#include "link.h"
#include "icd-gconf.h"

static struct modem_data *
ofono_modem_find_by_imsi(ofono_private *priv, const gchar *imsi)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, priv->modems);

  while (g_hash_table_iter_next (&iter, &key, &value))
  {
    struct modem_data *md = value;

    if (ofono_simmgr_valid(md->sim) && !strcmp(md->sim->imsi, imsi))
      return md;
  }

  return NULL;
}

#if 0
static gboolean
link_pending_execute(gpointer user_data)
{
  ofono_private *priv = user_data;

  pending_operation_group_list_execute(priv->operation_groups);

  return FALSE;
}
#endif


void
ofono_link_up(const gchar *network_type, const guint network_attrs,
              const gchar *network_id, icd_nw_link_up_cb_fn link_up_cb,
              const gpointer link_up_cb_token, gpointer *_priv)
{
  ofono_private *priv = *_priv;
  const char *err_msg = "no_network";
  gchar *imsi;

  OFONO_ENTER


  OFONO_DEBUG("Getting IMSI");
  imsi = icd_gconf_get_iap_string(network_id, SIM_IMSI);

  int hack_ok = 0;
  gchar* hack_interface = NULL;
  gchar* hack_ip = NULL;
  gchar* hack_gw = NULL;
  gchar* hack_netmask = NULL;

  if (imsi)
  {
    OFONO_DEBUG("Got IMSI: %s", imsi);
    struct modem_data *md = ofono_modem_find_by_imsi(priv, imsi);

    g_free(imsi);

    if (md)
    {
      OFONO_DEBUG("Got modem data");
      gchar *apn = icd_gconf_get_iap_string(network_id, "gprs_accesspointname");
      OFONO_DEBUG("Got APN: %s", apn);
      OfonoConnCtx *ctx = ofono_modem_get_context_by_apn(md, apn);
      /* TODO: free apn ? */
      /* XXX: unref this ctx, maybe? */

      OFONO_DEBUG("Got Ctx: %p", ctx);

      // XXX: call ofono_iap_provision_connection here?
      //
      // libgofono/include/gofono_connctx.h
      //
      //
      // method enums, etc

      g_free(apn);

      if (ctx)
      {
        if (!ctx->active) {
            OFONO_DEBUG("Context is not yet active, activating");
            ofono_connctx_activate(ctx);
        }

        if (ctx->active) {
            OFONO_DEBUG("Context is active");
            hack_interface = g_strdup(ctx->settings->ifname);
            hack_ip = g_strdup(ctx->settings->address);
            hack_gw = g_strdup(ctx->settings->gateway);
            hack_netmask = g_strdup(ctx->settings->netmask);
            OFONO_DEBUG("Context settings: %s %s (gw %s) (nm %s)", hack_interface, hack_ip, hack_gw, hack_netmask);

            ofono_icd_gconf_set_iap_string(priv, network_id, "ipv4_address", ctx->settings->address);
            ofono_icd_gconf_set_iap_string(priv, network_id, "ipv4_gateway", ctx->settings->gateway);
            ofono_icd_gconf_set_iap_string(priv, network_id, "ipv4_netmask", ctx->settings->netmask);
            ofono_icd_gconf_set_iap_string(priv, network_id, "ipv4_type", "STATIC");
            hack_ok = 1;
        }
        //
        //pending_operation_group *g =
        //    pending_operation_group_new(path, ofono_link_finish, priv);


#if 0
        if (ctx->active)
        {
          ofono_connctx_deactivate(ctx);
          g_idle_add(link_pending_execute, priv);
        }
        else
        {
            /* set ctx from gconf */
            /* XXX: probably should be: set ctx from gconf ? */
        }
#endif
      }
    }
    else
      ; // XXX
      //OFONO_ERROR("No modem found for imsi %s", imsi);

  }
  else
    ; // XXX
    //OFONO_ERROR("network_id %s is missing imsi gconf data", network_id);


  // TODO: As temporary hack, could set ip values (for static or dhcp) here to
  // gconf and then pass to ipv4 layer and it would pick these up

  // typedef void(* icd_nw_link_up_cb_fn)(const enum icd_nw_status status, const gchar *err_str, const gchar *interface_name, const gpointer link_up_cb_token,...) 
  //
  if (hack_ok == 1) {
      link_up_cb(ICD_NW_SUCCESS_NEXT_LAYER, NULL, hack_interface, link_up_cb_token, NULL);
  } else {
      link_up_cb(ICD_NW_ERROR, err_msg, NULL, link_up_cb_token, NULL);
  }

  OFONO_EXIT
}

void
ofono_link_down(const gchar *network_type, const guint network_attrs,
                const gchar *network_id, const gchar *interface_name,
                icd_nw_link_down_cb_fn link_down_cb,
                const gpointer link_down_cb_token, gpointer *_priv)
{
  ofono_private *priv = *_priv;
  gchar *imsi;

  OFONO_ENTER


  OFONO_DEBUG("Getting IMSI");
  imsi = icd_gconf_get_iap_string(network_id, SIM_IMSI);

  if (imsi)
  {
    OFONO_DEBUG("Got IMSI: %s", imsi);
    struct modem_data *md = ofono_modem_find_by_imsi(priv, imsi);
    g_free(imsi);
    if (md) {
      OFONO_DEBUG("Got modem data");
      gchar *apn = icd_gconf_get_iap_string(network_id, "gprs_accesspointname");
      OFONO_DEBUG("Got APN: %s", apn);
      OfonoConnCtx *ctx = ofono_modem_get_context_by_apn(md, apn);
      /* TODO: free apn ? */
      if (ctx->active) {
        ofono_connctx_deactivate(ctx);
      }
    }
  }


  link_down_cb(ICD_NW_SUCCESS, link_down_cb_token);

  OFONO_EXIT
}
