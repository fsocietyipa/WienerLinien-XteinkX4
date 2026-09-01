#include "WienerBoardText.h"

#include <algorithm>
#include <cstring>

#include "WienerBoardIcons.h"
#include "WienerDisplayFont.h"

namespace WienerBoardText {
namespace {

// A badge stands in for a whole one-letter word, so the characters on both
// sides must be separators. "S45" and "BUS" keep their letters.
const wiener_icons::DotIcon* badgeAt(const char* text, const size_t index) {
  const char value = text[index];
  if (value != 'S' && value != 'U') return nullptr;
  if (index > 0 && text[index - 1] != ' ') return nullptr;
  const char next = text[index + 1];
  if (next != '\0' && next != ' ') return nullptr;
  return value == 'U' ? &wiener_icons::ubahnInterchange() : &wiener_icons::sbahnInterchange();
}

// One dot of air after every cell, matching the font's own advance.
constexpr int GAP = 1;
// Text rows are seven dots tall; the ringed markers are taller, so they hang
// evenly above and below the text band the way they do on the platform signs.
constexpr int TEXT_ROWS = 7;

}  // namespace

bool hasMarker(const char* text) {
  if (text == nullptr) return false;
  for (size_t index = 0; text[index] != '\0'; ++index) {
    if (badgeAt(text, index) != nullptr) return true;
  }
  return false;
}

int markerOverhangRows() { return wiener_icons::ubahnInterchange().height - TEXT_ROWS; }

int width(const char* text, const int scale, const bool markers) {
  if (text == nullptr || text[0] == '\0') return 0;
  const int step = std::max(scale, 1);
  int total = 0;
  for (size_t index = 0; text[index] != '\0'; ++index) {
    const auto* badge = markers ? badgeAt(text, index) : nullptr;
    total += (badge != nullptr ? badge->width + GAP : 6) * step;
  }
  return total - step;
}

int fitScale(const char* text, const int maxWidth, const int maxHeight, const int maxScale, const bool markers) {
  if (text == nullptr || text[0] == '\0' || maxWidth <= 0 || maxHeight < TEXT_ROWS) return 1;
  for (int scale = std::max(maxScale, 1); scale > 1; --scale) {
    if (width(text, scale, markers) <= maxWidth && TEXT_ROWS * scale <= maxHeight) return scale;
  }
  return 1;
}

void draw(const GfxRenderer& renderer, const int x, const int y, const char* text, const int scale, const bool state,
          const bool markers) {
  if (text == nullptr || scale < 1) return;
  int cursor = x;
  for (size_t index = 0; text[index] != '\0'; ++index) {
    if (const auto* badge = markers ? badgeAt(text, index) : nullptr) {
      const int offset = (TEXT_ROWS - badge->height) * scale / 2;
      wiener_icons::draw(renderer, *badge, cursor, y + offset, scale, state);
      cursor += (badge->width + GAP) * scale;
      continue;
    }
    const char glyph[2] = {text[index], '\0'};
    WienerDisplayFont::drawText(renderer, cursor, y, glyph, scale, state);
    cursor += 6 * scale;
  }
}

void drawCentered(const GfxRenderer& renderer, const int x, const int y, const int boxWidth, const int boxHeight,
                  const char* text, const int scale, const bool state, const bool markers) {
  draw(renderer, x + std::max(0, (boxWidth - width(text, scale, markers)) / 2),
       y + std::max(0, (boxHeight - TEXT_ROWS * scale) / 2), text, scale, state, markers);
}

void trimToWidth(char* text, const int maxWidth, const int scale, const bool markers) {
  if (text == nullptr || maxWidth <= 0 || width(text, scale, markers) <= maxWidth) return;
  size_t length = strlen(text);
  while (length > 0) {
    --length;
    text[length] = '\0';
    // Mark the cut, but restore the characters if this length still does not
    // fit, so the ellipsis never eats into an earlier one.
    char first = 0;
    char second = 0;
    if (length >= 2) {
      first = text[length - 2];
      second = text[length - 1];
      text[length - 2] = '.';
      text[length - 1] = '.';
    }
    if (width(text, scale, markers) <= maxWidth) return;
    if (length >= 2) {
      text[length - 2] = first;
      text[length - 1] = second;
    }
  }
}

}  // namespace WienerBoardText
