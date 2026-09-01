#include "WienerBoardIcons.h"

#include <GfxRenderer.h>

#include <algorithm>

namespace wiener_icons {
namespace {

// Interchange marker for a stop name, e.g. "Praterstern S U". Filled disc with
// the letter knocked out, the way the platform signs carry it.
//
//   ..#####..
//   .#######.
//   ###.#.###
//   ###.#.###
//   ###.#.###
//   ###.#.###
//   ###...###
//   .#######.
//   ..#####..
constexpr uint16_t UBAHN_INTERCHANGE_ROWS[] = {
    0x007C,  // ..#####..
    0x00FE,  // .#######.
    0x01D7,  // ###.#.###
    0x01D7,  // ###.#.###
    0x01D7,  // ###.#.###
    0x01D7,  // ###.#.###
    0x01C7,  // ###...###
    0x00FE,  // .#######.
    0x007C,  // ..#####..
};
constexpr DotIcon UBAHN_INTERCHANGE{9, 9, UBAHN_INTERCHANGE_ROWS};

// S-Bahn interchange marker, same treatment.
//
//   ..#####..
//   .#######.
//   ###...###
//   ##.######
//   ###...###
//   ######.##
//   ###...###
//   .#######.
//   ..#####..
constexpr uint16_t SBAHN_INTERCHANGE_ROWS[] = {
    0x007C,  // ..#####..
    0x00FE,  // .#######.
    0x01C7,  // ###...###
    0x01BF,  // ##.######
    0x01C7,  // ###...###
    0x01FB,  // ######.##
    0x01C7,  // ###...###
    0x00FE,  // .#######.
    0x007C,  // ..#####..
};
constexpr DotIcon SBAHN_INTERCHANGE{9, 9, SBAHN_INTERCHANGE_ROWS};
// Accessibility pictogram shown against a low-floor departure, traced from a
// drawn reference: the figure faces left, the arm crosses the back to the rim,
// and the wheel is left open at the upper left so the seat and arm read as
// separate strokes instead of merging into it.
//
//   ........###.....
//   ........###.....
//   .........#......
//   .........#......
//   .....#########..
//   .....#...#....#.
//   .........#....#.
//   ....######.....#
//   ...##..........#
//   ...##..........#
//   ..#.#..........#
//   .#...#........#.
//   #....#........#.
//   ......##....##..
//   ........####....
constexpr uint16_t WHEELCHAIR_ROWS[] = {
    0x00E0,  // ........###.....
    0x00E0,  // ........###.....
    0x0040,  // .........#......
    0x0040,  // .........#......
    0x07FC,  // .....#########..
    0x0442,  // .....#...#....#.
    0x0042,  // .........#....#.
    0x0FC1,  // ....######.....#
    0x1801,  // ...##..........#
    0x1801,  // ...##..........#
    0x2801,  // ..#.#..........#
    0x4402,  // .#...#........#.
    0x8402,  // #....#........#.
    0x030C,  // ......##....##..
    0x00F0,  // ........####....
};
constexpr DotIcon WHEELCHAIR{16, 15, WHEELCHAIR_ROWS};

}  // namespace

const DotIcon& ubahnInterchange() { return UBAHN_INTERCHANGE; }

const DotIcon& sbahnInterchange() { return SBAHN_INTERCHANGE; }

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
