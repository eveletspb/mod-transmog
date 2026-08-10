ACoreTransmog = ACoreTransmog or {}

local Addon = ACoreTransmog
local PREFIX = "AC_TRANSMOG"
local VERSION = 4

Addon.connected = false
Addon.sessionId = nil
Addon.requestId = 0
Addon.pendingLists = {}
Addon.transmogSlots = {}
Addon.savedSets = {}
Addon.pendingSets = {}
Addon.protocolVersion = VERSION
Addon.lastProtocolMessage = "none"

local function Split(message)
    local values = {}
    local start = 1
    while true do
        local position = string.find(message, "\t", start, true)
        if not position then
            table.insert(values, string.sub(message, start))
            break
        end
        table.insert(values, string.sub(message, start, position - 1))
        start = position + 1
    end
    return values
end

function Addon:Send(command)
    if not UnitName("player") then
        return
    end
    SendAddonMessage(PREFIX, command, "WHISPER", UnitName("player"))
end

function Addon:NextRequestId()
    self.requestId = self.requestId + 1
    if self.requestId > 999999 then
        self.requestId = 1
    end
    return self.requestId
end

function Addon:Hello()
    self.lastProtocolMessage = "HELLO sent"
    self:Send("HELLO\t" .. VERSION)
end

function Addon:RequestList(slot, page, search)
    if not self.sessionId then
        self.UI:SetStatus(ACoreTransmogLocale.TALK_TO_NPC, 1, 0.3, 0.3)
        return
    end

    local requestId = self:NextRequestId()
    search = string.gsub(search or "", "[\t\r\n]", " ")
    if string.len(search) > 100 then
        self.UI:SetStatus(ACoreTransmogLocale.ERROR_SEARCH, 1, 0.3, 0.3)
        return
    end
    self.pendingLists[requestId] = { items = {}, received = {}, chunks = nil }
    self.latestListRequestId = requestId
    self.UI:SetLoading(true)
    self:Send(string.format("LIST\t%d\t%d\t%d\t%d\t%s", requestId, self.sessionId, slot, page or 0, search))
end

function Addon:Apply(slot, itemEntry)
    if not self.sessionId or not itemEntry then
        return
    end
    local requestId = self:NextRequestId()
    self.UI:SetBusy(true)
    self.UI:SetStatus(ACoreTransmogLocale.APPLYING, 1, 0.82, 0)
    self:Send(string.format("APPLY\t%d\t%d\t%d\t%d", requestId, self.sessionId, slot, itemEntry))
end

function Addon:ApplyOutfit()
    if not self.sessionId or self:GetDraftCount() == 0 then return end

    local slots = {}
    for slot in pairs(self.draftSlots) do
        table.insert(slots, slot)
    end
    table.sort(slots)

    local changes = {}
    for _, slot in ipairs(slots) do
        table.insert(changes, slot .. ":" .. self.draftSlots[slot])
    end

    local requestId = self:NextRequestId()
    self.UI:SetBusy(true)
    self.UI:SetStatus(ACoreTransmogLocale.APPLYING_OUTFIT, 1, 0.82, 0)
    self:Send(string.format("APPLY_OUTFIT\t%d\t%d\t%s", requestId, self.sessionId, table.concat(changes, ",")))
end

function Addon:Remove(slot)
    if not self.sessionId then
        return
    end
    local requestId = self:NextRequestId()
    self.UI:SetBusy(true)
    self:Send(string.format("REMOVE\t%d\t%d\t%d", requestId, self.sessionId, slot))
end

function Addon:RemoveAll()
    if not self.sessionId then
        return
    end
    local requestId = self:NextRequestId()
    self.UI:SetBusy(true)
    self.UI:SetStatus(ACoreTransmogLocale.REMOVING_ALL, 1, 0.82, 0)
    self:Send(string.format("REMOVE_ALL\t%d\t%d", requestId, self.sessionId))
end

local function NormalizeSetName(name)
    name = string.gsub(name or "", "[\t\r\n]", " ")
    name = string.gsub(name, "^%s+", "")
    name = string.gsub(name, "%s+$", "")
    if name == "" or string.len(name) > 64 then return nil end
    return name
end

function Addon:RequestSets()
    if not self.sessionId then return end
    local requestId = self:NextRequestId()
    self.pendingSets[requestId] = { sets = {}, received = {}, remaining = nil, maxSets = 0 }
    self.latestSetsRequestId = requestId
    self:Send(string.format("LIST_SETS\t%d\t%d", requestId, self.sessionId))
end

function Addon:GetEffectiveOutfit()
    local entries = {}
    for slot, itemEntry in pairs(self.transmogSlots or {}) do
        if itemEntry and itemEntry > 1 then entries[slot] = itemEntry end
    end
    for slot, itemEntry in pairs(self.draftSlots or {}) do
        if itemEntry and itemEntry > 1 then
            entries[slot] = itemEntry
        else
            entries[slot] = nil
        end
    end
    return entries
end

local function EncodeEntries(entries)
    local slots = {}
    for slot in pairs(entries) do table.insert(slots, slot) end
    table.sort(slots)
    local encoded = {}
    for _, slot in ipairs(slots) do
        table.insert(encoded, slot .. ":" .. entries[slot])
    end
    return table.concat(encoded, ",")
end

function Addon:SaveSet(name)
    if not self.sessionId then return end
    name = NormalizeSetName(name)
    if not name then
        self.UI:SetStatus(ACoreTransmogLocale.ERROR_SET_NAME, 1, 0.3, 0.3)
        return
    end
    local encoded = EncodeEntries(self:GetEffectiveOutfit())
    if encoded == "" then
        self.UI:SetStatus(ACoreTransmogLocale.ERROR_EMPTY_SET, 1, 0.3, 0.3)
        return
    end
    local requestId = self:NextRequestId()
    local command = string.format("SAVE_SET\t%d\t%d\t%s\t%s", requestId, self.sessionId, name, encoded)
    if string.len(command) > 240 then
        self.UI:SetStatus(ACoreTransmogLocale.ERROR_SET_TOO_LARGE, 1, 0.3, 0.3)
        return
    end
    self.UI:SetBusy(true)
    self:Send(command)
end

function Addon:RenameSet(presetId, name)
    name = NormalizeSetName(name)
    if not self.sessionId or presetId == nil or not name then
        self.UI:SetStatus(ACoreTransmogLocale.ERROR_SET_NAME, 1, 0.3, 0.3)
        return
    end
    local requestId = self:NextRequestId()
    self.UI:SetBusy(true)
    self:Send(string.format("RENAME_SET\t%d\t%d\t%d\t%s", requestId, self.sessionId, presetId, name))
end

function Addon:DeleteSet(presetId)
    if not self.sessionId or presetId == nil then return end
    local requestId = self:NextRequestId()
    self.UI:SetBusy(true)
    self:Send(string.format("DELETE_SET\t%d\t%d\t%d", requestId, self.sessionId, presetId))
end

function Addon:CloseSession()
    if self.sessionId then
        self:Send("CLOSE\t" .. self.sessionId)
    end
    self.sessionId = nil
end

local function ErrorText(code)
    local L = ACoreTransmogLocale
    local errors = {
        SESSION = L.ERROR_SESSION,
        THROTTLED = L.ERROR_THROTTLED,
        DUPLICATE = L.ERROR_DUPLICATE,
        SEARCH = L.ERROR_SEARCH,
        SLOT = L.ERROR_SLOT,
        EMPTY_SLOT = L.ERROR_EMPTY_SLOT,
        COLLECTION_DISABLED = L.ERROR_COLLECTION_DISABLED,
        NOT_COLLECTED = L.ERROR_NOT_COLLECTED,
        NOT_TRANSMOGRIFIED = L.ERROR_NOT_TRANSMOGRIFIED,
        TRANSMOG_2 = L.ERROR_SLOT,
        TRANSMOG_3 = L.ERROR_INVALID_ITEMS,
        TRANSMOG_4 = L.ERROR_INVALID_ITEMS,
        TRANSMOG_5 = L.ERROR_EMPTY_SLOT,
        TRANSMOG_6 = L.ERROR_INVALID_ITEMS,
        TRANSMOG_7 = L.ERROR_MONEY,
        TRANSMOG_8 = L.ERROR_TOKENS,
        SETS_DISABLED = L.ERROR_SETS_DISABLED,
        SET_NAME = L.ERROR_SET_NAME,
        SET_NAME_EXISTS = L.ERROR_SET_NAME_EXISTS,
        SET_LIMIT = L.ERROR_SET_LIMIT,
        SET_NOT_FOUND = L.ERROR_SET_NOT_FOUND,
        EMPTY_SET = L.ERROR_EMPTY_SET,
    }
    return errors[code] or L.ERROR_GENERIC
end

function Addon:HandleProtocolMessage(message)
    local parts = Split(message)
    local command = parts[1]
    self.lastProtocolMessage = command or "unknown"

    if command == "HELLO_OK" then
        self.connected = tonumber(parts[2]) == VERSION
        self.helloAcknowledged = self.connected
        return
    end

    if command == "HELLO_ERROR" then
        self.connected = false
        if self.UI then
            self.UI:SetStatus(ACoreTransmogLocale.INCOMPATIBLE, 1, 0.3, 0.3)
        end
        return
    end

    if command == "OPEN" then
        self.sessionId = tonumber(parts[2])
        local opened, errorMessage = pcall(function() self.UI:Open() end)
        if not opened then
            self.lastProtocolMessage = "UI_ERROR"
            DEFAULT_CHAT_FRAME:AddMessage("|cffff4040ACore Transmog UI error:|r " .. tostring(errorMessage))
        end
        if opened then self:RequestSets() end
        return
    end

    if command == "SETS" then
        local requestId = tonumber(parts[2])
        local pending = requestId and self.pendingSets[requestId]
        if not pending then return end
        pending.remaining = tonumber(parts[3]) or 0
        pending.maxSets = tonumber(parts[4]) or 0
        if pending.remaining == 0 and self.latestSetsRequestId == requestId then
            self.savedSets = {}
            self.UI:ShowSavedSets(self.savedSets, pending.maxSets)
            self.pendingSets[requestId] = nil
        end
        return
    end

    if command == "SET" then
        local requestId = tonumber(parts[2])
        local presetId = tonumber(parts[3])
        local pending = requestId and self.pendingSets[requestId]
        if not pending or presetId == nil or pending.received[presetId] then return end

        local entries = {}
        for value in string.gmatch(parts[5] or "", "[^,]+") do
            local separator = string.find(value, ":", 1, true)
            if separator then
                local slot = tonumber(string.sub(value, 1, separator - 1))
                local itemEntry = tonumber(string.sub(value, separator + 1))
                if slot and itemEntry then entries[slot] = itemEntry end
            end
        end
        pending.received[presetId] = true
        pending.sets[presetId] = { id = presetId, name = parts[4] or ("#" .. presetId), entries = entries }
        pending.remaining = math.max(0, (pending.remaining or 1) - 1)
        if pending.remaining == 0 then
            if self.latestSetsRequestId == requestId then
                self.savedSets = pending.sets
                self.UI:ShowSavedSets(self.savedSets, pending.maxSets)
            end
            self.pendingSets[requestId] = nil
        end
        return
    end

    if command == "SET_RESULT" then
        local requestId = tonumber(parts[2])
        if requestId then self.pendingSets[requestId] = nil end
        self.UI:SetBusy(false)
        if parts[3] == "OK" then
            local messages = {
                SAVED = ACoreTransmogLocale.SET_SAVED,
                RENAMED = ACoreTransmogLocale.SET_RENAMED,
                DELETED = ACoreTransmogLocale.SET_DELETED,
            }
            self.UI:SetStatus(messages[parts[4]] or ACoreTransmogLocale.SET_UPDATED, 0.3, 1, 0.3)
            self:RequestSets()
        else
            self.UI:SetStatus(ErrorText(parts[4]), 1, 0.3, 0.3)
        end
        return
    end

    if command == "SLOTS" then
        self.transmogSlots = {}
        if parts[2] and parts[2] ~= "" then
            for value in string.gmatch(parts[2], "[^,]+") do
                local separator = string.find(value, ":", 1, true)
                if separator then
                    local slot = tonumber(string.sub(value, 1, separator - 1))
                    local itemEntry = tonumber(string.sub(value, separator + 1))
                    if slot and itemEntry then
                        self.transmogSlots[slot] = itemEntry
                    end
                end
            end
        end
        if self.UI then
            self.UI:UpdateTransmogSlotMarkers()
        end
        return
    end

    if command == "PAGE" then
        local requestId = tonumber(parts[2])
        local pending = self.pendingLists[requestId]
        if not pending then
            return
        end
        pending.meta = {
            requestId = requestId,
            slot = tonumber(parts[3]),
            page = tonumber(parts[4]),
            pageCount = tonumber(parts[5]),
            total = tonumber(parts[6]),
            price = tonumber(parts[7]),
            current = tonumber(parts[8]),
            target = tonumber(parts[9]),
        }
        return
    end

    if command == "ITEMS" then
        local requestId = tonumber(parts[2])
        local chunk = tonumber(parts[3])
        local chunks = tonumber(parts[4])
        local pending = self.pendingLists[requestId]
        if not pending then
            return
        end

        pending.chunks = chunks
        if not pending.received[chunk] then
            pending.received[chunk] = true
            if parts[5] and parts[5] ~= "" then
                for value in string.gmatch(parts[5], "[^,]+") do
                    table.insert(pending.items, tonumber(value))
                end
            end
        end

        local complete = pending.meta ~= nil
        for index = 0, chunks - 1 do
            if not pending.received[index] then
                complete = false
                break
            end
        end
        if complete then
            if self.latestListRequestId == requestId then
                self.UI:ShowPage(pending.meta, pending.items)
            end
            self.pendingLists[requestId] = nil
        end
        return
    end

    if command == "RESULT" then
        local requestId = tonumber(parts[2])
        if requestId then
            self.pendingLists[requestId] = nil
        end
        self.UI:SetBusy(false)
        if parts[3] == "OK" then
            local slot = tonumber(parts[4])
            local itemEntry = tonumber(parts[5]) or 0
            if parts[4] == "OUTFIT" then
                self.UI.pendingResultStatus = string.format(ACoreTransmogLocale.OUTFIT_APPLIED, itemEntry)
                self:ClearDraft()
            elseif slot == 255 then
                self.transmogSlots = {}
                self.UI.pendingResultStatus = ACoreTransmogLocale.RESET_ALL_SUCCESS
                self:ClearDraft()
            elseif slot then
                self.transmogSlots[slot] = itemEntry ~= 0 and itemEntry or nil
                self.UI.pendingResultStatus = itemEntry == 0 and ACoreTransmogLocale.REMOVED or ACoreTransmogLocale.APPLIED
            end
            self.UI:UpdateTransmogSlotMarkers()
            self.UI:RefreshCurrentPage()
        else
            local errorCode = parts[4]
            self.UI:SetStatus(ErrorText(errorCode), 1, 0.3, 0.3)
            if errorCode == "SESSION" then
                self.sessionId = nil
            end
        end
    end
end

function Addon:OnAddonMessage(prefix, message, channel, sender)
    if prefix ~= PREFIX or channel ~= "WHISPER" then
        return
    end

    local shortSender = sender and string.match(sender, "^[^-]+")
    if shortSender and shortSender ~= UnitName("player") then
        return
    end
    self:HandleProtocolMessage(message)
end

Addon.PREFIX = PREFIX
