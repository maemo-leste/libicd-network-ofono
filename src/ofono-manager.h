#include "modem.h"
#include "notifier.h"

enum ofono_manager_modem_change
{
  /** a modem is added */
  OFONO_MANAGER_MODEM_ADD,
  /** a modem property has changed */
  OFONO_MANAGER_MODEM_CHANGE,
  /** a modem is removed */
  OFONO_MANAGER_MODEM_REMOVE
};

struct _modem_changed
{
  enum ofono_manager_modem_change type;
  const modem *modem;
};

typedef struct _modem_changed modem_changed;

gboolean ofono_manager_modems_register(notify_fn cb, gpointer user_data);
GHashTable *ofono_manager_get_modems(void);
void ofono_manager_modems_close(notify_fn cb, gpointer user_data);
