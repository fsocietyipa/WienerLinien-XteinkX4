#pragma once

#include <cstddef>

class GfxRenderer;

namespace WienerDisplayFont {

size_t normalize(const char* input, char* output, size_t capacity);
int textWidth(const char* text, int scale);
int fitScale(const char* text, int maxWidth, int maxHeight, int maxScale = 8);
void drawText(const GfxRenderer& renderer, int x, int y, const char* text, int scale, bool state);
void drawCentered(const GfxRenderer& renderer, int x, int y, int width, int height, const char* text, int scale,
                  bool state);
void splitBalanced(const char* text, char* first, size_t firstCapacity, char* second, size_t secondCapacity);

}  // namespace WienerDisplayFont
