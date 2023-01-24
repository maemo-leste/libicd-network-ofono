#ifndef _OFONO_MODEM_H_
#define _OFONO_MODEM_H_
#include <gofono_modem.h>
#include <gofono_simmgr.h>
#include <gofono_connmgr.h>
#include <gofono_connctx.h>

void ofono_modem_manager_init(ofono_private *priv);
void ofono_modem_manager_exit(ofono_private *priv);

enum modem_handler_id
{
  MODEM_HANDLER_VALID,
  MODEM_HANDLER_COUNT
};

enum simmgr_handler_id
{
  SIMMGR_HANDLER_PROPERTY,
  SIMMGR_HANDLER_COUNT
};

enum connmgr_handler_id
{
  CONNMGR_HANDLER_CONTEXT_ADDED,
  CONNMGR_HANDLER_COUNT
};

struct modem_data
{
  OfonoModem *modem;
  gulong modem_handler_id[MODEM_HANDLER_COUNT];

  OfonoSimMgr *sim;
  gulong simmgr_handler_id[SIMMGR_HANDLER_COUNT];

  OfonoConnMgr *connmgr;
  gulong connmgr_handler_id[CONNMGR_HANDLER_COUNT];

  /* this one contains ctx -> context activate changed handler data, so we can
   * clean-up if necesarry */
  GHashTable *ctxhd;
};

OfonoConnCtx *ofono_modem_get_last_internet_context(struct modem_data *md);
OfonoConnCtx *ofono_modem_get_context_by_id(struct modem_data *md,
                                            const char *context_id);
const char *ofono_context_get_id(OfonoConnCtx *ctx);

#endif /* _OFONO_MODEM_H_ */
