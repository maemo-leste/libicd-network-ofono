#include <gofono_modem.h>
#include <gofono_simmgr.h>

void ofono_modem_manager_init(ofono_private *priv);
void ofono_modem_manager_exit(ofono_private *priv);

enum modem_handler_id {
  MODEM_HANDLER_VALID,
  MODEM_HANDLER_COUNT
};

enum simmgr_handler_id {
  SIMMGR_HANDLER_PROPERTY,
  SIMMGR_HANDLER_COUNT
};

struct modem_data
{
  OfonoModem *modem;
  gulong modem_handler_id[MODEM_HANDLER_COUNT];

  OfonoSimMgr *sim;
  gulong simmgr_handler_id[SIMMGR_HANDLER_COUNT];
};
