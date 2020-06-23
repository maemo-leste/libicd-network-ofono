#ifndef _ICD_GCONF_H_
#define _ICD_GCONF_H_

#include "ofono-private.h"

void ofono_icd_gconf_check_error(GError **error);
gboolean ofono_icd_gconf_set_iap_string (ofono_private *priv, const char *iap_name, const char *key, const char *val);
gboolean ofono_icd_gconf_set_iap_bool (ofono_private *priv, const char *iap_name, const char *key, gboolean val);
GHashTable * get_gprs_iaps(ofono_private *priv);

#define NAME "name"
#define TYPE "type"

#endif /* _ICD_GCONF_H */
