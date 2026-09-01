#pragma once

#include <StreamingJsonParser.h>

#include <array>
#include <cstddef>
#include <cstdint>

struct WienerDeparture {
  char line[16]{};
  char destination[112]{};
  int16_t countdown = -1;
  // Low-floor vehicle. The monitor feed reports it per line and, when a run
  // differs, again on that departure's vehicle object.
  bool barrierFree = false;
};

class WienerLinienParser {
 public:
  static constexpr size_t MAX_DEPARTURES = 10;

  explicit WienerLinienParser(const char* lineFilter = "", size_t limit = 6);

  WienerLinienParser(const WienerLinienParser&) = delete;
  WienerLinienParser& operator=(const WienerLinienParser&) = delete;

  void feed(const char* data, size_t len);
  void finalize();

  bool hasError() const { return parser.hasError(); }
  size_t getDepartureCount() const { return departureCount; }
  const WienerDeparture& getDeparture(size_t index) const { return departures[index]; }
  const char* getStopTitle() const { return stopTitle; }

 private:
  enum class Context : uint8_t {
    ROOT,
    DATA,
    MONITORS_ARRAY,
    MONITOR,
    LOCATION_STOP,
    PROPERTIES,
    LINES_ARRAY,
    LINE,
    DEPARTURES,
    DEPARTURE_ARRAY,
    DEPARTURE,
    DEPARTURE_TIME,
    VEHICLE,
    OTHER,
  };

  enum class Key : uint8_t {
    NONE,
    DATA,
    MONITORS,
    LOCATION_STOP,
    PROPERTIES,
    TITLE,
    LINES,
    NAME,
    TOWARDS,
    DEPARTURES,
    DEPARTURE,
    DEPARTURE_TIME,
    COUNTDOWN,
    VEHICLE,
    BARRIER_FREE,
  };

  struct Frame {
    Context context = Context::OTHER;
    bool isArray = false;
  };

  static void sOnKey(void* ctx, const char* key, size_t len);
  static void sOnString(void* ctx, const char* value, size_t len);
  static void sOnNumber(void* ctx, const char* value, size_t len);
  static void sOnBool(void* ctx, bool value);
  static void sOnNull(void* ctx);
  static void sOnObjectStart(void* ctx);
  static void sOnObjectEnd(void* ctx);
  static void sOnArrayStart(void* ctx);
  static void sOnArrayEnd(void* ctx);

  Context currentContext() const;
  Context childContext(bool isArray) const;
  void pushContext(Context context, bool isArray);
  void popContext();
  void commitDeparture();
  bool matchesFilter(const char* line) const;

  StreamingJsonParser parser;
  std::array<Frame, StreamingJsonParser::MAX_NESTING> stack{};
  uint8_t depth = 0;
  Key lastKey = Key::NONE;

  char stopTitle[112]{};
  char lineName[16]{};
  char lineDestination[112]{};
  bool lineBarrierFree = false;
  WienerDeparture currentDeparture{};
  std::array<WienerDeparture, MAX_DEPARTURES> departures{};
  size_t departureCount = 0;
  size_t limit = 6;
  const char* lineFilter = nullptr;
};
