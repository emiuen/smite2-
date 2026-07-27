#pragma once

#include "imgui.h"

namespace odyssey::widgets
{
void SetLanguage(int languageIndex);

struct FloatingRootContext
{
    ImVec2 position{};
    ImVec2 size{};
    bool open = false;
};

FloatingRootContext BeginFloatingRoot(const char* id, ImVec2* position, const ImVec2& size, const ImVec2& viewportPos, const ImVec2& viewportSize);
void EndFloatingRoot();

bool BeginPanel(const char* id, const ImVec2& position, const ImVec2& size, const char* title, const char* subtitle = nullptr);
void EndPanel();

bool SidebarItem(const char* id, const char* label, const char* hint, bool active);
bool TopTab(const char* id, const char* label, bool active);

void ToggleRow(const char* id, const char* label, const char* hint, bool* value);
void CheckboxRow(const char* id, const char* label, bool* value, const char* badgeText = nullptr, const ImVec4* badgeColor = nullptr);
void CompactCheckboxRow(const char* id, const char* label, bool* value);
void CompactToggleRow(const char* id, const char* label, bool* value);
void CompactToggleColorRow(const char* id, const char* label, bool* value, float color[4], bool alpha = false);
void KeybindCaptureRow(const char* id, const char* label, int* key);
void CompactComboRow(const char* label, int* currentItem, const char* const items[], int itemCount, float width = 108.0f);
void CompactComboInlineRow(const char* label, int* currentItem, const char* const items[], int itemCount, float width = 120.0f);
void CompactSliderIntRow(const char* label, int* value, int minValue, int maxValue, float width = 108.0f);
void CompactSliderFloatRow(const char* label, float* value, float minValue, float maxValue, const char* format = "%.1f", float width = 108.0f);
void CompactSliderIntInlineRow(const char* label, int* value, int minValue, int maxValue, float width = 170.0f);
void CompactSliderFloatInlineRow(const char* label, float* value, float minValue, float maxValue, const char* format = "%.1f", float width = 170.0f);
void CompactSliderFloatInputRow(
    const char* id,
    const char* label,
    float* value,
    float minValue,
    float maxValue,
    float step,
    const char* inputFormat = "%.0f",
    float width = 268.0f,
    const float* discreteValues = nullptr,
    int discreteValueCount = 0);
void LabeledCombo(const char* label, int* currentItem, const char* const items[], int itemCount);
void LabeledSliderInt(const char* label, int* value, int minValue, int maxValue);
void LabeledSliderFloat(const char* label, float* value, float minValue, float maxValue, const char* format = "%.3f");
void ColorPickerRow(const char* id, const char* label, float color[4], bool alpha = false, bool* enabled = nullptr);
void ColorValueRow(const char* id, const char* label, float color[4], bool alpha = false);
bool ActionButton(const char* id, const char* label, const ImVec2& size, const ImVec4* accent = nullptr);
void InputField(const char* id, char* buffer, int bufferSize, const char* hint);
void ColorSwatch(const ImVec2& position, const ImVec2& size, const ImVec4& color);
}
