#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace render_ui
{
    namespace
    {
        constexpr float kMenuWidth = 980.0f;
        constexpr float kMenuHeight = 760.0f;
        constexpr float kHeaderHeight = 74.0f;
        constexpr float kOuterPadding = 18.0f;
        constexpr float kColumnGap = 14.0f;
        constexpr float kLeftColumnWidth = 456.0f;
        constexpr float kRightColumnWidth = kMenuWidth - (kOuterPadding * 2.0f) - kColumnGap - kLeftColumnWidth;
        constexpr const char* kBrandText = "oopz 315124";

        bool g_visible = false;
        float g_openAlpha = 0.0f;
        ImVec2 g_menuPosition{};
        bool g_menuPositionInitialized = false;
        bool g_draggingMenu = false;
        ImVec2 g_menuDragOffset{};

        int g_languageIndex = 0;
        int g_menuToggleKey = static_cast<int>(ImGuiKey_Insert);
        int g_themeProfile = 0;
        int g_fontProfile = 0;
        float g_accentColor[4] = { 0.45f, 0.72f, 1.00f, 1.0f };
        float g_menuColor[4] = { 0.035f, 0.045f, 0.065f, 1.0f };
        bool g_glassEnabled = true;
        float g_glassAmount = 0.28f;

        struct OverlaySettings
        {
            bool watermark = true;
            bool fps = true;
            bool ping = true;
            bool time = true;
        };

        struct CrosshairSettings
        {
            bool enabled = false;
            int type = 0;
            float size = 12.0f;
            float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        };

        OverlaySettings g_overlay{};
        CrosshairSettings g_crosshair{};
        ImVec2 g_watermarkPosition(18.0f, 18.0f);
        bool g_watermarkDragging = false;
        ImVec2 g_watermarkDragOffset{};
        bool g_initialized = false;

        const char* Text(
            const char* zh,
            const char* en,
            const char* ru = nullptr,
            const char* es = nullptr,
            const char* pt = nullptr,
            const char* ja = nullptr,
            const char* ko = nullptr)
        {
            switch (std::clamp(g_languageIndex, 0, 6))
            {
            case 0: return zh;
            case 1: return en;
            case 2: return ru ? ru : en;
            case 3: return es ? es : en;
            case 4: return pt ? pt : en;
            case 5: return ja ? ja : en;
            case 6: return ko ? ko : en;
            default: return en;
            }
        }

        odyssey::theme::ThemePreset ThemePresetFromIndex(int index)
        {
            switch (index)
            {
            case 1: return odyssey::theme::ThemePreset::DarkGreen;
            case 2: return odyssey::theme::ThemePreset::DarkBlue;
            default: return odyssey::theme::ThemePreset::DarkWhite;
            }
        }

        void ApplyTheme()
        {
            g_themeProfile = std::clamp(g_themeProfile, 0, 2);
            g_fontProfile = std::clamp(g_fontProfile, 0, 2);
            g_glassAmount = std::clamp(g_glassAmount, 0.05f, 0.60f);
            odyssey::theme::SetThemePreset(ThemePresetFromIndex(g_themeProfile));
            odyssey::theme::SetAccentColor(ImVec4(g_accentColor[0], g_accentColor[1], g_accentColor[2], 1.0f));
            odyssey::theme::SetFontProfile(g_fontProfile);
            odyssey::theme::SetGlassMorphStrength(g_glassEnabled ? g_glassAmount : 0.0f);
            accent_colour[0] = g_accentColor[0];
            accent_colour[1] = g_accentColor[1];
            accent_colour[2] = g_accentColor[2];
            accent_colour[3] = 1.0f;
        }

        int ImGuiKeyToVirtualKey(int key)
        {
            switch (key)
            {
            case 1: return VK_LBUTTON;
            case 2: return VK_RBUTTON;
            case 3: return VK_MBUTTON;
            case 4: return VK_XBUTTON1;
            case 5: return VK_XBUTTON2;
            case static_cast<int>(ImGuiKey_None): return 0;
            case static_cast<int>(ImGuiKey_Insert): return VK_INSERT;
            case static_cast<int>(ImGuiKey_Delete): return VK_DELETE;
            case static_cast<int>(ImGuiKey_Home): return VK_HOME;
            case static_cast<int>(ImGuiKey_End): return VK_END;
            case static_cast<int>(ImGuiKey_PageUp): return VK_PRIOR;
            case static_cast<int>(ImGuiKey_PageDown): return VK_NEXT;
            case static_cast<int>(ImGuiKey_Tab): return VK_TAB;
            case static_cast<int>(ImGuiKey_CapsLock): return VK_CAPITAL;
            case static_cast<int>(ImGuiKey_LeftCtrl): return VK_LCONTROL;
            case static_cast<int>(ImGuiKey_RightCtrl): return VK_RCONTROL;
            case static_cast<int>(ImGuiKey_LeftAlt): return VK_LMENU;
            case static_cast<int>(ImGuiKey_RightAlt): return VK_RMENU;
            case static_cast<int>(ImGuiKey_LeftShift): return VK_LSHIFT;
            case static_cast<int>(ImGuiKey_RightShift): return VK_RSHIFT;
            case static_cast<int>(ImGuiKey_Escape): return VK_ESCAPE;
            case static_cast<int>(ImGuiKey_Enter): return VK_RETURN;
            case static_cast<int>(ImGuiKey_Space): return VK_SPACE;
            case static_cast<int>(ImGuiKey_Backspace): return VK_BACK;
            default: break;
            }

            if (key >= static_cast<int>(ImGuiKey_A) && key <= static_cast<int>(ImGuiKey_Z))
                return 'A' + (key - static_cast<int>(ImGuiKey_A));
            if (key >= static_cast<int>(ImGuiKey_0) && key <= static_cast<int>(ImGuiKey_9))
                return '0' + (key - static_cast<int>(ImGuiKey_0));
            if (key >= static_cast<int>(ImGuiKey_F1) && key <= static_cast<int>(ImGuiKey_F12))
                return VK_F1 + (key - static_cast<int>(ImGuiKey_F1));
            return VK_INSERT;
        }

        int VirtualKeyToImGuiKey(int key)
        {
            switch (key)
            {
            case 0: return static_cast<int>(ImGuiKey_None);
            case VK_LBUTTON: return 1;
            case VK_RBUTTON: return 2;
            case VK_MBUTTON: return 3;
            case VK_XBUTTON1: return 4;
            case VK_XBUTTON2: return 5;
            case VK_INSERT: return static_cast<int>(ImGuiKey_Insert);
            case VK_DELETE: return static_cast<int>(ImGuiKey_Delete);
            case VK_HOME: return static_cast<int>(ImGuiKey_Home);
            case VK_END: return static_cast<int>(ImGuiKey_End);
            case VK_PRIOR: return static_cast<int>(ImGuiKey_PageUp);
            case VK_NEXT: return static_cast<int>(ImGuiKey_PageDown);
            case VK_TAB: return static_cast<int>(ImGuiKey_Tab);
            case VK_CAPITAL: return static_cast<int>(ImGuiKey_CapsLock);
            case VK_LCONTROL: return static_cast<int>(ImGuiKey_LeftCtrl);
            case VK_RCONTROL: return static_cast<int>(ImGuiKey_RightCtrl);
            case VK_LMENU: return static_cast<int>(ImGuiKey_LeftAlt);
            case VK_RMENU: return static_cast<int>(ImGuiKey_RightAlt);
            case VK_LSHIFT: return static_cast<int>(ImGuiKey_LeftShift);
            case VK_RSHIFT: return static_cast<int>(ImGuiKey_RightShift);
            case VK_ESCAPE: return static_cast<int>(ImGuiKey_Escape);
            case VK_RETURN: return static_cast<int>(ImGuiKey_Enter);
            case VK_SPACE: return static_cast<int>(ImGuiKey_Space);
            case VK_BACK: return static_cast<int>(ImGuiKey_Backspace);
            default: break;
            }

            if (key >= 'A' && key <= 'Z')
                return static_cast<int>(ImGuiKey_A) + (key - 'A');
            if (key >= '0' && key <= '9')
                return static_cast<int>(ImGuiKey_0) + (key - '0');
            if (key >= VK_F1 && key <= VK_F12)
                return static_cast<int>(ImGuiKey_F1) + (key - VK_F1);
            return static_cast<int>(ImGuiKey_Insert);
        }

        void SyncMenuKey()
        {
            g_menuKeyVirtual = ImGuiKeyToVirtualKey(g_menuToggleKey);
            for (int index = 0; index < static_cast<int>(IM_ARRAYSIZE(g_menuKeyCodes)); ++index)
            {
                if (g_menuKeyCodes[index] == g_menuKeyVirtual)
                {
                    g_selectedMenuKey = index;
                    break;
                }
            }
        }

        std::filesystem::path SettingsPath()
        {
            return GetUserSmite2ConfigDirectory() / L".oopz_ui.cfg";
        }

        void ParseColor(const std::string& value, float color[4])
        {
            std::stringstream stream(value);
            std::string part;
            for (int index = 0; index < 4 && std::getline(stream, part, ','); ++index)
                color[index] = std::clamp(static_cast<float>(std::atof(part.c_str())), 0.0f, 1.0f);
        }

        void SaveSettings()
        {
            const std::filesystem::path path = SettingsPath();
            std::error_code error;
            std::filesystem::create_directories(path.parent_path(), error);
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
                return;

            output
                << "language=" << g_languageIndex << '\n'
                << "menu_key=" << g_menuToggleKey << '\n'
                << "theme=" << g_themeProfile << '\n'
                << "font=" << g_fontProfile << '\n'
                << "accent=" << g_accentColor[0] << ',' << g_accentColor[1] << ',' << g_accentColor[2] << ',' << g_accentColor[3] << '\n'
                << "menu_color=" << g_menuColor[0] << ',' << g_menuColor[1] << ',' << g_menuColor[2] << ',' << g_menuColor[3] << '\n'
                << "glass=" << (g_glassEnabled ? 1 : 0) << '\n'
                << "glass_amount=" << g_glassAmount << '\n'
                << "watermark=" << (g_overlay.watermark ? 1 : 0) << '\n'
                << "fps=" << (g_overlay.fps ? 1 : 0) << '\n'
                << "ping=" << (g_overlay.ping ? 1 : 0) << '\n'
                << "time=" << (g_overlay.time ? 1 : 0) << '\n'
                << "crosshair=" << (g_crosshair.enabled ? 1 : 0) << '\n'
                << "crosshair_type=" << g_crosshair.type << '\n'
                << "crosshair_size=" << g_crosshair.size << '\n'
                << "crosshair_color=" << g_crosshair.color[0] << ',' << g_crosshair.color[1] << ',' << g_crosshair.color[2] << ',' << g_crosshair.color[3] << '\n'
                << "translation=" << (g_translationEnabled ? 1 : 0) << '\n'
                << "translation_incoming=" << (g_translationIncomingEnabled ? 1 : 0) << '\n'
                << "translation_outgoing=" << (g_translationOutgoingEnabled ? 1 : 0) << '\n'
                << "translation_original=" << (g_translationShowOriginal ? 1 : 0) << '\n'
                << "loading_stats=" << (g_showLoadingPlayerRanks ? 1 : 0) << '\n';
            output.close();
            SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_HIDDEN);
        }

        void LoadSettings()
        {
            const std::filesystem::path path = SettingsPath();
            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return;

            std::string line;
            while (std::getline(input, line))
            {
                const size_t separator = line.find('=');
                if (separator == std::string::npos)
                    continue;
                const std::string key = line.substr(0, separator);
                const std::string value = line.substr(separator + 1);
                try
                {
                    if (key == "language") g_languageIndex = std::stoi(value);
                    else if (key == "menu_key") g_menuToggleKey = std::stoi(value);
                    else if (key == "theme") g_themeProfile = std::stoi(value);
                    else if (key == "font") g_fontProfile = std::stoi(value);
                    else if (key == "accent") ParseColor(value, g_accentColor);
                    else if (key == "menu_color") ParseColor(value, g_menuColor);
                    else if (key == "glass") g_glassEnabled = std::stoi(value) != 0;
                    else if (key == "glass_amount") g_glassAmount = static_cast<float>(std::atof(value.c_str()));
                    else if (key == "watermark") g_overlay.watermark = std::stoi(value) != 0;
                    else if (key == "fps") g_overlay.fps = std::stoi(value) != 0;
                    else if (key == "ping") g_overlay.ping = std::stoi(value) != 0;
                    else if (key == "time") g_overlay.time = std::stoi(value) != 0;
                    else if (key == "crosshair") g_crosshair.enabled = std::stoi(value) != 0;
                    else if (key == "crosshair_type") g_crosshair.type = std::stoi(value);
                    else if (key == "crosshair_size") g_crosshair.size = static_cast<float>(std::atof(value.c_str()));
                    else if (key == "crosshair_color") ParseColor(value, g_crosshair.color);
                    else if (key == "translation") g_translationEnabled = std::stoi(value) != 0;
                    else if (key == "translation_incoming") g_translationIncomingEnabled = std::stoi(value) != 0;
                    else if (key == "translation_outgoing") g_translationOutgoingEnabled = std::stoi(value) != 0;
                    else if (key == "translation_original") g_translationShowOriginal = std::stoi(value) != 0;
                    else if (key == "loading_stats") g_showLoadingPlayerRanks = std::stoi(value) != 0;
                }
                catch (...)
                {
                }
            }

            g_languageIndex = std::clamp(g_languageIndex, 0, 6);
            g_crosshair.type = std::clamp(g_crosshair.type, 0, 3);
            g_crosshair.size = std::clamp(g_crosshair.size, 4.0f, 40.0f);
            SyncMenuKey();
            ApplyTheme();
        }

        void SaveAfterInteraction()
        {
            static double lastSaveTime = 0.0;
            if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsKeyReleased(ImGuiKey_Enter))
                return;
            const double now = ImGui::GetTime();
            if (now - lastSaveTime < 0.20)
                return;
            lastSaveTime = now;
            SaveSettings();
        }

        bool BeginCard(const char* id, const char* title, const ImVec2& position, const ImVec2& size)
        {
            const auto& palette = odyssey::theme::GetPalette();
            ImGui::SetCursorPos(position);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(palette.panel.x, palette.panel.y, palette.panel.z, 0.78f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(palette.panelBorder.x, palette.panelBorder.y, palette.panelBorder.z, 0.88f));
            const bool open = ImGui::BeginChild(id, size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::TextColored(palette.accent, "%s", title);
            ImGui::Separator();
            return open;
        }

        void EndCard()
        {
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }

        void DrawHeader()
        {
            const auto& palette = odyssey::theme::GetPalette();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 position = ImGui::GetWindowPos();
            const ImVec2 size = ImGui::GetWindowSize();
            const bool connected = GetMatchServerEndpointSnapshot().valid;
            const ImVec4 statusColor = connected ? palette.success : palette.warning;

            draw->AddRectFilled(position, position + ImVec2(size.x, kHeaderHeight), ImGui::GetColorU32(ImVec4(palette.bgTop.x, palette.bgTop.y, palette.bgTop.z, 0.94f)), 12.0f, ImDrawFlags_RoundCornersTop);
            draw->AddRectFilled(position, position + ImVec2(5.0f, kHeaderHeight), ImGui::GetColorU32(palette.accent), 3.0f, ImDrawFlags_RoundCornersTopLeft);
            draw->AddText(position + ImVec2(20.0f, 14.0f), ImGui::GetColorU32(palette.text), kBrandText);
            draw->AddText(position + ImVec2(21.0f, 43.0f), ImGui::GetColorU32(palette.textMuted), Text("精简控制台", "COMPACT CONTROL"));

            const char* status = connected ? Text("对局已连接", "MATCH CONNECTED") : Text("等待对局", "WAITING FOR MATCH");
            const ImVec2 statusSize = ImGui::CalcTextSize(status);
            const ImVec2 statusMin(position.x + size.x - statusSize.x - 74.0f, position.y + 24.0f);
            draw->AddRectFilled(statusMin, statusMin + ImVec2(statusSize.x + 34.0f, 28.0f), ImGui::GetColorU32(ImVec4(statusColor.x, statusColor.y, statusColor.z, 0.12f)), 14.0f);
            draw->AddCircleFilled(statusMin + ImVec2(12.0f, 14.0f), 4.0f, ImGui::GetColorU32(statusColor), 16);
            draw->AddText(statusMin + ImVec2(22.0f, 7.0f), ImGui::GetColorU32(statusColor), status);

            ImGui::SetCursorPos(ImVec2(kMenuWidth - 42.0f, 19.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.04f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(palette.danger.x, palette.danger.y, palette.danger.z, 0.75f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.danger);
            if (ImGui::Button("X", ImVec2(28.0f, 28.0f)))
                ForceClose();
            ImGui::PopStyleColor(3);
        }

        void UpdateMenuDrag()
        {
            const ImVec2 windowPosition = ImGui::GetWindowPos();
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const ImVec2 dragMaximum = windowPosition + ImVec2(kMenuWidth - 54.0f, kHeaderHeight);
            const bool insideDragArea =
                mouse.x >= windowPosition.x && mouse.x <= dragMaximum.x &&
                mouse.y >= windowPosition.y && mouse.y <= dragMaximum.y;
            if (!g_draggingMenu && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && insideDragArea)
            {
                g_draggingMenu = true;
                g_menuDragOffset = mouse - windowPosition;
            }
            if (g_draggingMenu)
            {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    g_menuPosition = mouse - g_menuDragOffset;
                else
                    g_draggingMenu = false;
            }
        }

        void DrawMatchCard(const ImVec2& position)
        {
            if (BeginCard("match_connection", Text("对局网络", "MATCH CONNECTION"), position, ImVec2(kLeftColumnWidth, 236.0f)))
            {
                const MatchServerEndpointSnapshot endpoint = GetMatchServerEndpointSnapshot();
                const auto& palette = odyssey::theme::GetPalette();
                const ImVec4 statusColor = endpoint.valid ? palette.success : palette.textMuted;

                ImGui::TextColored(palette.textMuted, "%s", Text("状态", "Status"));
                ImGui::SameLine(116.0f);
                ImGui::TextColored(statusColor, "%s", endpoint.valid ? Text("已连接", "Connected") : Text("未检测到", "Not detected"));
                ImGui::TextColored(palette.textMuted, "%s", Text("对局 IP", "Match IP"));
                ImGui::SameLine(116.0f);
                ImGui::TextUnformatted(endpoint.valid ? endpoint.address.c_str() : "-");
                ImGui::TextColored(palette.textMuted, "%s", Text("端口", "Port"));
                ImGui::SameLine(116.0f);
                if (endpoint.valid) ImGui::Text("%u", static_cast<unsigned>(endpoint.port));
                else ImGui::TextUnformatted("-");

                ImGui::Dummy(ImVec2(0.0f, 2.0f));
                odyssey::widgets::CompactToggleRow("loading_player_ranks", Text("选角 / 载入战绩", "Draft & Loading Stats"), &g_showLoadingPlayerRanks);
                odyssey::widgets::CompactToggleRow("draft_stats_position", Text("调整战绩框位置", "Move Draft Stats"), &g_draftStatsPositionEditEnabled);

                static ULONGLONG copiedUntil = 0;
                ImGui::BeginDisabled(!endpoint.valid);
                if (odyssey::widgets::ActionButton(
                    "copy_match_endpoint",
                    copiedUntil > GetTickCount64() ? Text("已复制", "Copied") : Text("复制 IP:端口", "Copy IP:Port"),
                    ImVec2(148.0f, 28.0f)))
                {
                    const std::string endpointText = endpoint.ipv6
                        ? "[" + endpoint.address + "]:" + std::to_string(endpoint.port)
                        : endpoint.address + ":" + std::to_string(endpoint.port);
                    ImGui::SetClipboardText(endpointText.c_str());
                    copiedUntil = GetTickCount64() + 1600;
                }
                ImGui::EndDisabled();
            }
            EndCard();
        }

        void DrawThemeCard(const ImVec2& position)
        {
            const char* themes[] = {
                Text("深色白", "Dark White"),
                Text("深色绿", "Dark Green"),
                Text("深色蓝", "Dark Blue")
            };
            const char* fonts[] = {
                Text("标准", "Standard"),
                Text("紧凑", "Compact"),
                Text("清晰", "Clear")
            };

            if (BeginCard("theme_settings", Text("主题", "THEME"), position, ImVec2(kLeftColumnWidth, 322.0f)))
            {
                odyssey::widgets::CompactComboRow(Text("主题", "Theme"), &g_themeProfile, themes, IM_ARRAYSIZE(themes), 190.0f);
                odyssey::widgets::CompactComboRow(Text("字体", "Font"), &g_fontProfile, fonts, IM_ARRAYSIZE(fonts), 190.0f);
                odyssey::widgets::ColorValueRow("accent_color", Text("强调色", "Accent Color"), g_accentColor, false);
                odyssey::widgets::ColorValueRow("menu_color", Text("菜单颜色", "Menu Color"), g_menuColor, false);
                odyssey::widgets::CompactToggleRow("glass_effect", Text("玻璃效果", "Glass Effect"), &g_glassEnabled);
                if (g_glassEnabled)
                    odyssey::widgets::CompactSliderFloatInlineRow(Text("玻璃强度", "Glass Amount"), &g_glassAmount, 0.05f, 0.60f, "%.2f", 150.0f);
                ApplyTheme();
            }
            EndCard();
        }

        void DrawDisplayTranslationCard(const ImVec2& position)
        {
            if (BeginCard("display_translation", Text("菜单显示 / 翻译", "OVERLAY / TRANSLATION"), position, ImVec2(kRightColumnWidth, 306.0f)))
            {
                odyssey::widgets::CompactToggleRow("watermark", Text("水印", "Watermark"), &g_overlay.watermark);
                odyssey::widgets::CompactToggleRow("show_fps", Text("显示 FPS", "Show FPS"), &g_overlay.fps);
                odyssey::widgets::CompactToggleRow("show_ping", Text("显示 Ping", "Show Ping"), &g_overlay.ping);
                odyssey::widgets::CompactToggleRow("show_time", Text("显示时间", "Show Time"), &g_overlay.time);
                ImGui::Separator();
                odyssey::widgets::CompactToggleRow("translation_enabled", Text("自动翻译", "Enable Translation"), &g_translationEnabled);
                odyssey::widgets::CompactToggleRow("translation_incoming", Text("翻译收到的英文", "Translate Incoming"), &g_translationIncomingEnabled);
                odyssey::widgets::CompactToggleRow("translation_outgoing", Text("中文自动发送英文", "Translate Outgoing"), &g_translationOutgoingEnabled);
                odyssey::widgets::CompactToggleRow("translation_original", Text("显示原文", "Show Original"), &g_translationShowOriginal);
                odyssey::widgets::CompactToggleRow("translation_position", Text("调整翻译框位置", "Move Translation Box"), &g_translationPositionEditEnabled);
                ImGui::TextColored(odyssey::theme::GetPalette().textMuted, "%s", TranslationStatusText());
            }
            EndCard();
        }

        void DrawCrosshairCard(const ImVec2& position)
        {
            const char* types[] = {
                Text("经典", "Classic"),
                Text("圆点", "Dot"),
                Text("圆环", "Circle"),
                Text("战术", "Tactical")
            };

            if (BeginCard("crosshair_settings", Text("准星", "CROSSHAIR"), position, ImVec2(kRightColumnWidth, 180.0f)))
            {
                odyssey::widgets::CompactToggleRow("crosshair_enable", Text("启用准星", "Enable Crosshair"), &g_crosshair.enabled);
                odyssey::widgets::CompactComboRow(Text("准星类型", "Crosshair Type"), &g_crosshair.type, types, IM_ARRAYSIZE(types), 180.0f);
                odyssey::widgets::CompactSliderFloatInlineRow(Text("准星大小", "Crosshair Size"), &g_crosshair.size, 4.0f, 40.0f, "%.0f", 146.0f);
                odyssey::widgets::ColorValueRow("crosshair_color", Text("准星颜色", "Crosshair Color"), g_crosshair.color, true);
                g_crosshair.type = std::clamp(g_crosshair.type, 0, 3);
                g_crosshair.size = std::clamp(g_crosshair.size, 4.0f, 40.0f);
            }
            EndCard();
        }

        void DrawLanguageAndKeyCard(const ImVec2& position)
        {
            const char* languages[] = { "简体中文", "English", "Русский", "Español", "Português", "日本語", "한국어" };
            if (BeginCard("language_key", Text("语言 / 按键", "LANGUAGE / KEYBIND"), position, ImVec2(kRightColumnWidth, 140.0f)))
            {
                const int previousLanguage = g_languageIndex;
                odyssey::widgets::CompactComboRow(Text("语言", "Language"), &g_languageIndex, languages, IM_ARRAYSIZE(languages), 180.0f);
                odyssey::widgets::KeybindCaptureRow("menu_toggle_key", Text("菜单按键", "Menu Key"), &g_menuToggleKey);
                if (previousLanguage != g_languageIndex)
                    odyssey::widgets::SetLanguage(g_languageIndex);
                SyncMenuKey();
            }
            EndCard();
        }

        void DrawMenu()
        {
            DrawHeader();
            UpdateMenuDrag();

            const float top = kHeaderHeight + 16.0f;
            DrawMatchCard(ImVec2(kOuterPadding, top));
            DrawThemeCard(ImVec2(kOuterPadding, top + 250.0f));
            DrawDisplayTranslationCard(ImVec2(kOuterPadding + kLeftColumnWidth + kColumnGap, top));
            DrawCrosshairCard(ImVec2(kOuterPadding + kLeftColumnWidth + kColumnGap, top + 320.0f));
            DrawLanguageAndKeyCard(ImVec2(kOuterPadding + kLeftColumnWidth + kColumnGap, top + 514.0f));
            SaveAfterInteraction();
        }

        void DrawCrosshair(const ImVec2& displaySize)
        {
            if (!g_crosshair.enabled)
                return;

            ImDrawList* draw = ImGui::GetForegroundDrawList();
            const ImVec2 center(displaySize.x * 0.5f, displaySize.y * 0.5f);
            const float size = std::clamp(g_crosshair.size, 4.0f, 40.0f);
            const ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(
                g_crosshair.color[0], g_crosshair.color[1], g_crosshair.color[2], g_crosshair.color[3]));
            const ImU32 shadow = IM_COL32(0, 0, 0, 190);

            switch (g_crosshair.type)
            {
            case 1:
                draw->AddCircleFilled(center, std::max(2.0f, size * 0.18f) + 1.0f, shadow, 24);
                draw->AddCircleFilled(center, std::max(2.0f, size * 0.18f), color, 24);
                break;
            case 2:
                draw->AddCircle(center, size * 0.52f, shadow, 40, 3.0f);
                draw->AddCircle(center, size * 0.52f, color, 40, 1.5f);
                break;
            case 3:
            {
                const float gap = size * 0.30f;
                const float arm = size * 0.72f;
                draw->AddLine(center + ImVec2(-arm, 0.0f), center + ImVec2(-gap, 0.0f), shadow, 4.0f);
                draw->AddLine(center + ImVec2(gap, 0.0f), center + ImVec2(arm, 0.0f), shadow, 4.0f);
                draw->AddLine(center + ImVec2(0.0f, -arm), center + ImVec2(0.0f, -gap), shadow, 4.0f);
                draw->AddLine(center + ImVec2(0.0f, gap), center + ImVec2(0.0f, arm), shadow, 4.0f);
                draw->AddLine(center + ImVec2(-arm, 0.0f), center + ImVec2(-gap, 0.0f), color, 2.0f);
                draw->AddLine(center + ImVec2(gap, 0.0f), center + ImVec2(arm, 0.0f), color, 2.0f);
                draw->AddLine(center + ImVec2(0.0f, -arm), center + ImVec2(0.0f, -gap), color, 2.0f);
                draw->AddLine(center + ImVec2(0.0f, gap), center + ImVec2(0.0f, arm), color, 2.0f);
                draw->AddCircleFilled(center, 2.0f, color, 16);
                break;
            }
            default:
                draw->AddLine(center + ImVec2(-size, 0.0f), center + ImVec2(size, 0.0f), shadow, 4.0f);
                draw->AddLine(center + ImVec2(0.0f, -size), center + ImVec2(0.0f, size), shadow, 4.0f);
                draw->AddLine(center + ImVec2(-size, 0.0f), center + ImVec2(size, 0.0f), color, 2.0f);
                draw->AddLine(center + ImVec2(0.0f, -size), center + ImVec2(0.0f, size), color, 2.0f);
                break;
            }
        }

        void DrawWatermark(const ImVec2& displaySize)
        {
            if (!g_overlay.watermark)
                return;

            std::string metrics;
            if (g_overlay.fps)
                metrics += "FPS " + std::to_string(std::max(0, static_cast<int>(ImGui::GetIO().Framerate + 0.5f)));
            if (g_overlay.ping)
            {
                const float ping = GetLocalPingMillisecondsSafe();
                char pingText[24]{};
                if (ping >= 0.0f) sprintf_s(pingText, "%.0fms", ping);
                else strcpy_s(pingText, "--ms");
                if (!metrics.empty()) metrics += "  |  ";
                metrics += "PING ";
                metrics += pingText;
            }
            if (g_overlay.time)
            {
                SYSTEMTIME time{};
                GetLocalTime(&time);
                char timeText[16]{};
                sprintf_s(timeText, "%02u:%02u:%02u", time.wHour, time.wMinute, time.wSecond);
                if (!metrics.empty()) metrics += "  |  ";
                metrics += timeText;
            }

            const auto& palette = odyssey::theme::GetPalette();
            ImDrawList* draw = ImGui::GetForegroundDrawList();
            const ImVec2 brandSize = ImGui::CalcTextSize(kBrandText);
            const ImVec2 metricSize = ImGui::CalcTextSize(metrics.c_str());
            const ImVec2 size(brandSize.x + metricSize.x + 52.0f, 34.0f);
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const ImVec2 watermarkMaximum = g_watermarkPosition + size;
            const bool insideWatermark =
                mouse.x >= g_watermarkPosition.x && mouse.x <= watermarkMaximum.x &&
                mouse.y >= g_watermarkPosition.y && mouse.y <= watermarkMaximum.y;

            if (g_visible && insideWatermark && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                g_watermarkDragging = true;
                g_watermarkDragOffset = mouse - g_watermarkPosition;
            }
            if (g_watermarkDragging)
            {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    g_watermarkPosition = mouse - g_watermarkDragOffset;
                else
                {
                    g_watermarkDragging = false;
                    SaveSettings();
                }
            }

            g_watermarkPosition.x = std::clamp(g_watermarkPosition.x, 8.0f, std::max(8.0f, displaySize.x - size.x - 8.0f));
            g_watermarkPosition.y = std::clamp(g_watermarkPosition.y, 8.0f, std::max(8.0f, displaySize.y - size.y - 8.0f));
            const ImVec2 maximum = g_watermarkPosition + size;
            draw->AddRectFilled(g_watermarkPosition + ImVec2(0.0f, 2.0f), maximum + ImVec2(0.0f, 2.0f), IM_COL32(0, 0, 0, 70), 9.0f);
            draw->AddRectFilled(g_watermarkPosition, maximum, ImGui::GetColorU32(ImVec4(palette.panel.x, palette.panel.y, palette.panel.z, 0.94f)), 9.0f);
            draw->AddRect(g_watermarkPosition, maximum, ImGui::GetColorU32(palette.panelBorder), 9.0f, 0, 1.0f);
            draw->AddRectFilled(g_watermarkPosition, g_watermarkPosition + ImVec2(4.0f, size.y), ImGui::GetColorU32(palette.accent), 2.0f, ImDrawFlags_RoundCornersLeft);
            draw->AddText(g_watermarkPosition + ImVec2(13.0f, 9.0f), ImGui::GetColorU32(palette.text), kBrandText);
            if (!metrics.empty())
            {
                const float separator = g_watermarkPosition.x + brandSize.x + 25.0f;
                draw->AddLine(ImVec2(separator, g_watermarkPosition.y + 8.0f), ImVec2(separator, maximum.y - 8.0f), ImGui::GetColorU32(palette.panelBorder), 1.0f);
                draw->AddText(ImVec2(separator + 11.0f, g_watermarkPosition.y + 9.0f), ImGui::GetColorU32(palette.textMuted), metrics.c_str());
            }
        }

        void Initialize()
        {
            if (g_initialized)
                return;
            g_menuToggleKey = VirtualKeyToImGuiKey(g_menuKeyVirtual);
            LoadSettings();
            odyssey::widgets::SetLanguage(g_languageIndex);
            ApplyTheme();
            g_initialized = true;
        }
    }

    int current_language_index()
    {
        return g_languageIndex;
    }

    bool IsVisible()
    {
        return g_visible && g_openAlpha > 0.01f;
    }

    bool WantsInput()
    {
        return g_visible;
    }

    void ForceOpen()
    {
        g_visible = true;
        g_openAlpha = 1.0f;
        g_draggingMenu = false;
        if (ImGui::GetCurrentContext())
        {
            ImGui::GetIO().MouseDrawCursor = true;
            ImGui::SetNextFrameWantCaptureMouse(true);
            ImGui::SetNextFrameWantCaptureKeyboard(true);
        }
    }

    void ForceClose()
    {
        g_visible = false;
        g_openAlpha = 0.0f;
        g_draggingMenu = false;
        g_watermarkDragging = false;
        SaveSettings();
        if (ImGui::GetCurrentContext())
        {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseDrawCursor = false;
            for (int button = 0; button < 5; ++button)
                io.AddMouseButtonEvent(button, false);
            io.ClearInputCharacters();
            io.ClearInputKeys();
            ImGui::SetNextFrameWantCaptureMouse(false);
            ImGui::SetNextFrameWantCaptureKeyboard(false);
        }
        if (GetCapture() != nullptr)
            ReleaseCapture();
    }

    void OnDeviceDestroyed()
    {
    }

    void Render()
    {
        Initialize();
        ApplyTheme();
        SyncMenuKey();
        odyssey::widgets::SetLanguage(g_languageIndex);

        static bool previousKeyDown = false;
        const bool keyDown = g_menuKeyVirtual > 0 && (GetAsyncKeyState(g_menuKeyVirtual) & 0x8000) != 0;
        if (keyDown && !previousKeyDown)
        {
            if (g_visible) ForceClose();
            else ForceOpen();
        }
        previousKeyDown = keyDown;

        ImGuiIO& io = ImGui::GetIO();
        io.MouseDrawCursor = WantsInput();
        const ImVec2 displaySize = io.DisplaySize;
        DrawCrosshair(displaySize);
        DrawWatermark(displaySize);

        g_openAlpha = std::clamp(g_openAlpha + io.DeltaTime * 9.0f * (g_visible ? 1.0f : -1.0f), 0.0f, 1.0f);
        if (g_openAlpha <= 0.01f)
            return;

        if (!g_menuPositionInitialized)
        {
            g_menuPosition = ImVec2(
                std::max(0.0f, (displaySize.x - kMenuWidth) * 0.5f),
                std::max(0.0f, (displaySize.y - kMenuHeight) * 0.5f));
            g_menuPositionInitialized = true;
        }
        g_menuPosition.x = std::clamp(g_menuPosition.x, 0.0f, std::max(0.0f, displaySize.x - kMenuWidth));
        g_menuPosition.y = std::clamp(g_menuPosition.y, 0.0f, std::max(0.0f, displaySize.y - kMenuHeight));

        const auto& palette = odyssey::theme::GetPalette();
        const float glass = g_glassEnabled ? std::clamp(g_glassAmount / 0.60f, 0.0f, 1.0f) : 0.0f;
        ImGui::SetNextWindowPos(g_menuPosition, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(kMenuWidth, kMenuHeight), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_openAlpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(
            g_menuColor[0], g_menuColor[1], g_menuColor[2], 0.98f + (0.72f - 0.98f) * glass));
        ImGui::Begin(
            "oopz compact control",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::GetWindowDrawList()->AddRect(
            ImGui::GetWindowPos(),
            ImGui::GetWindowPos() + ImGui::GetWindowSize(),
            ImGui::GetColorU32(palette.panelBorder),
            12.0f,
            0,
            1.0f);
        DrawMenu();
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }
}
