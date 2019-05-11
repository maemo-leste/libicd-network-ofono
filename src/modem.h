#include <icd/support/icd_dbus.h>

struct _sim
{
  gchar *imsi;
  gchar *spn;
  gint present;
};

typedef struct _sim sim;

struct _net
{
  gchar *name;
  gint registered;
  gint roaming;
};

typedef struct _net net;

/** @brief Represents the current state of OFONO modem */
struct _modem
{
  /** Modem object path */
  gchar *path;
  gint emergency_call;
  gboolean powered;
  gint online;
  gchar *imei;
  guint64 interfaces;
  sim sim;
  net net;
};

typedef struct _modem modem;

struct _property_changed
{
  const char *property;
  DBusBasicValue val;
};

typedef struct _property_changed property_changed;

#define OFONO_MODEM_INTERFACE_SIM_MANAGER                  0x0000000000000001LL
#define OFONO_MODEM_INTERFACE_LTE                          0x0000000000000002LL
#define OFONO_MODEM_INTERFACE_NETWORK_REGISTRATION         0x0000000000000004LL
#define OFONO_MODEM_INTERFACE_CONNECTION_MANAGER           0x0000000000000008LL

modem *modem_new(const char *path, gboolean powered);
void modem_free(modem *modem);
modem *modem_dup(const modem *modem);

GHashTable *modem_list_create(void);
void modem_list_add(GHashTable *modems, const modem *modem);
void modem_list_remove(GHashTable *modems, const gchar *path);
modem *modem_list_find(GHashTable *modems, const gchar *path);
void modem_list_free(GHashTable *modems);

void modem_add_interface(modem *modem, guint64 interface);
void modem_remove_interface(modem *modem, guint64 interface);
gboolean modem_interface_supported(modem *modem, guint64 interface);
