#include "WienerDisplayFont.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace WienerDisplayFont {
namespace {

constexpr uint8_t DIGITS[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00}, {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31}, {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
};

constexpr uint8_t LETTERS[26][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
};

const uint8_t* glyph(const char value) {
  static constexpr uint8_t SPACE[5] = {0, 0, 0, 0, 0};
  static constexpr uint8_t DASH[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static constexpr uint8_t DOT[5] = {0, 0x60, 0x60, 0, 0};
  static constexpr uint8_t COMMA[5] = {0, 0x40, 0x30, 0, 0};
  static constexpr uint8_t COLON[5] = {0, 0x36, 0x36, 0, 0};
  static constexpr uint8_t SLASH[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
  static constexpr uint8_t LEFT_PAREN[5] = {0, 0x1C, 0x22, 0x41, 0};
  static constexpr uint8_t RIGHT_PAREN[5] = {0, 0x41, 0x22, 0x1C, 0};
  static constexpr uint8_t PLUS[5] = {0x08, 0x08, 0x3E, 0x08, 0x08};
  static constexpr uint8_t AMPERSAND[5] = {0x36, 0x49, 0x55, 0x22, 0x50};
  static constexpr uint8_t APOSTROPHE[5] = {0, 0x05, 0x03, 0, 0};
  static constexpr uint8_t QUESTION[5] = {0x02, 0x01, 0x51, 0x09, 0x06};

  if (value >= '0' && value <= '9') return DIGITS[value - '0'];
  if (value >= 'A' && value <= 'Z') return LETTERS[value - 'A'];
  switch (value) {
    case ' ':
      return SPACE;
    case '-':
      return DASH;
    case '.':
      return DOT;
    case ',':
      return COMMA;
    case ':':
      return COLON;
    case '/':
      return SLASH;
    case '(':
      return LEFT_PAREN;
    case ')':
      return RIGHT_PAREN;
    case '+':
      return PLUS;
    case '&':
      return AMPERSAND;
    case '\'':
      return APOSTROPHE;
    default:
      return QUESTION;
  }
}

void append(char value, char* output, size_t capacity, size_t& length) {
  if (length + 1 >= capacity) return;
  output[length++] = value;
  output[length] = '\0';
}

}  // namespace

size_t normalize(const char* input, char* output, const size_t capacity) {
  if (capacity == 0) return 0;
  output[0] = '\0';
  if (!input) return 0;

  size_t length = 0;
  for (size_t index = 0; input[index] != '\0'; ++index) {
    const auto value = static_cast<uint8_t>(input[index]);
    if (value < 0x80) {
      if (value >= 'a' && value <= 'z')
        append(static_cast<char>(value - 'a' + 'A'), output, capacity, length);
      else if (value >= 0x20)
        append(static_cast<char>(value), output, capacity, length);
      continue;
    }

    if (value == 0xC3 && input[index + 1] != '\0') {
      const auto next = static_cast<uint8_t>(input[++index]);
      if (next == 0x9F) {
        append('S', output, capacity, length);
        append('S', output, capacity, length);
      } else if (next == 0x84 || next == 0xA4) {
        append('A', output, capacity, length);
      } else if (next == 0x96 || next == 0xB6) {
        append('O', output, capacity, length);
      } else if (next == 0x9C || next == 0xBC) {
        append('U', output, capacity, length);
      } else if (next >= 0x80 && next <= 0x85) {
        append('A', output, capacity, length);
      } else if (next >= 0x88 && next <= 0x8B) {
        append('E', output, capacity, length);
      } else if (next >= 0x8C && next <= 0x8F) {
        append('I', output, capacity, length);
      } else if (next >= 0x92 && next <= 0x98) {
        append('O', output, capacity, length);
      } else if (next >= 0x99 && next <= 0x9C) {
        append('U', output, capacity, length);
      } else {
        append('?', output, capacity, length);
      }
      continue;
    }
    if (value == 0xC4 && input[index + 1] != '\0') {
      const auto next = static_cast<uint8_t>(input[++index]);
      if (next == 0x84 || next == 0x85)
        append('A', output, capacity, length);
      else if (next == 0x86 || next == 0x87)
        append('C', output, capacity, length);
      else if (next == 0x98 || next == 0x99)
        append('E', output, capacity, length);
      else
        append('?', output, capacity, length);
      continue;
    }
    if (value == 0xC5 && input[index + 1] != '\0') {
      const auto next = static_cast<uint8_t>(input[++index]);
      if (next == 0x81 || next == 0x82)
        append('L', output, capacity, length);
      else if (next == 0x83 || next == 0x84)
        append('N', output, capacity, length);
      else if (next == 0x9A || next == 0x9B)
        append('S', output, capacity, length);
      else if (next == 0xB9 || next == 0xBA || next == 0xBB || next == 0xBC)
        append('Z', output, capacity, length);
      else
        append('?', output, capacity, length);
      continue;
    }
    append('?', output, capacity, length);
    while ((static_cast<uint8_t>(input[index + 1]) & 0xC0) == 0x80) ++index;
  }
  return length;
}

int textWidth(const char* text, const int scale) {
  const size_t length = text ? strlen(text) : 0;
  return length == 0 ? 0 : static_cast<int>((length * 6 - 1) * std::max(scale, 1));
}

int fitScale(const char* text, const int maxWidth, const int maxHeight, const int maxScale) {
  if (!text || text[0] == '\0' || maxWidth <= 0 || maxHeight < 7) return 1;
  const int heightScale = maxHeight / 7;
  const int widthAtOne = textWidth(text, 1);
  const int widthScale = widthAtOne == 0 ? maxScale : maxWidth / widthAtOne;
  return std::max(1, std::min({maxScale, heightScale, widthScale}));
}

void drawText(const GfxRenderer& renderer, const int x, const int y, const char* text, const int scale,
              const bool state) {
  if (!text || scale < 1) return;
  int cursor = x;
  for (size_t index = 0; text[index] != '\0'; ++index) {
    const uint8_t* columns = glyph(text[index]);
    for (int column = 0; column < 5; ++column) {
      for (int row = 0; row < 7; ++row) {
        if ((columns[column] & (1U << row)) != 0) {
          renderer.fillRect(cursor + column * scale, y + row * scale, scale, scale, state);
        }
      }
    }
    cursor += 6 * scale;
  }
}

void drawCentered(const GfxRenderer& renderer, const int x, const int y, const int width, const int height,
                  const char* text, const int scale, const bool state) {
  drawText(renderer, x + std::max(0, (width - textWidth(text, scale)) / 2), y + std::max(0, (height - 7 * scale) / 2),
           text, scale, state);
}

void splitBalanced(const char* text, char* first, const size_t firstCapacity, char* second,
                   const size_t secondCapacity) {
  if (firstCapacity == 0 || secondCapacity == 0) return;
  first[0] = '\0';
  second[0] = '\0';
  if (!text) return;

  const size_t length = strlen(text);
  size_t split = length;
  size_t bestDistance = length;
  for (size_t index = 1; index < length; ++index) {
    if (text[index] != ' ') continue;
    const size_t distance = index > length / 2 ? index - length / 2 : length / 2 - index;
    if (distance < bestDistance) {
      split = index;
      bestDistance = distance;
    }
  }
  if (split == length) {
    snprintf(first, firstCapacity, "%s", text);
    return;
  }

  const size_t firstLength = std::min(split, firstCapacity - 1);
  memcpy(first, text, firstLength);
  first[firstLength] = '\0';
  snprintf(second, secondCapacity, "%s", text + split + 1);
}

}  // namespace WienerDisplayFont
