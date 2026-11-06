#!/usr/bin/env python3
"""Run module payment/claim/vendor regressions without building or starting worldserver.

Compile the actual function bodies against small in-memory Core substitutes.
Real Core API compatibility is checked separately with -fsyntax-only.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


protocol = (ROOT / "src/TransmogAddon.cpp").read_text()
source = (ROOT / "src/Transmogrification.cpp").read_text()
scripts = (ROOT / "src/transmog_scripts.cpp").read_text()
bodies = "\n\n".join(function(source, signature) for signature in (
    "Item* Transmogrification::FindAppearanceSource(",
    "bool Transmogrification::HasAppearance(",
    "uint32 Transmogrification::GetPrice(",
    "uint32 Transmogrification::GetSetPrice(",
    "TransmogStrings Transmogrification::Transmogrify(Player* player, uint32",
    "TransmogStrings Transmogrification::Transmogrify(Player* player, Item*",
    "ClaimAppearanceResult Transmogrification::ClaimAppearance(",
    "TransmogStrings Transmogrification::ApplyAppearance(",
))
vendor = function(scripts, "    void OnPlayerBeforeBuyItemFromVendor(").replace(" override", "")
prelude = r'''
#include "TransmogPrice.h"
#include <array>
#include <charconv>
#include <sstream>
#include <cassert>
#include <climits>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
using uint8 = uint8_t; using uint32 = uint32_t; using uint64 = uint64_t; using int32 = int32_t;
using ObjectGuid = int;
constexpr int EQUIPMENT_SLOT_START=0, EQUIPMENT_SLOT_END=19, INVENTORY_SLOT_BAG_0=255;
constexpr int BANK_SLOT_ITEM_START=39, BANK_SLOT_ITEM_END=67, BANK_SLOT_BAG_START=67, BANK_SLOT_BAG_END=74;
constexpr int INVENTORY_SLOT_ITEM_START=23, INVENTORY_SLOT_ITEM_END=39;
constexpr int INVENTORY_SLOT_BAG_START=19, INVENTORY_SLOT_BAG_END=23;
constexpr int HIDDEN_ITEM_ID=1, BIND_WHEN_EQUIPPED=2, BIND_WHEN_USE=3;
constexpr int ITEM_CLASS_ARMOR=4, ITEM_CLASS_WEAPON=2, ITEM_CHANGED=1, UNIT_NPC_FLAG_GOSSIP=1;
constexpr int SETTING_VENDOR_INTERFACE=2;
constexpr uint32 CUSTOM_HIDE_ITEM_VENDOR_ID=57575, FALLBACK_HIDE_ITEM_VENDOR_ID=9172;
constexpr uint32 CUSTOM_REMOVE_TMOG_VENDOR_ID=57576, FALLBACK_REMOVE_TMOG_VENDOR_ID=1049;
enum TransmogStrings { LANG_TRANSMOG_OK=1, LANG_TRANSMOG_INVALID_SLOT=2,
    LANG_TRANSMOG_MISSING_SRC_ITEM=4, LANG_TRANSMOG_MISSING_DEST_ITEM=5,
    LANG_TRANSMOG_INVALID_ITEMS=6, LANG_TRANSMOG_NOT_ENOUGH_MONEY=7, LANG_TRANSMOG_NOT_ENOUGH_TOKENS=8 };
enum class ClaimAppearanceMode { Automatic, Explicit };
enum class ClaimAppearanceResult { Claimed, AlreadyOwned, Unsuitable, Refundable, Tradeable };
struct Transaction { std::vector<std::string> writes; int money=0, tokens=0; bool bound=false; };
using CharacterDatabaseTransaction=std::shared_ptr<Transaction>;
struct Database {
    std::vector<CharacterDatabaseTransaction> committed;
    auto BeginTransaction() { return std::make_shared<Transaction>(); }
    void CommitTransaction(CharacterDatabaseTransaction t) { committed.push_back(t); }
} CharacterDatabase;
struct ItemTemplate { uint32 ItemId=1001, Bonding=BIND_WHEN_EQUIPPED, Class=ITEM_CLASS_WEAPON; };
struct Player;
struct Item {
    ItemTemplate proto; int guid=1, owner=1; bool bound=false, refundable=false, tradeable=false;
    auto GetTemplate() const { return &proto; }
    uint32 GetEntry() const { return proto.ItemId; }
    int GetGUID() const { return guid; } int GetOwnerGUID() const { return owner; }
    bool IsRefundable() const { return refundable; } bool IsBOPTradable() const { return tradeable; }
    void SetOwnerGUID(int value) { owner=value; }
    void SetBinding(bool value) { bound=value; }
    void SetState(int,Player*) { }
    void UpdatePlayedTime(Player*) { }
    void SetNotRefundable(Player*,bool=true,CharacterDatabaseTransaction* = nullptr) { refundable=false; }
    void ClearSoulboundTradeable(Player*) { tradeable=false; }
};
struct Bag { uint32 GetBagSize() const { return 0; } };
struct Creature { uint32 GetEntry() const { return 190010; } };
struct Map { Creature vendor; Creature* GetCreature(int) { return &vendor; } };
struct Session { uint32 GetAccountId() { return 1; } };
struct Setting { bool IsEnabled() const { return true; } };
struct DataMap {
    struct Base { } flag;
    bool pending=false;
    template<class T> T* GetDefault(char const*) { pending=true;return &flag; }
    bool Erase(char const*) { bool value=pending;pending=false;return value; }
};
struct Player {
    DataMap CustomData;
    Item target, source; Session session; Map map;
    bool holdsSource=false, canInteract=true; int money=20000, tokens=1, fake=0, itemReads=0;
    Player() { target.proto.ItemId=2001; source.guid=2; }
    Session* GetSession() { return &session; } int GetGUID() const { return 1; }
    Item* GetItemByPos(int bag,int slot) {
        ++itemReads;
        if(bag!=INVENTORY_SLOT_BAG_0) return nullptr;
        if(slot==0) return &target;
        return holdsSource && slot==23 ? &source : nullptr;
    }
    Bag* GetBagByPos(int) { return nullptr; }
    bool HasEnoughMoney(uint32 amount) const { return money>=int(amount); }
    bool HasItemCount(uint32,uint32 amount) const { return tokens>=int(amount); }
    void DestroyItemCount(uint32,uint32 amount,bool) { tokens-=amount; }
    void ModifyMoney(int32 amount,bool) { money+=amount; }
    void SaveInventoryAndGoldToDB(CharacterDatabaseTransaction t) {
        t->writes.push_back("inventory");t->money=money;t->tokens=tokens;t->bound=source.bound;
    }
    Map* GetMap() { return &map; }
    Creature* GetNPCIfCanInteractWith(int,int) { return canInteract ? &map.vendor : nullptr; }
    Setting GetPlayerSetting(char const*,int) { return {}; }
};
struct ObjectMgr { ItemTemplate proto; ItemTemplate const* GetItemTemplate(uint32 entry) { proto.ItemId=entry;return &proto; } } mgr;
auto sObjectMgr=&mgr;
struct Transmogrification {
    using slotMap=std::map<uint8,uint32>;
    bool enabled=true, collection=true, suitable=true, RequireToken=true, HiddenTransmogIsFree=true;
    bool AllowHiddenTransmog=true, track=true, autoBind=true;
    uint32 TokenEntry=49426, TokenAmount=1; float ScaledCostModifier=1, SetCostModifier=3;
    int32 CopperCost=0, SetCopperCost=0;
    std::unordered_map<uint32,std::unordered_set<uint32>> collectionCache;
    std::unordered_map<int,uint8> selectionCache;
    Player* current=nullptr;
    bool IsEnabled() const { return enabled; } bool GetUseCollectionSystem() const { return collection; }
    bool GetAllowHiddenTransmog() const { return AllowHiddenTransmog; }
    bool GetAutoCollectInventoryAppearances() const { return true; }
    bool GetTrackUnusableItems() const { return track; } bool GetAutoCollectBindItems() const { return autoBind; }
    bool GetRequireToken() const { return RequireToken; }
    uint32 GetTokenEntry() const { return TokenEntry; } uint32 GetTokenAmount() const { return TokenAmount; }
    bool GetHiddenTransmogIsFree() const { return HiddenTransmogIsFree; }
    bool GetAllowTradeable() const { return false; } bool GetUseVendorInterface() const { return true; }
    bool IsTransmogVendor(uint32 entry) const { return entry==190010; }
    uint32 GetSpecialPrice(ItemTemplate const*) const { return 10000; }
    bool SuitableForTransmogrification(Player*,ItemTemplate const*) const { return suitable; }
    bool CanTransmogrifyItemWithItem(Player*,ItemTemplate const*,ItemTemplate const*) const { return suitable; }
    char const* GetSlotName(uint8 slot,Session*) const { return slot==0 ? "Head" : nullptr; }
    uint32 GetFakeEntry(int) const { return current->fake; }
    void SetFakeEntry(Player* p,uint32 entry,uint8,Item*,CharacterDatabaseTransaction* t) { p->fake=entry;(*t)->writes.push_back("fake"); }
    void DeleteFakeEntry(Player* p,uint8,Item*,CharacterDatabaseTransaction* t) { p->fake=0;(*t)->writes.push_back("fake"); }
    void AddToDatabase(Player*,ItemTemplate const* item,CharacterDatabaseTransaction* t) {
        collectionCache[1].insert(item->ItemId);(*t)->writes.push_back("appearance");
    }
    Item* FindAppearanceSource(Player*,uint32) const;
    bool HasAppearance(Player*,uint32) const;
    uint32 GetPrice(ItemTemplate const*) const;
    uint32 GetSetPrice(slotMap const&) const;
    TransmogStrings Transmogrify(Player*,uint32,uint8,bool=false);
    TransmogStrings Transmogrify(Player*,Item*,uint8,bool=false,bool=false);
    ClaimAppearanceResult ClaimAppearance(Player*,Item*,ClaimAppearanceMode,CharacterDatabaseTransaction* = nullptr);
    TransmogStrings ApplyAppearance(Player*,uint32,uint8,bool,Item*);
};
Transmogrification* sT;
int applied=0;
void PerformTransmogrification(Player*,uint32,uint32) { ++applied; }
void RemoveTransmogrification(Player*) { ++applied; }
struct npc_transmogrifier { static void ShowTransmogItemsInFakeVendor(Player*,Creature*,uint8) { } };
'''
collector = "struct CollectorScript {\n" + "\n".join(
    function(scripts, signature).replace(" override", "") for signature in (
        "    void CollectInventoryAppearances(", "    void OnPlayerStoreNewItem(",
        "    void OnPlayerAfterMoveItemToInventory(", "    void OnPlayerUpdate(")) + "\n};\n"
protocol_bodies = "template <typename T>\n" + function(protocol, "bool ParseUnsigned(") + "\n"
protocol_bodies += "\n".join(function(protocol, signature).replace("ObjectGuid::Empty", "0") for signature in (
    "void SendSlotStates(", "bool ValidateEquipment(", "bool ParseSetEntries(", "std::string EncodeSetEntries("))
protocol_prelude = r'''
#define sTransmogrification sT
struct ClientState { uint32 sessionId=1, quoteId=0; std::array<ObjectGuid,EQUIPMENT_SLOT_END> equipment; };
std::map<int,ClientState> Clients;
std::vector<std::string> messages;
void Send(Player*, std::string const& text) { messages.push_back(text); }
'''
main = r'''
int main() {
    Transmogrification t; Player p; t.current=&p; sT=&t;
    t.collectionCache[1].insert(1001);
    p.money=0;
    assert(t.Transmogrify(&p,uint32(1001),0)==LANG_TRANSMOG_NOT_ENOUGH_MONEY);
    assert(p.tokens==1 && p.fake==0 && CharacterDatabase.committed.empty());
    p.money=20000;p.tokens=0;
    assert(t.Transmogrify(&p,uint32(1001),0)==LANG_TRANSMOG_NOT_ENOUGH_TOKENS);
    assert(p.money==20000 && p.fake==0 && CharacterDatabase.committed.empty());
    p.tokens=1;
    assert(t.Transmogrify(&p,uint32(1002),0)==LANG_TRANSMOG_MISSING_SRC_ITEM);
    assert(p.money==20000 && p.tokens==1);
    assert(t.Transmogrify(&p,uint32(1001),0)==LANG_TRANSMOG_OK);
    auto committed=CharacterDatabase.committed.back();
    assert(committed->money==10000 && committed->tokens==0);
    assert((committed->writes==std::vector<std::string>{"fake","inventory"}));
    assert(t.Transmogrify(&p,uint32(1001),0)==LANG_TRANSMOG_OK && p.money==10000);
    std::cout<<"payment: missing funds/tokens/ownership preserve assets; success persists payment and appearance together\n";
    t.collection=false;p.fake=0;p.money=20000;p.tokens=1;
    assert(t.Transmogrify(&p,uint32(1001),0)==LANG_TRANSMOG_MISSING_SRC_ITEM);
    p.holdsSource=true;
    assert(t.Transmogrify(&p,uint32(1001),0)==LANG_TRANSMOG_OK);
    assert(p.source.bound && CharacterDatabase.committed.back()->bound);
    std::cout<<"physical source: require possession and persist BOE binding\n";
    t.collection=true;t.collectionCache.clear();CharacterDatabase.committed.clear();
    p.source.bound=false;p.source.refundable=true;
    assert(t.ClaimAppearance(&p,&p.source,ClaimAppearanceMode::Automatic)==ClaimAppearanceResult::Refundable);
    assert(!p.source.bound && t.collectionCache.empty() && CharacterDatabase.committed.empty());
    p.source.refundable=false;p.source.tradeable=true;
    assert(t.ClaimAppearance(&p,&p.source,ClaimAppearanceMode::Automatic)==ClaimAppearanceResult::Tradeable);
    p.source.tradeable=false;t.suitable=false;
    auto transaction=CharacterDatabase.BeginTransaction();
    assert(t.ClaimAppearance(&p,&p.source,ClaimAppearanceMode::Automatic,&transaction)==ClaimAppearanceResult::Claimed);
    assert(p.source.bound && CharacterDatabase.committed.empty());
    p.SaveInventoryAndGoldToDB(transaction);CharacterDatabase.CommitTransaction(transaction);
    assert((transaction->writes==std::vector<std::string>{"appearance","inventory"}));
    std::cout<<"claim: preserve refund/trade rights; batch track-unusable appearance and binding in one transaction\n";
    t.suitable=true;t.enabled=true;
    VendorScript script;uint32 entry=1001;
    script.OnPlayerBeforeBuyItemFromVendor(&p,1,0,entry,1,0,0);
    assert(entry==0 && applied==0);
    t.selectionCache[1]=0;p.canInteract=false;entry=1001;
    script.OnPlayerBeforeBuyItemFromVendor(&p,1,0,entry,1,0,0);
    assert(entry==0 && applied==0);
    p.canInteract=true;t.enabled=false;entry=1001;
    script.OnPlayerBeforeBuyItemFromVendor(&p,1,0,entry,1,0,0);
    assert(entry==0 && applied==0);
    t.enabled=true;entry=1001;
    script.OnPlayerBeforeBuyItemFromVendor(&p,1,0,entry,1,0,0);
    assert(entry==0 && applied==1);
    std::cout<<"vendor: reject missing selection, distance and disabled module before custom purchase\n";
    assert(t.GetSetPrice({{0,1001},{4,1002}})==60000);
    assert(t.GetSetPrice({{0,1},{4,1002}})==30000);
    assert(TransmogCost(10000,0,-100)==0);
    assert(TransmogCost(UINT64_MAX,100,0)==INT32_MAX);
    std::cout<<"prices: shared preset fee, hidden entries, negative offsets and bounded overflow\n";
    t.collectionCache.clear();CharacterDatabase.committed.clear();
    p.target.proto.Class=0;p.source.bound=false;p.source.refundable=false;p.itemReads=0;
    CollectorScript collector;
    collector.OnPlayerStoreNewItem(&p,&p.source,1);
    collector.OnPlayerStoreNewItem(&p,&p.source,1);
    assert(p.itemReads==0 && !p.source.bound);
    p.source.refundable=true;
    collector.OnPlayerUpdate(&p,50);
    assert(p.itemReads==35 && !p.source.bound && t.collectionCache.empty());
    collector.OnPlayerUpdate(&p,50);
    assert(p.itemReads==35);
    p.source.refundable=false;
    collector.OnPlayerAfterMoveItemToInventory(&p,&p.source,true);
    collector.OnPlayerUpdate(&p,50);
    assert(p.source.bound && t.collectionCache[1].contains(1001));
    assert(CharacterDatabase.committed.size()==1);
    std::cout<<"collection hooks: defer until refund flags exist; coalesce events; collect mail/trade through the same path\n";
    SendSlotStates(&p);
    assert(messages.size()==21 && messages.front()=="STATE_BEGIN\t1\t1\t49426\t1");
    assert(ValidateEquipment(&p,Clients[1],"1"));
    assert(!ValidateEquipment(&p,Clients[1],"0"));
    p.target.guid=3;
    assert(!ValidateEquipment(&p,Clients[1],"1"));
    SendSlotStates(&p);
    assert(!ValidateEquipment(&p,Clients[1],"1") && ValidateEquipment(&p,Clients[1],"2"));
    Transmogrification::slotMap entries;
    assert(ParseSetEntries("0:1,4:1002",entries));
    assert(EncodeSetEntries(entries,',',':')=="0:1,4:1002");
    entries.clear();assert(!ParseSetEntries("0:1,0:1002",entries));
    entries.clear();assert(!ParseSetEntries("19:1002",entries));
    entries.clear();assert(!ParseSetEntries("0:4294967296",entries));
    std::cout<<"protocol: complete price snapshot; reject changed item identity and stale quote; preserve hidden presets and validate parsing\n";
}
'''
with tempfile.TemporaryDirectory(prefix="transmog-regression-") as temp:
    cpp = Path(temp) / "regression.cpp"
    cpp.write_text(prelude + bodies + collector + protocol_prelude + protocol_bodies + "\nstruct VendorScript {\n" + vendor + "\n};\n" + main)
    binary = Path(temp) / "regression"
    subprocess.run(["c++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "src"),
                    str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
