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

// Interchange markers, for the bare letters inside a stop or destination name.
// The platform displays ring these rather than blocking them in, which is what
// tells an interchange note apart from a line number.
const DotIcon& ubahnInterchange();
const DotIcon& sbahnInterchange();

// Accessibility pictogram, shown against a low-floor departure.
const DotIcon& wheelchair();

int width(const DotIcon& icon, int scale);
int height(const DotIcon& icon, int scale);
void draw(const GfxRenderer& renderer, const DotIcon& icon, int x, int y, int scale, bool state);

}  // namespace wiener_icons
