#include <glib.h>

#include "log.h"
#include "modem.h"

/**
 * @brief Allocates and initializes #ofono_modem structure.
 *
 * @param path Modem object path
 * @param powered Whether modem is powered
 *
 * @return Newly allocated #ofono_modem structure. Must be freed with
 * #ofono_modem_free
 */
modem *
modem_new(const char *path, gboolean powered)
{
  modem *m = g_new0(modem, 1);

  m->path = g_strdup(path);
  m->powered = powered;

  m->emergency_call = -1;
  m->online = -1;
  m->sim.present = -1;
  m->net.registered = -1;
  m->net.roaming = -1;

  return m;
}

/**
 * @brief Duplicates #ofono_modem structure by copying all the data
 *
 * @param m Structure to be duplicated
 *
 * @return Newly allocated copy of the modem. Must be freed with
 * #ofono_modem_free
 */
modem *
modem_dup(const modem *m)
{
  modem *rv = modem_new(m->path, m->powered);

  rv->emergency_call = m->emergency_call;
  rv->online = m->online;
  rv->interfaces = m->interfaces;
  rv->imei = g_strdup(m->imei);

  rv->sim.present = m->sim.present;
  rv->sim.imsi = g_strdup(m->sim.imsi);
  rv->sim.spn = g_strdup(m->sim.spn);

  rv->net.registered = m->net.registered;
  rv->net.roaming = m->net.roaming;
  rv->net.name = g_strdup(m->net.name);

  return rv;
}

/**
 * @brief Creates new empty list of #ofono_modem structures
 *
 * #return The list
 */
GHashTable *
modem_list_create(void)
{
  return g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                               (GDestroyNotify)modem_free);
}

/**
 * @brief Appends modem in the list of modems. If modem with the same object
 * path already exists, the function will abort.
 *
 * @param modems Poniter to list to replace or add the modem in
 * @param modem Modem to be appended
 */
void
modem_list_add(GHashTable *modems, const modem *modem)
{
  g_assert(!g_hash_table_contains(modems, modem->path));

  g_hash_table_insert(modems, g_strdup(modem->path), modem_dup(modem));
}

modem *
modem_list_find(GHashTable *modems, const gchar *path)
{
  if (modems)
    return g_hash_table_lookup(modems, (gconstpointer)path);
  else
    return NULL;
}

/**
 * @brief remove modem from the list of modems. If there is no modem with
 * matching object, it does nothing.
 *
 * @param modems List to remove the modem from
 * @param path Modem object path to be removed
 */
void
modem_list_remove(GHashTable *modems, const gchar *path)
{
  g_hash_table_remove(modems, path);
}

/**
 * @brief Frees #ofono_modem structure and its data
 *
 * @param modem structure to free
 */
void
modem_free(modem *modem)
{
  g_free(modem->path);
  g_free(modem->imei);
  g_free(modem->sim.imsi);
  g_free(modem->sim.spn);
  g_free(modem->net.name);
  g_free(modem);
}

/**
  * @brief Frees a list of #ofono_modem structure, freeing all the data as well.
  *
  * @param modems Pointer to list to be freed.
  */
void
modem_list_free(GHashTable *modems)
{
  g_hash_table_remove_all(modems);
}

void
modem_add_interface(modem *modem, guint64 interface)
{
  modem->interfaces |= interface;
}

void
modem_remove_interface(modem *modem, guint64 interface)
{
  modem->interfaces &= ~interface;
}

gboolean
modem_interface_supported(modem *modem, guint64 interface)
{
  return modem->interfaces & interface;
}
