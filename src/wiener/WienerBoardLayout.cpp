#include "WienerBoardLayout.h"

#include <algorithm>

namespace wiener_board {

BoardLayout planColumns(const size_t stopCount, const size_t activeStopIndex, const size_t configuredColumns,
                        const size_t rowsPerStop) {
  BoardLayout layout;
  if (stopCount == 0 || configuredColumns == 0) return layout;

  const size_t rows = std::max<size_t>(1, rowsPerStop);
  // Whole stops only: enough of them to reach the target, never a stop split
  // across a column boundary.
  const size_t sectionsPerColumn = std::min(MAX_SECTIONS_PER_COLUMN, (TARGET_ROWS_PER_COLUMN + rows - 1) / rows);
  const size_t availableColumns = std::min({configuredColumns, MAX_COLUMNS, stopCount});

  const size_t totalSections = std::min(stopCount, availableColumns * sectionsPerColumn);
  // Spread over as many columns as there are sections before stacking any of
  // them, so two stops in a two-column layout sit side by side rather than
  // stacking in the first column and leaving the second empty.
  layout.columnCount = std::min(availableColumns, totalSections);
  if (layout.columnCount == 0) return layout;

  // Columns fill top-to-bottom then left-to-right; the remainder goes to the
  // leftmost columns, so any height difference sits at the right edge.
  const size_t base = totalSections / layout.columnCount;
  const size_t extra = totalSections % layout.columnCount;
  size_t consumed = 0;
  for (size_t column = 0; column < layout.columnCount; ++column) {
    const size_t count = std::min(base + (column < extra ? 1 : 0), MAX_SECTIONS_PER_COLUMN);
    ColumnLayout& target = layout.columns[column];
    for (size_t index = 0; index < count; ++index) {
      target.stopIndices[target.sectionCount++] = (activeStopIndex + consumed) % stopCount;
      ++consumed;
    }
  }
  return layout;
}

}  // namespace wiener_board
