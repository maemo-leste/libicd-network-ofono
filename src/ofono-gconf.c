#include <icd/icd_gconf.h>

#include "ofono-private.h"
#include "log.h"

#include "ofono-gconf.h"

gboolean
ofono_gconf_init(ofono_private *priv)
{
  priv->gconf = gconf_client_get_default();

  if (!priv->gconf)
  {
    OFONO_ERR("ERROR: GConf error");
    return FALSE;
  }

  return TRUE;
}

void
ofono_gconf_exit(ofono_private *priv)
{
  if (priv->gconf)
  {
    g_object_unref(priv->gconf);
    priv->gconf = NULL;
  }
}


