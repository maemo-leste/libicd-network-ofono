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

struct connctx_handler_data
{
  ofono_private *priv;
  gchar *network_id;
  icd_nw_link_up_cb_fn link_up_cb;
  gpointer link_up_cb_token;
  struct modem_data *md;
  OfonoConnCtx *ctx;
  guint timeout_id;
  gulong id;
};

static gboolean
_link_up_idle(gpointer user_data)
{
  struct connctx_handler_data *data = user_data;
  ofono_private *priv = data->priv;
  gchar *net_id = data->network_id;
  OfonoConnCtx *ctx = data->ctx;
  const OfonoConnCtxSettings* s = ctx->settings;
  gchar *ipv4_type = ofono_icd_gconf_get_iap_string(priv, net_id, "ipv4_type");

  if (!ipv4_type)
  {
    ipv4_type = g_strdup("AUTO");
    ofono_icd_gconf_set_iap_string(priv, net_id, "ipv4_type", "AUTO");
  }

  OFONO_DEBUG("Calling next layer, ipv4_type: %s", ipv4_type);

  /* hack settings so next layer to take it from there */
  if (!strcmp(ipv4_type, "AUTO"))
  {
    if (s->method != OFONO_CONNCTX_METHOD_DHCP)
    {
      OFONO_DEBUG("ipv4 settings: %s %s (gw %s) (nm %s) (dns %s %s)",
                  s->ifname, s->address, s->gateway, s->netmask,
                  s->dns[0], s->dns[0] ? s->dns[1] : s->dns[0]);
      ofono_icd_gconf_set_iap_string(priv, net_id, "ipv4_address", s->address);
      ofono_icd_gconf_set_iap_string(priv, net_id, "ipv4_gateway", s->gateway);
      ofono_icd_gconf_set_iap_string(priv, net_id, "ipv4_netmask", s->netmask);
      ofono_icd_gconf_set_iap_string(priv, net_id, "ipv4_type", "STATIC");

      if (s->dns[0])
      {
        ofono_icd_gconf_set_iap_string(priv, net_id, "ipv4_dns1", s->dns[0]);

        if (s->dns[1])
          ofono_icd_gconf_set_iap_string(priv, net_id, "ipv4_dns2", s->dns[1]);
      }
    }
    else
      OFONO_DEBUG("ipv4 settings: dhcp");
  }

  data->timeout_id = 0;
  data->link_up_cb(ICD_NW_SUCCESS_NEXT_LAYER, NULL, data->ctx->settings->ifname,
                   data->link_up_cb_token, NULL);

  /* restore what we found initially */
  ofono_icd_gconf_set_iap_string(priv, net_id, "ipv4_type", ipv4_type);
  g_free(ipv4_type);

  g_hash_table_remove(data->md->ctxhd, data->ctx);

  return G_SOURCE_REMOVE;
}

static void
connctx_activate(struct connctx_handler_data *data, gboolean activate)
{
  if (activate)
  {
    /* we shall set here user/pwd/whatever from gconf */
    OFONO_DEBUG("Activate ctx: %p", data->ctx);
    ofono_connctx_activate(data->ctx);
  }
  else
  {
    OFONO_DEBUG("Dectivate ctx: %p", data->ctx);
    ofono_connctx_deactivate(data->ctx);
  }
}

static void
_ctx_active_changed_cb(OfonoConnCtx *ctx, void* user_data)
{
  struct connctx_handler_data *data = user_data;

  OFONO_DEBUG("ctx %p active state changed to %d", ctx, ctx->active);

  if (ctx->active)
    data->timeout_id = g_idle_add(_link_up_idle, data);
  else
    connctx_activate(data, TRUE);
}

static void
_connctx_weak_notify(gpointer user_data, GObject *ctx)
{
  struct connctx_handler_data *data = user_data;

  g_hash_table_remove(data->md->ctxhd, ctx);
}

void
ofono_connctx_handler_data_destroy(gpointer ctxhd)
{
  struct connctx_handler_data *data = ctxhd;

  g_object_weak_unref((GObject *)data->ctx, _connctx_weak_notify, data);
  ofono_connctx_remove_handler(data->ctx, data->id);

  if (data->timeout_id)
    g_source_remove(data->timeout_id);

  g_free(data->network_id);
  g_free(data);
}

void
ofono_link_up(const gchar *network_type, const guint network_attrs,
              const gchar *network_id, icd_nw_link_up_cb_fn link_up_cb,
              const gpointer link_up_cb_token, gpointer *_priv)
{
  ofono_private *priv = *_priv;
  const char *err_msg = "no_network";
  gchar *imsi;
  gboolean error = TRUE;
  struct modem_data *md;
  gchar *apn = NULL;
  OfonoConnCtx *ctx;
  struct connctx_handler_data *data;

  OFONO_ENTER

  imsi = icd_gconf_get_iap_string(network_id, SIM_IMSI);

  if (!imsi)
  {
      OFONO_WARN("network_id %s is missing imsi gconf data", network_id);
      goto out;
  }

  OFONO_DEBUG("Got IMSI: %s", imsi);

  md = ofono_modem_find_by_imsi(priv, imsi);

  if (!md)
  {
    OFONO_WARN("No modem found for imsi %s", imsi);
    goto out;
  }

  OFONO_DEBUG("Got modem data");

  apn = icd_gconf_get_iap_string(network_id, "gprs_accesspointname");

  OFONO_DEBUG("Got APN: %s", apn);

  ctx = ofono_modem_get_context_by_apn(md, apn);

  if (!ctx)
  {
    OFONO_WARN("No context found for apn %s", apn);
    goto out;
  }
  else
    OFONO_DEBUG("Got ctx: %p", ctx);

  data = g_try_new(struct connctx_handler_data, 1);

  if (!data)
    goto out;

  data->priv = priv;
  data->network_id = g_strdup(network_id);
  data->link_up_cb = link_up_cb;
  data->link_up_cb_token = link_up_cb_token;
  data->md = md;
  data->ctx = ctx;
  data->timeout_id = 0;
  data->id = ofono_connctx_add_active_changed_handler(
        ctx, _ctx_active_changed_cb, data);

  g_hash_table_insert(md->ctxhd, ctx, data);

  /* in case ctx gets destroyed behind our back */
  g_object_weak_ref((GObject *)ctx, _connctx_weak_notify, data);

  connctx_activate(data, !ctx->active);
  error = FALSE;

out:
  g_free(apn);
  g_free(imsi);

  if (error)
    link_up_cb(ICD_NW_ERROR, err_msg, NULL, link_up_cb_token, NULL);

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

    if (md)
    {
      OfonoConnCtx *ctx ;
      gchar *apn = icd_gconf_get_iap_string(network_id, "gprs_accesspointname");

      OFONO_DEBUG("Got modem data, APN %s", apn);

      ctx = ofono_modem_get_context_by_apn(md, apn);
      g_free(apn);

      if (ctx)
      {
        g_hash_table_remove(md->ctxhd, ctx);

        if (ctx->active)
          ofono_connctx_deactivate(ctx);
      }

    }
  }

  link_down_cb(ICD_NW_SUCCESS, link_down_cb_token);

  OFONO_EXIT
}
