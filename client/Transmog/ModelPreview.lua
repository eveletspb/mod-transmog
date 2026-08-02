ACoreTransmog = ACoreTransmog or {}

local Addon = ACoreTransmog

Addon.draftSlots = Addon.draftSlots or {}

function Addon:ResetPreview()
    if not self.UI or not self.UI.model then
        return
    end
    self.UI.model:SetUnit("player")
end

function Addon:GetDraftCount()
    local count = 0
    for _ in pairs(self.draftSlots) do
        count = count + 1
    end
    return count
end

function Addon:PreviewDraft()
    if not self.UI or not self.UI.model then return false end

    self:ResetPreview()
    self.UI.model:Show()
    self.pendingDraftPreview = false

    for _, itemEntry in pairs(self.draftSlots) do
        local _, link = GetItemInfo("item:" .. itemEntry)
        if link then
            self.UI.model:TryOn(link)
        else
            self.pendingDraftPreview = true
        end
    end

    if self.pendingDraftPreview then
        self.UI:SetStatus(ACoreTransmogLocale.PREVIEW_LOADING, 1, 0.82, 0)
        return false
    end

    local count = self:GetDraftCount()
    if count > 0 then
        self.UI:SetStatus(string.format(ACoreTransmogLocale.DRAFT_READY, count), 0.35, 0.8, 1)
    end
    return count > 0
end

function Addon:SetDraftAppearance(slot, itemEntry)
    if slot == nil or not itemEntry then return end

    if self.transmogSlots[slot] == itemEntry then
        self.draftSlots[slot] = nil
    else
        self.draftSlots[slot] = itemEntry
    end
    self:PreviewDraft()
    if self.UI then self.UI:UpdateDraftState() end
end

function Addon:ClearDraft()
    self.draftSlots = {}
    self.pendingDraftPreview = false
    self:ResetPreview()
    if self.UI then
        self.UI.selectedEntry = nil
        self.UI:UpdateDraftState()
        self.UI:UpdateSelection()
        self.UI:UpdateSelectedItemText()
    end
end
