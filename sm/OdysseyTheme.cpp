#define IMGUI_DEFINE_MATH_OPERATORS
#include "OdysseyTheme.h"

#include "imgui_internal.h"

// ImGui 1.90 removed the FontSize member from ImFont.  To preserve
// compatibility with older UI code we alias FontSize to LegacySize.  This
// allows expressions like fonts.ui->FontSize to compile correctly against
// newer ImGui versions.
#define FontSize LegacySize

#include <array>
#include <cmath>
#include <filesystem>
#include <windows.h>

namespace
{
odyssey::theme::Fonts g_fonts{};
odyssey::theme::Fonts g_activeFonts{};
odyssey::theme::ThemePreset g_themePreset = odyssey::theme::ThemePreset::DarkWhite;
int g_fontProfile = 0;
float g_glassMorphStrength = 0.0f;
odyssey::theme::Palette g_palette{
    ImVec4(0.105f, 0.114f, 0.133f, 1.00f),
    ImVec4(0.046f, 0.051f, 0.061f, 1.00f),
    ImVec4(0.082f, 0.090f, 0.106f, 1.00f),
    ImVec4(0.135f, 0.148f, 0.176f, 1.00f),
    ImVec4(0.205f, 0.225f, 0.270f, 1.00f),
    ImVec4(0.310f, 0.550f, 1.000f, 1.00f),
    ImVec4(0.310f, 0.550f, 1.000f, 0.14f),
    ImVec4(0.945f, 0.955f, 0.975f, 1.00f),
    ImVec4(0.590f, 0.625f, 0.690f, 1.00f),
    ImVec4(0.220f, 0.770f, 0.540f, 1.00f),
    ImVec4(0.960f, 0.690f, 0.270f, 1.00f),
    ImVec4(0.940f, 0.360f, 0.430f, 1.00f)
};

constexpr float kDefaultRounding = 12.0f;

void RefreshActiveFonts()
{
    g_activeFonts = g_fonts;

    switch (g_fontProfile)
    {
    case 1: // Compact
        g_activeFonts.ui = g_fonts.uiSmall ? g_fonts.uiSmall : g_fonts.ui;
        g_activeFonts.uiSmall = g_activeFonts.ui;
        g_activeFonts.title = g_fonts.ui ? g_fonts.ui : g_activeFonts.ui;
        g_activeFonts.titleLarge = g_fonts.title ? g_fonts.title : g_activeFonts.title;
        break;
    case 2: // Bold/Title
        g_activeFonts.ui = g_fonts.title ? g_fonts.title : g_fonts.ui;
        g_activeFonts.uiSmall = g_fonts.ui ? g_fonts.ui : g_activeFonts.ui;
        g_activeFonts.title = g_fonts.title ? g_fonts.title : g_activeFonts.ui;
        g_activeFonts.titleLarge = g_fonts.titleLarge ? g_fonts.titleLarge : g_activeFonts.title;
        break;
    case 0:
    default:
        break;
    }
}

bool TryLoadFont(ImGuiIO& io, ImFont*& target, const char* path, float size, const ImWchar* glyphRanges)
{
    if (!std::filesystem::exists(path))
    {
        return false;
    }

    target = io.Fonts->AddFontFromFileTTF(path, size, nullptr, glyphRanges);
    return target != nullptr;
}

ImFont* TryLoadFirstAvailableFont(
    ImGuiIO& io,
    const std::array<const char*, 4>& candidates,
    float size,
    const ImFontConfig& config,
    const ImWchar* glyphRanges)
{
    for (const char* path : candidates)
    {
        if (path == nullptr || path[0] == '\0')
        {
            continue;
        }

        if (!std::filesystem::exists(path))
        {
            continue;
        }

        ImFont* font = io.Fonts->AddFontFromFileTTF(path, size, &config, glyphRanges);
        if (font != nullptr)
        {
            return font;
        }
    }

    return nullptr;
}

void MergeFontIfExists(ImGuiIO& io, const char* path, float size, const ImFontConfig& baseConfig, const ImWchar* glyphRanges)
{
    if (path == nullptr || path[0] == '\0' || !std::filesystem::exists(path))
    {
        return;
    }

    ImFontConfig mergeConfig = baseConfig;
    mergeConfig.MergeMode = true;
    mergeConfig.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF(path, size, &mergeConfig, glyphRanges);
}

void MergeCjkFallbacks(ImGuiIO& io, float size, const ImFontConfig& config, const ImWchar* glyphRanges)
{
    MergeFontIfExists(io, "C:/Windows/Fonts/msyh.ttc", size, config, glyphRanges);
    MergeFontIfExists(io, "C:/Windows/Fonts/meiryo.ttc", size, config, glyphRanges);
    MergeFontIfExists(io, "C:/Windows/Fonts/malgun.ttf", size, config, glyphRanges);
}
}

namespace odyssey::theme
{
namespace
{
Palette BuildPalette(ThemePreset preset)
{
    Palette palette{
        ImVec4(0.105f, 0.114f, 0.133f, 1.00f),
        ImVec4(0.046f, 0.051f, 0.061f, 1.00f),
        ImVec4(0.082f, 0.090f, 0.106f, 1.00f),
        ImVec4(0.135f, 0.148f, 0.176f, 1.00f),
        ImVec4(0.205f, 0.225f, 0.270f, 1.00f),
        ImVec4(0.310f, 0.550f, 1.000f, 1.00f),
        ImVec4(0.310f, 0.550f, 1.000f, 0.14f),
        ImVec4(0.945f, 0.955f, 0.975f, 1.00f),
        ImVec4(0.590f, 0.625f, 0.690f, 1.00f),
        ImVec4(0.220f, 0.770f, 0.540f, 1.00f),
        ImVec4(0.960f, 0.690f, 0.270f, 1.00f),
        ImVec4(0.940f, 0.360f, 0.430f, 1.00f)
    };

    switch (preset)
    {
    case ThemePreset::DarkBlue:
        palette.bgTop = ImVec4(0.040f, 0.055f, 0.078f, 1.00f);
        palette.bgBottom = ImVec4(0.010f, 0.014f, 0.024f, 1.00f);
        palette.panel = ImVec4(0.070f, 0.086f, 0.112f, 0.90f);
        palette.panelSoft = ImVec4(0.120f, 0.150f, 0.190f, 0.72f);
        palette.panelBorder = ImVec4(0.760f, 0.870f, 1.000f, 0.18f);
        palette.accent = ImVec4(0.500f, 0.760f, 1.000f, 0.96f);
        palette.accentSoft = ImVec4(0.500f, 0.760f, 1.000f, 0.15f);
        palette.text = ImVec4(0.930f, 0.960f, 1.000f, 1.0f);
        palette.textMuted = ImVec4(0.590f, 0.680f, 0.780f, 1.0f);
        break;
    case ThemePreset::DarkGreen:
        palette.bgTop = ImVec4(0.045f, 0.064f, 0.052f, 1.00f);
        palette.bgBottom = ImVec4(0.012f, 0.018f, 0.015f, 1.00f);
        palette.panel = ImVec4(0.070f, 0.096f, 0.084f, 0.90f);
        palette.panelSoft = ImVec4(0.112f, 0.154f, 0.132f, 0.72f);
        palette.panelBorder = ImVec4(0.750f, 1.000f, 0.850f, 0.18f);
        palette.accent = ImVec4(0.640f, 0.940f, 0.720f, 0.96f);
        palette.accentSoft = ImVec4(0.640f, 0.940f, 0.720f, 0.15f);
        palette.text = ImVec4(0.930f, 0.985f, 0.950f, 1.0f);
        palette.textMuted = ImVec4(0.600f, 0.720f, 0.650f, 1.0f);
        break;
    case ThemePreset::DarkPurple:
        palette.bgTop = ImVec4(0.060f, 0.045f, 0.078f, 1.00f);
        palette.bgBottom = ImVec4(0.018f, 0.013f, 0.025f, 1.00f);
        palette.panel = ImVec4(0.090f, 0.078f, 0.112f, 0.90f);
        palette.panelSoft = ImVec4(0.150f, 0.124f, 0.190f, 0.72f);
        palette.panelBorder = ImVec4(0.890f, 0.780f, 1.000f, 0.18f);
        palette.accent = ImVec4(0.850f, 0.720f, 1.000f, 0.96f);
        palette.accentSoft = ImVec4(0.850f, 0.720f, 1.000f, 0.15f);
        palette.text = ImVec4(0.970f, 0.950f, 1.000f, 1.0f);
        palette.textMuted = ImVec4(0.710f, 0.650f, 0.800f, 1.0f);
        break;
    case ThemePreset::DarkWhite:
    default:
        break;
    }

    return palette;
}
}

const Fonts& GetFonts()
{
    return g_activeFonts;
}

const Palette& GetPalette()
{
    return g_palette;
}

ThemePreset GetThemePreset()
{
    return g_themePreset;
}

int GetFontProfile()
{
    return g_fontProfile;
}

void LoadFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    static const ImWchar glyphRanges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin supplement
        0x0400, 0x052F, // Cyrillic
        0x2000, 0x206F, // General punctuation
        0x3000, 0x30FF, // CJK punctuation, Hiragana, Katakana
        0x31F0, 0x31FF, // Katakana phonetic extensions
        0x4E00, 0x9FFF, // CJK unified ideographs
        0xAC00, 0xD7A3, // Hangul syllables
        0
    };

    ImFontConfig uiConfig{};
    uiConfig.OversampleH = 4;
    uiConfig.OversampleV = 2;
    uiConfig.PixelSnapH = false;
    uiConfig.RasterizerMultiply = 1.06f;
    uiConfig.SizePixels = 16.0f;

    const std::array<const char*, 4> uiCandidates = {
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/malgun.ttf",
        "C:/Windows/Fonts/meiryo.ttc",
        "C:/Windows/Fonts/segoeui.ttf",
    };

    g_fonts.ui = TryLoadFirstAvailableFont(io, uiCandidates, 16.0f, uiConfig, glyphRanges);
    if (g_fonts.ui != nullptr)
    {
        MergeCjkFallbacks(io, 16.0f, uiConfig, glyphRanges);
    }

    ImFontConfig smallConfig = uiConfig;
    smallConfig.RasterizerMultiply = 1.10f;
    g_fonts.uiSmall = TryLoadFirstAvailableFont(io, uiCandidates, 13.5f, smallConfig, glyphRanges);
    if (g_fonts.uiSmall != nullptr)
    {
        MergeCjkFallbacks(io, 13.5f, smallConfig, glyphRanges);
    }

    ImFontConfig titleConfig = uiConfig;
    titleConfig.RasterizerMultiply = 1.12f;
    const std::array<const char*, 4> titleCandidates = {
        "C:/Windows/Fonts/msyhbd.ttc",
        "C:/Windows/Fonts/malgunbd.ttf",
        "C:/Windows/Fonts/meiryob.ttc",
        "C:/Windows/Fonts/segoeuib.ttf",
    };
    g_fonts.title = TryLoadFirstAvailableFont(io, titleCandidates, 19.0f, titleConfig, glyphRanges);
    if (g_fonts.title != nullptr)
    {
        MergeCjkFallbacks(io, 19.0f, titleConfig, glyphRanges);
    }

    ImFontConfig titleLargeConfig = titleConfig;
    titleLargeConfig.RasterizerMultiply = 1.16f;
    g_fonts.titleLarge = TryLoadFirstAvailableFont(io, titleCandidates, 27.0f, titleLargeConfig, glyphRanges);
    if (g_fonts.titleLarge != nullptr)
    {
        MergeCjkFallbacks(io, 27.0f, titleLargeConfig, glyphRanges);
    }

    if (g_fonts.ui == nullptr)
    {
        g_fonts.ui = io.Fonts->AddFontDefault(&uiConfig);
    }

    if (g_fonts.uiSmall == nullptr)
    {
        g_fonts.uiSmall = io.Fonts->AddFontDefault(&smallConfig);
    }

    if (g_fonts.title == nullptr)
    {
        g_fonts.title = io.Fonts->AddFontDefault(&titleConfig);
    }

    if (g_fonts.titleLarge == nullptr)
    {
        g_fonts.titleLarge = io.Fonts->AddFontDefault(&titleLargeConfig);
    }

    if (g_fonts.uiSmall == nullptr)
    {
        g_fonts.uiSmall = g_fonts.ui;
    }

    if (g_fonts.title == nullptr)
    {
        g_fonts.title = g_fonts.ui;
    }

    if (g_fonts.titleLarge == nullptr)
    {
        g_fonts.titleLarge = g_fonts.title;
    }

    RefreshActiveFonts();
}

void SetThemePreset(ThemePreset preset)
{
    g_themePreset = preset;
    g_palette = BuildPalette(preset);
    SetupStyle();
}

void SetAccentColor(const ImVec4& accentColor)
{
    const float r = ImClamp(accentColor.x, 0.0f, 1.0f);
    const float g = ImClamp(accentColor.y, 0.0f, 1.0f);
    const float b = ImClamp(accentColor.z, 0.0f, 1.0f);

    g_palette.accent = ImVec4(r, g, b, 0.94f);
    g_palette.accentSoft = ImVec4(r, g, b, 0.12f);
    g_palette.text = ImVec4(0.935f, 0.955f, 0.965f, 1.0f);
    g_palette.textMuted = ImVec4(0.600f, 0.660f, 0.700f, 1.0f);
    SetupStyle();
}

void SetFontProfile(int profile)
{
    g_fontProfile = profile;
    RefreshActiveFonts();
}

void SetGlassMorphStrength(float strength)
{
    const float clamped = ImClamp(strength, 0.0f, 1.0f);
    if (std::fabs(clamped - g_glassMorphStrength) < 0.0001f)
    {
        return;
    }

    g_glassMorphStrength = clamped;
    SetupStyle();
}

void SetupStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle{};
    const float glass = ImClamp(g_glassMorphStrength, 0.0f, 1.0f);
    const float alphaMulPanels = ImLerp(1.00f, 0.54f, glass);
    const float alphaMulFrames = ImLerp(1.00f, 0.72f, glass);

    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.CellPadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 5.0f);
    style.ItemInnerSpacing = ImVec2(7.0f, 5.0f);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 9.0f;
    style.GrabMinSize = 12.0f;

    style.WindowRounding = 8.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(g_palette.panel.x, g_palette.panel.y, g_palette.panel.z, g_palette.panel.w * alphaMulPanels);
    style.Colors[ImGuiCol_Border] = ImVec4(g_palette.panelBorder.x, g_palette.panelBorder.y, g_palette.panelBorder.z, g_palette.panelBorder.w * ImLerp(1.0f, 0.78f, glass));
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.105f, 0.114f, 0.134f, 0.96f * alphaMulFrames);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.135f, 0.148f, 0.176f, 0.98f * alphaMulFrames);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.155f, 0.170f, 0.205f, 1.00f * alphaMulFrames);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_CheckMark] = g_palette.accent;
    style.Colors[ImGuiCol_SliderGrab] = g_palette.accent;
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.120f, 0.132f, 0.158f, 0.96f * alphaMulFrames);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(g_palette.accent.x, g_palette.accent.y, g_palette.accent.z, 0.20f * alphaMulFrames);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(g_palette.accent.x, g_palette.accent.y, g_palette.accent.z, 0.34f * alphaMulFrames);
    style.Colors[ImGuiCol_Header] = ImVec4(0.110f, 0.116f, 0.134f, 0.72f * alphaMulFrames);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(g_palette.accent.x, g_palette.accent.y, g_palette.accent.z, 0.15f * alphaMulFrames);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(g_palette.accent.x, g_palette.accent.y, g_palette.accent.z, 0.24f * alphaMulFrames);
    style.Colors[ImGuiCol_Separator] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_ResizeGrip] = g_palette.accentSoft;
    style.Colors[ImGuiCol_ResizeGripHovered] = g_palette.accent;
    style.Colors[ImGuiCol_ResizeGripActive] = g_palette.accent;
    style.Colors[ImGuiCol_Text] = g_palette.text;
    style.Colors[ImGuiCol_TextDisabled] = g_palette.textMuted;
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.020f, 0.022f, 0.028f, 0.28f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.45f, 0.48f, 0.54f, 0.58f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = g_palette.accent;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = g_palette.accent;
}

static ImU32 StyleAlphaU32(ImU32 color)
{
    ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);
    rgba.w *= ImGui::GetStyle().Alpha;
    return ImGui::GetColorU32(rgba);
}

void DrawGlowRect(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, ImU32 color, float rounding, float thickness)
{
    const ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);

    for (int i = 0; i < 3; ++i)
    {
        const float expand = static_cast<float>(i) * 1.1f;
        const float alpha = (18.0f - static_cast<float>(i * 5)) / 255.0f;
        const ImU32 glow = ImGui::GetColorU32(ImVec4(rgba.x, rgba.y, rgba.z, ImMax(alpha, 0.03f)));
        drawList->AddRect(min - ImVec2(expand, expand), max + ImVec2(expand, expand), glow, rounding + expand, 0, thickness);
    }

    drawList->AddRect(min, max, color, rounding, 0, thickness);
}

void DrawBackdrop(const ImVec2& origin, const ImVec2& size)
{
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const ImVec2 max = origin + size;
    const float rounding = 20.0f;

    for (int i = 0; i < 7; ++i)
    {
        const float expand = 8.0f + static_cast<float>(i) * 4.0f;
        const float alpha = 0.105f - static_cast<float>(i) * 0.012f;
        drawList->AddRectFilled(
            origin - ImVec2(expand, expand * 0.65f) + ImVec2(0.0f, 16.0f + i * 1.4f),
            max + ImVec2(expand, expand * 1.05f) + ImVec2(0.0f, 16.0f + i * 1.4f),
            ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, ImMax(alpha, 0.018f))),
            rounding + expand);
    }

    drawList->AddRectFilled(
        origin - ImVec2(1.0f, 1.0f),
        max + ImVec2(1.0f, 1.0f),
        ImGui::GetColorU32(ImVec4(g_palette.accent.x, g_palette.accent.y, g_palette.accent.z, 0.055f)),
        rounding + 1.0f);
    drawList->AddRectFilled(
        origin,
        max,
        ImGui::GetColorU32(ImVec4(g_palette.bgTop.x, g_palette.bgTop.y, g_palette.bgTop.z, 0.20f)),
        rounding);
}

void DrawSectionFrame(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const char* title, ImFont* titleFont)
{
    const float glass = ImClamp(g_glassMorphStrength, 0.0f, 1.0f);
    const bool hasTitle = title != nullptr && title[0] != '\0';
    const float shellRounding = 14.0f;
    const float innerRounding = 11.0f;
    const float headerHeight = hasTitle ? 38.0f : 0.0f;
    const float panelAlpha = ImLerp(0.88f, 0.50f, glass);
    const float borderAlpha = ImLerp(0.20f, 0.12f, glass);

    const ImVec2 shellMin = min;
    const ImVec2 shellMax = max;
    const ImVec2 innerMin = min + ImVec2(1.0f, 1.0f);
    const ImVec2 innerMax = max - ImVec2(1.0f, 1.0f);

    const ImU32 shadowDeep = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, ImLerp(0.20f, 0.10f, glass)));
    const ImU32 shadowSoft = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, ImLerp(0.12f, 0.06f, glass)));
    const ImU32 shellFill = ImGui::GetColorU32(ImVec4(g_palette.panel.x, g_palette.panel.y, g_palette.panel.z, panelAlpha));
    const ImU32 shellLift = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, ImLerp(0.055f, 0.035f, glass)));
    const ImU32 borderStrong = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, borderAlpha));
    const ImU32 borderSoft = ImGui::GetColorU32(ImVec4(g_palette.accent.x, g_palette.accent.y, g_palette.accent.z, 0.075f));
    const ImU32 innerWash = ImGui::GetColorU32(ImVec4(g_palette.panelSoft.x, g_palette.panelSoft.y, g_palette.panelSoft.z, 0.12f));

    drawList->AddRectFilled(shellMin + ImVec2(0.0f, 8.0f), shellMax + ImVec2(0.0f, 11.0f), shadowDeep, shellRounding + 2.0f);
    drawList->AddRectFilled(shellMin + ImVec2(0.0f, 3.0f), shellMax + ImVec2(0.0f, 5.0f), shadowSoft, shellRounding + 1.0f);
    drawList->AddRectFilled(shellMin, shellMax, shellFill, shellRounding);
    drawList->AddRectFilled(innerMin, innerMax, innerWash, innerRounding);
    drawList->AddRect(shellMin, shellMax, borderStrong, shellRounding, ImDrawFlags_RoundCornersAll, 1.0f);
    drawList->AddRect(innerMin, innerMax, borderSoft, innerRounding, ImDrawFlags_RoundCornersAll, 1.0f);
    drawList->AddLine(shellMin + ImVec2(12.0f, 1.0f), ImVec2(shellMax.x - 12.0f, shellMin.y + 1.0f), shellLift, 1.0f);

    if (hasTitle)
    {
        if (titleFont == nullptr)
            titleFont = g_fonts.title;

        const ImVec2 headerMin = innerMin;
        const ImVec2 headerMax(innerMax.x, innerMin.y + headerHeight);
        const ImU32 headerBase = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, ImLerp(0.052f, 0.030f, glass)));
        
        // Efeito de onda animada baseada no tempo
        float waveAlpha = 0.0f;
        float waveStretch = 0.0f;
        const ImU32 headerLine = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, ImLerp(0.070f, 0.045f, glass)));
        const ImU32 accentPill = ImGui::GetColorU32(ImVec4(g_palette.accent.x, g_palette.accent.y, g_palette.accent.z, 0.90f));
        const ImU32 accentHalo = ImGui::GetColorU32(ImVec4(g_palette.accent.x, g_palette.accent.y, g_palette.accent.z, 0.14f));
        
        const ImU32 titleColor = ImGui::GetColorU32(g_palette.text);

        // Em vez de preencher com a cor base de uma vez, desenhamos tudo com uma lógica customizada para não sobrepor cantos:
        drawList->AddRectFilled(headerMin, headerMax, headerBase, innerRounding, ImDrawFlags_RoundCornersTop);
        
        // Add a smooth animated gradient to the left half of the header using the accent color
        const float halfWidth = (headerMax.x - headerMin.x) * waveStretch;
        const float r = innerRounding;
        
        // Gradiente vertical perfeito para respeitar a curva de borda arredondada sem emendas/cortes
        for (float x = 0; x < halfWidth; x += 1.0f) {
            float t = x / halfWidth;
            float alpha = waveAlpha * (1.0f - t);
            ImU32 col = ImGui::GetColorU32(ImVec4(g_palette.accent.x, g_palette.accent.y, g_palette.accent.z, alpha));
            
            float topY = headerMin.y;
            if (x < r) {
                float dx = r - x;
                topY = headerMin.y + r - sqrtf(r * r - dx * dx);
            }
            // Usa 1.5f pro width para evitar gaps devido ao antialiasing
            drawList->AddLine(ImVec2(headerMin.x + x, topY), ImVec2(headerMin.x + x, headerMax.y), col, 1.5f);
        }

        drawList->AddRectFilled(headerMin + ImVec2(8.0f, 10.0f), headerMin + ImVec2(22.0f, 28.0f), accentHalo, 999.0f);
        drawList->AddRectFilled(headerMin + ImVec2(12.0f, 14.0f), headerMin + ImVec2(18.0f, 24.0f), accentPill, 999.0f);
        drawList->AddLine(ImVec2(headerMin.x + 10.0f, headerMax.y), ImVec2(headerMax.x - 10.0f, headerMax.y), headerLine, 1.0f);
        drawList->AddText(titleFont, titleFont->FontSize, ImVec2(headerMin.x + 28.0f, headerMin.y + 9.0f), titleColor, title);
    }
}
}
