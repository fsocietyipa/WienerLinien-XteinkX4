#include <gtest/gtest.h>

#include <cstring>

#include "WienerBoardIcons.h"
#include "WienerBoardText.h"

namespace {

// Advance per cell at scale 1: a glyph takes six dots, a ringed marker its own
// width plus one. The trailing gap is dropped.
int plainWidth(const char* text) { return static_cast<int>(strlen(text)) * 6 - 1; }

TEST(WienerBoardTextTest, PlainTextMatchesTheFontMetrics) {
  EXPECT_EQ(WienerBoardText::width("RING", 1), plainWidth("RING"));
  EXPECT_EQ(WienerBoardText::width("RING", 3), plainWidth("RING") * 3);
  EXPECT_EQ(WienerBoardText::width("", 1), 0);
  EXPECT_EQ(WienerBoardText::width(nullptr, 1), 0);
}

// "Praterstern S U" is the real shape of these names: the trailing letters are
// interchange markers, and each is wider than the glyph it replaces.
TEST(WienerBoardTextTest, InterchangeLettersRenderWiderThanGlyphs) {
  const int marker = wiener_icons::ubahnInterchange().width;
  EXPECT_EQ(WienerBoardText::width("PRATERSTERN U", 1), plainWidth("PRATERSTERN U") - 6 + (marker + 1));
  // Two markers, each replacing one glyph.
  EXPECT_EQ(WienerBoardText::width("PRATERSTERN S U", 1), plainWidth("PRATERSTERN S U") + 2 * (marker + 1 - 6));
}

// A badge stands in for a whole one-letter word only.
TEST(WienerBoardTextTest, OnlyStandaloneLettersBecomeMarkers) {
  for (const char* text : {"S45", "U2", "BUS", "SUD", "USA", "ST"}) {
    EXPECT_EQ(WienerBoardText::width(text, 1), plainWidth(text)) << text;
  }
}

TEST(WienerBoardTextTest, MarkersAreRecognisedAtEitherEnd) {
  const int marker = wiener_icons::ubahnInterchange().width;
  EXPECT_EQ(WienerBoardText::width("U RING", 1), plainWidth("U RING") + (marker + 1 - 6));
  EXPECT_EQ(WienerBoardText::width("RING U", 1), plainWidth("RING U") + (marker + 1 - 6));
  EXPECT_EQ(WienerBoardText::width("RING U WEST", 1), plainWidth("RING U WEST") + (marker + 1 - 6));
}

TEST(WienerBoardTextTest, FitScaleRespectsTheMeasuredWidth) {
  const char* text = "PRATERSTERN S U";
  const int scale = WienerBoardText::fitScale(text, 200, 40, 5);
  EXPECT_LE(WienerBoardText::width(text, scale), 200);
  EXPECT_LE(7 * scale, 40);
  // A box that cannot fit even one dot per cell still returns a usable scale.
  EXPECT_GE(WienerBoardText::fitScale(text, 1, 7, 5), 1);
}

TEST(WienerBoardTextTest, TrimShortensUntilItFits) {
  char text[64] = "PRATERSTERN VOLKSTHEATER";
  WienerBoardText::trimToWidth(text, 60, 1);
  EXPECT_LE(WienerBoardText::width(text, 1), 60);
  EXPECT_LT(strlen(text), strlen("PRATERSTERN VOLKSTHEATER"));
}

// A marker is wider than a glyph, so trimming has to measure rather than count
// characters or the ring would overflow the cell.
TEST(WienerBoardTextTest, TrimAccountsForMarkerWidth) {
  char text[64] = "PRATERSTERN S U";
  const int budget = 90;
  WienerBoardText::trimToWidth(text, budget, 1);
  EXPECT_LE(WienerBoardText::width(text, 1), budget);
}

TEST(WienerBoardTextTest, TrimLeavesTextThatAlreadyFits) {
  char text[64] = "RING";
  WienerBoardText::trimToWidth(text, 1000, 1);
  EXPECT_STREQ(text, "RING");
}

TEST(WienerBoardTextTest, UAndSUseDifferentMarkers) {
  EXPECT_NE(&wiener_icons::ubahnInterchange(), &wiener_icons::sbahnInterchange());
  EXPECT_NE(wiener_icons::ubahnInterchange().rows[3], wiener_icons::sbahnInterchange().rows[3]);
}

}  // namespace
