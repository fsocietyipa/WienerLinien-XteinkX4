#include <gtest/gtest.h>

#include "FirmwareVersion.h"

namespace {

using firmware_version::isNewer;
using firmware_version::parse;

TEST(FirmwareVersionTest, ParsesAFullTriple) {
  const auto v = parse("1.2.3");
  EXPECT_EQ(v.major, 1);
  EXPECT_EQ(v.minor, 2);
  EXPECT_EQ(v.patch, 3);
}

// The release tags this project publishes are two-segment ("0.1"), so the
// missing patch segment must read as 0 rather than as uninitialised memory.
TEST(FirmwareVersionTest, MissingSegmentsReadAsZero) {
  const auto twoSegment = parse("0.1");
  EXPECT_EQ(twoSegment.major, 0);
  EXPECT_EQ(twoSegment.minor, 1);
  EXPECT_EQ(twoSegment.patch, 0);

  const auto oneSegment = parse("2");
  EXPECT_EQ(oneSegment.major, 2);
  EXPECT_EQ(oneSegment.minor, 0);
  EXPECT_EQ(oneSegment.patch, 0);
}

TEST(FirmwareVersionTest, AcceptsALeadingV) {
  const auto v = parse("v0.2.1");
  EXPECT_EQ(v.major, 0);
  EXPECT_EQ(v.minor, 2);
  EXPECT_EQ(v.patch, 1);
}

TEST(FirmwareVersionTest, UnparsableTextIsZero) {
  const auto v = parse("not-a-version");
  EXPECT_EQ(v.major, 0);
  EXPECT_EQ(v.minor, 0);
  EXPECT_EQ(v.patch, 0);
  EXPECT_EQ(parse(nullptr).major, 0);
}

// A "0.1" tag against a "0.1.0" build is the same release, not an update.
TEST(FirmwareVersionTest, ShortTagEqualsPaddedVersion) {
  EXPECT_FALSE(isNewer("0.1", "0.1.0"));
  EXPECT_FALSE(isNewer("v0.1", "0.1.0"));
}

TEST(FirmwareVersionTest, DetectsNewerReleases) {
  EXPECT_TRUE(isNewer("0.2", "0.1.0"));
  EXPECT_TRUE(isNewer("1.0", "0.9.9"));
  EXPECT_TRUE(isNewer("0.1.1", "0.1.0"));
}

TEST(FirmwareVersionTest, RejectsOlderReleases) {
  EXPECT_FALSE(isNewer("0.1", "0.2.0"));
  EXPECT_FALSE(isNewer("0.9.9", "1.0.0"));
  EXPECT_FALSE(isNewer("0.1.0", "0.1.1"));
}

// Development builds carry a branch/SHA suffix; the leading triple still decides.
TEST(FirmwareVersionTest, IgnoresBuildSuffixOnCurrentVersion) {
  EXPECT_FALSE(isNewer("0.1", "0.1.0-develop-0eb832b"));
  EXPECT_TRUE(isNewer("0.2", "0.1.0-develop-0eb832b"));
}

// An equal triple still upgrades when the running build is a release candidate.
TEST(FirmwareVersionTest, ReleaseCandidateUpgradesToFinal) {
  EXPECT_TRUE(isNewer("0.1.0", "0.1.0-rc+abc1234"));
  EXPECT_FALSE(isNewer("0.1.0", "0.1.0"));
}

TEST(FirmwareVersionTest, EmptyInputsAreNeverNewer) {
  EXPECT_FALSE(isNewer("", "0.1.0"));
  EXPECT_FALSE(isNewer("0.2", ""));
  EXPECT_FALSE(isNewer(nullptr, "0.1.0"));
  EXPECT_FALSE(isNewer("0.2", nullptr));
}

}  // namespace
