#define IMGUI_DEFINE_MATH_OPERATORS
#include "OdysseyWidgets.h"

// ImGui removed the FontSize member from ImFont in versions >= 1.90.  Use LegacySize
// instead.  Define FontSize as an alias for LegacySize to allow existing
// references (e.g. fonts.ui->FontSize) to compile without changes.
#define FontSize LegacySize

#include "OdysseyTheme.h"

#include "imgui_internal.h"

#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>

namespace
{
ImGuiID g_activeKeybindId = 0;
int g_keybindActivationFrame = -1;
int g_widgetLanguage = 0;
constexpr int kMouseBindLeft = 1;
constexpr int kMouseBindRight = 2;
constexpr int kMouseBindMiddle = 3;
constexpr int kMouseBindX1 = 4;
constexpr int kMouseBindX2 = 5;

ImVec4 LerpVec4(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t);
}

float Clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }

    if (value > 1.0f)
    {
        return 1.0f;
    }

    return value;
}

float ClampFloat(float value, float minValue, float maxValue)
{
    if (value < minValue)
    {
        return minValue;
    }

    if (value > maxValue)
    {
        return maxValue;
    }

    return value;
}

float AnimateFloat(ImGuiID id, bool active, float speed = 10.0f)
{
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const float current = storage->GetFloat(id, active ? 1.0f : 0.0f);
    const float target = active ? 1.0f : 0.0f;
    const float t = Clamp01(ImGui::GetIO().DeltaTime * speed);
    const float next = current + (target - current) * t;
    storage->SetFloat(id, next);
    return next;
}

const char* WLang(const char* en, const char* ru, const char* es, const char* pt)
{
    struct CleanEntry
    {
        const char* en;
        const char* zh;
        const char* ja;
        const char* ko;
    };

    static const CleanEntry cleanEntries[] = {
        {"None", "无", "なし", "없음"},
        {"Mouse 1", "鼠标 1", "マウス 1", "마우스 1"},
        {"Mouse 2", "鼠标 2", "マウス 2", "마우스 2"},
        {"Mouse 3", "鼠标 3", "マウス 3", "마우스 3"},
        {"Mouse 4", "鼠标 4", "マウス 4", "마우스 4"},
        {"Mouse 5", "鼠标 5", "マウス 5", "마우스 5"},
        {"Unknown", "未知", "不明", "알 수 없음"},
        {"Press key...", "按下按键...", "キーを押してください...", "키를 누르세요..."}
    };

    if (g_widgetLanguage == 0 || g_widgetLanguage == 5 || g_widgetLanguage == 6)
    {
        for (const CleanEntry& entry : cleanEntries)
        {
            if (std::strcmp(entry.en, en) == 0)
            {
                if (g_widgetLanguage == 5) return entry.ja;
                if (g_widgetLanguage == 6) return entry.ko;
                return entry.zh;
            }
        }
    }

    struct Entry
    {
        const char* en;
        const char* zh;
        const char* ja;
        const char* ko;
    };

    static const Entry entries[] = {
        {"None", "无", "なし", "없음"},
        {"Mouse 1", "鼠标1", "マウス1", "마우스 1"},
        {"Mouse 2", "鼠标2", "マウス2", "마우스 2"},
        {"Mouse 3", "鼠标3", "マウス3", "마우스 3"},
        {"Mouse 4", "鼠标4", "マウス4", "마우스 4"},
        {"Mouse 5", "鼠标5", "マウス5", "마우스 5"},
        {"Unknown", "未知", "不明", "알 수 없음"},
        {"Press key...", "按下按键...", "キーを押してください...", "키를 누르세요..."}
    };

    switch (g_widgetLanguage)
    {
    case 1: return en;
    case 2: return ru;
    case 3: return es;
    case 4: return pt;
    case 5:
    case 6:
    case 0:
        for (const Entry& entry : entries)
        {
            if (std::strcmp(entry.en, en) == 0)
            {
                if (g_widgetLanguage == 5) return entry.ja;
                if (g_widgetLanguage == 6) return entry.ko;
                return entry.zh;
            }
        }
        return en;
    default: return en;
    }
}

ImVec2 Add(const ImVec2& a, const ImVec2& b)
{
    return ImVec2(a.x + b.x, a.y + b.y);
}

ImVec2 Subtract(const ImVec2& a, const ImVec2& b)
{
    return ImVec2(a.x - b.x, a.y - b.y);
}

ImU32 WidgetAlphaU32(ImU32 color)
{
    ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);
    rgba.w *= ImGui::GetStyle().Alpha;
    return ImGui::GetColorU32(rgba);
}

bool HandleSliderBehavior(const char* id, const ImRect& rect, float* normalizedValue, bool* hovered)
{
    ImGui::SetCursorScreenPos(rect.Min);
    ImGui::InvisibleButton(id, ImVec2(ImMax(rect.GetWidth(), 1.0f), ImMax(rect.GetHeight(), 1.0f)));
    *hovered = ImGui::IsItemHovered();

    if (ImGui::IsItemActive())
    {
        const float width = rect.Max.x - rect.Min.x;
        if (width > 0.0f)
        {
            *normalizedValue = Clamp01((ImGui::GetIO().MousePos.x - rect.Min.x) / width);
            return true;
        }
    }

    return false;
}

void DrawPremiumSlider(ImDrawList* drawList, const ImRect& rect, float normalizedValue, const ImVec4& accent, bool hovered)
{
    const ImGuiID itemId = ImGui::GetItemID();
    const bool active = ImGui::IsItemActive();
    const float hoverAnim = AnimateFloat(itemId ^ 0x15721u, hovered, 14.0f);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    const float target = Clamp01(normalizedValue);
    float displayed = storage->GetFloat(itemId ^ 0x77291u, target);
    displayed += (target - displayed) * Clamp01(ImGui::GetIO().DeltaTime * 14.0f);
    storage->SetFloat(itemId ^ 0x77291u, displayed);

    const float trackHeight = 5.0f;
    const float centerY = (rect.Min.y + rect.Max.y) * 0.5f;
    const ImRect track(
        ImVec2(rect.Min.x, centerY - trackHeight * 0.5f),
        ImVec2(rect.Max.x, centerY + trackHeight * 0.5f));
    const float fillX = ImLerp(track.Min.x, track.Max.x, displayed);

    drawList->AddRectFilled(track.Min, track.Max, WidgetAlphaU32(IM_COL32(27, 31, 39, 255)), 3.0f);
    drawList->AddRect(track.Min, track.Max, WidgetAlphaU32(IM_COL32(255, 255, 255, static_cast<int>(12 + hoverAnim * 18))), 3.0f, 0, 1.0f);

    if (fillX > track.Min.x + 0.5f)
        drawList->AddRectFilled(track.Min, ImVec2(fillX, track.Max.y), ImGui::GetColorU32(accent), 3.0f);

    const float thumbRadius = 5.0f + hoverAnim * 0.8f + (active ? 0.7f : 0.0f);
    const float thumbCenterX = ClampFloat(fillX, track.Min.x + thumbRadius, track.Max.x - thumbRadius);
    const ImVec2 thumbCenter(thumbCenterX, centerY);
    drawList->AddCircleFilled(thumbCenter, thumbRadius, WidgetAlphaU32(IM_COL32(245, 247, 250, 255)), 20);
    drawList->AddCircle(thumbCenter, thumbRadius, ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.72f)), 20, 1.0f);
}

void DrawCompactDarkSlider(ImDrawList* drawList, const ImRect& rect, float normalizedValue, bool hovered)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const ImGuiID itemId = ImGui::GetItemID();
    const bool active = ImGui::IsItemActive();
    const float hoverAnim = AnimateFloat(itemId ^ 0x95A1u, hovered, 15.0f);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    const float target = Clamp01(normalizedValue);
    float displayed = storage->GetFloat(itemId ^ 0x95C5u, target);
    displayed += (target - displayed) * Clamp01(ImGui::GetIO().DeltaTime * 16.0f);
    storage->SetFloat(itemId ^ 0x95C5u, displayed);

    const float centerY = (rect.Min.y + rect.Max.y) * 0.5f;
    const float trackH = 4.0f; // thinner track
    const ImRect track(ImVec2(rect.Min.x, centerY - trackH * 0.5f), ImVec2(rect.Max.x, centerY + trackH * 0.5f));
    const float fillX = ImLerp(track.Min.x, track.Max.x, displayed);

    drawList->AddRectFilled(track.Min, track.Max, WidgetAlphaU32(IM_COL32(27, 31, 39, 255)), 2.0f);
    drawList->AddRect(track.Min, track.Max, WidgetAlphaU32(IM_COL32(255, 255, 255, static_cast<int>(10 + hoverAnim * 18))), 2.0f, 0, 1.0f);
    if (fillX > track.Min.x + 0.5f)
        drawList->AddRectFilled(track.Min, ImVec2(fillX, track.Max.y), ImGui::GetColorU32(palette.accent), 2.0f);

    const float thumbRadius = 4.5f + hoverAnim * 0.7f + (active ? 0.6f : 0.0f);
    const float thumbCenterX = ClampFloat(fillX, track.Min.x + thumbRadius, track.Max.x - thumbRadius);
    const ImVec2 thumbCenter(thumbCenterX, centerY);

    drawList->AddCircleFilled(thumbCenter, thumbRadius, WidgetAlphaU32(IM_COL32(225, 229, 238, 255)), 24);
    drawList->AddCircle(thumbCenter, thumbRadius, ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.62f)), 24, 1.0f);
}

bool DrawFlatCombo(const char* id, int* currentItem, const char* const items[], int itemCount, float width)
{
    if (currentItem == nullptr || items == nullptr || itemCount <= 0)
        return false;

    if (*currentItem < 0)
        *currentItem = 0;
    if (*currentItem >= itemCount)
        *currentItem = itemCount - 1;

    ImGui::PushID(id);

    const float boxWidth = ImMax(width, 48.0f);
    const float boxHeight = 22.0f;
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 size(boxWidth, boxHeight);
    const bool pressed = ImGui::InvisibleButton("##flat_combo_btn", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const auto& palette = odyssey::theme::GetPalette();
    const ImU32 bgColor = held 
        ? ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.15f))
        : ImGui::GetColorU32(ImVec4(palette.panel.x, palette.panel.y, palette.panel.z, 0.95f));
    const ImU32 borderColor = held 
        ? ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.80f))
        : ImGui::GetColorU32(ImVec4(palette.panelBorder.x, palette.panelBorder.y, palette.panelBorder.z, hovered ? 0.90f : 0.40f));
        
    const ImU32 textColor = hovered || held ? ImGui::GetColorU32(palette.text) : ImGui::GetColorU32(palette.textMuted);
    const ImU32 arrowColor = hovered || held ? ImGui::GetColorU32(palette.accent) : ImGui::GetColorU32(palette.textMuted);
    const float splitX = min.x + boxWidth - 24.0f;
    const float rounding = 4.0f;

    // Main Box
    drawList->AddRectFilled(min, Add(min, size), bgColor, rounding);
    drawList->AddRect(min, Add(min, size), borderColor, rounding, ImDrawFlags_RoundCornersAll, held ? 1.5f : 1.0f);
    
    // Split line
    drawList->AddLine(ImVec2(splitX, min.y + 2.0f), ImVec2(splitX, min.y + boxHeight - 2.0f), WidgetAlphaU32(IM_COL32(255, 255, 255, 12)), 1.0f);

    const char* preview = items[*currentItem];
    const ImVec2 textPos(min.x + 8.0f, min.y + (boxHeight - ImGui::GetTextLineHeight()) * 0.5f - 0.5f);
    drawList->AddText(textPos, textColor, preview);

    // Chevron Arrow
    const ImVec2 arrowCenter(splitX + 12.0f, min.y + boxHeight * 0.5f + 1.0f);
    drawList->AddLine(arrowCenter + ImVec2(-3.5f, -2.0f), arrowCenter + ImVec2(0.0f, 1.5f), arrowColor, 1.5f);
    drawList->AddLine(arrowCenter + ImVec2(0.0f, 1.5f), arrowCenter + ImVec2(3.5f, -2.0f), arrowColor, 1.5f);

    if (pressed)
        ImGui::OpenPopup("##flat_combo_popup");

    if (ImGui::IsPopupOpen("##flat_combo_popup"))
    {
        const float popupHeight = ImMin(static_cast<float>(itemCount), 8.0f) * 24.0f + 8.0f;
        ImGui::SetNextWindowPos(ImVec2(min.x, min.y + boxHeight + 4.0f));
        ImGui::SetNextWindowSize(ImVec2(boxWidth, popupHeight));
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, WidgetAlphaU32(IM_COL32(22, 24, 28, 250))); // Lighter drop down bg
    ImGui::PushStyleColor(ImGuiCol_Border, WidgetAlphaU32(IM_COL32(255, 255, 255, 25)));
    ImGui::PushStyleColor(ImGuiCol_Header, WidgetAlphaU32(IM_COL32(255, 255, 255, 15)));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, WidgetAlphaU32(IM_COL32(255, 255, 255, 30)));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, WidgetAlphaU32(IM_COL32(255, 255, 255, 40)));
    ImGui::PushStyleColor(ImGuiCol_Text, WidgetAlphaU32(IM_COL32(210, 215, 225, 255)));

    bool valueChanged = false;
    if (ImGui::BeginPopup("##flat_combo_popup", ImGuiWindowFlags_NoMove))
    {
        for (int i = 0; i < itemCount; ++i)
        {
            const bool selected = (*currentItem == i);
            if (ImGui::Selectable(items[i], selected))
            {
                *currentItem = i;
                valueChanged = true;
                ImGui::CloseCurrentPopup();
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);

    ImGui::PopID();
    return valueChanged;
}

const char* GetKeybindDisplayName(int key)
{
    switch (key)
    {
    case 0: return WLang("None", "Net", "Ninguna", "Nenhuma");
    case kMouseBindLeft: return WLang("Mouse 1", "Mysh 1", "Mouse 1", "Mouse 1");
    case kMouseBindRight: return WLang("Mouse 2", "Mysh 2", "Mouse 2", "Mouse 2");
    case kMouseBindMiddle: return WLang("Mouse 3", "Mysh 3", "Mouse 3", "Mouse 3");
    case kMouseBindX1: return WLang("Mouse 4", "Mysh 4", "Mouse 4", "Mouse 4");
    case kMouseBindX2: return WLang("Mouse 5", "Mysh 5", "Mouse 5", "Mouse 5");
    default:
        break;
    }

    const char* name = key >= static_cast<int>(ImGuiKey_NamedKey_BEGIN) && key < static_cast<int>(ImGuiKey_NamedKey_END)
        ? ImGui::GetKeyName(static_cast<ImGuiKey>(key))
        : nullptr;
    return (name != nullptr && name[0] != '\0') ? name : WLang("Unknown", "Neizvestno", "Desconocido", "Desconhecido");
}
}

namespace odyssey::widgets
{
void SetLanguage(int languageIndex)
{
    if (languageIndex < 0) languageIndex = 0;
    if (languageIndex > 6) languageIndex = 6;
    g_widgetLanguage = languageIndex;
}

FloatingRootContext BeginFloatingRoot(const char* id, ImVec2* position, const ImVec2& size, const ImVec2& viewportPos, const ImVec2& viewportSize)
{
    FloatingRootContext context{};
    context.size = size;

    static bool railDragging = false;
    static ImVec2 dragOffset{};

    position->x = ClampFloat(position->x, viewportPos.x + 20.0f, viewportPos.x + viewportSize.x - size.x - 20.0f);
    position->y = ClampFloat(position->y, viewportPos.y + 20.0f, viewportPos.y + viewportSize.y - size.y - 20.0f);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    constexpr float kRailWidth = 84.0f;

    const ImVec2 rootMin = *position;
    const ImVec2 rootMax = Add(rootMin, size);
    const ImRect railRect(rootMin, ImVec2(rootMin.x + kRailWidth, rootMax.y));

    if (!railDragging && ImGui::IsMouseHoveringRect(railRect.Min, railRect.Max, false) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        railDragging = true;
        dragOffset = Subtract(ImGui::GetIO().MousePos, *position);
    }

    if (railDragging)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            *position = Subtract(ImGui::GetIO().MousePos, dragOffset);
            position->x = ClampFloat(position->x, viewportPos.x + 20.0f, viewportPos.x + viewportSize.x - size.x - 20.0f);
            position->y = ClampFloat(position->y, viewportPos.y + 20.0f, viewportPos.y + viewportSize.y - size.y - 20.0f);
        }
        else
        {
            railDragging = false;
        }
    }

    context.position = *position;
    const ImVec2 drawRootMin = *position;
    const ImVec2 drawRootMax = Add(drawRootMin, size);
    const ImVec4 rootFill = ImVec4(palette.bgBottom.x, palette.bgBottom.y, palette.bgBottom.z, 0.52f);
    const ImVec4 rootBorder = ImVec4(palette.panelBorder.x, palette.panelBorder.y, palette.panelBorder.z, 0.95f);
    const ImVec4 railFill = ImVec4(palette.bgTop.x, palette.bgTop.y, palette.bgTop.z, 0.78f);
    const ImVec4 innerFill = ImVec4(palette.panel.x, palette.panel.y, palette.panel.z, 0.86f);
    const ImVec4 innerBorder = ImVec4(palette.panelBorder.x, palette.panelBorder.y, palette.panelBorder.z, 0.85f);

    drawList->AddRectFilled(drawRootMin, drawRootMax, ImGui::GetColorU32(rootFill), 12.0f);
    drawList->AddRect(drawRootMin, drawRootMax, ImGui::GetColorU32(rootBorder), 12.0f, 0, 1.0f);
    drawList->AddRectFilled(drawRootMin, ImVec2(drawRootMin.x + kRailWidth, drawRootMax.y), ImGui::GetColorU32(railFill), 12.0f, ImDrawFlags_RoundCornersLeft);
    // Inset X coordinates by corner radius (12.0f) to avoid gradient lines bleeding out of rounded corners
    drawList->AddRectFilledMultiColor(ImVec2(drawRootMin.x + 12.0f, drawRootMin.y), ImVec2(drawRootMax.x - 12.0f, drawRootMin.y + 1.0f), WidgetAlphaU32(IM_COL32(255, 255, 255, 0)), ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.18f)), ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.18f)), WidgetAlphaU32(IM_COL32(255, 255, 255, 0)));
    drawList->AddRectFilledMultiColor(ImVec2(drawRootMin.x + 12.0f, drawRootMax.y - 1.0f), ImVec2(drawRootMax.x - 12.0f, drawRootMax.y), WidgetAlphaU32(IM_COL32(255, 255, 255, 0)), ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.15f)), ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.15f)), WidgetAlphaU32(IM_COL32(255, 255, 255, 0)));
    drawList->AddLine(ImVec2(drawRootMin.x + kRailWidth, drawRootMin.y + 14.0f), ImVec2(drawRootMin.x + kRailWidth, drawRootMax.y - 14.0f), WidgetAlphaU32(IM_COL32(255, 255, 255, 20)), 1.0f);

    const ImVec2 innerMin = Add(drawRootMin, ImVec2(kRailWidth + 14.0f, 12.0f));
    const ImVec2 innerMax = Subtract(drawRootMax, ImVec2(12.0f, 12.0f));
    drawList->AddRectFilled(innerMin, innerMax, ImGui::GetColorU32(innerFill), 8.0f);
    drawList->AddRect(innerMin, innerMax, ImGui::GetColorU32(innerBorder), 8.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(drawRootMin);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, WidgetAlphaU32(IM_COL32(0, 0, 0, 0)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
    context.open = ImGui::BeginChild("##odyssey_root", ImVec2(ImMax(size.x, 1.0f), ImMax(size.y, 1.0f)), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    return context;
}

void EndFloatingRoot()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

bool BeginPanel(const char* id, const ImVec2& position, const ImVec2& size, const char* title, const char* subtitle)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const bool hasHeader = title != nullptr && title[0] != '\0';

    const ImVec2 panelMin = position;
    const ImVec2 panelMax = Add(position, size);

    odyssey::theme::DrawSectionFrame(drawList, panelMin, panelMax, title, fonts.title);

    if (subtitle != nullptr && subtitle[0] != '\0')
    {
        drawList->AddText(fonts.uiSmall, fonts.uiSmall->FontSize, Add(panelMin, ImVec2(12.0f, 20.0f)), ImGui::GetColorU32(palette.textMuted), subtitle);
    }

    ImGui::SetCursorScreenPos(Add(panelMin, ImVec2(10.0f, hasHeader ? 32.0f : 8.0f))); // Reduced header gap
    ImGui::PushStyleColor(ImGuiCol_ChildBg, WidgetAlphaU32(IM_COL32(0, 0, 0, 0)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f)); // Reduced item spacing between widgets
    const ImVec2 childSize = Subtract(size, ImVec2(20.0f, hasHeader ? 40.0f : 14.0f));
    return ImGui::BeginChild(id, ImVec2(ImMax(childSize.x, 1.0f), ImMax(childSize.y, 1.0f)), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
}

void EndPanel()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

bool SidebarItem(const char* id, const char* label, const char* hint, bool active)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    const ImVec2 size(ImMax(ImGui::GetContentRegionAvail().x, 1.0f), 38.0f);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max = Add(min, size);
    const ImGuiID itemId = window->GetID(id);
    const bool pressed = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();

    const float t = AnimateFloat(itemId, active || hovered, 10.0f);
    const float activeT = AnimateFloat(window->GetID((std::string(id) + "_active").c_str()), active, 8.0f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const bool hasHint = hint != nullptr && hint[0] != '\0';

    const ImVec4 bgBase = active ? ImVec4(0.15f, 0.16f, 0.20f, 0.80f) : ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    const ImVec4 bgHover = active ? ImVec4(0.18f, 0.19f, 0.23f, 0.90f) : ImVec4(0.09f, 0.10f, 0.13f, 0.48f);
    const ImVec4 bg = ImLerp(bgBase, bgHover, t);
    const float glowInset = 2.0f;
    const ImVec2 glowMin = Add(min, ImVec2(glowInset, glowInset));
    const ImVec2 glowMax = Subtract(max, ImVec2(glowInset, glowInset));

    if (activeT > 0.01f)
    {
        drawList->AddRectFilled(
            glowMin,
            glowMax,
            ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.045f * activeT)),
            8.0f);
        drawList->AddRect(
            glowMin,
            glowMax,
            ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.16f * activeT)),
            8.0f,
            0,
            1.0f);
    }
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(bg), 6.0f);
    drawList->AddRect(min, max, ImGui::GetColorU32(active ? ImVec4(1, 1, 1, 0.14f) : ImVec4(1, 1, 1, 0.03f + 0.05f * t)), 6.0f, 0, 1.0f);
    if (active)
    {
        drawList->AddRectFilled(min, ImVec2(min.x + 2.0f, max.y), ImGui::GetColorU32(palette.accent), 6.0f, ImDrawFlags_RoundCornersLeft);
        // Offset Y slightly to avoid gradient bleeding out of the rounded corners
        drawList->AddRectFilledMultiColor(
            Add(min, ImVec2(2.0f, 2.0f)),
            Add(min, ImVec2(30.0f, size.y - 2.0f)),
            ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.10f * activeT)),
            WidgetAlphaU32(IM_COL32(255, 255, 255, 0)),
            WidgetAlphaU32(IM_COL32(255, 255, 255, 0)),
            ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.10f * activeT)));
    }
    drawList->AddCircleFilled(Add(min, ImVec2(12.0f, 19.0f)), 4.0f, ImGui::GetColorU32(active ? palette.accent : ImVec4(palette.textMuted.x, palette.textMuted.y, palette.textMuted.z, 0.30f + 0.22f * t)), 24);
    drawList->AddText(
        fonts.ui,
        fonts.ui->FontSize,
        Add(min, hasHint ? ImVec2(24.0f, 8.0f) : ImVec2(24.0f, 9.0f)),
        ImGui::GetColorU32(active ? palette.text : ImLerp(palette.textMuted, palette.text, t)),
        label);

    if (hasHint)
    {
        drawList->AddText(fonts.uiSmall, fonts.uiSmall->FontSize, Add(min, ImVec2(24.0f, 22.0f)), ImGui::GetColorU32(palette.textMuted), hint);
    }

    if (active)
    {
        drawList->AddCircleFilled(Add(min, ImVec2(size.x - 14.0f, 19.0f)), 5.0f, ImGui::GetColorU32(palette.accent), 24);
        drawList->AddCircle(Add(min, ImVec2(size.x - 14.0f, 19.0f)), 8.0f, ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.20f * activeT)), 24, 1.0f);
    }

    return pressed;
}

bool TopTab(const char* id, const char* label, bool active)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    const ImVec2 size(76.0f, 24.0f);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImGuiID itemId = window->GetID(id);
    const bool pressed = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const float t = AnimateFloat(itemId, active || hovered, 10.0f);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (active)
    {
        drawList->AddRectFilled(Add(min, ImVec2(-8.0f, -2.0f)), Add(min, ImVec2(56.0f, 22.0f)), ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.06f)), 6.0f);
    }
    drawList->AddText(fonts.title, fonts.title->FontSize, Add(min, ImVec2(1.0f, 1.0f)), ImGui::GetColorU32(active ? palette.accent : ImLerp(palette.textMuted, palette.text, t * 0.45f)), label);

    if (active)
    {
        drawList->AddLine(Add(min, ImVec2(0.0f, 23.0f)), Add(min, ImVec2(40.0f, 23.0f)), ImGui::GetColorU32(palette.accent), 2.0f);
        drawList->AddLine(Add(min, ImVec2(0.0f, 23.0f)), Add(min, ImVec2(52.0f, 23.0f)), ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.12f)), 4.0f);
    }

    return pressed;
}

void ToggleRow(const char* id, const char* label, const char* hint, bool* value)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    ImGui::PushID(id);
    const bool hasHint = hint != nullptr && hint[0] != '\0';
    const float rowHeight = hasHint ? 36.0f : 28.0f;
    const ImVec2 rowSize(ImMax(ImGui::GetContentRegionAvail().x, 1.0f), rowHeight);
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowMax = Add(rowMin, rowSize);
    const ImGuiID itemId = window->GetID("toggle_row");
    const bool pressed = ImGui::InvisibleButton("toggle_row", rowSize);
    const bool hovered = ImGui::IsItemHovered();

    if (pressed)
    {
        *value = !*value;
    }

    const float t = AnimateFloat(itemId, *value, 12.0f);
    const float hoverT = AnimateFloat(window->GetID("toggle_hover"), hovered, 12.0f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const float switchY = hasHint ? 9.0f : 6.0f;
    const float labelY = hasHint ? 2.0f : 6.0f;
    const ImVec2 switchMin = Add(rowMin, ImVec2(rowSize.x - 42.0f, switchY));
    const ImVec2 switchMax = Add(switchMin, ImVec2(34.0f, 18.0f));
    const ImVec2 knobCenter = Add(switchMin, ImVec2(8.0f + 16.0f * t, 9.0f));

    drawList->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(ImVec4(1, 1, 1, hovered ? 0.012f : 0.0f)), 6.0f);
    drawList->AddLine(Add(rowMin, ImVec2(0.0f, rowSize.y - 1.0f)), Add(rowMax, ImVec2(0.0f, -1.0f)), WidgetAlphaU32(IM_COL32(255, 255, 255, 10)), 1.0f);
    drawList->AddText(fonts.ui, fonts.ui->FontSize, Add(rowMin, ImVec2(0.0f, labelY)), ImGui::GetColorU32(ImLerp(palette.textMuted, palette.text, 0.55f + hoverT * 0.35f)), label);

    if (hasHint)
    {
        drawList->AddText(fonts.uiSmall, fonts.uiSmall->FontSize, Add(rowMin, ImVec2(0.0f, 20.0f)), ImGui::GetColorU32(palette.textMuted), hint);
    }

    const ImVec4 switchBg = LerpVec4(ImVec4(0.10f, 0.11f, 0.14f, 0.90f), ImVec4(0.27f, 0.24f, 0.43f, 0.98f), t);
    drawList->AddRectFilled(
        Subtract(switchMin, ImVec2(1.0f, 1.0f)),
        Add(switchMax, ImVec2(1.0f, 1.0f)),
        ImGui::GetColorU32(ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.06f * t)),
        999.0f);
    drawList->AddRectFilled(switchMin, switchMax, ImGui::GetColorU32(switchBg), 999.0f);
    drawList->AddRect(switchMin, switchMax, ImGui::GetColorU32(ImLerp(ImVec4(1, 1, 1, 0.06f), palette.accentSoft, t)), 999.0f);
    drawList->AddLine(
        Add(switchMin, ImVec2(5.0f, 9.0f)),
        Add(switchMax, ImVec2(-5.0f, -9.0f)),
        WidgetAlphaU32(IM_COL32(255, 255, 255, 18)),
        1.0f);
    drawList->AddCircleFilled(knobCenter, 6.2f, WidgetAlphaU32(IM_COL32(238, 241, 249, 255)), 24);
    drawList->AddCircle(knobCenter, 6.2f, WidgetAlphaU32(IM_COL32(0, 0, 0, 45)), 24, 1.0f);
    if (*value)
    {
        drawList->AddCircleFilled(knobCenter, 2.2f, ImGui::GetColorU32(palette.accent), 16);
    }

    ImGui::PopID();
}

void KeybindCaptureRow(const char* id, const char* label, int* key)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    ImGui::PushID(id);

    const float rowWidth = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    const float buttonWidth = 108.0f;
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(rowWidth, 28.0f);
    const ImGuiID rowId = window->GetID("keybind_row");
    const ImVec2 buttonMin = Add(rowMin, ImVec2(rowWidth - buttonWidth, 0.0f));
    const ImVec2 buttonMax = Add(buttonMin, ImVec2(buttonWidth, 28.0f));

    ImGui::InvisibleButton("keybind_row", rowSize);
    const bool rowHovered = ImGui::IsItemHovered();

    if (ImGui::IsMouseHoveringRect(buttonMin, buttonMax, false) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        g_activeKeybindId = rowId;
        g_keybindActivationFrame = ImGui::GetFrameCount();
    }

    const bool capturing = g_activeKeybindId == rowId;
    if (capturing && ImGui::GetFrameCount() > g_keybindActivationFrame)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            *key = static_cast<int>(ImGuiKey_None);
            g_activeKeybindId = 0;
        }
        else
        {
            for (int mouseButton = 0; mouseButton < 5; ++mouseButton)
            {
                if (ImGui::IsMouseClicked(mouseButton))
                {
                    *key = kMouseBindLeft + mouseButton;
                    g_activeKeybindId = 0;
                    break;
                }
            }

            if (g_activeKeybindId == rowId)
            {
                for (int keyIndex = static_cast<int>(ImGuiKey_NamedKey_BEGIN); keyIndex < static_cast<int>(ImGuiKey_NamedKey_END); ++keyIndex)
                {
                    if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(keyIndex), false))
                    {
                        *key = keyIndex;
                        g_activeKeybindId = 0;
                        break;
                    }
                }
            }
        }
    }

    drawList->AddLine(Add(rowMin, ImVec2(0.0f, rowSize.y - 1.0f)), Add(rowMin, ImVec2(rowWidth, rowSize.y - 1.0f)), WidgetAlphaU32(IM_COL32(255, 255, 255, 10)), 1.0f);
    drawList->AddText(fonts.ui, fonts.ui->FontSize, Add(rowMin, ImVec2(0.0f, 4.0f)), ImGui::GetColorU32(palette.text), label);

    const float rounding = 10.0f; // Pill shape
    float pulse = 0.0f;
    if (capturing) {
        float time = (float)ImGui::GetTime();
        pulse = (sinf(time * 6.0f) + 1.0f) * 0.5f;
    }

    const ImVec4 frameColor = capturing 
        ? ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.15f + pulse * 0.20f)
        : ImVec4(palette.panel.x, palette.panel.y, palette.panel.z, 0.95f);
    const ImVec4 borderColor = capturing 
        ? ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.80f + pulse * 0.20f)
        : ImVec4(palette.panelBorder.x, palette.panelBorder.y, palette.panelBorder.z, rowHovered ? 0.90f : 0.40f);
        
    drawList->AddRectFilled(buttonMin, buttonMax, ImGui::GetColorU32(frameColor), rounding);
    drawList->AddRect(buttonMin, buttonMax, ImGui::GetColorU32(borderColor), rounding, ImDrawFlags_RoundCornersAll, capturing ? 1.5f : 1.0f);

    const char* valueText = capturing ? WLang("Press key...", "Nazhmi klavishu...", "Pulsa una tecla...", "Pressione uma tecla...") : GetKeybindDisplayName(*key);
    const ImVec2 valueSize = ImGui::CalcTextSize(valueText);
    
    drawList->AddText(
        fonts.uiSmall,
        fonts.uiSmall->FontSize,
        ImVec2(buttonMin.x + ImFloor((buttonWidth - valueSize.x) * 0.5f), buttonMin.y + ImFloor((28.0f - valueSize.y) * 0.5f) - 1.0f),
        ImGui::GetColorU32(capturing ? palette.accent : (rowHovered ? palette.text : palette.textMuted)),
        valueText);

    ImGui::Dummy(rowSize);
    ImGui::PopID();
}

void CompactComboRow(const char* label, int* currentItem, const char* const items[], int itemCount, float width)
{
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - width);

    DrawFlatCombo((std::string("##compact_") + label).c_str(), currentItem, items, itemCount, width);
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
}

void CompactComboInlineRow(const char* label, int* currentItem, const char* const items[], int itemCount, float width)
{
    const float rowWidth = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    const float comboWidth = ImClamp(width, 72.0f, rowWidth - 70.0f);
    const float startX = ImGui::GetCursorPosX();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::SetCursorPosX(startX + rowWidth - comboWidth);

    DrawFlatCombo((std::string("##compact_inline_") + label).c_str(), currentItem, items, itemCount, comboWidth);
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

void CompactSliderIntRow(const char* label, int* value, int minValue, int maxValue, float width)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGui::PushID(label);

    const float sliderWidth = width <= 0.0f ? ImMax(ImGui::GetContentRegionAvail().x, 1.0f) : width;
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(sliderWidth, 18.0f); // Reduced height from 24 to 18
    const std::string valueText = std::to_string(*value);
    const ImVec2 valueTextSize = ImGui::CalcTextSize(valueText.c_str());

    drawList->AddText(fonts.ui, fonts.ui->FontSize, rowMin, ImGui::GetColorU32(palette.text), label);
    drawList->AddText(
        fonts.uiSmall,
        fonts.uiSmall->FontSize,
        Add(rowMin, ImVec2(sliderWidth - valueTextSize.x, 0.0f)),
        ImGui::GetColorU32(palette.textMuted),
        valueText.c_str());

    const ImVec2 sliderMin = Add(rowMin, ImVec2(0.0f, 13.0f)); // Adjusted Y pos
    const ImRect rect(sliderMin, Add(sliderMin, ImVec2(sliderWidth, 5.0f))); // Reduced track height
    float normalized = static_cast<float>(*value - minValue) / static_cast<float>(maxValue - minValue);
    normalized = Clamp01(normalized);
    bool hovered = false;
    if (HandleSliderBehavior("##compact_slider", rect, &normalized, &hovered))
    {
        *value = minValue + static_cast<int>((maxValue - minValue) * normalized + 0.5f);
    }

    DrawPremiumSlider(drawList, rect, normalized, palette.accent, hovered);
    ImGui::Dummy(rowSize);
    ImGui::PopID();
}

void CompactSliderIntInlineRow(const char* label, int* value, int minValue, int maxValue, float width)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGui::PushID(label);

    const float rowWidth = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    const float sliderWidth = ImClamp(width, 88.0f, rowWidth - 68.0f);
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(rowWidth, 20.0f);

    const std::string valueText = std::to_string(*value);
    drawList->AddText(fonts.ui, fonts.ui->FontSize, Add(rowMin, ImVec2(0.0f, 1.0f)), ImGui::GetColorU32(palette.text), label);

    const float sliderX = rowMin.x + rowWidth - sliderWidth;
    const ImRect rect(ImVec2(sliderX, rowMin.y + 11.0f), ImVec2(sliderX + sliderWidth, rowMin.y + 15.0f));
    float normalized = static_cast<float>(*value - minValue) / static_cast<float>(maxValue - minValue);
    normalized = Clamp01(normalized);
    bool hovered = false;
    if (HandleSliderBehavior("##compact_inline_slider", rect, &normalized, &hovered))
        *value = minValue + static_cast<int>((maxValue - minValue) * normalized + 0.5f);

    DrawCompactDarkSlider(drawList, rect, normalized, hovered);
    const ImVec2 valueTextSize = ImGui::CalcTextSize(valueText.c_str());
    drawList->AddText(
        fonts.uiSmall,
        fonts.uiSmall->FontSize,
        Add(rowMin, ImVec2(rowWidth - valueTextSize.x, -2.0f)),
        ImGui::GetColorU32(palette.textMuted),
        valueText.c_str());
    ImGui::Dummy(rowSize);
    ImGui::PopID();
}

void CompactSliderFloatRow(const char* label, float* value, float minValue, float maxValue, const char* format, float width)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGui::PushID(label);

    const float sliderWidth = width <= 0.0f ? ImMax(ImGui::GetContentRegionAvail().x, 1.0f) : width;
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(sliderWidth, 18.0f); // Reduced height from 24 to 18

    char valueBuffer[32];
    sprintf_s(valueBuffer, format, *value);
    const ImVec2 valueTextSize = ImGui::CalcTextSize(valueBuffer);

    drawList->AddText(fonts.ui, fonts.ui->FontSize, rowMin, ImGui::GetColorU32(palette.text), label);
    drawList->AddText(
        fonts.uiSmall,
        fonts.uiSmall->FontSize,
        Add(rowMin, ImVec2(sliderWidth - valueTextSize.x, 0.0f)),
        ImGui::GetColorU32(palette.textMuted),
        valueBuffer);

    const ImVec2 sliderMin = Add(rowMin, ImVec2(0.0f, 13.0f)); // Adjusted Y pos
    const ImRect rect(sliderMin, Add(sliderMin, ImVec2(sliderWidth, 5.0f))); // Reduced track height
    float normalized = (*value - minValue) / (maxValue - minValue);
    normalized = Clamp01(normalized);
    bool hovered = false;
    if (HandleSliderBehavior("##compact_float_slider", rect, &normalized, &hovered))
    {
        *value = minValue + (maxValue - minValue) * normalized;
    }

    DrawPremiumSlider(drawList, rect, normalized, palette.accent, hovered);
    ImGui::Dummy(rowSize);
    ImGui::PopID();
}

void CompactSliderFloatInlineRow(const char* label, float* value, float minValue, float maxValue, const char* format, float width)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGui::PushID(label);

    const float rowWidth = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    const float maxSliderWidth = ImMax(rowWidth - 68.0f, 88.0f);
    const float sliderWidth = width <= 0.0f ? maxSliderWidth : ImMin(ImMax(width, 88.0f), maxSliderWidth);
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(rowWidth, 20.0f);

    char valueBuffer[32];
    sprintf_s(valueBuffer, format, *value);
    drawList->AddText(fonts.ui, fonts.ui->FontSize, Add(rowMin, ImVec2(0.0f, 1.0f)), ImGui::GetColorU32(palette.text), label);

    const float sliderX = rowMin.x + rowWidth - sliderWidth;
    const ImRect rect(ImVec2(sliderX, rowMin.y + 11.0f), ImVec2(sliderX + sliderWidth, rowMin.y + 15.0f));
    float normalized = (*value - minValue) / (maxValue - minValue);
    normalized = Clamp01(normalized);
    bool hovered = false;
    if (HandleSliderBehavior("##compact_inline_float_slider", rect, &normalized, &hovered))
        *value = minValue + (maxValue - minValue) * normalized;

    DrawCompactDarkSlider(drawList, rect, normalized, hovered);
    const ImVec2 valueTextSize = ImGui::CalcTextSize(valueBuffer);
    drawList->AddText(
        fonts.uiSmall,
        fonts.uiSmall->FontSize,
        Add(rowMin, ImVec2(rowWidth - valueTextSize.x, -2.0f)),
        ImGui::GetColorU32(palette.textMuted),
        valueBuffer);
    ImGui::Dummy(rowSize);
    ImGui::PopID();
}

void CompactSliderFloatInputRow(
    const char* id,
    const char* label,
    float* value,
    float minValue,
    float maxValue,
    float step,
    const char* inputFormat,
    float width,
    const float* discreteValues,
    int discreteValueCount)
{
    if (!id || !label || !value || !std::isfinite(minValue) || !std::isfinite(maxValue) || maxValue <= minValue)
        return;

    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGui::PushID(id);

    const bool useDiscreteValues = discreteValues && discreteValueCount > 0;
    auto nearestDiscreteIndex = [&](float candidate) {
        int nearest = 0;
        float nearestDistance = FLT_MAX;
        for (int i = 0; i < discreteValueCount; ++i) {
            const float distance = std::fabs(candidate - discreteValues[i]);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = i;
            }
        }
        return nearest;
    };
    auto snapValue = [&](float candidate) {
        if (!std::isfinite(candidate))
            candidate = minValue;
        candidate = ImClamp(candidate, minValue, maxValue);
        if (useDiscreteValues)
            return ImClamp(discreteValues[nearestDiscreteIndex(candidate)], minValue, maxValue);
        if (step > 0.0f)
            candidate = minValue + std::round((candidate - minValue) / step) * step;
        return ImClamp(candidate, minValue, maxValue);
    };

    const float rowWidth = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    const float maxControlWidth = ImMax(rowWidth - 96.0f, 190.0f);
    const float controlWidth = width <= 0.0f
        ? maxControlWidth
        : ImMin(ImMax(width, 190.0f), maxControlWidth);
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(rowWidth, 24.0f);
    const float buttonWidth = 20.0f;
    const float inputWidth = 58.0f;
    const float gap = 4.0f;
    const float sliderWidth = ImMax(controlWidth - buttonWidth * 2.0f - inputWidth - gap * 3.0f, 72.0f);
    const float controlX = rowMin.x + rowWidth - controlWidth;

    drawList->AddText(
        fonts.ui,
        fonts.ui->FontSize,
        Add(rowMin, ImVec2(0.0f, 3.0f)),
        ImGui::GetColorU32(palette.text),
        label);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID editValueId = ImGui::GetID("##numeric_edit_value");
    const ImGuiID editingId = ImGui::GetID("##numeric_editing");
    bool editing = storage->GetBool(editingId, false);
    if (!editing) {
        *value = snapValue(*value);
        storage->SetFloat(editValueId, *value);
    }

    ImGui::SetCursorScreenPos(ImVec2(controlX, rowMin.y + 2.0f));
    const bool minusPressed = ActionButton("minus", "-", ImVec2(buttonWidth, 20.0f));

    const float sliderX = controlX + buttonWidth + gap;
    const ImRect sliderRect(
        ImVec2(sliderX, rowMin.y + 10.0f),
        ImVec2(sliderX + sliderWidth, rowMin.y + 14.0f));
    float normalized = 0.0f;
    if (useDiscreteValues && discreteValueCount > 1)
        normalized = static_cast<float>(nearestDiscreteIndex(*value)) / static_cast<float>(discreteValueCount - 1);
    else
        normalized = (*value - minValue) / (maxValue - minValue);
    normalized = Clamp01(normalized);
    bool sliderHovered = false;
    const bool sliderChanged = HandleSliderBehavior("##numeric_slider", sliderRect, &normalized, &sliderHovered);
    if (sliderChanged) {
        if (useDiscreteValues) {
            const int index = ImClamp(
                static_cast<int>(std::round(normalized * static_cast<float>(discreteValueCount - 1))),
                0,
                discreteValueCount - 1);
            *value = discreteValues[index];
        }
        else {
            *value = snapValue(minValue + (maxValue - minValue) * normalized);
        }
        storage->SetBool(editingId, false);
        storage->SetFloat(editValueId, *value);
        editing = false;
    }
    DrawCompactDarkSlider(drawList, sliderRect, normalized, sliderHovered);

    const float inputX = sliderX + sliderWidth + gap;
    ImGui::SetCursorScreenPos(ImVec2(inputX, rowMin.y + 1.0f));
    ImGui::SetNextItemWidth(inputWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(palette.panel.x + 0.025f, palette.panel.y + 0.025f, palette.panel.z + 0.03f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.10f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.22f));
    float editValue = storage->GetFloat(editValueId, *value);
    const bool submitted = ImGui::InputFloat(
        "##numeric_input",
        &editValue,
        0.0f,
        0.0f,
        inputFormat ? inputFormat : "%.0f",
        ImGuiInputTextFlags_CharsDecimal |
            ImGuiInputTextFlags_AutoSelectAll |
            ImGuiInputTextFlags_EnterReturnsTrue);
    storage->SetFloat(editValueId, editValue);
    const bool inputActive = ImGui::IsItemActive();
    const bool inputFinished = submitted || (editing && ImGui::IsItemDeactivatedAfterEdit());
    if (inputActive) {
        storage->SetBool(editingId, true);
        editing = true;
    }
    if (inputFinished) {
        *value = snapValue(editValue);
        storage->SetFloat(editValueId, *value);
        storage->SetBool(editingId, false);
        editing = false;
    }
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(3);

    const float plusX = inputX + inputWidth + gap;
    ImGui::SetCursorScreenPos(ImVec2(plusX, rowMin.y + 2.0f));
    const bool plusPressed = ActionButton("plus", "+", ImVec2(buttonWidth, 20.0f));

    if (minusPressed || plusPressed) {
        float nextValue = *value;
        if (useDiscreteValues) {
            int index = nearestDiscreteIndex(nextValue);
            index = ImClamp(index + (plusPressed ? 1 : -1), 0, discreteValueCount - 1);
            nextValue = discreteValues[index];
        }
        else {
            nextValue += plusPressed ? step : -step;
        }
        *value = snapValue(nextValue);
        storage->SetFloat(editValueId, *value);
        storage->SetBool(editingId, false);
    }

    ImGui::SetCursorScreenPos(Add(rowMin, ImVec2(0.0f, rowSize.y)));
    ImGui::Dummy(ImVec2(rowWidth, 0.0f));
    ImGui::PopID();
}

void CompactCheckboxRow(const char* id, const char* label, bool* value)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();

    ImGui::PushID(id);
    const float width = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(width, 24.0f);
    const bool pressed = ImGui::InvisibleButton("##compact_row", rowSize);
    const bool hovered = ImGui::IsItemHovered();
    if (pressed)
    {
        *value = !*value;
    }

    const ImVec2 boxMin = Add(rowMin, ImVec2(0.0f, 4.0f));
    const ImVec2 boxMax = Add(boxMin, ImVec2(14.0f, 18.0f));
    const float boxRounding = 3.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const float hoverT = AnimateFloat(ImGui::GetID("##ccb_hover"), hovered, 15.0f);
    const float activeT = AnimateFloat(ImGui::GetID("##ccb_active"), *value, 15.0f);
    ImVec4 bgCol = LerpVec4(ImVec4(0.12f, 0.13f, 0.16f, 0.96f), ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.95f), activeT);
    ImVec4 borderCol = LerpVec4(ImVec4(1.0f, 1.0f, 1.0f, 0.1f + hoverT * 0.1f), palette.accent, activeT);

    drawList->AddRectFilled(boxMin, boxMax, ImGui::GetColorU32(bgCol), boxRounding);
    drawList->AddRect(boxMin, boxMax, ImGui::GetColorU32(borderCol), boxRounding, 0, 1.5f);
    
    if (activeT > 0.1f)
    {
        float tickAlpha = activeT;
        drawList->AddLine(Add(boxMin, ImVec2(3.0f, 7.0f)), Add(boxMin, ImVec2(6.0f, 10.0f)), ImGui::GetColorU32(ImVec4(1, 1, 1, tickAlpha)), 1.5f);
        drawList->AddLine(Add(boxMin, ImVec2(6.0f, 10.0f)), Add(boxMin, ImVec2(10.5f, 4.0f)), ImGui::GetColorU32(ImVec4(1, 1, 1, tickAlpha)), 1.5f);
    }

    drawList->AddText(fonts.ui, fonts.ui->FontSize, Add(rowMin, ImVec2(22.0f, 2.0f)), ImGui::GetColorU32(palette.text), label);
    ImGui::Dummy(rowSize);
    ImGui::PopID();
}

void DrawSignatureToggleSwitch(ImDrawList* drawList, const ImRect& rect, const ImVec4& accent, float hoverT, float activeT)
{
    const float rounding = rect.GetHeight() * 0.5f;
    const ImVec4 offFill(0.18f, 0.20f, 0.24f, 1.0f);
    const ImVec4 onFill(accent.x, accent.y, accent.z, 1.0f);
    const ImVec4 trackFill = LerpVec4(offFill, onFill, activeT);

    drawList->AddRectFilled(rect.Min, rect.Max, ImGui::GetColorU32(trackFill), rounding);
    drawList->AddRect(rect.Min, rect.Max, WidgetAlphaU32(IM_COL32(255, 255, 255, static_cast<int>(20 + hoverT * 22))), rounding, 0, 1.0f);

    const float thumbRadius = (rect.GetHeight() * 0.5f) - 2.5f;
    const float thumbX = ImLerp(rect.Min.x + rounding, rect.Max.x - rounding, activeT);
    const float thumbY = rect.Min.y + rect.GetHeight() * 0.5f;
    const ImVec2 thumbCenter(thumbX, thumbY);
    drawList->AddCircleFilled(thumbCenter, thumbRadius, WidgetAlphaU32(IM_COL32(245, 247, 250, 255)), 20);
}

void CompactToggleRow(const char* id, const char* label, bool* value)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();

    ImGui::PushID(id);
    const float width = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(width, 22.0f);
    const bool pressed = ImGui::InvisibleButton("##compact_toggle_row", rowSize);
    const bool hovered = ImGui::IsItemHovered();
    if (pressed)
    {
        *value = !*value;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float hoverT = AnimateFloat(ImGui::GetID("##toggle_hover_anim"), hovered, 15.0f);
    const float activeT = AnimateFloat(ImGui::GetID("##toggle_state_anim"), *value, 13.5f);
    if (hoverT > 0.01f)
        drawList->AddRectFilled(rowMin, Add(rowMin, rowSize), ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.025f * hoverT)), 3.0f);
    drawList->AddLine(Add(rowMin, ImVec2(0.0f, rowSize.y - 1.0f)), Add(rowMin, ImVec2(width, rowSize.y - 1.0f)), ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.035f)), 1.0f);
    drawList->AddText(
        fonts.ui,
        fonts.ui->FontSize,
        Add(rowMin, ImVec2(0.0f, 2.0f)),
        ImGui::GetColorU32(LerpVec4(palette.textMuted, palette.text, 0.56f + hoverT * 0.44f)),
        label);

    const ImRect toggleRect(Add(rowMin, ImVec2(width - 42.0f, 3.0f)), Add(rowMin, ImVec2(width - 8.0f, 19.0f)));
    DrawSignatureToggleSwitch(drawList, toggleRect, palette.accent, hoverT, activeT);

    ImGui::PopID();
}

void CompactToggleColorRow(const char* id, const char* label, bool* value, float color[4], bool alpha)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();

    ImGui::PushID(id);
    const float width = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(width, 22.0f);

    const ImRect toggleRect(Add(rowMin, ImVec2(width - 42.0f, 3.0f)), Add(rowMin, ImVec2(width - 8.0f, 19.0f)));
    const ImVec2 swatchMin = Add(toggleRect.Min, ImVec2(-24.0f, 0.0f));
    const ImVec2 swatchMax = Add(swatchMin, ImVec2(16.0f, 16.0f));
    const bool swatchHovered = ImGui::IsMouseHoveringRect(swatchMin, swatchMax, false);

    const bool pressed = ImGui::InvisibleButton("##compact_toggle_color_row", rowSize);
    const bool hovered = ImGui::IsItemHovered();
    if (pressed && !swatchHovered)
        *value = !*value;

    if (swatchHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        ImGui::OpenPopup("##compact_toggle_color_popup");

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float hoverT = AnimateFloat(ImGui::GetID("##toggle_color_hover_anim"), hovered, 15.0f);
    const float activeT = AnimateFloat(ImGui::GetID("##toggle_color_state_anim"), *value, 13.5f);
    drawList->AddText(
        fonts.ui,
        fonts.ui->FontSize,
        Add(rowMin, ImVec2(0.0f, 2.0f)),
        ImGui::GetColorU32(LerpVec4(palette.textMuted, palette.text, 0.56f + hoverT * 0.44f)),
        label);

    drawList->AddRectFilled(swatchMin, swatchMax, WidgetAlphaU32(IM_COL32(24, 25, 30, 230)), 3.0f);
    drawList->AddRectFilled(
        Add(swatchMin, ImVec2(1.8f, 1.8f)),
        Add(swatchMax, ImVec2(-1.8f, -1.8f)),
        ImGui::GetColorU32(ImVec4(color[0], color[1], color[2], alpha ? color[3] : 1.0f)),
        2.5f);
    drawList->AddRect(swatchMin, swatchMax, WidgetAlphaU32(IM_COL32(255, 255, 255, swatchHovered ? 54 : 28)), 3.0f);

    DrawSignatureToggleSwitch(drawList, toggleRect, palette.accent, hoverT, activeT);

    ImGui::SetNextWindowSize(ImVec2(218.0f, alpha ? 300.0f : 276.0f), ImGuiCond_Appearing);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, WidgetAlphaU32(IM_COL32(15, 17, 22, 236)));
    ImGui::PushStyleColor(ImGuiCol_Border, WidgetAlphaU32(IM_COL32(255, 255, 255, 20)));
    if (ImGui::BeginPopup("##compact_toggle_color_popup"))
    {
        ImGui::ColorPicker4(
            "##compact_toggle_color_picker",
            color,
            ImGuiColorEditFlags_DisplayRGB |
                ImGuiColorEditFlags_NoSidePreview |
                ImGuiColorEditFlags_NoInputs |
                (alpha ? ImGuiColorEditFlags_AlphaBar : ImGuiColorEditFlags_NoAlpha));
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);

    ImGui::PopID();
}

void CheckboxRow(const char* id, const char* label, bool* value, const char* badgeText, const ImVec4* badgeColor)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();

    ImGui::PushID(id);
    const float width = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(width, 30.0f);
    const bool pressed = ImGui::InvisibleButton("##row", rowSize);
    const bool hovered = ImGui::IsItemHovered();
    if (pressed)
    {
        *value = !*value;
    }

    const ImVec2 boxMin = Add(rowMin, ImVec2(0.0f, 6.0f));
    const ImVec2 boxMax = Add(boxMin, ImVec2(16.0f, 22.0f)); // Taller for better look
    const float boxRounding = 4.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddLine(Add(rowMin, ImVec2(0.0f, 31.0f)), Add(rowMin, ImVec2(width, 31.0f)), WidgetAlphaU32(IM_COL32(255, 255, 255, 8)), 1.0f);
    
    // Smooth hover animation
    const float hoverT = AnimateFloat(ImGui::GetID("##cb_hover"), hovered, 15.0f);
    const float activeT = AnimateFloat(ImGui::GetID("##cb_active"), *value, 15.0f);
    
    ImVec4 bgCol = LerpVec4(ImVec4(0.12f, 0.13f, 0.16f, 0.96f), ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.95f), activeT);
    ImVec4 borderCol = LerpVec4(ImVec4(1.0f, 1.0f, 1.0f, 0.1f + hoverT * 0.1f), palette.accent, activeT);

    drawList->AddRectFilled(boxMin, boxMax, ImGui::GetColorU32(bgCol), boxRounding);
    drawList->AddRect(boxMin, boxMax, ImGui::GetColorU32(borderCol), boxRounding, 0, 1.5f);
    
    if (activeT > 0.1f)
    {
        // Animated tick mark
        float tickAlpha = activeT;
        drawList->AddLine(Add(boxMin, ImVec2(4.0f, 8.0f)), Add(boxMin, ImVec2(7.0f, 12.0f)), ImGui::GetColorU32(ImVec4(1, 1, 1, tickAlpha)), 2.0f);
        drawList->AddLine(Add(boxMin, ImVec2(7.0f, 12.0f)), Add(boxMin, ImVec2(12.0f, 5.0f)), ImGui::GetColorU32(ImVec4(1, 1, 1, tickAlpha)), 2.0f);
    }

    drawList->AddText(fonts.ui, fonts.ui->FontSize, Add(rowMin, ImVec2(26.0f, 3.0f)), ImGui::GetColorU32(palette.text), label);

    if (badgeText != nullptr && badgeText[0] != '\0')
    {
        const ImVec4 badge = badgeColor ? *badgeColor : palette.textMuted;
        const ImVec2 badgeMin = Add(rowMin, ImVec2(width - 64.0f, 2.0f));
        const ImVec2 badgeMax = Add(badgeMin, ImVec2(44.0f, 18.0f));
        drawList->AddRectFilled(badgeMin, badgeMax, WidgetAlphaU32(IM_COL32(22, 24, 30, 180)), 4.0f);
        drawList->AddRect(badgeMin, badgeMax, WidgetAlphaU32(IM_COL32(255, 255, 255, 14)), 4.0f);
        drawList->AddText(fonts.uiSmall, fonts.uiSmall->FontSize, Add(badgeMin, ImVec2(8.0f, 3.0f)), ImGui::GetColorU32(badge), badgeText);
    }

    ImGui::Dummy(rowSize);
    ImGui::PopID();
}

void LabeledCombo(const char* label, int* currentItem, const char* const items[], int itemCount)
{
    ImGui::TextUnformatted(label);
    const float width = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    DrawFlatCombo((std::string("##") + label).c_str(), currentItem, items, itemCount, width);
}

void LabeledSliderInt(const char* label, int* value, int minValue, int maxValue)
{
    CompactSliderIntRow(label, value, minValue, maxValue, -1.0f);
}

void LabeledSliderFloat(const char* label, float* value, float minValue, float maxValue, const char* format)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGui::PushID(label);

    const float sliderWidth = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    char valueBuffer[32];
    sprintf_s(valueBuffer, format, *value);
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 valueTextSize = ImGui::CalcTextSize(valueBuffer);

    drawList->AddText(fonts.ui, fonts.ui->FontSize, rowMin, ImGui::GetColorU32(palette.text), label);
    drawList->AddText(
        fonts.uiSmall,
        fonts.uiSmall->FontSize,
        Add(rowMin, ImVec2(sliderWidth - valueTextSize.x, 0.0f)),
        ImGui::GetColorU32(palette.textMuted),
        valueBuffer);

    const ImVec2 sliderMin = Add(rowMin, ImVec2(0.0f, 13.0f));
    const ImRect rect(sliderMin, Add(sliderMin, ImVec2(sliderWidth, 8.0f)));

    float normalized = (*value - minValue) / (maxValue - minValue);
    normalized = Clamp01(normalized);
    bool hovered = false;
    if (HandleSliderBehavior("##float_slider", rect, &normalized, &hovered))
    {
        *value = minValue + (maxValue - minValue) * normalized;
    }

    DrawPremiumSlider(drawList, rect, normalized, palette.accent, hovered);
    ImGui::Dummy(ImVec2(sliderWidth, 24.0f));
    ImGui::PopID();
}

void ColorPickerRow(const char* id, const char* label, float color[4], bool alpha, bool* enabled)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    const std::string popupId = std::string("##color_popup_") + id;

    ImGui::PushID(id);
    const float width = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(width, 28.0f);
    const ImVec2 checkboxMin = Add(rowMin, ImVec2(0.0f, 8.0f));
    const ImVec2 checkboxMax = Add(checkboxMin, ImVec2(12.0f, 12.0f));
    const ImVec2 swatchMin = Add(rowMin, ImVec2(width - 54.0f, 5.0f));
    const ImVec2 swatchMax = Add(swatchMin, ImVec2(18.0f, 20.0f));
    const ImVec2 toolMin = Add(rowMin, ImVec2(width - 28.0f, 6.0f));
    const ImVec2 toolMax = Add(toolMin, ImVec2(18.0f, 18.0f));

    if (ImGui::InvisibleButton("##color_row", rowSize) && enabled != nullptr)
    {
        *enabled = !*enabled;
    }
    const bool hovered = ImGui::IsItemHovered();
    const bool active = enabled == nullptr ? true : *enabled;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(rowMin, Add(rowMin, rowSize), ImGui::GetColorU32(ImVec4(1, 1, 1, hovered ? 0.012f : 0.0f)), 6.0f);
    drawList->AddLine(Add(rowMin, ImVec2(0.0f, rowSize.y - 1.0f)), Add(rowMin, ImVec2(width, rowSize.y - 1.0f)), WidgetAlphaU32(IM_COL32(255, 255, 255, 10)), 1.0f);
    drawList->AddRectFilled(checkboxMin, checkboxMax, ImGui::GetColorU32(active ? palette.accentSoft : ImVec4(0.12f, 0.13f, 0.16f, 0.7f)), 3.0f);
    drawList->AddRect(checkboxMin, checkboxMax, ImGui::GetColorU32(active ? palette.accent : ImVec4(1, 1, 1, 0.10f)), 3.0f);
    if (active)
    {
        drawList->AddLine(Add(checkboxMin, ImVec2(2.5f, 6.0f)), Add(checkboxMin, ImVec2(5.0f, 8.5f)), WidgetAlphaU32(IM_COL32(255, 255, 255, 255)), 1.3f);
        drawList->AddLine(Add(checkboxMin, ImVec2(5.0f, 8.5f)), Add(checkboxMin, ImVec2(9.5f, 3.0f)), WidgetAlphaU32(IM_COL32(255, 255, 255, 255)), 1.3f);
    }
    drawList->AddText(fonts.ui, fonts.ui->FontSize, Add(rowMin, ImVec2(18.0f, 5.0f)), ImGui::GetColorU32(active ? palette.text : palette.textMuted), label);

    drawList->AddRectFilled(swatchMin, swatchMax, WidgetAlphaU32(IM_COL32(22, 22, 29, 220)), 4.0f);
    drawList->AddRectFilled(ImVec2(swatchMin.x + 2.0f, swatchMin.y + 2.0f), ImVec2(swatchMax.x - 2.0f, swatchMax.y - 2.0f), ImGui::GetColorU32(ImVec4(color[0], color[1], color[2], alpha ? color[3] : 1.0f)), 3.0f);
    drawList->AddRect(swatchMin, swatchMax, WidgetAlphaU32(IM_COL32(255, 255, 255, 18)), 4.0f);
    drawList->AddRectFilled(toolMin, toolMax, WidgetAlphaU32(IM_COL32(20, 21, 28, 240)), 4.0f);
    drawList->AddRect(toolMin, toolMax, WidgetAlphaU32(IM_COL32(255, 255, 255, 14)), 4.0f);
    drawList->AddLine(Add(toolMin, ImVec2(4.5f, 13.0f)), Add(toolMin, ImVec2(11.0f, 6.5f)), ImGui::GetColorU32(palette.text), 1.3f);
    drawList->AddLine(Add(toolMin, ImVec2(10.0f, 5.5f)), Add(toolMin, ImVec2(13.0f, 8.5f)), ImGui::GetColorU32(palette.text), 1.3f);
    drawList->AddLine(Add(toolMin, ImVec2(3.5f, 14.0f)), Add(toolMin, ImVec2(7.0f, 15.0f)), ImGui::GetColorU32(palette.accent), 1.2f);

    if (ImGui::IsMouseHoveringRect(swatchMin, toolMax, false) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ImGui::OpenPopup(popupId.c_str());
    }

    ImGui::SetNextWindowSize(ImVec2(260.0f, alpha ? 320.0f : 300.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, WidgetAlphaU32(IM_COL32(15, 17, 22, 236)));
    ImGui::PushStyleColor(ImGuiCol_Border, WidgetAlphaU32(IM_COL32(255, 255, 255, 20)));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, WidgetAlphaU32(IM_COL32(24, 25, 32, 220)));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, WidgetAlphaU32(IM_COL32(29, 31, 38, 228)));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, WidgetAlphaU32(IM_COL32(34, 36, 44, 236)));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImGui::GetColorU32(palette.accent));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImGui::GetColorU32(palette.accent));
    ImGui::PushStyleColor(ImGuiCol_Button, WidgetAlphaU32(IM_COL32(24, 25, 32, 220)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WidgetAlphaU32(IM_COL32(31, 33, 40, 232)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, WidgetAlphaU32(IM_COL32(36, 38, 46, 240)));
    if (ImGui::BeginPopup(popupId.c_str()))
    {
        ImGui::ColorPicker4(
            "##picker",
            color,
            ImGuiColorEditFlags_DisplayRGB |
                ImGuiColorEditFlags_AlphaBar |
                ImGuiColorEditFlags_NoSidePreview |
                (alpha ? 0 : ImGuiColorEditFlags_NoAlpha));
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(10);

    ImGui::SetCursorScreenPos(Add(rowMin, ImVec2(0.0f, rowSize.y)));
    ImGui::Dummy(ImVec2(width, 0.0f));
    ImGui::PopID();
}

void ColorValueRow(const char* id, const char* label, float color[4], bool alpha)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    const std::string popupId = std::string("##color_value_popup_") + id;

    ImGui::PushID(id);
    const float width = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(width, 28.0f);
    const ImVec2 swatchMin = Add(rowMin, ImVec2(width - 54.0f, 5.0f));
    const ImVec2 swatchMax = Add(swatchMin, ImVec2(18.0f, 20.0f));
    const ImVec2 toolMin = Add(rowMin, ImVec2(width - 28.0f, 6.0f));
    const ImVec2 toolMax = Add(toolMin, ImVec2(18.0f, 18.0f));

    ImGui::InvisibleButton("##color_value_row", rowSize);
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(rowMin, Add(rowMin, rowSize), ImGui::GetColorU32(ImVec4(1, 1, 1, hovered ? 0.012f : 0.0f)), 6.0f);
    drawList->AddLine(Add(rowMin, ImVec2(0.0f, rowSize.y - 1.0f)), Add(rowMin, ImVec2(width, rowSize.y - 1.0f)), WidgetAlphaU32(IM_COL32(255, 255, 255, 10)), 1.0f);
    drawList->AddText(fonts.ui, fonts.ui->FontSize, Add(rowMin, ImVec2(0.0f, 5.0f)), ImGui::GetColorU32(palette.text), label);

    drawList->AddRectFilled(swatchMin, swatchMax, WidgetAlphaU32(IM_COL32(22, 22, 29, 220)), 4.0f);
    drawList->AddRectFilled(ImVec2(swatchMin.x + 2.0f, swatchMin.y + 2.0f), ImVec2(swatchMax.x - 2.0f, swatchMax.y - 2.0f), ImGui::GetColorU32(ImVec4(color[0], color[1], color[2], alpha ? color[3] : 1.0f)), 3.0f);
    drawList->AddRect(swatchMin, swatchMax, WidgetAlphaU32(IM_COL32(255, 255, 255, 18)), 4.0f);
    drawList->AddRectFilled(toolMin, toolMax, WidgetAlphaU32(IM_COL32(20, 21, 28, 240)), 4.0f);
    drawList->AddRect(toolMin, toolMax, WidgetAlphaU32(IM_COL32(255, 255, 255, 14)), 4.0f);
    drawList->AddLine(Add(toolMin, ImVec2(4.5f, 13.0f)), Add(toolMin, ImVec2(11.0f, 6.5f)), ImGui::GetColorU32(palette.text), 1.3f);
    drawList->AddLine(Add(toolMin, ImVec2(10.0f, 5.5f)), Add(toolMin, ImVec2(13.0f, 8.5f)), ImGui::GetColorU32(palette.text), 1.3f);
    drawList->AddLine(Add(toolMin, ImVec2(3.5f, 14.0f)), Add(toolMin, ImVec2(7.0f, 15.0f)), ImGui::GetColorU32(palette.accent), 1.2f);

    if (ImGui::IsMouseHoveringRect(swatchMin, toolMax, false) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ImGui::OpenPopup(popupId.c_str());
    }

    ImGui::SetNextWindowSize(ImVec2(260.0f, alpha ? 320.0f : 300.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, WidgetAlphaU32(IM_COL32(15, 17, 22, 236)));
    ImGui::PushStyleColor(ImGuiCol_Border, WidgetAlphaU32(IM_COL32(255, 255, 255, 20)));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, WidgetAlphaU32(IM_COL32(24, 25, 32, 220)));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, WidgetAlphaU32(IM_COL32(29, 31, 38, 228)));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, WidgetAlphaU32(IM_COL32(34, 36, 44, 236)));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImGui::GetColorU32(palette.accent));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImGui::GetColorU32(palette.accent));
    ImGui::PushStyleColor(ImGuiCol_Button, WidgetAlphaU32(IM_COL32(24, 25, 32, 220)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WidgetAlphaU32(IM_COL32(31, 33, 40, 232)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, WidgetAlphaU32(IM_COL32(36, 38, 46, 240)));
    if (ImGui::BeginPopup(popupId.c_str()))
    {
        ImGui::ColorPicker4(
            "##value_picker",
            color,
            ImGuiColorEditFlags_DisplayRGB |
                ImGuiColorEditFlags_AlphaBar |
                ImGuiColorEditFlags_NoSidePreview |
                (alpha ? 0 : ImGuiColorEditFlags_NoAlpha));
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(10);

    ImGui::SetCursorScreenPos(Add(rowMin, ImVec2(0.0f, rowSize.y)));
    ImGui::Dummy(ImVec2(width, 0.0f));
    ImGui::PopID();
}

bool ActionButton(const char* id, const char* label, const ImVec2& size, const ImVec4* accent)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    const odyssey::theme::Fonts& fonts = odyssey::theme::GetFonts();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImVec2 resolvedSize(size.x <= 0.0f ? ImGui::GetContentRegionAvail().x : size.x, size.y);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max = Add(min, resolvedSize);
    const ImGuiID itemId = window->GetID(id);
    const bool pressed = ImGui::InvisibleButton(id, resolvedSize);
    const bool hovered = ImGui::IsItemHovered();
    const float t = AnimateFloat(itemId, hovered, 12.0f);

    const ImVec4 accentColor = accent ? *accent : palette.accent;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const bool held = ImGui::IsItemActive();
    const float heldT = AnimateFloat(window->GetID((std::string(id) + "_held").c_str()), held, 18.0f);
    const ImVec4 baseButton = ImVec4(palette.panel.x + 0.044f, palette.panel.y + 0.048f, palette.panel.z + 0.058f, 0.82f);
    const ImVec4 hoverButton = ImVec4(accentColor.x, accentColor.y, accentColor.z, 0.16f);
    const ImVec4 activeButton = ImVec4(accentColor.x, accentColor.y, accentColor.z, 0.28f);
    const ImVec4 fill = ImLerp(ImLerp(baseButton, hoverButton, t), activeButton, heldT);
    const float rounding = 4.0f;
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(fill), rounding);
    drawList->AddRect(min, max, ImGui::GetColorU32(ImVec4(accentColor.x, accentColor.y, accentColor.z, 0.18f + 0.24f * t + 0.12f * heldT)), rounding, 0, 1.0f);
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    drawList->AddText(
        fonts.uiSmall,
        fonts.uiSmall->FontSize,
        ImVec2(min.x + (resolvedSize.x - textSize.x) * 0.5f, min.y + ImFloor((resolvedSize.y - textSize.y) * 0.5f) - 1.0f + heldT),
        ImGui::GetColorU32(palette.text),
        label);
    return pressed;
}

void InputField(const char* id, char* buffer, int bufferSize, const char* hint)
{
    const odyssey::theme::Palette& palette = odyssey::theme::GetPalette();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, WidgetAlphaU32(IM_COL32(18, 21, 27, 188)));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, WidgetAlphaU32(IM_COL32(24, 28, 36, 210)));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, WidgetAlphaU32(IM_COL32(28, 33, 42, 232)));
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImVec4(palette.panelBorder.x, palette.panelBorder.y, palette.panelBorder.z, 0.52f)));
    ImGui::InputTextWithHint(id, hint, buffer, bufferSize);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

void ColorSwatch(const ImVec2& position, const ImVec2& size, const ImVec4& color)
{
    ImGui::GetWindowDrawList()->AddRectFilled(position, Add(position, size), ImGui::GetColorU32(color), 2.0f);
}
}
