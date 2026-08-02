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

    self:ResetPreview()
    local _, link = GetItemInfo("item:" .. itemEntry)
    self.UI.model:TryOn(link or ("item:" .. itemEntry))
end
