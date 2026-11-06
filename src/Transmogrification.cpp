#include "DatabaseEnv.h"
#include "ItemTemplate.h"
#include "SpellMgr.h"
#include "Tokenize.h"
#include "Transmogrification.h"
#include "TransmogPrice.h"
#include "TransmogAddon.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

Transmogrification* Transmogrification::instance()
{
    static Transmogrification instance;
    return &instance;
}

#ifdef PRESETS
void Transmogrification::PresetTransmog(Player* player, Item* itemTransmogrified, uint32 fakeEntry, uint8 slot)
{
    if (EnableSets && player && itemTransmogrified)
        Transmogrify(player, fakeEntry == HIDDEN_ITEM_ID ? UINT_MAX : fakeEntry, slot, true);
}

void Transmogrification::LoadPlayerSets(ObjectGuid pGUID)
{
    LOG_DEBUG("module", "Transmogrification::LoadPlayerSets");

    for (presetData::iterator it = presetById[pGUID].begin(); it != presetById[pGUID].end(); ++it)
        it->second.clear();

    presetById[pGUID].clear();

    presetByName[pGUID].clear();

    QueryResult result = CharacterDatabase.Query("SELECT `PresetID`, `SetName`, `SetData` FROM `custom_transmogrification_sets` WHERE Owner = {}", pGUID.GetCounter());
    if (result)
    {
        do
        {
            uint8 PresetID = (*result)[0].Get<uint8>();
            std::string SetName = (*result)[1].Get<std::string>();
            std::istringstream SetData((*result)[2].Get<std::string>());
            while (SetData.good())
            {
                uint32 slot;
                uint32 entry;
                SetData >> slot >> entry;
                if (SetData.fail())
                    break;
                if (slot >= EQUIPMENT_SLOT_END)
                {
                    LOG_ERROR("module", "Item entry (FakeEntry: {}, player: {}, slot: {}, presetId: {}) has invalid slot, ignoring.", entry, pGUID.ToString(), slot, PresetID);
                    continue;
                }
                if (entry == HIDDEN_ITEM_ID || sObjectMgr->GetItemTemplate(entry))
                    presetById[pGUID][PresetID][slot] = entry; // Transmogrification::Preset(presetName, fakeEntry);
            }

            if (!presetById[pGUID][PresetID].empty())
            {
                presetByName[pGUID][PresetID] = SetName;
                // load all presets anyways
                //if (presetByName[pGUID].size() >= GetMaxSets())
                //    break;
            }
            else // should be deleted on startup, so  this never runs (shouldnt..)
            {
                presetById[pGUID].erase(PresetID);
                CharacterDatabase.Execute("DELETE FROM `custom_transmogrification_sets` WHERE Owner = {} AND PresetID = {}", pGUID.GetCounter(), PresetID);
            }
        } while (result->NextRow());
    }
}

bool Transmogrification::GetEnableSets() const
{
    return EnableSets;
}
uint8 Transmogrification::GetMaxSets() const
{
    return MaxSets;
}
float Transmogrification::GetSetCostModifier() const
{
    return SetCostModifier;
}
int32 Transmogrification::GetSetCopperCost() const
{
    return SetCopperCost;
}

void Transmogrification::UnloadPlayerSets(ObjectGuid pGUID)
{
    for (presetData::iterator it = presetById[pGUID].begin(); it != presetById[pGUID].end(); ++it)
        it->second.clear();
    presetById[pGUID].clear();

    presetByName[pGUID].clear();
}
#endif

const char* Transmogrification::GetSlotName(uint8 slot, WorldSession* session) const
{
    LOG_DEBUG("module", "Transmogrification::GetSlotName");

    LocaleConstant locale = session->GetSessionDbLocaleIndex();

    static std::unordered_map<uint8, char const*> const defaultNames = {
        { EQUIPMENT_SLOT_HEAD, "Head" }, // session->GetAcoreString(LANG_SLOT_NAME_HEAD);
        { EQUIPMENT_SLOT_SHOULDERS, "Shoulders" }, // session->GetAcoreString(LANG_SLOT_NAME_SHOULDERS);
        { EQUIPMENT_SLOT_BODY, "Shirt" }, // session->GetAcoreString(LANG_SLOT_NAME_BODY);
        { EQUIPMENT_SLOT_CHEST, "Chest" }, // session->GetAcoreString(LANG_SLOT_NAME_CHEST);
        { EQUIPMENT_SLOT_WAIST, "Waist" }, // session->GetAcoreString(LANG_SLOT_NAME_WAIST);
        { EQUIPMENT_SLOT_LEGS, "Legs" }, // session->GetAcoreString(LANG_SLOT_NAME_LEGS);
        { EQUIPMENT_SLOT_FEET, "Feet" }, // session->GetAcoreString(LANG_SLOT_NAME_FEET);
        { EQUIPMENT_SLOT_WRISTS, "Wrists" }, // session->GetAcoreString(LANG_SLOT_NAME_WRISTS);
        { EQUIPMENT_SLOT_HANDS, "Hands" }, // session->GetAcoreString(LANG_SLOT_NAME_HANDS);
        { EQUIPMENT_SLOT_BACK, "Back" }, // session->GetAcoreString(LANG_SLOT_NAME_BACK);
        { EQUIPMENT_SLOT_MAINHAND, "Main Hand" }, // session->GetAcoreString(LANG_SLOT_NAME_MAINHAND);
        { EQUIPMENT_SLOT_OFFHAND, "Off Hand" }, // session->GetAcoreString(LANG_SLOT_NAME_OFFHAND);
        { EQUIPMENT_SLOT_RANGED, "Ranged" }, // session->GetAcoreString(LANG_SLOT_NAME_RANGED);
        { EQUIPMENT_SLOT_TABARD, "Tabard" }, // session->GetAcoreString(LANG_SLOT_NAME_TABARD);
    };

    static std::unordered_map<uint8, std::unordered_map<LocaleConstant, char const*>> const slotNames = {
        { EQUIPMENT_SLOT_HEAD, {
            { LOCALE_koKR, "머리" },
            { LOCALE_frFR, "Tête" },
            { LOCALE_deDE, "Kopf" },
            { LOCALE_zhCN, "头部" },
            { LOCALE_zhTW, "頭部" },
            { LOCALE_esES, "Cabeza" },
            { LOCALE_esMX, "Cabeza" },
            { LOCALE_ruRU, "Голова" }
        } },
        { EQUIPMENT_SLOT_SHOULDERS, {
            { LOCALE_koKR, "어깨" },
            { LOCALE_frFR, "Épaules" },
            { LOCALE_deDE, "Schultern" },
            { LOCALE_zhCN, "肩部" },
            { LOCALE_zhTW, "肩部" },
            { LOCALE_esES, "Hombros" },
            { LOCALE_esMX, "Hombros" },
            { LOCALE_ruRU, "Плечи" }
        } },
        { EQUIPMENT_SLOT_BODY, {
            { LOCALE_koKR, "셔츠" },
            { LOCALE_frFR, "Chemise" },
            { LOCALE_deDE, "Hemd" },
            { LOCALE_zhCN, "衬衫" },
            { LOCALE_zhTW, "襯衫" },
            { LOCALE_esES, "Camisa" },
            { LOCALE_esMX, "Camisa" },
            { LOCALE_ruRU, "Рубашка" }
        } },
        { EQUIPMENT_SLOT_CHEST, {
            { LOCALE_koKR, "가슴" },
            { LOCALE_frFR, "Torse" },
            { LOCALE_deDE, "Brust" },
            { LOCALE_zhCN, "胸部" },
            { LOCALE_zhTW, "胸部" },
            { LOCALE_esES, "Pecho" },
            { LOCALE_esMX, "Pecho" },
            { LOCALE_ruRU, "Грудь" }
        } },
        { EQUIPMENT_SLOT_WAIST, {
            { LOCALE_koKR, "허리" },
            { LOCALE_frFR, "Taille" },
            { LOCALE_deDE, "Taille" },
            { LOCALE_zhCN, "腰部" },
            { LOCALE_zhTW, "腰部" },
            { LOCALE_esES, "Cintura" },
            { LOCALE_esMX, "Cintura" },
            { LOCALE_ruRU, "Пояс" }
        } },
        { EQUIPMENT_SLOT_LEGS, {
            { LOCALE_koKR, "다리" },
            { LOCALE_frFR, "Jambes" },
            { LOCALE_deDE, "Beine" },
            { LOCALE_zhCN, "腿部" },
            { LOCALE_zhTW, "腿部" },
            { LOCALE_esES, "Piernas" },
            { LOCALE_esMX, "Piernas" },
            { LOCALE_ruRU, "Ноги" }
        } },
        { EQUIPMENT_SLOT_FEET, {
            { LOCALE_koKR, "발" },
            { LOCALE_frFR, "Pieds" },
            { LOCALE_deDE, "Füße" },
            { LOCALE_zhCN, "脚" },
            { LOCALE_zhTW, "腳" },
            { LOCALE_esES, "Pies" },
            { LOCALE_esMX, "Pies" },
            { LOCALE_ruRU, "Ступни" }
        } },
        { EQUIPMENT_SLOT_WRISTS, {
            { LOCALE_koKR, "손목" },
            { LOCALE_frFR, "Poignets" },
            { LOCALE_deDE, "Handgelenke" },
            { LOCALE_zhCN, "腕部" },
            { LOCALE_zhTW, "手腕" },
            { LOCALE_esES, "Muñecas" },
            { LOCALE_esMX, "Muñecas" },
            { LOCALE_ruRU, "Запястья" }
        } },
        { EQUIPMENT_SLOT_HANDS, {
            { LOCALE_koKR, "손" },
            { LOCALE_frFR, "Mains" },
            { LOCALE_deDE, "Hände" },
            { LOCALE_zhCN, "手" },
            { LOCALE_zhTW, "手" },
            { LOCALE_esES, "Manos" },
            { LOCALE_esMX, "Manos" },
            { LOCALE_ruRU, "Кисти рук" }
        } },
        { EQUIPMENT_SLOT_BACK, {
            { LOCALE_koKR, "등" },
            { LOCALE_frFR, "Dos" },
            { LOCALE_deDE, "Rücken" },
            { LOCALE_zhCN, "背部" },
            { LOCALE_zhTW, "背部" },
            { LOCALE_esES, "Espalda" },
            { LOCALE_esMX, "Espalda" },
            { LOCALE_ruRU, "Спина" }
        } },
        { EQUIPMENT_SLOT_MAINHAND, {
            { LOCALE_koKR, "주장비" },
            { LOCALE_frFR, "Main droite" },
            { LOCALE_deDE, "Haupthand" },
            { LOCALE_zhCN, "主手" },
            { LOCALE_zhTW, "主手" },
            { LOCALE_esES, "Mano derecha" },
            { LOCALE_esMX, "Mano derecha" },
            { LOCALE_ruRU, "Правая рука" }
        } },
        { EQUIPMENT_SLOT_OFFHAND, {
            { LOCALE_koKR, "보조장비" },
            { LOCALE_frFR, "Main gauche" },
            { LOCALE_deDE, "Nebenhand" },
            { LOCALE_zhCN, "副手" },
            { LOCALE_zhTW, "副手" },
            { LOCALE_esES, "Mano izquierda" },
            { LOCALE_esMX, "Mano izquierda" },
            { LOCALE_ruRU, "Левая рука" }
        } },
        { EQUIPMENT_SLOT_RANGED, {
            { LOCALE_koKR, "원거리" },
            { LOCALE_frFR, "À distance" },
            { LOCALE_deDE, "Distanz" },
            { LOCALE_zhCN, "远程" },
            { LOCALE_zhTW, "遠程" },
            { LOCALE_esES, "A distancia" },
            { LOCALE_esMX, "A distancia" },
            { LOCALE_ruRU, "Дальний бой" }
        } },
        { EQUIPMENT_SLOT_TABARD, {
            { LOCALE_koKR, "휘장" },
            { LOCALE_frFR, "Tabard" },
            { LOCALE_deDE, "Wappenrock" },
            { LOCALE_zhCN, "战袍" },
            { LOCALE_zhTW, "戰袍" },
            { LOCALE_esES, "Tabardo" },
            { LOCALE_esMX, "Tabardo" },
            { LOCALE_ruRU, "Гербовая накидка" }
        } },
    };

    auto it = slotNames.find(slot);
    if (it != slotNames.end()) {
        auto& namesByLocale = it->second;
        auto nameIt = namesByLocale.find(locale);
        if (nameIt != namesByLocale.end()) {
            return nameIt->second;
        }
    }

    auto defaultIt = defaultNames.find(slot);
    if (defaultIt != defaultNames.end()) {
        return defaultIt->second;
    }

    return NULL;
}

std::string Transmogrification::GetItemIcon(uint32 entry, uint32 width, uint32 height, int x, int y) const
{
    LOG_DEBUG("module", "Transmogrification::GetItemIcon");

    std::ostringstream ss;
    ss << "|TInterface";
    const ItemTemplate* temp = sObjectMgr->GetItemTemplate(entry);
    const ItemDisplayInfoEntry* dispInfo = NULL;
    if (temp)
    {
        dispInfo = sItemDisplayInfoStore.LookupEntry(temp->DisplayInfoID);
        if (dispInfo)
            ss << "/ICONS/" << dispInfo->inventoryIcon;
    }
    if (!dispInfo)
        ss << "/InventoryItems/WoWUnknownItem01";
    ss << ":" << width << ":" << height << ":" << x << ":" << y << "|t";
    return ss.str();
}

std::string Transmogrification::GetSlotIcon(uint8 slot, uint32 width, uint32 height, int x, int y) const
{
    LOG_DEBUG("module", "Transmogrification::GetSlotIcon");

    std::ostringstream ss;
    ss << "|TInterface/PaperDoll/";
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD: ss << "UI-PaperDoll-Slot-Head"; break;
        case EQUIPMENT_SLOT_SHOULDERS: ss << "UI-PaperDoll-Slot-Shoulder"; break;
        case EQUIPMENT_SLOT_BODY: ss << "UI-PaperDoll-Slot-Shirt"; break;
        case EQUIPMENT_SLOT_CHEST: ss << "UI-PaperDoll-Slot-Chest"; break;
        case EQUIPMENT_SLOT_WAIST: ss << "UI-PaperDoll-Slot-Waist"; break;
        case EQUIPMENT_SLOT_LEGS: ss << "UI-PaperDoll-Slot-Legs"; break;
        case EQUIPMENT_SLOT_FEET: ss << "UI-PaperDoll-Slot-Feet"; break;
        case EQUIPMENT_SLOT_WRISTS: ss << "UI-PaperDoll-Slot-Wrists"; break;
        case EQUIPMENT_SLOT_HANDS: ss << "UI-PaperDoll-Slot-Hands"; break;
        case EQUIPMENT_SLOT_BACK: ss << "UI-PaperDoll-Slot-Chest"; break;
        case EQUIPMENT_SLOT_MAINHAND: ss << "UI-PaperDoll-Slot-MainHand"; break;
        case EQUIPMENT_SLOT_OFFHAND: ss << "UI-PaperDoll-Slot-SecondaryHand"; break;
        case EQUIPMENT_SLOT_RANGED: ss << "UI-PaperDoll-Slot-Ranged"; break;
        case EQUIPMENT_SLOT_TABARD: ss << "UI-PaperDoll-Slot-Tabard"; break;
        default: ss << "UI-Backpack-EmptySlot";
    }
    ss << ":" << width << ":" << height << ":" << x << ":" << y << "|t";
    return ss.str();
}

std::string Transmogrification::GetItemLink(Item* item, WorldSession* session) const
{
    LOG_DEBUG("module", "Transmogrification::GetItemLink");

    int loc_idx = session->GetSessionDbLocaleIndex();
    const ItemTemplate* temp = item->GetTemplate();
    std::string name = temp->Name1;
    if (ItemLocale const* il = sObjectMgr->GetItemLocale(temp->ItemId))
        ObjectMgr::GetLocaleString(il->Name, loc_idx, name);

    if (int32 itemRandPropId = item->GetItemRandomPropertyId())
    {
        std::array<char const*, 16> const* suffix = nullptr;
        if (itemRandPropId < 0)
        {
            if (const ItemRandomSuffixEntry* itemRandEntry = sItemRandomSuffixStore.LookupEntry(-itemRandPropId))
                suffix = &itemRandEntry->Name;
        }
        else
        {
            if (const ItemRandomPropertiesEntry* itemRandEntry = sItemRandomPropertiesStore.LookupEntry(itemRandPropId))
                suffix = &itemRandEntry->Name;
        }
        if (suffix)
        {
            std::string_view test((*suffix)[(name != temp->Name1) ? loc_idx : DEFAULT_LOCALE]);
            if (!test.empty())
            {
                name += ' ';
                name += test;
            }
        }
    }

    std::ostringstream oss;
    oss << "|c" << std::hex << ItemQualityColors[temp->Quality] << std::dec <<
        "|Hitem:" << temp->ItemId << ":" <<
        item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT) << ":" <<
        item->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT) << ":" <<
        item->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_2) << ":" <<
        item->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_3) << ":" <<
        item->GetEnchantmentId(BONUS_ENCHANTMENT_SLOT) << ":" <<
        item->GetItemRandomPropertyId() << ":" << item->GetItemSuffixFactor() << ":" <<
//        (uint32)item->GetOwner()->GetLevel() << "|h[" << name << "]|h|r";
        (uint32)0 << "|h[" << name << "]|h|r";

    return oss.str();
}

std::string Transmogrification::GetItemLink(uint32 entry, WorldSession* session) const
{
    LOG_DEBUG("module", "Transmogrification::GetItemLink");

    if (entry == HIDDEN_ITEM_ID)
    {
        std::ostringstream oss;
        oss << "(Hidden)";

        return oss.str();
    }
    const ItemTemplate* temp = sObjectMgr->GetItemTemplate(entry);
    int loc_idx = session->GetSessionDbLocaleIndex();
    std::string name = temp->Name1;
    if (ItemLocale const* il = sObjectMgr->GetItemLocale(entry))
        ObjectMgr::GetLocaleString(il->Name, loc_idx, name);

    std::ostringstream oss;
    oss << "|c" << std::hex << ItemQualityColors[temp->Quality] << std::dec <<
        "|Hitem:" << entry << ":0:0:0:0:0:0:0:0:0|h[" << name << "]|h|r";

    return oss.str();
}

uint32 Transmogrification::GetFakeEntry(ObjectGuid itemGUID) const
{
    LOG_DEBUG("module", "Transmogrification::GetFakeEntry");

    transmogData::const_iterator itr = dataMap.find(itemGUID);
    if (itr == dataMap.end()) return 0;
    transmogMap::const_iterator itr2 = entryMap.find(itr->second);
    if (itr2 == entryMap.end()) return 0;
    transmog2Data::const_iterator itr3 = itr2->second.find(itemGUID);
    if (itr3 == itr2->second.end()) return 0;
    return itr3->second;
}

void Transmogrification::UpdateItem(Player* player, Item* item) const
{
    LOG_DEBUG("module", "Transmogrification::UpdateItem");

    if (item->IsEquipped())
    {
        player->SetVisibleItemSlot(item->GetSlot(), item);
        if (player->IsInWorld())
            item->SendUpdateToPlayer(player);
    }
}

void Transmogrification::DeleteFakeEntry(Player* player, uint8 /*slot*/, Item* itemTransmogrified, CharacterDatabaseTransaction* trans /*= nullptr*/)
{
    //if (!GetFakeEntry(item))
    //    return false;
    DeleteFakeFromDB(itemTransmogrified->GetGUID().GetCounter(), trans);
    UpdateItem(player, itemTransmogrified);
}

void Transmogrification::SetFakeEntry(Player* player, uint32 newEntry, uint8 /*slot*/, Item* itemTransmogrified, CharacterDatabaseTransaction* trans /*= nullptr*/)
{
    ObjectGuid itemGUID = itemTransmogrified->GetGUID();
    entryMap[player->GetGUID()][itemGUID] = newEntry;
    dataMap[itemGUID] = player->GetGUID();
    if (trans)
        (*trans)->Append("REPLACE INTO custom_transmogrification (GUID, FakeEntry, Owner) VALUES ({}, {}, {})", itemGUID.GetCounter(), newEntry, player->GetGUID().GetCounter());
    else
        CharacterDatabase.Execute("REPLACE INTO custom_transmogrification (GUID, FakeEntry, Owner) VALUES ({}, {}, {})", itemGUID.GetCounter(), newEntry, player->GetGUID().GetCounter());
    UpdateItem(player, itemTransmogrified);
}

bool Transmogrification::AddCollectedAppearance(uint32 accountId, uint32 itemId)
{
    if (!collectionCache.contains(accountId))
    {
        collectionCache.insert({ accountId, {itemId} });
        return true;
    }

    auto res = collectionCache[accountId].insert(itemId);
    bool inserted = res.second;
    return inserted;
}

void Transmogrification::AddToDatabase(Player* player, ItemTemplate const* itemTemplate, CharacterDatabaseTransaction* trans)
{
    if (!player || !itemTemplate)
        return;
    if (!GetTrackUnusableItems() && !SuitableForTransmogrification(player, itemTemplate))
        return;
    if (itemTemplate->Class != ITEM_CLASS_ARMOR && itemTemplate->Class != ITEM_CLASS_WEAPON)
        return;
    uint32 itemId = itemTemplate->ItemId;
    WorldSession* session = player->GetSession();
    uint32 accountId = session->GetAccountId();
    std::string itemName = itemTemplate->Name1;

    int locIndex = session->GetSessionDbLocaleIndex();
    if (ItemLocale const* il = sObjectMgr->GetItemLocale(itemId))
        ObjectMgr::GetLocaleString(il->Name, locIndex, itemName);

    std::stringstream tempStream;
    tempStream << std::hex << ItemQualityColors[itemTemplate->Quality];
    std::string itemQuality = tempStream.str();
    bool showChatMessage = !(player->GetPlayerSetting("mod-transmog", SETTING_HIDE_TRANSMOG).value) && !CanNeverTransmog(itemTemplate);
    if (AddCollectedAppearance(accountId, itemId))
    {
        if (showChatMessage)
            ChatHandler(session).PSendSysMessage(R"(|c{}|Hitem:{}:0:0:0:0:0:0:0:0|h[{}]|h|r {})", itemQuality, itemId, itemName, *session->GetModuleString("mod-transmog", LANG_TRANSMOG_ADDED_APPEARANCE));

        if (trans)
            (*trans)->Append("INSERT IGNORE INTO custom_unlocked_appearances (account_id, item_template_id) VALUES ({}, {})", accountId, itemId);
        else
            CharacterDatabase.Execute("INSERT IGNORE INTO custom_unlocked_appearances (account_id, item_template_id) VALUES ({}, {})",
                accountId, itemId);
    }
}

ClaimAppearanceResult Transmogrification::ClaimAppearance(Player* player, Item* item, ClaimAppearanceMode mode, CharacterDatabaseTransaction* trans)
{
    if (!player || !item || !player->GetSession())
        return ClaimAppearanceResult::Unsuitable;

    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
        return ClaimAppearanceResult::Unsuitable;

    uint32 accountId = player->GetSession()->GetAccountId();
    auto accountCollection = collectionCache.find(accountId);
    if (accountCollection != collectionCache.end() && accountCollection->second.contains(itemTemplate->ItemId))
        return ClaimAppearanceResult::AlreadyOwned;

    bool isAutomatic = mode == ClaimAppearanceMode::Automatic;
    if ((itemTemplate->Class != ITEM_CLASS_ARMOR && itemTemplate->Class != ITEM_CLASS_WEAPON) ||
        ((!isAutomatic || !GetTrackUnusableItems()) && !SuitableForTransmogrification(player, itemTemplate)))
        return ClaimAppearanceResult::Unsuitable;
    if (isAutomatic && item->IsRefundable())
        return ClaimAppearanceResult::Refundable;
    if (isAutomatic && item->IsBOPTradable())
        return ClaimAppearanceResult::Tradeable;

    auto transaction = trans ? *trans : CharacterDatabase.BeginTransaction();
    bool shouldBind = mode == ClaimAppearanceMode::Explicit || GetAutoCollectBindItems();
    if (shouldBind)
    {
        item->SetOwnerGUID(player->GetGUID());
        item->SetNotRefundable(player, true, &transaction);
        if (!GetAllowTradeable())
            item->ClearSoulboundTradeable(player);
        item->SetBinding(true);
        item->SetState(ITEM_CHANGED, player);
    }

    AddToDatabase(player, itemTemplate, &transaction);
    if (!trans)
    {
        player->SaveInventoryAndGoldToDB(transaction);
        CharacterDatabase.CommitTransaction(transaction);
    }
    return ClaimAppearanceResult::Claimed;
}

Item* Transmogrification::FindAppearanceSource(Player* player, uint32 entry) const
{
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            if (item->GetEntry() == entry)
                return item;
    for (uint8 pos = INVENTORY_SLOT_BAG_START; pos < INVENTORY_SLOT_BAG_END; ++pos)
        if (Bag* bag = player->GetBagByPos(pos))
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                if (Item* item = player->GetItemByPos(pos, slot))
                    if (item->GetEntry() == entry)
                        return item;
    return nullptr;
}

bool Transmogrification::HasAppearance(Player* player, uint32 entry) const
{
    if (entry == HIDDEN_ITEM_ID)
        return GetAllowHiddenTransmog();
    if (!GetUseCollectionSystem())
        return FindAppearanceSource(player, entry) != nullptr;
    auto collection = collectionCache.find(player->GetSession()->GetAccountId());
    return collection != collectionCache.end() && collection->second.contains(entry);
}

uint32 Transmogrification::GetPrice(ItemTemplate const* target) const
{
    return target ? TransmogCost(GetSpecialPrice(target), ScaledCostModifier, CopperCost) : 0;
}

uint32 Transmogrification::GetSetPrice(slotMap const& entries) const
{
    uint64 base = 0;
    for (auto const& [slot, entry] : entries)
        if (entry != HIDDEN_ITEM_ID)
            if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry))
                base += GetSpecialPrice(proto);
    return TransmogCost(base, SetCostModifier, SetCopperCost);
}

TransmogStrings Transmogrification::Transmogrify(Player* player, uint32 itemEntry, uint8 slot, bool no_cost)
{
    if (itemEntry == UINT_MAX)
        itemEntry = HIDDEN_ITEM_ID;
    if (!HasAppearance(player, itemEntry))
        return LANG_TRANSMOG_MISSING_SRC_ITEM;
    Item* source = !GetUseCollectionSystem() && itemEntry != HIDDEN_ITEM_ID ? FindAppearanceSource(player, itemEntry) : nullptr;
    return ApplyAppearance(player, itemEntry, slot, no_cost, source);
}

TransmogStrings Transmogrification::Transmogrify(Player* player, ObjectGuid itemGUID, uint8 slot, bool no_cost)
{
    Item* source = itemGUID ? player->GetItemByGuid(itemGUID) : nullptr;
    if (itemGUID && !source)
        return LANG_TRANSMOG_MISSING_SRC_ITEM;
    return Transmogrify(player, source, slot, no_cost);
}

TransmogStrings Transmogrification::Transmogrify(Player* player, Item* source, uint8 slot, bool no_cost, bool hidden_transmog)
{
    return ApplyAppearance(player, hidden_transmog ? HIDDEN_ITEM_ID : (source ? source->GetEntry() : 0), slot, no_cost, source);
}

TransmogStrings Transmogrification::ApplyAppearance(Player* player, uint32 entry, uint8 slot, bool no_cost, Item* source)
{
    if (!IsEnabled() || slot >= EQUIPMENT_SLOT_END)
        return LANG_TRANSMOG_INVALID_SLOT;
    Item* target = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!target)
        return LANG_TRANSMOG_MISSING_DEST_ITEM;
    bool hidden = entry == HIDDEN_ITEM_ID;
    if (hidden && (!AllowHiddenTransmog || !GetSlotName(slot, player->GetSession())))
        return LANG_TRANSMOG_INVALID_ITEMS;
    if (source && source->GetOwnerGUID() != player->GetGUID())
        return LANG_TRANSMOG_MISSING_SRC_ITEM;
    if (entry > HIDDEN_ITEM_ID &&
        !CanTransmogrifyItemWithItem(player, target->GetTemplate(), sObjectMgr->GetItemTemplate(entry)))
        return LANG_TRANSMOG_INVALID_ITEMS;
    if (GetFakeEntry(target->GetGUID()) == entry)
        return LANG_TRANSMOG_OK;
    uint32 cost = (!no_cost && (entry > HIDDEN_ITEM_ID || (hidden && !HiddenTransmogIsFree))) ? GetPrice(target->GetTemplate()) : 0;
    bool payTokens = entry > HIDDEN_ITEM_ID && !no_cost && RequireToken;
    if (!player->HasEnoughMoney(cost))
        return LANG_TRANSMOG_NOT_ENOUGH_MONEY;
    if (payTokens && !player->HasItemCount(TokenEntry, TokenAmount))
        return LANG_TRANSMOG_NOT_ENOUGH_TOKENS;
    // Token configuration may name an equipped/source item. Do not retain its pointer across destruction.
    if (payTokens && (target->GetEntry() == TokenEntry || (source && source->GetEntry() == TokenEntry)))
        return LANG_TRANSMOG_INVALID_ITEMS;
    if (payTokens)
        player->DestroyItemCount(TokenEntry, TokenAmount, true);
    player->ModifyMoney(-static_cast<int32>(cost), false);
    auto transaction = CharacterDatabase.BeginTransaction();
    if (entry)
    {
        SetFakeEntry(player, entry, slot, target, &transaction);
        target->UpdatePlayedTime(player);
        target->SetNotRefundable(player, true, &transaction);
        target->ClearSoulboundTradeable(player);
    }
    else
        DeleteFakeEntry(player, slot, target, &transaction);
    if (source)
    {
        if (source->GetTemplate()->Bonding == BIND_WHEN_EQUIPPED || source->GetTemplate()->Bonding == BIND_WHEN_USE)
            source->SetBinding(true);
        source->SetNotRefundable(player, true, &transaction);
        source->ClearSoulboundTradeable(player);
        source->SetState(ITEM_CHANGED, player);
    }
    player->SaveInventoryAndGoldToDB(transaction);
    CharacterDatabase.CommitTransaction(transaction);
    return LANG_TRANSMOG_OK;
}

bool Transmogrification::CanTransmogrifyItemWithItem(Player* player, ItemTemplate const* target, ItemTemplate const* source) const
{

    if (!target || !source)
        return false;

    if (source->ItemId == target->ItemId)
        return false;

    if (source->DisplayInfoID == target->DisplayInfoID)
        return false;

    if (source->Class != target->Class)
        return false;

    if (source->InventoryType == INVTYPE_BAG ||
        source->InventoryType == INVTYPE_RELIC ||
        // source->InventoryType == INVTYPE_BODY ||
        source->InventoryType == INVTYPE_FINGER ||
        source->InventoryType == INVTYPE_TRINKET ||
        source->InventoryType == INVTYPE_AMMO ||
        source->InventoryType == INVTYPE_QUIVER)
        return false;

    if (target->InventoryType == INVTYPE_BAG ||
        target->InventoryType == INVTYPE_RELIC ||
        // target->InventoryType == INVTYPE_BODY ||
        target->InventoryType == INVTYPE_FINGER ||
        target->InventoryType == INVTYPE_TRINKET ||
        target->InventoryType == INVTYPE_AMMO ||
        target->InventoryType == INVTYPE_QUIVER)
        return false;

    if (!SuitableForTransmogrification(player, target) || !SuitableForTransmogrification(player, source))
        return false;

    if (IsRangedWeapon(source->Class, source->SubClass) != IsRangedWeapon(target->Class, target->SubClass))
        return false;

    if (source->SubClass != target->SubClass && !IsSubclassMismatchAllowed(player, source, target))
        return false;

    if (source->InventoryType != target->InventoryType && !IsInvTypeMismatchAllowed(source, target))
        return false;

    return true;
}

bool Transmogrification::IsSubclassMismatchAllowed(Player *player, const ItemTemplate *source, const ItemTemplate *target) const
{
    if (IsAllowed(source->ItemId)) return true;

    uint32 sourceType  = source->InventoryType;
    uint32 targetType  = target->InventoryType;
    uint32 sourceClass = source->Class;
    uint32 targetClass = target->Class;
    uint32 sourceSub   = source->SubClass;
    uint32 targetSub   = target->SubClass;

    if (targetClass == ITEM_CLASS_WEAPON)
    {
        if (IsRangedWeapon(sourceClass, sourceSub))
            return true;

        if (AllowMixedWeaponTypes == MIXED_WEAPONS_MODERN)
        {
            switch (targetSub)
            {
                case ITEM_SUBCLASS_WEAPON_AXE:
                case ITEM_SUBCLASS_WEAPON_SWORD:
                case ITEM_SUBCLASS_WEAPON_MACE:
                    if (sourceSub == ITEM_SUBCLASS_WEAPON_AXE   ||
                        sourceSub == ITEM_SUBCLASS_WEAPON_SWORD ||
                        sourceSub == ITEM_SUBCLASS_WEAPON_MACE   )
                        return true;
                    break;
                case ITEM_SUBCLASS_WEAPON_AXE2:
                case ITEM_SUBCLASS_WEAPON_SWORD2:
                case ITEM_SUBCLASS_WEAPON_MACE2:
                case ITEM_SUBCLASS_WEAPON_STAFF:
                case ITEM_SUBCLASS_WEAPON_POLEARM:
                    if (sourceSub == ITEM_SUBCLASS_WEAPON_AXE2   ||
                        sourceSub == ITEM_SUBCLASS_WEAPON_SWORD2 ||
                        sourceSub == ITEM_SUBCLASS_WEAPON_MACE2  ||
                        sourceSub == ITEM_SUBCLASS_WEAPON_STAFF  ||
                        sourceSub == ITEM_SUBCLASS_WEAPON_POLEARM )
                        return true;
                    break;
            }
        }
        else if (AllowMixedWeaponTypes == MIXED_WEAPONS_LOOSE)
        {
            return true;
        }
        if (sourceSub == ITEM_SUBCLASS_WEAPON_MISC)
            return sourceType == targetType;
    }
    else if (targetClass == ITEM_CLASS_ARMOR)
    {
        if (AllowMixedArmorTypes)
            return true;
        if (AllowLowerTiers && IsTieredArmorSubclass(targetSub) && TierAvailable(player, 0, sourceSub))
            return true;
        if (AllowMixedOffhandArmorTypes && IsValidOffhandArmor(targetSub, targetType) && IsValidOffhandArmor(sourceSub, sourceType))
            return true;
        if (sourceSub == ITEM_SUBCLASS_ARMOR_MISC)
            return sourceType == targetType;
    }

    return false;
}

bool Transmogrification::IsInvTypeMismatchAllowed(const ItemTemplate *source, const ItemTemplate *target) const
{
    uint32 sourceType  = source->InventoryType;
    uint32 targetType  = target->InventoryType;
    uint32 sourceClass = source->Class;
    uint32 targetClass = target->Class;
    uint32 sourceSub   = source->SubClass;
    uint32 targetSub   = target->SubClass;

    if (targetClass == ITEM_CLASS_WEAPON)
    {
        if (IsRangedWeapon(sourceClass, sourceSub))
            return true;

        // Main-hand to offhand restrictions - see https://wowpedia.fandom.com/wiki/Transmogrification
        if (AllowMixedWeaponTypes == MIXED_WEAPONS_LOOSE)
            return true;
        else if (targetType == INVTYPE_WEAPONMAINHAND || targetType == INVTYPE_WEAPONOFFHAND)
        {
            if (sourceType == INVTYPE_WEAPONMAINHAND || sourceType == INVTYPE_WEAPONOFFHAND)
                return AllowMixedWeaponHandedness;
            if (sourceType == INVTYPE_WEAPON)
                return true;
        }
        else if (targetType == INVTYPE_WEAPON)
        {
            return sourceType == INVTYPE_WEAPONMAINHAND || (AllowMixedWeaponHandedness && sourceType == INVTYPE_WEAPONOFFHAND);
        }
    }
    else if (targetClass == ITEM_CLASS_ARMOR)
    {
        if (AllowMixedOffhandArmorTypes && IsValidOffhandArmor(targetSub, targetType) && IsValidOffhandArmor(sourceSub, sourceType))
            return true;
        if (targetType == INVTYPE_CHEST || targetType == INVTYPE_ROBE)
            return sourceType == INVTYPE_CHEST || sourceType == INVTYPE_ROBE;
    }

    return false;
}

bool Transmogrification::IsValidOffhandArmor(uint32 subclass, uint32 invType) const
{
    return subclass == ITEM_SUBCLASS_ARMOR_BUCKLER || (subclass == ITEM_SUBCLASS_ARMOR_MISC && invType == INVTYPE_HOLDABLE) || subclass == ITEM_SUBCLASS_ARMOR_SHIELD;
}

bool Transmogrification::IsTieredArmorSubclass(uint32 subclass) const
{
    return subclass == ITEM_SUBCLASS_ARMOR_PLATE || subclass == ITEM_SUBCLASS_ARMOR_MAIL || subclass == ITEM_SUBCLASS_ARMOR_LEATHER || subclass == ITEM_SUBCLASS_ARMOR_CLOTH;
}

bool Transmogrification::SuitableForTransmogrification(Player* player, ItemTemplate const* proto) const
{
    // ItemTemplate const* proto = item->GetTemplate();
    if (!player || !proto)
        return false;

    if (proto->Class != ITEM_CLASS_ARMOR &&
        proto->Class != ITEM_CLASS_WEAPON)
        return false;

    // Skip all checks for allowed items
    if (IsAllowed(proto->ItemId))
        return true;

    if (!IsItemTransmogrifiable(proto, player->GetGUID()))
        return false;

    //[AZTH] Yehonal
    uint32 subclassSkill = proto->GetSkill();
    if (proto->SubClass > 0 && subclassSkill && player->GetSkillValue(proto->GetSkill()) == 0)
    {
        if (proto->Class == ITEM_CLASS_ARMOR && !AllowMixedArmorTypes)
        {
            return false;
        }

        if (proto->Class == ITEM_CLASS_WEAPON && AllowMixedWeaponTypes != MIXED_WEAPONS_LOOSE)
        {
            return false;
        }
    }

    if (proto->HasFlag2(ITEM_FLAG2_FACTION_HORDE) && player->GetTeamId() != TEAM_HORDE)
        return false;

    if (proto->HasFlag2(ITEM_FLAG2_FACTION_ALLIANCE) && player->GetTeamId() != TEAM_ALLIANCE)
        return false;

    if (!IgnoreReqClass && (proto->AllowableClass & player->getClassMask()) == 0)
        return false;

    if (!IgnoreReqRace && (proto->AllowableRace & player->getRaceMask()) == 0)
        return false;

    if (!IgnoreReqSkill && proto->RequiredSkill != 0)
    {
        if (player->GetSkillValue(proto->RequiredSkill) == 0)
            return false;

        if (player->GetSkillValue(proto->RequiredSkill) < proto->RequiredSkillRank)
            return false;
    }

    if (!IgnoreLevelRequirement(player->GetGUID()) && player->GetLevel() < proto->RequiredLevel)
        return false;

    if (AllowLowerTiers && TierAvailable(player, 0, proto->SubClass))
        return true;

    if (!IgnoreReqSpell && proto->RequiredSpell != 0 && !player->HasSpell(proto->RequiredSpell))
        return false;

    return true;
}

bool Transmogrification::SuitableForTransmogrification(ObjectGuid guid, ItemTemplate const* proto) const
{
    if (!guid || !proto)
        return false;

    if (proto->Class != ITEM_CLASS_ARMOR &&
        proto->Class != ITEM_CLASS_WEAPON)
        return false;

    // Skip all checks for allowed items
    if (IsAllowed(proto->ItemId))
        return true;

    if (!IsItemTransmogrifiable(proto, guid))
        return false;

    auto playerGuid = guid.GetCounter();
    CharacterCacheEntry const* playerData = sCharacterCache->GetCharacterCacheByGuid(guid);
    if (!playerData)
        return false;

    uint8 playerRace = playerData->Race;
    uint8 playerLevel = playerData->Level;
    uint32 playerRaceMask = 1 << (playerRace - 1);
    uint32 playerClassMask = 1 << (playerData->Class - 1);
    TeamId playerTeamId = Player::TeamIdForRace(playerRace);

    std::unordered_map<uint32, uint32> playerSkillValues;
    if (QueryResult resultSkills = CharacterDatabase.Query("SELECT `skill`, `value` FROM `character_skills` WHERE `guid` = {}", playerGuid))
    {
        do
        {
            Field* fields = resultSkills->Fetch();
            uint16 skill = fields[0].Get<uint16>();
            uint16 value = fields[1].Get<uint16>();
            playerSkillValues[skill] = value;
        } while (resultSkills->NextRow());
    }
    else {
        LOG_ERROR("module", "Transmogification could not find skills for player with guid {} in database.", playerGuid);
        return false;
    }

    if (proto->SubClass > 0 && playerSkillValues[proto->GetSkill()] == 0)
    {
        if (proto->Class == ITEM_CLASS_ARMOR && !AllowMixedArmorTypes)
        {
            return false;
        }

        if (proto->Class == ITEM_CLASS_WEAPON && !AllowMixedWeaponTypes)
        {
            return false;
        }
    }

    if (proto->HasFlag2(ITEM_FLAG2_FACTION_HORDE) && playerTeamId != TEAM_HORDE)
        return false;

    if (proto->HasFlag2(ITEM_FLAG2_FACTION_ALLIANCE) && playerTeamId != TEAM_ALLIANCE)
        return false;

    if (!IgnoreReqClass && (proto->AllowableClass & playerClassMask) == 0)
        return false;

    if (!IgnoreReqRace && (proto->AllowableRace & playerRaceMask) == 0)
        return false;

    if (!IgnoreReqSkill && proto->RequiredSkill != 0)
    {
        if (playerSkillValues[proto->RequiredSkill] == 0)
            return false;

        if (playerSkillValues[proto->RequiredSkill] < proto->RequiredSkillRank)
            return false;
    }

    if (!IgnoreLevelRequirement(guid) && playerLevel < proto->RequiredLevel)
        return false;

    if (AllowLowerTiers && TierAvailable(NULL, playerGuid, proto->SubClass))
        return true;

    if (!IgnoreReqSpell && proto->RequiredSpell != 0 && !(CharacterDatabase.Query("SELECT `spell` FROM `character_spell` WHERE `guid` = {} and `spell` = {}", playerGuid, proto->RequiredSpell)))
        return false;

    return true;
}



bool Transmogrification::TierAvailable(Player *player, int playerGuid, uint32 tier) const
{
    if (!player && !playerGuid) return false;
    if (!IsTieredArmorSubclass(tier)) return false;

    uint32 playerHighest = ITEM_SUBCLASS_ARMOR_CLOTH;
    if (player)
        playerHighest = GetHighestAvailableForPlayer(player);
    else if (playerGuid)
        playerHighest = GetHighestAvailableForPlayer(playerGuid);

    switch (playerHighest)
    {
    case ITEM_SUBCLASS_ARMOR_PLATE:
        return true;
    case ITEM_SUBCLASS_ARMOR_MAIL:
        return tier != ITEM_SUBCLASS_ARMOR_PLATE;
    case ITEM_SUBCLASS_ARMOR_LEATHER:
        return tier == ITEM_SUBCLASS_ARMOR_LEATHER || tier == ITEM_SUBCLASS_ARMOR_CLOTH;
    case ITEM_SUBCLASS_ARMOR_CLOTH:
        return tier == ITEM_SUBCLASS_ARMOR_CLOTH;
    }

    return true;
}

uint32 Transmogrification::GetHighestAvailableForPlayer(int playerGuid) const
{
    for (int i = 0; i < 4; i++)
    {
        if (CharacterDatabase.Query("SELECT `spell` FROM `character_spell` WHERE `guid` = {} and `spell` = {}", playerGuid, AllArmorSpellIds[i]))
            return AllArmorTiers[i];
    }

    return ITEM_SUBCLASS_ARMOR_CLOTH;
}

uint32 Transmogrification::GetHighestAvailableForPlayer(Player *player) const
{
    for (int i = 0; i < 4; i++)
    {
        if (player->HasSpell(AllArmorSpellIds[i]))
            return AllArmorTiers[i];
    }

    return ITEM_SUBCLASS_ARMOR_CLOTH;
}

bool Transmogrification::IsItemTransmogrifiable(ItemTemplate const* proto, ObjectGuid const &playerGuid) const
{
    if (!proto)
        return false;

    if (IsNotAllowed(proto->ItemId))
        return false;

    if (!AllowFishingPoles && proto->Class == ITEM_CLASS_WEAPON && proto->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE)
        return false;

    if (!IsAllowedQuality(proto->Quality, playerGuid)) // (proto->Quality == ITEM_QUALITY_LEGENDARY)
        return false;

    // If World Event is not active, prevent using event dependant items
    if (!IgnoreReqEvent && proto->HolidayId && !IsHolidayActive((HolidayIds)proto->HolidayId))
        return false;

    if (!IgnoreReqStats)
    {
        if (!proto->RandomProperty && !proto->RandomSuffix
            /*[AZTH] Yehonal: we should transmorg also items without stats*/
            && proto->StatsCount > 0)
        {
            bool found = false;
            for (uint8 i = 0; i < proto->StatsCount; ++i)
            {
                if (proto->ItemStat[i].ItemStatValue != 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        }
    }

    return true;
}

uint32 Transmogrification::GetSpecialPrice(ItemTemplate const* proto) const
{
    uint32 cost = proto->SellPrice < 10000 ? 10000 : proto->SellPrice;
    return cost;
}
bool Transmogrification::IsRangedWeapon(uint32 Class, uint32 SubClass) const
{
    return Class == ITEM_CLASS_WEAPON && (
        SubClass == ITEM_SUBCLASS_WEAPON_BOW ||
        SubClass == ITEM_SUBCLASS_WEAPON_GUN ||
        SubClass == ITEM_SUBCLASS_WEAPON_CROSSBOW);
}

bool Transmogrification::IsAllowed(uint32 entry) const
{
    return Allowed.find(entry) != Allowed.end();
}

bool Transmogrification::IsNotAllowed(uint32 entry) const
{
    return NotAllowed.find(entry) != NotAllowed.end();
}

bool Transmogrification::IsAllowedQuality(uint32 quality, ObjectGuid const &playerGuid) const
{
    switch (quality)
    {
        case ITEM_QUALITY_POOR: return AllowPoor || IsPlusFeatureEligible(playerGuid, PLUS_FEATURE_GREY_ITEMS);
        case ITEM_QUALITY_NORMAL: return AllowCommon || IsPlusFeatureEligible(playerGuid, PLUS_FEATURE_GREY_ITEMS);
        case ITEM_QUALITY_UNCOMMON: return AllowUncommon;
        case ITEM_QUALITY_RARE: return AllowRare;
        case ITEM_QUALITY_EPIC: return AllowEpic;
        case ITEM_QUALITY_LEGENDARY: return AllowLegendary || IsPlusFeatureEligible(playerGuid, PLUS_FEATURE_LEGENDARY_ITEMS);
        case ITEM_QUALITY_ARTIFACT: return AllowArtifact;
        case ITEM_QUALITY_HEIRLOOM: return AllowHeirloom;
        default: return false;
    }
}

bool Transmogrification::CanNeverTransmog(ItemTemplate const* itemTemplate)
{
    return (itemTemplate->InventoryType == INVTYPE_BAG ||
        itemTemplate->InventoryType == INVTYPE_RELIC ||
        itemTemplate->InventoryType == INVTYPE_FINGER ||
        itemTemplate->InventoryType == INVTYPE_TRINKET ||
        itemTemplate->InventoryType == INVTYPE_AMMO ||
        itemTemplate->InventoryType == INVTYPE_QUIVER);
}

void Transmogrification::LoadConfig(bool reload)
{
#ifdef PRESETS
    EnableSetInfo = sConfigMgr->GetOption<bool>("Transmogrification.EnableSetInfo", true);
    SetNpcText = sConfigMgr->GetOption<uint32>("Transmogrification.SetNpcText", 601084);

    EnableSets = sConfigMgr->GetOption<bool>("Transmogrification.EnableSets", true);
    MaxSets = sConfigMgr->GetOption<uint8>("Transmogrification.MaxSets", 10);
    SetCostModifier = sConfigMgr->GetOption<float>("Transmogrification.SetCostModifier", 3.0f);
    SetCopperCost = sConfigMgr->GetOption<int32>("Transmogrification.SetCopperCost", 0);

    if (MaxSets > MAX_OPTIONS)
        MaxSets = MAX_OPTIONS;

    if (reload) // dont store presets for nothing
    {
        auto const& sessions = sWorldSessionMgr->GetAllSessions();
        for (auto const& session : sessions)
        {
            if (Player* player = session.second->GetPlayer())
            {
                // skipping session check
                UnloadPlayerSets(player->GetGUID());
                if (GetEnableSets())
                    LoadPlayerSets(player->GetGUID());
            }
        }
    }
#endif

    EnableTransmogInfo = sConfigMgr->GetOption<bool>("Transmogrification.EnableTransmogInfo", true);
    TransmogNpcText = uint32(sConfigMgr->GetOption<uint32>("Transmogrification.TransmogNpcText", 601083));

    Allowed.clear();
    NotAllowed.clear();
    TransmogAddon::OnConfigReload();
    std::istringstream issAllowed(sConfigMgr->GetOption<std::string>("Transmogrification.Allowed", ""));
    std::istringstream issNotAllowed(sConfigMgr->GetOption<std::string>("Transmogrification.NotAllowed", ""));
    while (issAllowed.good())
    {
        uint32 entry;
        issAllowed >> entry;
        if (issAllowed.fail())
            break;
        Allowed.insert(entry);
    }
    while (issNotAllowed.good())
    {
        uint32 entry;
        issNotAllowed >> entry;
        if (issNotAllowed.fail())
            break;
        NotAllowed.insert(entry);
    }

    ScaledCostModifier = sConfigMgr->GetOption<float>("Transmogrification.ScaledCostModifier", 1.0f);
    CopperCost = sConfigMgr->GetOption<int32>("Transmogrification.CopperCost", 0);

    RequireToken = sConfigMgr->GetOption<bool>("Transmogrification.RequireToken", false);
    TokenEntry = sConfigMgr->GetOption<uint32>("Transmogrification.TokenEntry", 49426);
    TokenAmount = sConfigMgr->GetOption<uint32>("Transmogrification.TokenAmount", 1);

    AllowPoor = sConfigMgr->GetOption<bool>("Transmogrification.AllowPoor", false);
    AllowCommon = sConfigMgr->GetOption<bool>("Transmogrification.AllowCommon", false);
    AllowUncommon = sConfigMgr->GetOption<bool>("Transmogrification.AllowUncommon", true);
    AllowRare = sConfigMgr->GetOption<bool>("Transmogrification.AllowRare", true);
    AllowEpic = sConfigMgr->GetOption<bool>("Transmogrification.AllowEpic", true);
    AllowLegendary = sConfigMgr->GetOption<bool>("Transmogrification.AllowLegendary", false);
    AllowArtifact = sConfigMgr->GetOption<bool>("Transmogrification.AllowArtifact", false);
    AllowHeirloom = sConfigMgr->GetOption<bool>("Transmogrification.AllowHeirloom", true);
    AllowTradeable = sConfigMgr->GetOption<bool>("Transmogrification.AllowTradeable", false);

    AllowMixedArmorTypes = sConfigMgr->GetOption<bool>("Transmogrification.AllowMixedArmorTypes", false);
    AllowLowerTiers = sConfigMgr->GetOption<bool>("Transmogrification.AllowLowerTiers", false);
    AllowMixedOffhandArmorTypes = sConfigMgr->GetOption<bool>("Transmogrification.AllowMixedOffhandArmorTypes", false);
    AllowMixedWeaponHandedness = sConfigMgr->GetOption<bool>("Transmogrification.AllowMixedWeaponHandedness", false);
    AllowFishingPoles = sConfigMgr->GetOption<bool>("Transmogrification.AllowFishingPoles", false);

    AllowMixedWeaponTypes = sConfigMgr->GetOption<uint8>("Transmogrification.AllowMixedWeaponTypes", MIXED_WEAPONS_STRICT);

    IgnoreReqRace = sConfigMgr->GetOption<bool>("Transmogrification.IgnoreReqRace", false);
    IgnoreReqClass = sConfigMgr->GetOption<bool>("Transmogrification.IgnoreReqClass", false);
    IgnoreReqSkill = sConfigMgr->GetOption<bool>("Transmogrification.IgnoreReqSkill", false);
    IgnoreReqSpell = sConfigMgr->GetOption<bool>("Transmogrification.IgnoreReqSpell", false);
    IgnoreReqLevel = sConfigMgr->GetOption<bool>("Transmogrification.IgnoreReqLevel", false);
    IgnoreReqEvent = sConfigMgr->GetOption<bool>("Transmogrification.IgnoreReqEvent", false);
    IgnoreReqStats = sConfigMgr->GetOption<bool>("Transmogrification.IgnoreReqStats", false);
    UseCollectionSystem = sConfigMgr->GetOption<bool>("Transmogrification.UseCollectionSystem", true);
    AutoCollectInventoryAppearances = sConfigMgr->GetOption<bool>("Transmogrification.AutoCollectInventoryAppearances", true);
    AutoCollectBindItems = sConfigMgr->GetOption<bool>("Transmogrification.AutoCollectBindItems", true);
    UseVendorInterface = sConfigMgr->GetOption<bool>("Transmogrification.UseVendorInterface", false);
    AllowHiddenTransmog = sConfigMgr->GetOption<bool>("Transmogrification.AllowHiddenTransmog", true);
    HiddenTransmogIsFree = sConfigMgr->GetOption<bool>("Transmogrification.HiddenTransmogIsFree", true);
    TrackUnusableItems = sConfigMgr->GetOption<bool>("Transmogrification.TrackUnusableItems", true);
    RetroActiveAppearances = sConfigMgr->GetOption<bool>("Transmogrification.RetroActiveAppearances", true);
    ResetRetroActiveAppearances = sConfigMgr->GetOption<bool>("Transmogrification.ResetRetroActiveAppearancesFlag", false);

    IsTransmogEnabled = sConfigMgr->GetOption<bool>("Transmogrification.Enable", true);
    IsPortableNPCEnabled = sConfigMgr->GetOption<bool>("Transmogrification.EnablePortable", true);
    ShowSetDisclaimer = sConfigMgr->GetOption<bool>("Transmogrification.ShowSetDisclaimer", false);

    if (!sObjectMgr->GetItemTemplate(TokenEntry))
    {
        TokenEntry = 49426;
    }

    /* TransmogPlus */
    IsTransmogPlusEnabled = sConfigMgr->GetOption<bool>("Transmogrification.EnablePlus", false);

    plusDataMap.clear();

    std::string stringMembershipIds = sConfigMgr->GetOption<std::string>("Transmogrification.MembershipLevels", "");
    for (auto& itr : Acore::Tokenize(stringMembershipIds, ',', false))
    {
        plusDataMap[PLUS_FEATURE_GREY_ITEMS].push_back(Acore::StringTo<uint32>(itr).value());
    }

    stringMembershipIds = sConfigMgr->GetOption<std::string>("Transmogrification.MembershipLevelsLegendary", "");
    for (auto& itr : Acore::Tokenize(stringMembershipIds, ',', false))
    {
        plusDataMap[PLUS_FEATURE_LEGENDARY_ITEMS].push_back(Acore::StringTo<uint32>(itr).value());
    }

    stringMembershipIds = sConfigMgr->GetOption<std::string>("Transmogrification.MembershipLevelsPet", "");
    for (auto& itr : Acore::Tokenize(stringMembershipIds, ',', false))
    {
        plusDataMap[PLUS_FEATURE_PET].push_back(Acore::StringTo<uint32>(itr).value());
    }

    stringMembershipIds = sConfigMgr->GetOption<std::string>("Transmogrification.MembershipLevelsSkipLevelReq", "");
    for (auto& itr : Acore::Tokenize(stringMembershipIds, ',', false))
    {
        plusDataMap[PLUS_FEATURE_SKIP_LEVEL_REQ].push_back(Acore::StringTo<uint32>(itr).value());
    }

    PetSpellId = sConfigMgr->GetOption<uint32>("Transmogrification.PetSpellId", 2000100);

    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(PetSpellId))
        PetEntry = spellInfo->Effects[EFFECT_0].MiscValue;
}

void Transmogrification::DeleteFakeFromDB(ObjectGuid::LowType itemLowGuid, CharacterDatabaseTransaction* trans /*= nullptr*/)
{
    ObjectGuid itemGUID = ObjectGuid::Create<HighGuid::Item>(itemLowGuid);

    if (dataMap.find(itemGUID) != dataMap.end())
    {
        if (entryMap.find(dataMap[itemGUID]) != entryMap.end())
            entryMap[dataMap[itemGUID]].erase(itemGUID);
        dataMap.erase(itemGUID);
    }
    if (trans)
        (*trans)->Append("DELETE FROM custom_transmogrification WHERE GUID = {}", itemLowGuid);
    else
        CharacterDatabase.Execute("DELETE FROM custom_transmogrification WHERE GUID = {}", itemGUID.GetCounter());
}

bool Transmogrification::IsPlusFeatureEligible(ObjectGuid const &playerGuid, uint32 feature) const
{
    if (!IsTransmogPlusEnabled)
        return false;

    auto it = plusDataMap.find(feature);
    if (it == plusDataMap.end() || it->second.empty())
        return false;

    Player* player = ObjectAccessor::FindConnectedPlayer(playerGuid);

    if (!player)
        return false;

    if (player->IsGameMaster())
        return true; // GM can use all features

    const auto membershipLevel = GetPlayerMembershipLevel(player);

    if (!membershipLevel)
        return false;

    const auto& membershipLevels = it->second;
    for (const auto& level : membershipLevels)
    {
        if (level == membershipLevel)
            return true;
    }

    return false;
}

void Transmogrification::LoadCollections()
{
    if (sTransmogrification->GetUseCollectionSystem())
    {
        LOG_INFO("module", "Loading transmog appearance collection cache....");
        uint32 collectedAppearanceCount = 0;
        QueryResult result = CharacterDatabase.Query("SELECT account_id, item_template_id FROM custom_unlocked_appearances");
        if (result)
        {
            do
            {
                uint32 accountId = (*result)[0].Get<uint32>();
                uint32 itemId = (*result)[1].Get<uint32>();
                if (sTransmogrification->AddCollectedAppearance(accountId, itemId))
                    collectedAppearanceCount++;

            } while (result->NextRow());
        }

        LOG_INFO("module", "Loaded {} collected appearances into cache", collectedAppearanceCount);
    }
}

bool Transmogrification::GetEnableTransmogInfo() const
{
    return EnableTransmogInfo;
}
uint32 Transmogrification::GetTransmogNpcText() const
{
    return TransmogNpcText;
}
bool Transmogrification::GetEnableSetInfo() const
{
    return EnableSetInfo;
}
uint32 Transmogrification::GetSetNpcText() const
{
    return SetNpcText;
}
float Transmogrification::GetScaledCostModifier() const
{
    return ScaledCostModifier;
}
int32 Transmogrification::GetCopperCost() const
{
    return CopperCost;
}
bool Transmogrification::GetRequireToken() const
{
    return RequireToken;
}
uint32 Transmogrification::GetTokenEntry() const
{
    return TokenEntry;
}
uint32 Transmogrification::GetTokenAmount() const
{
    return TokenAmount;
}
bool Transmogrification::GetAllowMixedArmorTypes() const
{
    return AllowMixedArmorTypes;
};
bool Transmogrification::GetAllowLowerTiers() const
{
    return AllowLowerTiers;
};
bool Transmogrification::GetAllowMixedOffhandArmorTypes() const
{
    return AllowMixedOffhandArmorTypes;
};
uint8 Transmogrification::GetAllowMixedWeaponTypes() const
{
    return AllowMixedWeaponTypes;
};
bool Transmogrification::GetUseCollectionSystem() const
{
    return UseCollectionSystem;
};
bool Transmogrification::GetAutoCollectInventoryAppearances() const
{
    return AutoCollectInventoryAppearances;
}
bool Transmogrification::GetAutoCollectBindItems() const
{
    return AutoCollectBindItems;
}
bool Transmogrification::GetUseVendorInterface() const
{
    return UseVendorInterface;
}
bool Transmogrification::GetAllowHiddenTransmog() const
{
    return AllowHiddenTransmog;
}
bool Transmogrification::GetHiddenTransmogIsFree() const
{
    return HiddenTransmogIsFree;
}
bool Transmogrification::GetAllowTradeable() const
{
    return AllowTradeable;
}

bool Transmogrification::GetTrackUnusableItems() const
{
    return TrackUnusableItems;
}

bool Transmogrification::EnableRetroActiveAppearances() const
{
    return RetroActiveAppearances;
}

bool Transmogrification::EnableResetRetroActiveAppearances() const
{
    return ResetRetroActiveAppearances;
}

bool Transmogrification::IsEnabled() const
{
    return IsTransmogEnabled;
};
