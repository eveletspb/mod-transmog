#ifndef MOD_TRANSMOG_ADDON_H
#define MOD_TRANSMOG_ADDON_H

#include <string>

class Creature;
class Player;

namespace TransmogAddon
{
bool TryOpen(Player* player, Creature* creature);
bool HandleMessage(Player* player, std::string const& message);
void OnLogout(Player* player);
}

#endif
