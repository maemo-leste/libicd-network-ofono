#include <icd/icd_iap.h>
#include <icd/osso-ic-gconf.h>
#include <icd/icd_gconf.h>

#include <string.h>

#include "ofono-private.h"
#include "log.h"

#include "iap.h"

#define SIM_IMSI "sim_imsi"
#define NAME "name"
#define TYPE "type"

static void
ofono_icd_gconf_check_error(GError **error)
{
  if (error && *error)
  {
    OFONO_ERR("icd gconf error: %s", (*error)->message);
    g_clear_error(error);
    *error = NULL;
  }
}

static gboolean
ofono_icd_gconf_set_iap_string (ofono_private *priv, const char *iap_name,
                                const char *key, const char *val)
{
  GError *err = NULL;
  char *id = gconf_escape_key(iap_name, -1);
  gchar *dir = g_strdup_printf(ICD_GCONF_PATH "/%s/%s", id, key);
  gboolean rv = gconf_client_set_string(priv->gconf, dir, val, &err);

  g_free(id);
  g_free(dir);
  ofono_icd_gconf_check_error(&err);

  return rv;
}

/* sim_imsi->iap_id */
static GHashTable *
get_gprs_iaps(ofono_private *priv)
{
  GHashTable *gprs_iaps =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  GSList *dirs = gconf_client_all_dirs(priv->gconf, ICD_GCONF_PATH, NULL);
  GSList *l;

  for (l = dirs; l; l = l->next)
  {
    const gchar *dir = dirs->data;
    gchar *key = g_strconcat(dir, "/type", NULL);
    gchar *type = gconf_client_get_string(priv->gconf, key, NULL);

    g_free(key);

    if (type && !strcmp(type, "GPRS"))
    {
      gchar *sim_imsi;

      key = g_strconcat(dir, "/", SIM_IMSI, NULL);
      sim_imsi = gconf_client_get_string(priv->gconf, key, NULL);
      g_free(key);

      if (sim_imsi)
      {
        const char *id = g_strrstr(dir, "/");

        if (*sim_imsi)
        {
          if (id)
          {
            g_hash_table_insert(gprs_iaps, sim_imsi,
                                gconf_unescape_key(id + 1, -1));
          }
        }
        else
        {
          OFONO_ERR("Empty SIM imsi found for IAP %s", id);
          g_free(sim_imsi);
        }
      }
    }
  }

  return gprs_iaps;
}

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
ofono_iap_provision_sim(const gchar *imsi, const gchar *spn, ofono_private *priv)
{
  GHashTable *gprs_iaps = get_gprs_iaps(priv);
  gchar *id = g_hash_table_lookup(gprs_iaps, imsi);

  if (id)
    id = g_strdup(id);
  else
  {
    id = create_iap_id();
    ofono_icd_gconf_set_iap_string(priv, id, SIM_IMSI, imsi);
    ofono_icd_gconf_set_iap_string(priv, id, TYPE, "GPRS");
  }

  ofono_icd_gconf_set_iap_string(priv, id, NAME, spn);

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

gchar *
ofono_iap_get_name(const gchar *id)
{
  return icd_gconf_get_iap_string(id, NAME);
}
