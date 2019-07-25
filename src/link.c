#include <icd/icd_gconf.h>

#include "ofono-private.h"
#include "ofono-modem.h"

#include "log.h"
#include "link.h"

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

static gboolean
link_pending_execute(gpointer user_data)
{
  ofono_private *priv = user_data;

  pending_operation_group_list_execute(priv->operation_groups);

  return FALSE;
}

void
ofono_link_up(const gchar *network_type, const guint network_attrs,
              const gchar *network_id, icd_nw_link_up_cb_fn link_up_cb,
              const gpointer link_up_cb_token, gpointer *_priv)
{
  ofono_private *priv = *_priv;
  const char *err_msg = "no_network";
  gchar *imsi;

  OFONO_ENTER

  imsi = icd_gconf_get_iap_string(network_id, SIM_IMSI);

  if (imsi)
  {
    struct modem_data *md = ofono_modem_find_by_imsi(priv, imsi);

    g_free(imsi);

    if (md)
    {
      gchar *apn = icd_gconf_get_iap_string(network_id, "gprs_accesspointname");
      OfonoConnCtx *ctx = ofono_modem_get_context_by_apn(md, apn);

      g_free(apn);

      if (ctx)
      {
        pending_operation_group *g =
            pending_operation_group_new(path, ofono_link_finish, priv);

        if (ctx->active)
        {
          ofono_connctx_deactivate(ctx);
          g_idle_add(link_pending_execute, priv);
        }
        else
        {
          /* set ctx from gconf */
        }
      }
    }
    else
      OFONO_ERROR("No modem found for imsi %s", imsi);

  }
  else
    OFONO_ERROR("network_id %s is missing imsi gconf data", network_id);

  link_up_cb(ICD_NW_ERROR, err_msg, 0, link_up_cb_token, 0);

  OFONO_EXIT
}

void
ofono_link_down(const gchar *network_type, const guint network_attrs,
                const gchar *network_id, const gchar *interface_name,
                icd_nw_link_down_cb_fn link_down_cb,
                const gpointer link_down_cb_token, gpointer *_priv)
{

  OFONO_ENTER

  link_down_cb(ICD_NW_SUCCESS, link_down_cb_token);

  OFONO_EXIT
}
