ACoreTransmog = ACoreTransmog or {}

local Addon = ACoreTransmog
local eventFrame = CreateFrame("Frame")
local helloDelay = nil

eventFrame:RegisterEvent("PLAYER_LOGIN")
eventFrame:RegisterEvent("CHAT_MSG_ADDON")
eventFrame:RegisterEvent("GET_ITEM_INFO_RECEIVED")

eventFrame:SetScript("OnEvent", function(self, event, ...)
    if event == "PLAYER_LOGIN" then
        if RegisterAddonMessagePrefix then
            RegisterAddonMessagePrefix(Addon.PREFIX)
        end
        helloDelay = 1
    elseif event == "CHAT_MSG_ADDON" then
        Addon:OnAddonMessage(...)
    elseif event == "GET_ITEM_INFO_RECEIVED" then
        if Addon.UI and Addon.UI.frame and Addon.UI.frame:IsShown() and Addon.UI.page ~= nil then
            Addon.UI:RefreshItemInfo()
        end
    end
end)

eventFrame:SetScript("OnUpdate", function(self, elapsed)
    if not helloDelay then
        return
    end
    helloDelay = helloDelay - elapsed
    if helloDelay <= 0 then
        helloDelay = nil
        Addon:Hello()
    end
end)

SLASH_ACORETRANSMOG1 = "/transmog"
SlashCmdList.ACORETRANSMOG = function()
    Addon.UI:Create()
    if Addon.sessionId then
        Addon.UI:Open()
    else
        Addon.UI.frame:Show()
        Addon:ResetPreview()
        Addon.UI:SetStatus(ACoreTransmogLocale.TALK_TO_NPC, 1, 0.3, 0.3)
    end
end
