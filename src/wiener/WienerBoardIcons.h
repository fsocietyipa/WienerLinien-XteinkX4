#pragma once

#include <cstdint>

class GfxRenderer;

// Dot-matrix pictograms for the departure board.
//
// They are plotted on the same grid as WienerDisplayFont and drawn at the same
// integer scale, so a badge sits on the text baseline and grows with it. A
// conventional anti-aliased bitmap would read as a foreign object next to the
// 5x7 dot text.
namespace wiener_icons {

struct DotIcon {
  uint8_t width;
  uint8_t height;
  // One entry per row; bit (width - 1 - column) set means that dot is drawn.
  const uint16_t* rows;
};

// The badge for a line name, or nullptr when the line does not carry one.
// U-Bahn and S-Bahn lines are a letter followed by digits ("U2", "S45");
// trams and buses ("D", "13A", "N25") keep plain text.
const DotIcon* badgeForLine(const char* line);

// Accessibility pictogram, shown against a low-floor departure.
const DotIcon& wheelchair();

int width(const DotIcon& icon, int scale);
int height(const DotIcon& icon, int scale);
void draw(const GfxRenderer& renderer, const DotIcon& icon, int x, int y, int scale, bool state);

}  // namespace wiener_icons
