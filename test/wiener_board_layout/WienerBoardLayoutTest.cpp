#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "WienerBoardLayout.h"

namespace {

using wiener_board::BoardLayout;
using wiener_board::planColumns;

// Flattens a layout into "stop indices in reading order, per column".
std::vector<std::vector<size_t>> columnsOf(const BoardLayout& layout) {
  std::vector<std::vector<size_t>> out;
  for (size_t column = 0; column < layout.columnCount; ++column) {
    std::vector<size_t> stops;
    for (size_t index = 0; index < layout.columns[column].sectionCount; ++index) {
      stops.push_back(layout.columns[column].stopIndices[index]);
    }
    out.push_back(stops);
  }
  return out;
}

size_t totalSections(const BoardLayout& layout) {
  size_t total = 0;
  for (size_t column = 0; column < layout.columnCount; ++column) total += layout.columns[column].sectionCount;
  return total;
}

// --- A stop that already fills the column keeps it to itself ----------------

TEST(WienerBoardLayoutTest, ThreeRowsPerStopIsOneStopPerColumn) {
  const auto layout = planColumns(/*stopCount=*/3, /*activeStopIndex=*/0, /*configuredColumns=*/1, /*rowsPerStop=*/3);
  EXPECT_EQ(columnsOf(layout), (std::vector<std::vector<size_t>>{{0}}));
}

TEST(WienerBoardLayoutTest, ManyRowsPerStopIsOneStopPerColumn) {
  const auto layout = planColumns(4, 0, 3, 6);
  EXPECT_EQ(columnsOf(layout), (std::vector<std::vector<size_t>>{{0}, {1}, {2}}));
}

// --- A low per-stop setting pulls in following stops ------------------------

TEST(WienerBoardLayoutTest, OneRowPerStopStacksThreeStopsInOneColumn) {
  const auto layout = planColumns(5, 0, 1, 1);
  EXPECT_EQ(columnsOf(layout), (std::vector<std::vector<size_t>>{{0, 1, 2}}));
}

TEST(WienerBoardLayoutTest, TwoRowsPerStopStacksTwoStops) {
  const auto layout = planColumns(5, 0, 1, 2);
  EXPECT_EQ(columnsOf(layout), (std::vector<std::vector<size_t>>{{0, 1}}));
}

// --- Two-column behaviour ---------------------------------------------------

// Columns read top-to-bottom then left-to-right.
TEST(WienerBoardLayoutTest, TwoColumnsFillDownThenAcross) {
  const auto layout = planColumns(6, 0, 2, 1);
  EXPECT_EQ(columnsOf(layout), (std::vector<std::vector<size_t>>{{0, 1, 2}, {3, 4, 5}}));
}

// The stacking must not swallow the second column: two stops across two
// columns belong side by side, not stacked in the first.
TEST(WienerBoardLayoutTest, TwoStopsInTwoColumnsSitSideBySide) {
  const auto layout = planColumns(2, 0, 2, 1);
  EXPECT_EQ(columnsOf(layout), (std::vector<std::vector<size_t>>{{0}, {1}}));
}

TEST(WienerBoardLayoutTest, ThreeStopsInTwoColumnsFavourTheLeft) {
  const auto layout = planColumns(3, 0, 2, 1);
  EXPECT_EQ(columnsOf(layout), (std::vector<std::vector<size_t>>{{0, 1}, {2}}));
}

TEST(WienerBoardLayoutTest, ThreeColumnsSpreadEightStops) {
  const auto layout = planColumns(8, 0, 3, 1);
  EXPECT_EQ(columnsOf(layout), (std::vector<std::vector<size_t>>{{0, 1, 2}, {3, 4, 5}, {6, 7}}));
}

// --- Previous/Next slides the whole visible set by one stop -----------------

TEST(WienerBoardLayoutTest, ActiveStopSlidesTheWholeSet) {
  const auto layout = planColumns(6, 1, 2, 1);
  EXPECT_EQ(columnsOf(layout), (std::vector<std::vector<size_t>>{{1, 2, 3}, {4, 5, 0}}));
}

TEST(WienerBoardLayoutTest, SlidingWrapsPastTheEnd) {
  const auto layout = planColumns(4, 3, 1, 1);
  EXPECT_EQ(columnsOf(layout), (std::vector<std::vector<size_t>>{{3, 0, 1}}));
}

// --- No stop is ever shown twice on one screen ------------------------------

TEST(WienerBoardLayoutTest, FewerStopsThanSlotsNeverRepeatsAStop) {
  for (size_t stopCount = 1; stopCount <= 8; ++stopCount) {
    for (size_t columns = 1; columns <= 3; ++columns) {
      for (size_t rows = 1; rows <= 10; ++rows) {
        const auto layout = planColumns(stopCount, 0, columns, rows);
        std::vector<size_t> seen;
        for (const auto& column : columnsOf(layout)) seen.insert(seen.end(), column.begin(), column.end());
        std::vector<size_t> unique = seen;
        std::sort(unique.begin(), unique.end());
        unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
        EXPECT_EQ(seen.size(), unique.size()) << "stops=" << stopCount << " columns=" << columns << " rows=" << rows;
        EXPECT_LE(seen.size(), stopCount);
      }
    }
  }
}

TEST(WienerBoardLayoutTest, SectionCountNeverExceedsTheColumnCap) {
  for (size_t stopCount = 1; stopCount <= 8; ++stopCount) {
    for (size_t columns = 1; columns <= 3; ++columns) {
      for (size_t rows = 1; rows <= 10; ++rows) {
        const auto layout = planColumns(stopCount, 0, columns, rows);
        EXPECT_LE(layout.columnCount, wiener_board::MAX_COLUMNS);
        for (size_t column = 0; column < layout.columnCount; ++column) {
          EXPECT_GE(layout.columns[column].sectionCount, 1u);
          EXPECT_LE(layout.columns[column].sectionCount, wiener_board::MAX_SECTIONS_PER_COLUMN);
        }
      }
    }
  }
}

// --- Degenerate inputs ------------------------------------------------------

TEST(WienerBoardLayoutTest, NoStopsProducesNoColumns) { EXPECT_EQ(planColumns(0, 0, 3, 3).columnCount, 0u); }

TEST(WienerBoardLayoutTest, ZeroColumnsProducesNoColumns) { EXPECT_EQ(planColumns(4, 0, 0, 3).columnCount, 0u); }

TEST(WienerBoardLayoutTest, ZeroRowsPerStopIsTreatedAsOne) {
  const auto layout = planColumns(4, 0, 1, 0);
  EXPECT_EQ(totalSections(layout), 3u);
}

TEST(WienerBoardLayoutTest, MoreConfiguredColumnsThanStopsUsesOnlyWhatIsNeeded) {
  const auto layout = planColumns(1, 0, 3, 3);
  EXPECT_EQ(columnsOf(layout), (std::vector<std::vector<size_t>>{{0}}));
}

}  // namespace
