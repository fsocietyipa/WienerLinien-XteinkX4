#pragma once

#include <cstddef>

// Assignment of configured stops to the board's columns.
//
// A column is packed with whole stop sections until it holds at least
// TARGET_ROWS_PER_COLUMN departure rows. With "Schedules per stop" set low a
// single stop would otherwise leave most of the panel empty, so following stops
// fill the column instead; a stop that already supplies enough rows keeps the
// column to itself, which is the single-stop-per-column layout.
//
// Pure arithmetic, no rendering or hardware, so the column/section rules are
// covered by host tests.
namespace wiener_board {

inline constexpr size_t MAX_COLUMNS = 3;
inline constexpr size_t TARGET_ROWS_PER_COLUMN = 3;
// Every section contributes at least one row, so the target bounds the count.
inline constexpr size_t MAX_SECTIONS_PER_COLUMN = TARGET_ROWS_PER_COLUMN;

// The stops one column stacks, top to bottom.
struct ColumnLayout {
  size_t stopIndices[MAX_SECTIONS_PER_COLUMN]{};
  size_t sectionCount = 0;
};

struct BoardLayout {
  ColumnLayout columns[MAX_COLUMNS]{};
  size_t columnCount = 0;
};

// `activeStopIndex` is the leftmost, topmost stop; the rest follow it in order
// and wrap, so moving the active stop by one slides the whole visible set.
// `configuredColumns` and `rowsPerStop` come from settings and are clamped.
BoardLayout planColumns(size_t stopCount, size_t activeStopIndex, size_t configuredColumns, size_t rowsPerStop);

}  // namespace wiener_board
