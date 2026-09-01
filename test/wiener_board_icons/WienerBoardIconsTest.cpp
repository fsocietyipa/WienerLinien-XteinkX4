#include <gtest/gtest.h>

#include "WienerBoardIcons.h"

namespace {

using wiener_icons::badgeForLine;

TEST(WienerBoardIconsTest, BadgesRapidTransitLines) {
  EXPECT_NE(badgeForLine("U1"), nullptr);
  EXPECT_NE(badgeForLine("U6"), nullptr);
  EXPECT_NE(badgeForLine("S1"), nullptr);
  EXPECT_NE(badgeForLine("S45"), nullptr);
}

TEST(WienerBoardIconsTest, UAndSGetDifferentBadges) { EXPECT_NE(badgeForLine("U2"), badgeForLine("S2")); }

// Trams and buses keep plain text: "D" and "O" are tram lines, "13A" and "N25"
// are buses, and none of them carry a rapid-transit badge.
TEST(WienerBoardIconsTest, LeavesTramAndBusLinesAlone) {
  EXPECT_EQ(badgeForLine("1"), nullptr);
  EXPECT_EQ(badgeForLine("D"), nullptr);
  EXPECT_EQ(badgeForLine("O"), nullptr);
  EXPECT_EQ(badgeForLine("13A"), nullptr);
  EXPECT_EQ(badgeForLine("N25"), nullptr);
  EXPECT_EQ(badgeForLine("VRT"), nullptr);
}

// A bare letter is not a line number, and a letter after the digits means a
// bus variant rather than an S-Bahn branch.
TEST(WienerBoardIconsTest, RequiresDigitsAfterTheLetter) {
  EXPECT_EQ(badgeForLine("U"), nullptr);
  EXPECT_EQ(badgeForLine("S"), nullptr);
  EXPECT_EQ(badgeForLine("S4A"), nullptr);
  EXPECT_EQ(badgeForLine("U2X"), nullptr);
}

TEST(WienerBoardIconsTest, HandlesEmptyAndNullSafely) {
  EXPECT_EQ(badgeForLine(nullptr), nullptr);
  EXPECT_EQ(badgeForLine(""), nullptr);
}

TEST(WienerBoardIconsTest, ScalesWithTheBoardText) {
  const auto& chair = wiener_icons::wheelchair();
  EXPECT_EQ(wiener_icons::width(chair, 1), chair.width);
  EXPECT_EQ(wiener_icons::height(chair, 3), chair.height * 3);
  // A scale below one must not collapse the icon.
  EXPECT_EQ(wiener_icons::width(chair, 0), chair.width);
}

// Every row must fit the declared width, or a stray high bit would draw
// outside the icon's box.
TEST(WienerBoardIconsTest, RowBitsStayInsideTheDeclaredWidth) {
  for (const auto* icon : {badgeForLine("U1"), badgeForLine("S1"), &wiener_icons::wheelchair()}) {
    ASSERT_NE(icon, nullptr);
    const uint16_t mask = static_cast<uint16_t>((1U << icon->width) - 1U);
    for (int row = 0; row < icon->height; ++row) {
      EXPECT_EQ(icon->rows[row] & ~mask, 0) << "row " << row;
    }
  }
}

}  // namespace
