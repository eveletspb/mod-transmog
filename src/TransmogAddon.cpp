#include "TransmogAddon.h"

#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "Transmogrification.h"
#include "Util.h"
#include "WorldPacket.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
constexpr std::string_view AddonPrefix = "AC_TRANSMOG";
constexpr uint32 ProtocolVersion = 3;
constexpr uint32 MinListIntervalMs = 150;
constexpr uint32 MinOperationIntervalMs = 500;
constexpr std::size_t ItemsPerChunk = 20;

struct ClientState
{
    bool compatible = false;
    uint32 sessionId = 0;
    ObjectGuid npcGuid;
    uint64 expiresAtMs = 0;
    uint64 lastListAtMs = 0;
    uint64 lastOperationAtMs = 0;
    std::unordered_set<uint32> completedOperations;
};

std::unordered_map<ObjectGuid, ClientState> Clients;
uint32 NextSessionId = 0;

bool IsEnabled()
{
    return sConfigMgr->GetOption<bool>("Transmogrification.Addon.Enabled", true);
}

uint32 GetPageSize()
{
    return std::clamp(sConfigMgr->GetOption<uint32>("Transmogrification.Addon.PageSize", 60), 1U, 60U);
}

uint64 GetSessionTtlMs()
{
    uint32 ttlSeconds = std::clamp(sConfigMgr->GetOption<uint32>("Transmogrification.Addon.SessionTtlSeconds", 60), 10U, 300U);
    return uint64(ttlSeconds) * IN_MILLISECONDS;
}

uint64 NowMs()
{
    return GameTime::GetGameTimeMS().count();
}

void Send(Player* player, std::string const& payload)
{
    if (!player || !player->GetSession())
        return;

    std::string message;
    message.reserve(AddonPrefix.size() + payload.size() + 1);
    message.append(AddonPrefix);
    message.push_back('\t');
    message.append(payload);

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
    player->GetSession()->SendPacket(&data);
}

std::vector<std::string> Split(std::string_view value)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size())
    {
        std::size_t end = value.find('\t', start);
        if (end == std::string_view::npos)
            end = value.size();
        parts.emplace_back(value.substr(start, end - start));
        if (end == value.size())
            break;
        start = end + 1;
    }
    return parts;
}

template <typename T>
bool ParseUnsigned(std::string const& value, T& result)
{
    if (value.empty())
        return false;

    T parsed = 0;
    auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc() || end != value.data() + value.size())
        return false;
    result = parsed;
    return true;
}

uint32 GetPrice(ItemTemplate const* target)
{
    if (!target)
        return 0;

    double value = sTransmogrification->GetSpecialPrice(target) * sTransmogrification->GetScaledCostModifier();
    value += sTransmogrification->GetCopperCost();
    if (value <= 0)
        return 0;
    if (value >= std::numeric_limits<uint32>::max())
        return std::numeric_limits<uint32>::max();
    return static_cast<uint32>(value);
}

bool ValidateSession(Player* player, uint32 sessionId, ClientState*& state)
{
    if (!sTransmogrification->IsEnabled())
        return false;

    auto client = Clients.find(player->GetGUID());
    if (client == Clients.end() || !client->second.compatible || !client->second.sessionId || client->second.sessionId != sessionId)
        return false;

    uint64 now = NowMs();
    if (client->second.expiresAtMs < now)
        return false;

    Creature* creature = ObjectAccessor::GetCreatureOrPetOrVehicle(*player, client->second.npcGuid);
    if (!creature || !sTransmogrification->IsTransmogVendor(creature->GetEntry()) ||
        !creature->IsWithinDistInMap(player, INTERACTION_DISTANCE))
        return false;

    client->second.expiresAtMs = now + GetSessionTtlMs();
    state = &client->second;
    return true;
}

void SendError(Player* player, uint32 requestId, std::string_view error)
{
    Send(player, "RESULT\t" + std::to_string(requestId) + "\tERROR\t" + std::string(error));
}

void SendSlotStates(Player* player)
{
    std::ostringstream states;
    bool first = true;
    for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        uint32 fakeEntry = item ? sTransmogrification->GetFakeEntry(item->GetGUID()) : 0;
        if (!fakeEntry)
            continue;

        if (!first)
            states << ',';
        states << uint32(slot) << ':' << fakeEntry;
        first = false;
    }
    Send(player, "SLOTS\t" + states.str());
}

void HandleHello(Player* player, std::vector<std::string> const& parts)
{
    uint32 version = 0;
    if (parts.size() < 2 || !ParseUnsigned(parts[1], version) || version != ProtocolVersion || !IsEnabled())
    {
        Clients.erase(player->GetGUID());
        Send(player, "HELLO_ERROR\t" + std::to_string(ProtocolVersion));
        return;
    }

    ClientState& state = Clients[player->GetGUID()];
    state.compatible = true;
    Send(player, "HELLO_OK\t" + std::to_string(ProtocolVersion));
}

void HandleList(Player* player, std::vector<std::string> const& parts)
{
    uint32 requestId = 0;
    uint32 sessionId = 0;
    uint8 slot = 0;
    uint32 page = 0;
    if (parts.size() < 5 || !ParseUnsigned(parts[1], requestId) || !ParseUnsigned(parts[2], sessionId) ||
        !ParseUnsigned(parts[3], slot) || !ParseUnsigned(parts[4], page))
        return;

    ClientState* state = nullptr;
    if (!ValidateSession(player, sessionId, state))
    {
        SendError(player, requestId, "SESSION");
        return;
    }

    uint64 now = NowMs();
    if (state->lastListAtMs && now - state->lastListAtMs < MinListIntervalMs)
    {
        SendError(player, requestId, "THROTTLED");
        return;
    }
    state->lastListAtMs = now;

    if (slot >= EQUIPMENT_SLOT_END)
    {
        SendError(player, requestId, "SLOT");
        return;
    }

    Item* target = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!target)
    {
        SendError(player, requestId, "EMPTY_SLOT");
        return;
    }

    if (!sTransmogrification->GetUseCollectionSystem())
    {
        SendError(player, requestId, "COLLECTION_DISABLED");
        return;
    }

    std::wstring search;
    if (parts.size() > 5)
    {
        if (parts[5].size() > 100)
        {
            SendError(player, requestId, "SEARCH");
            return;
        }
        std::string const& searchUtf8 = parts[5];
        if (!Utf8toWStr(searchUtf8, search))
        {
            SendError(player, requestId, "SEARCH");
            return;
        }
        wstrToLower(search);
    }

    struct Appearance
    {
        ItemTemplate const* item;
        std::string name;
    };

    std::vector<Appearance> appearances;
    uint32 accountId = player->GetSession()->GetAccountId();
    auto collection = sTransmogrification->collectionCache.find(accountId);
    if (collection != sTransmogrification->collectionCache.end())
    {
        for (uint32 itemEntry : collection->second)
        {
            ItemTemplate const* appearance = sObjectMgr->GetItemTemplate(itemEntry);
            if (!appearance)
                continue;
            if (!sTransmogrification->CanTransmogrifyItemWithItem(player, target->GetTemplate(), appearance))
                continue;

            std::string localizedName = appearance->Name1;
            if (ItemLocale const* locale = sObjectMgr->GetItemLocale(itemEntry))
                ObjectMgr::GetLocaleString(locale->Name, player->GetSession()->GetSessionDbLocaleIndex(), localizedName);
            if (!search.empty() && !Utf8FitTo(localizedName, search))
                continue;
            appearances.push_back({ appearance, std::move(localizedName) });
        }
    }

    std::sort(appearances.begin(), appearances.end(), [](Appearance const& left, Appearance const& right)
    {
        if (left.item->Quality != right.item->Quality)
            return left.item->Quality > right.item->Quality;
        return left.name < right.name;
    });

    uint32 pageSize = GetPageSize();
    uint32 total = appearances.size();
    uint32 pageCount = std::max(1U, (total + pageSize - 1) / pageSize);
    if (page >= pageCount)
        page = pageCount - 1;

    uint32 current = sTransmogrification->GetFakeEntry(target->GetGUID());
    Send(player, "PAGE\t" + std::to_string(requestId) + "\t" + std::to_string(slot) + "\t" +
        std::to_string(page) + "\t" + std::to_string(pageCount) + "\t" + std::to_string(total) + "\t" +
        std::to_string(GetPrice(target->GetTemplate())) + "\t" + std::to_string(current) + "\t" +
        std::to_string(target->GetEntry()));

    std::size_t begin = std::size_t(page) * pageSize;
    std::size_t end = std::min(begin + pageSize, appearances.size());
    uint32 chunks = std::max(1U, static_cast<uint32>((end - begin + ItemsPerChunk - 1) / ItemsPerChunk));
    for (uint32 chunk = 0; chunk < chunks; ++chunk)
    {
        std::size_t chunkBegin = begin + chunk * ItemsPerChunk;
        std::size_t chunkEnd = std::min(chunkBegin + ItemsPerChunk, end);
        std::ostringstream entries;
        for (std::size_t index = chunkBegin; index < chunkEnd; ++index)
        {
            if (index != chunkBegin)
                entries << ',';
            entries << appearances[index].item->ItemId;
        }
        Send(player, "ITEMS\t" + std::to_string(requestId) + "\t" + std::to_string(chunk) + "\t" +
            std::to_string(chunks) + "\t" + entries.str());
    }
}

void HandleApply(Player* player, std::vector<std::string> const& parts)
{
    uint32 requestId = 0;
    uint32 sessionId = 0;
    uint8 slot = 0;
    uint32 itemEntry = 0;
    if (parts.size() < 5 || !ParseUnsigned(parts[1], requestId) || !ParseUnsigned(parts[2], sessionId) ||
        !ParseUnsigned(parts[3], slot) || !ParseUnsigned(parts[4], itemEntry))
        return;

    ClientState* state = nullptr;
    if (!ValidateSession(player, sessionId, state))
    {
        SendError(player, requestId, "SESSION");
        return;
    }

    uint64 now = NowMs();
    if (state->completedOperations.contains(requestId))
    {
        SendError(player, requestId, "DUPLICATE");
        return;
    }
    if (state->lastOperationAtMs && now - state->lastOperationAtMs < MinOperationIntervalMs)
    {
        SendError(player, requestId, "THROTTLED");
        return;
    }
    state->lastOperationAtMs = now;

    if (slot >= EQUIPMENT_SLOT_END || !player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
    {
        SendError(player, requestId, "SLOT");
        return;
    }

    uint32 accountId = player->GetSession()->GetAccountId();
    auto collection = sTransmogrification->collectionCache.find(accountId);
    if (!sTransmogrification->GetUseCollectionSystem() || collection == sTransmogrification->collectionCache.end() ||
        !collection->second.contains(itemEntry))
    {
        SendError(player, requestId, "NOT_COLLECTED");
        return;
    }

    TransmogStrings result = sTransmogrification->Transmogrify(player, itemEntry, slot);
    if (result != LANG_TRANSMOG_OK)
    {
        SendError(player, requestId, "TRANSMOG_" + std::to_string(static_cast<uint32>(result)));
        return;
    }

    state->completedOperations.insert(requestId);
    state->lastListAtMs = 0;
    Send(player, "RESULT\t" + std::to_string(requestId) + "\tOK\t" + std::to_string(slot) + "\t" +
        std::to_string(itemEntry) + "\t" + std::to_string(player->GetMoney()));
    SendSlotStates(player);
}

void HandleApplyOutfit(Player* player, std::vector<std::string> const& parts)
{
    uint32 requestId = 0;
    uint32 sessionId = 0;
    if (parts.size() < 4 || !ParseUnsigned(parts[1], requestId) || !ParseUnsigned(parts[2], sessionId) || parts[3].empty())
        return;

    ClientState* state = nullptr;
    if (!ValidateSession(player, sessionId, state))
    {
        SendError(player, requestId, "SESSION");
        return;
    }

    uint64 now = NowMs();
    if (state->completedOperations.contains(requestId))
    {
        SendError(player, requestId, "DUPLICATE");
        return;
    }
    if (state->lastOperationAtMs && now - state->lastOperationAtMs < MinOperationIntervalMs)
    {
        SendError(player, requestId, "THROTTLED");
        return;
    }
    state->lastOperationAtMs = now;

    struct OutfitChange
    {
        uint8 slot;
        uint32 itemEntry;
        Item* target;
    };

    std::vector<OutfitChange> changes;
    std::unordered_set<uint8> usedSlots;
    std::stringstream encodedChanges(parts[3]);
    std::string encodedChange;
    while (std::getline(encodedChanges, encodedChange, ','))
    {
        std::size_t separator = encodedChange.find(':');
        uint32 slot = 0;
        uint32 itemEntry = 0;
        if (separator == std::string::npos || encodedChange.find(':', separator + 1) != std::string::npos ||
            !ParseUnsigned(encodedChange.substr(0, separator), slot) ||
            !ParseUnsigned(encodedChange.substr(separator + 1), itemEntry) ||
            slot >= EQUIPMENT_SLOT_END || !itemEntry || !usedSlots.insert(static_cast<uint8>(slot)).second)
        {
            SendError(player, requestId, "SLOT");
            return;
        }
        changes.push_back({ static_cast<uint8>(slot), itemEntry, nullptr });
    }

    if (changes.empty() || changes.size() > EQUIPMENT_SLOT_END)
    {
        SendError(player, requestId, "SLOT");
        return;
    }

    uint32 accountId = player->GetSession()->GetAccountId();
    auto collection = sTransmogrification->collectionCache.find(accountId);
    if (!sTransmogrification->GetUseCollectionSystem())
    {
        SendError(player, requestId, "COLLECTION_DISABLED");
        return;
    }
    if (collection == sTransmogrification->collectionCache.end())
    {
        SendError(player, requestId, "NOT_COLLECTED");
        return;
    }

    uint64 totalCost = 0;
    std::vector<OutfitChange> preparedChanges;
    preparedChanges.reserve(changes.size());
    for (OutfitChange& change : changes)
    {
        change.target = player->GetItemByPos(INVENTORY_SLOT_BAG_0, change.slot);
        if (!change.target)
        {
            SendError(player, requestId, "EMPTY_SLOT");
            return;
        }

        ItemTemplate const* appearance = sObjectMgr->GetItemTemplate(change.itemEntry);
        if (!appearance || !collection->second.contains(change.itemEntry))
        {
            SendError(player, requestId, "NOT_COLLECTED");
            return;
        }
        if (!sTransmogrification->CanTransmogrifyItemWithItem(player, change.target->GetTemplate(), appearance))
        {
            SendError(player, requestId, "TRANSMOG_3");
            return;
        }

        if (sTransmogrification->GetFakeEntry(change.target->GetGUID()) == change.itemEntry)
            continue;

        totalCost += GetPrice(change.target->GetTemplate());
        preparedChanges.push_back(change);
    }

    uint64 requiredTokens = uint64(sTransmogrification->GetTokenAmount()) * preparedChanges.size();
    if (sTransmogrification->GetRequireToken() &&
        (requiredTokens > std::numeric_limits<uint32>::max() ||
         !player->HasItemCount(sTransmogrification->GetTokenEntry(), static_cast<uint32>(requiredTokens))))
    {
        SendError(player, requestId, "TRANSMOG_8");
        return;
    }
    if (player->GetMoney() < totalCost)
    {
        SendError(player, requestId, "TRANSMOG_7");
        return;
    }

    if (sTransmogrification->GetRequireToken() && requiredTokens)
        player->DestroyItemCount(sTransmogrification->GetTokenEntry(), static_cast<uint32>(requiredTokens), true);
    if (totalCost)
        player->ModifyMoney(-static_cast<int64>(totalCost), false);

    auto transaction = CharacterDatabase.BeginTransaction();
    for (OutfitChange const& change : preparedChanges)
    {
        sTransmogrification->SetFakeEntry(player, change.itemEntry, change.slot, change.target, &transaction);
        change.target->UpdatePlayedTime(player);
        change.target->SetOwnerGUID(player->GetGUID());
        change.target->SetNotRefundable(player);
        change.target->ClearSoulboundTradeable(player);
    }
    if (!preparedChanges.empty())
        CharacterDatabase.CommitTransaction(transaction);

    state->completedOperations.insert(requestId);
    state->lastListAtMs = 0;
    Send(player, "RESULT\t" + std::to_string(requestId) + "\tOK\tOUTFIT\t" +
        std::to_string(preparedChanges.size()) + "\t" + std::to_string(player->GetMoney()));
    SendSlotStates(player);
}

void HandleRemove(Player* player, std::vector<std::string> const& parts)
{
    uint32 requestId = 0;
    uint32 sessionId = 0;
    uint8 slot = 0;
    if (parts.size() < 4 || !ParseUnsigned(parts[1], requestId) || !ParseUnsigned(parts[2], sessionId) ||
        !ParseUnsigned(parts[3], slot))
        return;

    ClientState* state = nullptr;
    if (!ValidateSession(player, sessionId, state))
    {
        SendError(player, requestId, "SESSION");
        return;
    }

    uint64 now = NowMs();
    if (state->completedOperations.contains(requestId))
    {
        SendError(player, requestId, "DUPLICATE");
        return;
    }
    if (state->lastOperationAtMs && now - state->lastOperationAtMs < MinOperationIntervalMs)
    {
        SendError(player, requestId, "THROTTLED");
        return;
    }
    state->lastOperationAtMs = now;

    Item* target = slot < EQUIPMENT_SLOT_END ? player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot) : nullptr;
    if (!target)
    {
        SendError(player, requestId, "SLOT");
        return;
    }

    if (!sTransmogrification->GetFakeEntry(target->GetGUID()))
    {
        SendError(player, requestId, "NOT_TRANSMOGRIFIED");
        return;
    }

    sTransmogrification->DeleteFakeEntry(player, slot, target);
    state->completedOperations.insert(requestId);
    state->lastListAtMs = 0;
    Send(player, "RESULT\t" + std::to_string(requestId) + "\tOK\t" + std::to_string(slot) + "\t0\t" +
        std::to_string(player->GetMoney()));
    SendSlotStates(player);
}

void HandleRemoveAll(Player* player, std::vector<std::string> const& parts)
{
    uint32 requestId = 0;
    uint32 sessionId = 0;
    if (parts.size() < 3 || !ParseUnsigned(parts[1], requestId) || !ParseUnsigned(parts[2], sessionId))
        return;

    ClientState* state = nullptr;
    if (!ValidateSession(player, sessionId, state))
    {
        SendError(player, requestId, "SESSION");
        return;
    }

    uint64 now = NowMs();
    if (state->completedOperations.contains(requestId))
    {
        SendError(player, requestId, "DUPLICATE");
        return;
    }
    if (state->lastOperationAtMs && now - state->lastOperationAtMs < MinOperationIntervalMs)
    {
        SendError(player, requestId, "THROTTLED");
        return;
    }
    state->lastOperationAtMs = now;

    bool removed = false;
    auto transaction = CharacterDatabase.BeginTransaction();
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item || !sTransmogrification->GetFakeEntry(item->GetGUID()))
            continue;

        sTransmogrification->DeleteFakeEntry(player, slot, item, &transaction);
        removed = true;
    }

    if (removed)
        CharacterDatabase.CommitTransaction(transaction);

    state->completedOperations.insert(requestId);
    state->lastListAtMs = 0;
    Send(player, "RESULT\t" + std::to_string(requestId) + "\tOK\t255\t0\t" +
        std::to_string(player->GetMoney()));
    SendSlotStates(player);
}
}

namespace TransmogAddon
{
bool TryOpen(Player* player, Creature* creature)
{
    if (!player || !creature || !IsEnabled() || !sTransmogrification->IsEnabled() ||
        !sTransmogrification->GetUseCollectionSystem() ||
        !sConfigMgr->GetOption<bool>("Transmogrification.Addon.PreferInterface", true))
        return false;

    auto client = Clients.find(player->GetGUID());
    if (client == Clients.end() || !client->second.compatible)
        return false;

    ClientState& state = client->second;
    if (++NextSessionId == 0)
        ++NextSessionId;
    state.sessionId = NextSessionId;
    state.npcGuid = creature->GetGUID();
    state.expiresAtMs = NowMs() + GetSessionTtlMs();
    state.lastListAtMs = 0;
    state.lastOperationAtMs = 0;
    state.completedOperations.clear();
    Send(player, "OPEN\t" + std::to_string(state.sessionId));
    SendSlotStates(player);
    return true;
}

bool HandleMessage(Player* player, std::string const& message)
{
    if (!player || message.size() <= AddonPrefix.size() || message.compare(0, AddonPrefix.size(), AddonPrefix) != 0 ||
        message[AddonPrefix.size()] != '\t')
        return false;

    std::vector<std::string> parts = Split(std::string_view(message).substr(AddonPrefix.size() + 1));
    if (parts.empty())
        return true;

    if (parts[0] == "HELLO")
        HandleHello(player, parts);
    else if (parts[0] == "LIST" && IsEnabled())
        HandleList(player, parts);
    else if (parts[0] == "APPLY" && IsEnabled())
        HandleApply(player, parts);
    else if (parts[0] == "APPLY_OUTFIT" && IsEnabled())
        HandleApplyOutfit(player, parts);
    else if (parts[0] == "REMOVE" && IsEnabled())
        HandleRemove(player, parts);
    else if (parts[0] == "REMOVE_ALL" && IsEnabled())
        HandleRemoveAll(player, parts);
    else if (parts[0] == "CLOSE")
    {
        auto client = Clients.find(player->GetGUID());
        if (client != Clients.end())
            client->second.sessionId = 0;
    }

    return true;
}

void OnLogout(Player* player)
{
    if (player)
        Clients.erase(player->GetGUID());
}
}
