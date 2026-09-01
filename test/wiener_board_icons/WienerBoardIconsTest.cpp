#include <gtest/gtest.h>

#include "WienerBoardIcons.h"

namespace {

TEST(WienerBoardIconsTest, UAndSUseDifferentMarkers) {
  EXPECT_NE(&wiener_icons::ubahnInterchange(), &wiener_icons::sbahnInterchange());
}

TEST(WienerBoardIconsTest, MarkersShareOneBoxSoTheyAlignInText) {
  const auto& u = wiener_icons::ubahnInterchange();
  const auto& s = wiener_icons::sbahnInterchange();
  EXPECT_EQ(u.width, s.width);
  EXPECT_EQ(u.height, s.height);
}

// The markers are discs with the letter knocked out, so the outer ring of dots
// is identical and only the interior differs.
TEST(WienerBoardIconsTest, MarkersDifferOnlyInTheirInterior) {
  const auto& u = wiener_icons::ubahnInterchange();
  const auto& s = wiener_icons::sbahnInterchange();
  EXPECT_EQ(u.rows[0], s.rows[0]);
  EXPECT_EQ(u.rows[u.height - 1], s.rows[s.height - 1]);
  bool interiorDiffers = false;
  for (int row = 2; row < u.height - 2; ++row) {
    if (u.rows[row] != s.rows[row]) interiorDiffers = true;
  }
  EXPECT_TRUE(interiorDiffers);
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
  for (const auto* icon :
       {&wiener_icons::ubahnInterchange(), &wiener_icons::sbahnInterchange(), &wiener_icons::wheelchair()}) {
    const uint16_t mask = static_cast<uint16_t>((1U << icon->width) - 1U);
    for (int row = 0; row < icon->height; ++row) {
      EXPECT_EQ(icon->rows[row] & ~mask, 0) << "row " << row;
    }
  }
}

}  // namespace
