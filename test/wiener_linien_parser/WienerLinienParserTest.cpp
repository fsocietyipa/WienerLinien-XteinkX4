#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>

#include "WienerLinienParser.h"

namespace {
constexpr const char* RESPONSE = R"({
  "data": {
    "monitors": [
      {
        "locationStop": {"properties": {"title": "Parlament, U Volkstheater"}},
        "lines": [{
          "name": "1",
          "towards": "Stefan-Fadinger-Platz",
          "departures": {"departure": [
            {"departureTime": {"countdown": 9}, "vehicle": {"name": "1", "towards": "Stefan-Fadinger-Platz"}},
            {"departureTime": {"countdown": 3}, "vehicle": {"name": "1", "towards": "Stefan-Fadinger-Platz"}}
          ]}
        }]
      },
      {
        "locationStop": {"properties": {"title": "Parlament, U Volkstheater"}},
        "lines": [{
          "name": "D",
          "towards": "Nußdorf",
          "departures": {"departure": [
            {"departureTime": {"countdown": 5}, "vehicle": {"name": "D", "towards": "Nußdorf"}}
          ]}
        }]
      }
    ]
  },
  "message": {"value": "OK"}
})";

void feedChunked(WienerLinienParser& parser, const size_t chunkSize) {
  const size_t length = strlen(RESPONSE);
  for (size_t offset = 0; offset < length; offset += chunkSize) {
    const size_t count = std::min(chunkSize, length - offset);
    parser.feed(RESPONSE + offset, count);
  }
  parser.finalize();
}
}  // namespace

TEST(WienerLinienParser, ParsesAndSortsChunkedResponse) {
  WienerLinienParser parser;
  feedChunked(parser, 7);

  ASSERT_FALSE(parser.hasError());
  EXPECT_STREQ(parser.getStopTitle(), "Parlament, U Volkstheater");
  ASSERT_EQ(parser.getDepartureCount(), 3u);
  EXPECT_EQ(parser.getDeparture(0).countdown, 3);
  EXPECT_STREQ(parser.getDeparture(0).line, "1");
  EXPECT_EQ(parser.getDeparture(1).countdown, 5);
  EXPECT_STREQ(parser.getDeparture(1).line, "D");
  EXPECT_EQ(parser.getDeparture(2).countdown, 9);
}

TEST(WienerLinienParser, AppliesCommaSeparatedLineFilter) {
  WienerLinienParser parser("D, 2");
  feedChunked(parser, 31);

  ASSERT_EQ(parser.getDepartureCount(), 1u);
  EXPECT_STREQ(parser.getDeparture(0).line, "D");
  EXPECT_STREQ(parser.getDeparture(0).destination, "Nußdorf");
}

TEST(WienerLinienParser, RetainsEarliestDeparturesAtLimit) {
  WienerLinienParser parser("", 2);
  feedChunked(parser, strlen(RESPONSE));

  ASSERT_EQ(parser.getDepartureCount(), 2u);
  EXPECT_EQ(parser.getDeparture(0).countdown, 3);
  EXPECT_EQ(parser.getDeparture(1).countdown, 5);
}

// The monitor feed marks accessibility on the line and, for a run that
// differs, again on that departure's own vehicle object.
TEST(WienerLinienParser, ReadsBarrierFreeFromTheLine) {
  constexpr const char* json = R"({
    "data": {"monitors": [{
      "locationStop": {"properties": {"title": "Test"}},
      "lines": [{
        "name": "U2", "towards": "Seestadt", "barrierFree": true,
        "departures": {"departure": [
          {"departureTime": {"countdown": 4}},
          {"departureTime": {"countdown": 9}}
        ]}
      }]
    }]}
  })";
  WienerLinienParser parser("", 6);
  parser.feed(json, strlen(json));
  parser.finalize();
  ASSERT_EQ(parser.getDepartureCount(), 2u);
  EXPECT_TRUE(parser.getDeparture(0).barrierFree);
  EXPECT_TRUE(parser.getDeparture(1).barrierFree);
}

TEST(WienerLinienParser, VehicleOverridesTheLineAccessibilityFlag) {
  constexpr const char* json = R"({
    "data": {"monitors": [{
      "locationStop": {"properties": {"title": "Test"}},
      "lines": [{
        "name": "1", "towards": "Prater", "barrierFree": true,
        "departures": {"departure": [
          {"departureTime": {"countdown": 2}, "vehicle": {"name": "1", "towards": "Prater", "barrierFree": false}},
          {"departureTime": {"countdown": 8}}
        ]}
      }]
    }]}
  })";
  WienerLinienParser parser("", 6);
  parser.feed(json, strlen(json));
  parser.finalize();
  ASSERT_EQ(parser.getDepartureCount(), 2u);
  EXPECT_FALSE(parser.getDeparture(0).barrierFree);
  EXPECT_TRUE(parser.getDeparture(1).barrierFree);
}

TEST(WienerLinienParser, AccessibilityDefaultsToFalseWhenAbsent) {
  constexpr const char* json = R"({
    "data": {"monitors": [{
      "locationStop": {"properties": {"title": "Test"}},
      "lines": [{"name": "D", "towards": "Nussdorf",
        "departures": {"departure": [{"departureTime": {"countdown": 3}}]}}]
    }]}
  })";
  WienerLinienParser parser("", 6);
  parser.feed(json, strlen(json));
  parser.finalize();
  ASSERT_EQ(parser.getDepartureCount(), 1u);
  EXPECT_FALSE(parser.getDeparture(0).barrierFree);
}
