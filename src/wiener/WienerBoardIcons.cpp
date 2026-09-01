#include "WienerBoardIcons.h"

#include <GfxRenderer.h>

#include <algorithm>

namespace wiener_icons {
namespace {

// U-Bahn badge: the letter is knocked out of a solid block, matching the
// white-on-blue line logo once the panel polarity is applied.
//
//   #######
//   #.###.#
//   #.###.#
//   #.###.#
//   #.###.#
//   ##...##
//   #######
constexpr uint16_t UBAHN_ROWS[] = {
    0x007F,  // #######
    0x005D,  // #.###.#
    0x005D,  // #.###.#
    0x005D,  // #.###.#
    0x005D,  // #.###.#
    0x0063,  // ##...##
    0x007F,  // #######
};
constexpr DotIcon UBAHN{7, 7, UBAHN_ROWS};

// S-Bahn badge, same treatment as the U-Bahn one.
//
//   #######
//   #.....#
//   #.#####
//   #.....#
//   #####.#
//   #.....#
//   #######
constexpr uint16_t SBAHN_ROWS[] = {
    0x007F,  // #######
    0x0041,  // #.....#
    0x005F,  // #.#####
    0x0041,  // #.....#
    0x007D,  // #####.#
    0x0041,  // #.....#
    0x007F,  // #######
};
constexpr DotIcon SBAHN{7, 7, SBAHN_ROWS};

// Accessibility pictogram shown against a low-floor departure. The wheel is
// left open along its top edge so the seat reads as a separate stroke instead
// of merging into the rim at this size.
//
//   .....##........
//   .....##........
//   ...............
//   ....###........
//   ....##.#####...
//   ....##.....#...
//   ....##.........
//   ...#######.....
//   ..##.....##....
//   ..#.......#.##.
//   .##.......##.#.
//   .#.........#.#.
//   .##.......##...
//   ..#########....
//   ...............
constexpr uint16_t WHEELCHAIR_ROWS[] = {
    0x0300,  // .....##........
    0x0300,  // .....##........
    0x0000,  // ...............
    0x0700,  // ....###........
    0x06F8,  // ....##.#####...
    0x0608,  // ....##.....#...
    0x0600,  // ....##.........
    0x0FE0,  // ...#######.....
    0x1830,  // ..##.....##....
    0x1016,  // ..#.......#.##.
    0x301A,  // .##.......##.#.
    0x200A,  // .#.........#.#.
    0x3018,  // .##.......##...
    0x1FF0,  // ..#########....
    0x0000,  // ...............
};
constexpr DotIcon WHEELCHAIR{15, 15, WHEELCHAIR_ROWS};

}  // namespace

const DotIcon* badgeForLine(const char* line) {
  if (line == nullptr) return nullptr;
  if (line[0] != 'U' && line[0] != 'S') return nullptr;
  // A bare letter, or anything with a further letter in it, is a tram or bus
  // line rather than a rapid-transit one.
  if (line[1] == '\0') return nullptr;
  for (const char* c = line + 1; *c != '\0'; ++c) {
    if (*c < '0' || *c > '9') return nullptr;
  }
  return line[0] == 'U' ? &UBAHN : &SBAHN;
}

const DotIcon& wheelchair() { return WHEELCHAIR; }

int width(const DotIcon& icon, const int scale) { return icon.width * std::max(scale, 1); }

int height(const DotIcon& icon, const int scale) { return icon.height * std::max(scale, 1); }

void draw(const GfxRenderer& renderer, const DotIcon& icon, const int x, const int y, const int scale,
          const bool state) {
  const int step = std::max(scale, 1);
  for (int row = 0; row < icon.height; ++row) {
    const uint16_t bits = icon.rows[row];
    for (int column = 0; column < icon.width; ++column) {
      if ((bits & (1U << (icon.width - 1 - column))) != 0) {
        renderer.fillRect(x + column * step, y + row * step, step, step, state);
      }
    }
  }
}

}  // namespace wiener_icons
