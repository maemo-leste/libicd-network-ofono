#include <icd/icd_iap.h>
#include <icd/osso-ic-gconf.h>
#include <icd/icd_gconf.h>

#include <string.h>

#include "ofono-private.h"
#include "ofono-modem.h"

#include "log.h"
#include "iap.h"
#include "icd-gconf.h"


/* sim_imsi->iap_id */

static gchar *
create_iap_id()
{
  struct icd_iap *iap = icd_iap_new();
  gchar *rv = NULL;

  if (icd_iap_id_create(iap, NULL))
    rv = iap->id;

  g_free(iap);

  return rv;
}

gchar *
ofono_iap_get_name(const gchar *id)
{
  return icd_gconf_get_iap_string(id, NAME);
}

static void
ofono_iap_provision_connection(const gchar *id, struct modem_data *md,
                               ofono_private *priv)
{
  OfonoConnCtx *ctx = ofono_modem_get_last_internet_context(md);

  if (!ctx)
    return;

  if (ctx->apn)
    ofono_icd_gconf_set_iap_string(priv, id, "gprs_accesspointname", ctx->apn);

  if (ctx->username)
    ofono_icd_gconf_set_iap_string(priv, id, "gprs_username", ctx->username);

  if (ctx->password)
    ofono_icd_gconf_set_iap_string(priv, id, "gprs_password", ctx->password);

  // Parse method from ctx->settings->method
  ofono_icd_gconf_set_iap_string(priv, id, "ipv4_type", "AUTO");
  ofono_icd_gconf_set_iap_bool(priv, id, "ipv4_autodns", TRUE);
  ofono_icd_gconf_set_iap_bool(priv, id, "ask_password", FALSE);

  if (!ctx->settings)
    return;

  if (ctx->settings->address)
  {
    ofono_icd_gconf_set_iap_string(priv, id, "ipv4_address",
                                   ctx->settings->address);
  }

  if (ctx->settings->dns[0])
  {
    ofono_icd_gconf_set_iap_string(priv, id, "ipv4_dns1",
                                   ctx->settings->dns[0]);
    if (ctx->settings->dns[1])
    {
      ofono_icd_gconf_set_iap_string(priv, id, "ipv4_dns2",
                                     ctx->settings->dns[1]);
    }
  }
}

gchar *
ofono_iap_provision_sim(struct modem_data *md, ofono_private *priv)
{
  GHashTable *gprs_iaps = get_gprs_iaps(priv);
  gchar *id = g_hash_table_lookup(gprs_iaps, md->sim->imsi);

  if (id)
    id = g_strdup(id);
  else
  {
    id = create_iap_id();
    ofono_icd_gconf_set_iap_string(priv, id, SIM_IMSI, md->sim->imsi);
    ofono_icd_gconf_set_iap_string(priv, id, TYPE, "GPRS");
    ofono_iap_provision_connection(id, md, priv);
  }

  ofono_icd_gconf_set_iap_string(priv, id, NAME, md->sim->spn);

  g_hash_table_destroy(gprs_iaps);

  return id;
}

gchar *
ofono_iap_sim_is_provisioned(const gchar *imsi, ofono_private *priv)
{
  GHashTable *gprs_iaps = get_gprs_iaps(priv);
  gchar *rv = g_hash_table_lookup(gprs_iaps, imsi);

  if (rv)
    rv = g_strdup(rv);

  g_hash_table_destroy(gprs_iaps);

  return rv;
}
