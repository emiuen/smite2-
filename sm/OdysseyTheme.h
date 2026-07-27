#pragma once

#include "imgui.h"

namespace odyssey::theme
{
enum class ThemePreset
{
    DarkWhite = 0,
    DarkBlue,
    DarkGreen,
    DarkPurple
};

struct Fonts
{
    ImFont* ui = nullptr;
    ImFont* uiSmall = nullptr;
    ImFont* title = nullptr;
    ImFont* titleLarge = nullptr;
};

struct Palette
{
    ImVec4 bgTop;
    ImVec4 bgBottom;
    ImVec4 panel;
    ImVec4 panelSoft;
    ImVec4 panelBorder;
    ImVec4 accent;
    ImVec4 accentSoft;
    ImVec4 text;
    ImVec4 textMuted;
    ImVec4 success;
    ImVec4 warning;
    ImVec4 danger;
};

const Fonts& GetFonts();
const Palette& GetPalette();
ThemePreset GetThemePreset();
int GetFontProfile();

void SetupStyle();
void LoadFonts();
void SetThemePreset(ThemePreset preset);
void SetAccentColor(const ImVec4& accentColor);
void SetFontProfile(int profile);
void SetGlassMorphStrength(float strength);

void DrawBackdrop(const ImVec2& origin, const ImVec2& size);
void DrawGlowRect(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, ImU32 color, float rounding, float thickness = 1.0f);
void DrawSectionFrame(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const char* title, ImFont* titleFont = nullptr);
}
