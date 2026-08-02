ACoreTransmog = ACoreTransmog or {}

local Addon = ACoreTransmog
local PREFIX = "AC_TRANSMOG"
local VERSION = 1

Addon.connected = false
Addon.sessionId = nil
Addon.requestId = 0
Addon.pendingLists = {}
Addon.transmogSlots = {}
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

function Addon:Remove(slot)
    if not self.sessionId then
        return
    end
    local requestId = self:NextRequestId()
    self.UI:SetBusy(true)
    self:Send(string.format("REMOVE\t%d\t%d\t%d", requestId, self.sessionId, slot))
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
            if slot then
                self.transmogSlots[slot] = itemEntry ~= 0 and itemEntry or nil
                self.UI:UpdateTransmogSlotMarkers()
            end
            self.UI.pendingResultStatus = itemEntry == 0 and ACoreTransmogLocale.REMOVED or ACoreTransmogLocale.APPLIED
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
