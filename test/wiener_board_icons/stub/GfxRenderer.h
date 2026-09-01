#pragma once

// Host stub: the icon module only ever calls fillRect, and the layout rules
// under test do not need a real framebuffer.
class GfxRenderer {
 public:
  void fillRect(int, int, int, int, bool) const {}
};
