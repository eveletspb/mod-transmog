ACoreTransmog = ACoreTransmog or {}

local Addon = ACoreTransmog
local L = ACoreTransmogLocale

local FRAME_WIDTH = 1000
local FRAME_HEIGHT = 660
local PREVIEW_WIDTH = 350
local PANEL_HEIGHT = 548
local COLLECTION_WIDTH = 590
local ITEM_SIZE = 42
local ITEM_GAP = 6
local GRID_COLUMNS = 10
local GRID_ROWS = 6
local BUTTON_COUNT = GRID_COLUMNS * GRID_ROWS

local LEFT_SLOTS = { 0, 2, 14, 4, 3, 18 }
local RIGHT_SLOTS = { 9, 8, 5, 6, 7, 15, 16, 17 }
local SLOT_TEXTURES = {
    [0] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Head",
    [2] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Shoulder",
    [3] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Shirt",
    [4] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Chest",
    [5] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Waist",
    [6] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Legs",
    [7] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Feet",
    [8] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Wrists",
    [9] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Hands",
    [14] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Chest",
    [15] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-MainHand",
    [16] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-SecondaryHand",
    [17] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Ranged",
    [18] = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Tabard",
}

local UI = {}
Addon.UI = UI

local function AddSolidTexture(parent, layer, red, green, blue, alpha)
    local texture = parent:CreateTexture(nil, layer or "BACKGROUND")
    texture:SetTexture(red, green, blue, alpha or 1)
    return texture
end

local function AddOutline(parent, thickness, red, green, blue, alpha)
    local outline = {}
    outline.top = AddSolidTexture(parent, "OVERLAY", red, green, blue, alpha)
    outline.top:SetPoint("TOPLEFT")
    outline.top:SetPoint("TOPRIGHT")
    outline.top:SetHeight(thickness)
    outline.bottom = AddSolidTexture(parent, "OVERLAY", red, green, blue, alpha)
    outline.bottom:SetPoint("BOTTOMLEFT")
    outline.bottom:SetPoint("BOTTOMRIGHT")
    outline.bottom:SetHeight(thickness)
    outline.left = AddSolidTexture(parent, "OVERLAY", red, green, blue, alpha)
    outline.left:SetPoint("TOPLEFT")
    outline.left:SetPoint("BOTTOMLEFT")
    outline.left:SetWidth(thickness)
    outline.right = AddSolidTexture(parent, "OVERLAY", red, green, blue, alpha)
    outline.right:SetPoint("TOPRIGHT")
    outline.right:SetPoint("BOTTOMRIGHT")
    outline.right:SetWidth(thickness)
    return outline
end

local function SetOutlineShown(outline, shown)
    for _, texture in pairs(outline) do
        if shown then texture:Show() else texture:Hide() end
    end
end

local function CreatePanel(parent, width, height)
    local panel = CreateFrame("Frame", nil, parent)
    panel:SetWidth(width)
    panel:SetHeight(height)
    panel:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true,
        tileSize = 16,
        edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 }
    })
    panel:SetBackdropColor(0.025, 0.03, 0.04, 0.96)
    panel:SetBackdropBorderColor(0.22, 0.24, 0.29, 1)
    return panel
end

local function CreateButton(parent, text, width, height)
    local button = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    button:SetWidth(width)
    button:SetHeight(height)
    button:SetText(text)
    return button
end

local function SetButtonEnabled(button, enabled)
    if enabled then button:Enable() else button:Disable() end
end

local function GetQualityColor(quality)
    local color = quality and ITEM_QUALITY_COLORS and ITEM_QUALITY_COLORS[quality]
    if color then
        return color.r, color.g, color.b
    end
    return 0.45, 0.45, 0.45
end

function UI:CreateSlotButton(parent, slot, side, index)
    local button = CreateFrame("Button", nil, parent)
    button:SetWidth(46)
    button:SetHeight(46)
    button.slot = slot

    if side == "LEFT" then
        button:SetPoint("TOPLEFT", 12, -58 - (index - 1) * 57)
    else
        button:SetPoint("TOPRIGHT", -12, -40 - (index - 1) * 57)
    end

    local background = AddSolidTexture(button, "BACKGROUND", 0.055, 0.06, 0.075, 1)
    background:SetAllPoints()

    local icon = button:CreateTexture(nil, "ARTWORK")
    icon:SetPoint("TOPLEFT", 4, -4)
    icon:SetPoint("BOTTOMRIGHT", -4, 4)
    icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    button.icon = icon

    button.normalOutline = AddOutline(button, 1, 0.32, 0.34, 0.4, 1)
    button.transmogOutline = AddOutline(button, 2, 0.15, 0.95, 0.45, 1)
    SetOutlineShown(button.transmogOutline, false)
    button.selectedOutline = AddOutline(button, 2, 1, 0.72, 0.15, 1)
    SetOutlineShown(button.selectedOutline, false)

    local transmogMarker = AddSolidTexture(button, "OVERLAY", 0.15, 0.95, 0.45, 1)
    transmogMarker:SetWidth(9)
    transmogMarker:SetHeight(9)
    transmogMarker:SetPoint("BOTTOMRIGHT", -3, 3)
    transmogMarker:Hide()
    button.transmogMarker = transmogMarker

    local highlight = AddSolidTexture(button, "HIGHLIGHT", 1, 0.82, 0.32, 0.16)
    highlight:SetPoint("TOPLEFT", 2, -2)
    highlight:SetPoint("BOTTOMRIGHT", -2, 2)

    button:SetScript("OnClick", function(self)
        UI:SelectSlot(self.slot)
    end)
    button:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, side == "LEFT" and "ANCHOR_RIGHT" or "ANCHOR_LEFT")
        GameTooltip:SetText(L["SLOT_" .. self.slot] or tostring(self.slot), 1, 0.82, 0)
        GameTooltip:SetInventoryItem("player", self.slot + 1)
        if Addon.transmogSlots and Addon.transmogSlots[self.slot] then
            GameTooltip:AddLine(L.SLOT_TRANSMOGRIFIED, 0.15, 0.95, 0.45)
        end
        GameTooltip:Show()
    end)
    button:SetScript("OnLeave", function() GameTooltip:Hide() end)

    self.slotButtons[slot] = button
end

function UI:CreateItemButton(parent, index)
    local button = CreateFrame("Button", nil, parent)
    button:SetWidth(ITEM_SIZE)
    button:SetHeight(ITEM_SIZE)
    button:EnableMouse(true)
    button:RegisterForClicks("LeftButtonUp")

    local column = (index - 1) % GRID_COLUMNS
    local row = math.floor((index - 1) / GRID_COLUMNS)
    button:SetPoint("TOPLEFT", 58 + column * (ITEM_SIZE + ITEM_GAP), -102 - row * (ITEM_SIZE + ITEM_GAP))

    local background = AddSolidTexture(button, "BACKGROUND", 0.035, 0.04, 0.055, 1)
    background:SetAllPoints()

    local icon = button:CreateTexture(nil, "ARTWORK")
    icon:SetPoint("TOPLEFT", 3, -3)
    icon:SetPoint("BOTTOMRIGHT", -3, 3)
    icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    button.icon = icon

    button.qualityOutline = AddOutline(button, 1, 0.4, 0.4, 0.4, 1)
    button.selectionOutline = AddOutline(button, 3, 1, 0.72, 0.12, 1)
    SetOutlineShown(button.selectionOutline, false)

    local highlight = AddSolidTexture(button, "HIGHLIGHT", 1, 1, 1, 0.15)
    highlight:SetPoint("TOPLEFT", 2, -2)
    highlight:SetPoint("BOTTOMRIGHT", -2, 2)

    local current = AddSolidTexture(button, "OVERLAY", 0.15, 0.95, 0.45, 1)
    current:SetWidth(9)
    current:SetHeight(9)
    current:SetPoint("BOTTOMRIGHT", -3, 3)
    current:Hide()
    button.currentMarker = current

    button:SetScript("OnClick", function(self)
        if not self.itemEntry then
            return
        end
        UI.selectedEntry = self.itemEntry
        UI:UpdateSelection()
        UI:UpdateSelectedItemText()
        local previewed, errorMessage = pcall(function() Addon:PreviewItem(self.itemEntry) end)
        if not previewed then
            DEFAULT_CHAT_FRAME:AddMessage("|cffff4040ACore Transmog preview error:|r " .. tostring(errorMessage))
        end
    end)
    button:SetScript("OnEnter", function(self)
        if not self.itemEntry then
            return
        end
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetHyperlink("item:" .. self.itemEntry)
        GameTooltip:Show()
    end)
    button:SetScript("OnLeave", function() GameTooltip:Hide() end)

    self.itemButtons[index] = button
end

function UI:Create()
    if self.frame then
        return
    end

    local frame = CreateFrame("Frame", "ACoreTransmogFrame", UIParent)
    frame:SetWidth(FRAME_WIDTH)
    frame:SetHeight(FRAME_HEIGHT)
    frame:SetPoint("CENTER")
    frame:SetFrameStrata("DIALOG")
    frame:SetClampedToScreen(true)
    frame:SetMovable(true)
    frame:EnableMouse(true)
    frame:RegisterForDrag("LeftButton")
    frame:SetScript("OnDragStart", frame.StartMoving)
    frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
    frame:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
        tile = true,
        tileSize = 32,
        edgeSize = 28,
        insets = { left = 9, right = 9, top = 9, bottom = 9 }
    })
    frame:SetBackdropColor(0.018, 0.022, 0.03, 0.98)
    frame:Hide()

    table.insert(UISpecialFrames, "ACoreTransmogFrame")
    frame:SetScript("OnHide", function() Addon:CloseSession() end)

    local header = AddSolidTexture(frame, "BACKGROUND", 0.045, 0.052, 0.07, 1)
    header:SetPoint("TOPLEFT", 10, -10)
    header:SetPoint("TOPRIGHT", -10, -10)
    header:SetHeight(43)

    local accent = AddSolidTexture(frame, "ARTWORK", 0.86, 0.58, 0.12, 1)
    accent:SetPoint("TOPLEFT", 18, -52)
    accent:SetPoint("TOPRIGHT", -18, -52)
    accent:SetHeight(1)

    local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("LEFT", header, "LEFT", 18, 0)
    title:SetText(L.TITLE)
    title:SetTextColor(1, 0.82, 0.35)

    local subtitle = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    subtitle:SetPoint("LEFT", title, "RIGHT", 14, -1)
    subtitle:SetText(L.SUBTITLE)
    subtitle:SetTextColor(0.58, 0.62, 0.7)

    local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", -8, -8)

    local previewPanel = CreatePanel(frame, PREVIEW_WIDTH, PANEL_HEIGHT)
    previewPanel:SetPoint("TOPLEFT", 18, -62)

    local previewTitle = previewPanel:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    previewTitle:SetPoint("TOPLEFT", 16, -15)
    previewTitle:SetText(L.PREVIEW)
    previewTitle:SetTextColor(0.82, 0.84, 0.9)

    local modelBackground = AddSolidTexture(previewPanel, "BACKGROUND", 0.012, 0.016, 0.024, 1)
    modelBackground:SetPoint("TOPLEFT", 58, -42)
    modelBackground:SetPoint("BOTTOMRIGHT", -58, 45)

    -- This HD 3.3.5 client exposes DressUpModel as a creatable frame type.
    -- Unlike a generic PlayerModel, it provides the TryOn method.
    local model = CreateFrame("DressUpModel", "ACoreTransmogModel", previewPanel)
    model:SetPoint("TOPLEFT", 53, -38)
    model:SetPoint("BOTTOMRIGHT", -53, 39)
    model:EnableMouse(true)
    model:EnableMouseWheel(true)
    model:SetScript("OnMouseDown", function(self, button)
        if button == "LeftButton" then
            self.dragging = true
            self.lastX = GetCursorPosition()
            self.facing = self.facing or 0
        end
    end)
    model:SetScript("OnMouseUp", function(self) self.dragging = false end)
    model:SetScript("OnUpdate", function(self)
        if self.dragging then
            local x = GetCursorPosition()
            self.facing = self.facing + (x - self.lastX) * 0.01
            self:SetFacing(self.facing)
            self.lastX = x
        end
    end)
    model:SetScript("OnMouseWheel", function(self, delta)
        self.modelScale = math.max(0.55, math.min(1.75, (self.modelScale or 1) + delta * 0.08))
        self:SetModelScale(self.modelScale)
    end)

    local resetPreview = CreateButton(previewPanel, L.RESET_PREVIEW, 126, 24)
    resetPreview:SetPoint("BOTTOM", 0, 12)
    resetPreview:SetScript("OnClick", function()
        self.selectedEntry = nil
        Addon:ResetPreview()
        self:UpdateSelection()
        self:UpdateSelectedItemText()
    end)

    self.slotButtons = {}
    for index, slot in ipairs(LEFT_SLOTS) do
        self:CreateSlotButton(previewPanel, slot, "LEFT", index)
    end
    for index, slot in ipairs(RIGHT_SLOTS) do
        self:CreateSlotButton(previewPanel, slot, "RIGHT", index)
    end

    local collectionPanel = CreatePanel(frame, COLLECTION_WIDTH, PANEL_HEIGHT)
    collectionPanel:SetPoint("TOPRIGHT", -18, -62)
    collectionPanel:SetScript("OnUpdate", function(self, elapsed)
        if not UI.hasMissingItemInfo or (UI.itemInfoRefreshAttempts or 0) >= 20 then
            return
        end
        self.itemInfoElapsed = (self.itemInfoElapsed or 0) + elapsed
        if self.itemInfoElapsed >= 0.5 then
            self.itemInfoElapsed = 0
            UI.itemInfoRefreshAttempts = (UI.itemInfoRefreshAttempts or 0) + 1
            UI:RefreshItemInfo()
        end
    end)

    local collectionTitle = collectionPanel:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    collectionTitle:SetPoint("TOPLEFT", 18, -14)
    collectionTitle:SetText(L.COLLECTION)
    collectionTitle:SetTextColor(0.9, 0.91, 0.94)

    local slotTitle = collectionPanel:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    slotTitle:SetPoint("TOPRIGHT", -18, -18)
    slotTitle:SetJustifyH("RIGHT")

    local search = CreateFrame("EditBox", "ACoreTransmogSearch", collectionPanel, "InputBoxTemplate")
    search:SetWidth(350)
    search:SetHeight(28)
    search:SetPoint("TOPLEFT", 24, -55)
    search:SetAutoFocus(false)
    search:SetMaxLetters(50)

    local searchPlaceholder = search:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    searchPlaceholder:SetPoint("LEFT", 7, 0)
    searchPlaceholder:SetText(L.SEARCH_HINT)
    searchPlaceholder:SetTextColor(0.48, 0.5, 0.56)
    search.placeholder = searchPlaceholder
    search:SetScript("OnTextChanged", function(self)
        if self:GetText() == "" and not self:HasFocus() then self.placeholder:Show() else self.placeholder:Hide() end
    end)
    search:SetScript("OnEditFocusGained", function(self) self.placeholder:Hide() end)
    search:SetScript("OnEditFocusLost", function(self)
        if self:GetText() == "" then self.placeholder:Show() end
    end)
    search:SetScript("OnEnterPressed", function(self)
        self:ClearFocus()
        UI:RequestPage(0)
    end)
    search:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)

    local searchButton = CreateButton(collectionPanel, L.SEARCH, 96, 26)
    searchButton:SetPoint("LEFT", search, "RIGHT", 10, 0)
    searchButton:SetScript("OnClick", function() UI:RequestPage(0) end)

    self.itemButtons = {}
    for index = 1, BUTTON_COUNT do
        self:CreateItemButton(collectionPanel, index)
    end

    local gridOverlay = CreateFrame("Frame", nil, collectionPanel)
    gridOverlay:SetPoint("TOPLEFT", 52, -94)
    gridOverlay:SetWidth(486)
    gridOverlay:SetHeight(296)
    gridOverlay:EnableMouse(true)
    local gridShade = AddSolidTexture(gridOverlay, "BACKGROUND", 0.01, 0.015, 0.025, 0.72)
    gridShade:SetAllPoints()
    local loadingText = gridOverlay:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    loadingText:SetPoint("CENTER")
    loadingText:SetText(L.LOADING)
    gridOverlay:Hide()

    local divider = AddSolidTexture(collectionPanel, "ARTWORK", 0.18, 0.2, 0.25, 1)
    divider:SetPoint("LEFT", 18, 0)
    divider:SetPoint("RIGHT", -18, 0)
    divider:SetPoint("TOP", 0, -402)
    divider:SetHeight(1)

    local previous = CreateButton(collectionPanel, L.PREVIOUS, 104, 25)
    previous:SetPoint("TOPLEFT", 24, -416)
    previous:SetScript("OnClick", function() UI:RequestPage((UI.page or 0) - 1) end)

    local nextButton = CreateButton(collectionPanel, L.NEXT, 104, 25)
    nextButton:SetPoint("TOPRIGHT", -24, -416)
    nextButton:SetScript("OnClick", function() UI:RequestPage((UI.page or 0) + 1) end)

    local pageText = collectionPanel:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    pageText:SetPoint("TOP", 0, -422)
    pageText:SetWidth(280)
    pageText:SetJustifyH("CENTER")

    local selectedText = collectionPanel:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    selectedText:SetPoint("TOPLEFT", 24, -458)
    selectedText:SetPoint("TOPRIGHT", -190, -458)
    selectedText:SetJustifyH("LEFT")
    selectedText:SetHeight(20)

    local priceText = collectionPanel:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    priceText:SetPoint("TOPRIGHT", -24, -458)
    priceText:SetJustifyH("RIGHT")

    local status = collectionPanel:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    status:SetPoint("BOTTOMLEFT", 24, 14)
    status:SetWidth(260)
    status:SetHeight(28)
    status:SetJustifyH("LEFT")
    status:SetJustifyV("MIDDLE")

    local remove = CreateButton(collectionPanel, L.REMOVE, 112, 28)
    remove:SetPoint("BOTTOMRIGHT", -176, 14)
    remove:SetScript("OnClick", function()
        if UI.selectedSlot then Addon:Remove(UI.selectedSlot) end
    end)

    local apply = CreateButton(collectionPanel, L.APPLY, 142, 28)
    apply:SetPoint("BOTTOMRIGHT", -24, 14)
    apply:SetScript("OnClick", function()
        if UI.selectedEntry then Addon:Apply(UI.selectedSlot, UI.selectedEntry) end
    end)

    self.frame = frame
    self.model = model
    self.collectionPanel = collectionPanel
    self.slotTitle = slotTitle
    self.search = search
    self.previous = previous
    self.next = nextButton
    self.pageText = pageText
    self.selectedText = selectedText
    self.priceText = priceText
    self.apply = apply
    self.remove = remove
    self.status = status
    self.gridOverlay = gridOverlay
end

function UI:Open()
    self:Create()
    self.frame:Show()
    Addon:ResetPreview()
    self:RefreshSlotIcons()

    local initialSlot = self.selectedSlot
    if initialSlot == nil or not GetInventoryItemTexture("player", initialSlot + 1) then
        initialSlot = nil
        for _, slot in ipairs(LEFT_SLOTS) do
            if GetInventoryItemTexture("player", slot + 1) then initialSlot = slot break end
        end
        if initialSlot == nil then
            for _, slot in ipairs(RIGHT_SLOTS) do
                if GetInventoryItemTexture("player", slot + 1) then initialSlot = slot break end
            end
        end
    end
    self:SelectSlot(initialSlot or 0)
end

function UI:Close()
    if self.frame then self.frame:Hide() end
end

function UI:RefreshSlotIcons()
    for slot, button in pairs(self.slotButtons) do
        local texture = GetInventoryItemTexture("player", slot + 1)
        button.icon:SetTexture(texture or SLOT_TEXTURES[slot])
        if texture then
            button.icon:SetVertexColor(1, 1, 1)
        else
            button.icon:SetVertexColor(0.35, 0.37, 0.42)
        end
    end
    self:UpdateTransmogSlotMarkers()
end

function UI:UpdateTransmogSlotMarkers()
    if not self.slotButtons then return end
    for slot, button in pairs(self.slotButtons) do
        local hasTransmog = Addon.transmogSlots and Addon.transmogSlots[slot] ~= nil
        SetOutlineShown(button.transmogOutline, hasTransmog and slot ~= self.selectedSlot)
        if hasTransmog then button.transmogMarker:Show() else button.transmogMarker:Hide() end
    end
end

function UI:SelectSlot(slot)
    self.selectedSlot = slot
    self.selectedEntry = nil
    Addon.pendingPreviewEntry = nil
    for slotId, button in pairs(self.slotButtons) do
        SetOutlineShown(button.selectedOutline, slotId == slot)
    end
    self:UpdateTransmogSlotMarkers()
    self.slotTitle:SetText(L["SLOT_" .. slot] or tostring(slot))
    Addon:ResetPreview()
    self:UpdateSelectedItemText()
    self:RequestPage(0)
end

function UI:RequestPage(page)
    if self.selectedSlot == nil then return end
    Addon:RequestList(self.selectedSlot, math.max(0, page or 0), self.search:GetText() or "")
end

function UI:RefreshCurrentPage()
    Addon.pendingPreviewEntry = nil
    Addon:ResetPreview()
    self:RefreshSlotIcons()
    self:UpdateSelectedItemText()
    self:RequestPage(self.page or 0)
end

function UI:SetLoading(loading)
    if not self.gridOverlay then return end
    if loading then
        self.gridOverlay:Show()
        self:SetStatus(L.LOADING, 1, 0.82, 0)
    else
        self.gridOverlay:Hide()
    end
    SetButtonEnabled(self.apply, false)
end

function UI:SetBusy(busy)
    SetButtonEnabled(self.apply, not busy and self.selectedEntry ~= nil and self.selectedEntry ~= self.currentEntry)
    SetButtonEnabled(self.remove, not busy and (self.currentEntry or 0) ~= 0)
end

function UI:SetStatus(text, red, green, blue)
    if not self.status then return end
    self.status:SetText(text or "")
    self.status:SetTextColor(red or 1, green or 1, blue or 1)
end

function UI:UpdateSelection()
    for _, button in ipairs(self.itemButtons) do
        SetOutlineShown(button.selectionOutline, button.itemEntry ~= nil and button.itemEntry == self.selectedEntry)
    end
    SetButtonEnabled(self.apply, self.selectedEntry ~= nil and self.selectedEntry ~= self.currentEntry)
end

function UI:UpdateSelectedItemText()
    if not self.selectedText then return end
    if not self.selectedEntry then
        self.selectedText:SetText(L.SELECT_APPEARANCE)
        self.selectedText:SetTextColor(0.55, 0.58, 0.65)
        return
    end

    local name, _, quality = GetItemInfo("item:" .. self.selectedEntry)
    local red, green, blue = GetQualityColor(quality)
    self.selectedText:SetText(name or ("#" .. self.selectedEntry))
    self.selectedText:SetTextColor(red, green, blue)
end

function UI:RefreshItemInfo()
    if not self.itemButtons then return end
    self.hasMissingItemInfo = false
    for _, button in ipairs(self.itemButtons) do
        if button.itemEntry then
            local _, _, quality, _, _, _, _, _, _, texture = GetItemInfo("item:" .. button.itemEntry)
            if texture then
                button.icon:SetTexture(texture)
            else
                self.hasMissingItemInfo = true
            end
            local red, green, blue = GetQualityColor(quality)
            for _, border in pairs(button.qualityOutline) do
                border:SetTexture(red, green, blue, 1)
            end
        end
    end
    self:UpdateSelectedItemText()
    if Addon.pendingPreviewEntry then
        local _, link = GetItemInfo("item:" .. Addon.pendingPreviewEntry)
        if link then
            local previewed, errorMessage = pcall(function() Addon:PreviewItem(Addon.pendingPreviewEntry) end)
            if not previewed then
                DEFAULT_CHAT_FRAME:AddMessage("|cffff4040ACore Transmog preview error:|r " .. tostring(errorMessage))
            end
        end
    end
end

function UI:ShowPage(meta, items)
    self:SetLoading(false)
    self.page = meta.page
    self.pageCount = meta.pageCount
    self.currentEntry = meta.current
    Addon.transmogSlots[meta.slot] = meta.current ~= 0 and meta.current or nil
    self:UpdateTransmogSlotMarkers()
    self.hasMissingItemInfo = false
    self.itemInfoRefreshAttempts = 0

    local selectionIsVisible = self.selectedEntry == nil
    if self.selectedEntry then
        for _, itemEntry in ipairs(items) do
            if itemEntry == self.selectedEntry then
                selectionIsVisible = true
                break
            end
        end
    end
    if not selectionIsVisible then
        self.selectedEntry = nil
    end

    for index, button in ipairs(self.itemButtons) do
        local itemEntry = items[index]
        button.itemEntry = itemEntry
        if itemEntry then
            local _, _, quality, _, _, _, _, _, _, texture = GetItemInfo("item:" .. itemEntry)
            button.icon:SetTexture(texture or "Interface\\Icons\\INV_Misc_QuestionMark")
            if not texture then self.hasMissingItemInfo = true end
            local red, green, blue = GetQualityColor(quality)
            for _, border in pairs(button.qualityOutline) do
                border:SetTexture(red, green, blue, 1)
            end
            if itemEntry == meta.current then button.currentMarker:Show() else button.currentMarker:Hide() end
            button:Show()
        else
            button.currentMarker:Hide()
            button:Hide()
        end
    end

    SetButtonEnabled(self.previous, meta.page > 0)
    SetButtonEnabled(self.next, meta.page + 1 < meta.pageCount)
    self.pageText:SetText(string.format(L.PAGE, meta.page + 1, meta.pageCount, meta.total))
    self.priceText:SetText(L.PRICE .. " " .. GetCoinTextureString(meta.price or 0))
    SetButtonEnabled(self.remove, (meta.current or 0) ~= 0)
    if self.pendingResultStatus then
        self:SetStatus(self.pendingResultStatus, 0.3, 1, 0.3)
        self.pendingResultStatus = nil
    else
        self:SetStatus(meta.total == 0 and L.EMPTY or "", 1, 0.82, 0)
    end
    self:UpdateSelection()
    self:UpdateSelectedItemText()
end
