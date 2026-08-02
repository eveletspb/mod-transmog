ACoreTransmog = ACoreTransmog or {}

local Addon = ACoreTransmog

function Addon:ResetPreview()
    if not self.UI or not self.UI.model then
        return
    end
    self.UI.model:SetUnit("player")
end

function Addon:PreviewItem(itemEntry)
    if not self.UI or not self.UI.model or not itemEntry then
        return
    end

    local _, link = GetItemInfo("item:" .. itemEntry)
    if not link then
        self.pendingPreviewEntry = itemEntry
        self.UI:SetStatus(ACoreTransmogLocale.PREVIEW_LOADING, 1, 0.82, 0)
        return false
    end

    self.pendingPreviewEntry = nil
    self:ResetPreview()
    self.UI.model:Show()
    self.UI.model:TryOn(link)
    self.UI:SetStatus(ACoreTransmogLocale.PREVIEWING, 0.45, 0.85, 1)
    return true
end
