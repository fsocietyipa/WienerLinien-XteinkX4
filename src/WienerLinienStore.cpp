#include "WienerLinienStore.h"

#include <algorithm>
#include <utility>

#include "wiener/WienerBoardLayout.h"
#include "wiener/WienerLinienParser.h"

namespace {
constexpr uint8_t MIN_DEPARTURES = 1;
// Clamps mirror what the board can actually render, so the settings screen
// cannot offer a value the layout or the parser would silently ignore.
constexpr uint8_t MAX_DEPARTURES = static_cast<uint8_t>(WienerLinienParser::MAX_DEPARTURES);
constexpr uint8_t MIN_COLUMNS = 1;
constexpr uint8_t MAX_COLUMNS = static_cast<uint8_t>(wiener_board::MAX_COLUMNS);
constexpr uint8_t MIN_BOARD_ROWS = static_cast<uint8_t>(wiener_board::MIN_TARGET_ROWS);
constexpr uint8_t MAX_BOARD_ROWS = static_cast<uint8_t>(wiener_board::MAX_TARGET_ROWS);
constexpr uint8_t DEFAULT_BOARD_ROWS = static_cast<uint8_t>(wiener_board::DEFAULT_TARGET_ROWS);
constexpr uint16_t MIN_REFRESH_SECONDS = 30;
constexpr uint16_t MAX_REFRESH_SECONDS = 300;
}  // namespace

void WienerLinienStore::toJson(JsonDocument& doc) const {
  JsonArray stops = doc["stops"].to<JsonArray>();
  for (const auto& stop : config.stops) {
    JsonObject value = stops.add<JsonObject>();
    value["name"] = stop.name;
    value["rbl"] = stop.rbl;
    value["line_filter"] = stop.lineFilter;
  }
  doc["active_stop"] = config.activeStopIndex;
  doc["max_departures"] = config.maxDepartures;
  doc["column_count"] = config.columnCount;
  doc["board_rows"] = config.boardRows;
  doc["stop_symbols"] = config.stopSymbols;
  doc["destination_symbols"] = config.destinationSymbols;
  doc["wheelchair"] = config.wheelchair;
  doc["dark_theme"] = config.darkTheme;
  doc["refresh_seconds"] = config.refreshSeconds;
}

bool WienerLinienStore::fromJson(JsonVariantConst doc) {
  config.stops.clear();
  JsonArrayConst stops = doc["stops"].as<JsonArrayConst>();
  config.stops.reserve(std::min(stops.size(), WIENER_MAX_STOPS));
  for (JsonObjectConst value : stops) {
    if (config.stops.size() >= WIENER_MAX_STOPS) break;
    WienerLinienStop stop;
    stop.name = value["name"] | "";
    stop.rbl = value["rbl"] | "";
    stop.lineFilter = value["line_filter"] | "";
    config.stops.push_back(std::move(stop));
  }

  // Migrate the first single-stop development format.
  const char* legacyRbl = doc["rbl"] | "";
  if (config.stops.empty() && legacyRbl[0] != '\0') {
    config.stops.push_back(WienerLinienStop{"", legacyRbl, doc["line_filter"] | ""});
    requestResave();
  }
  config.activeStopIndex = static_cast<uint8_t>(doc["active_stop"] | 0);
  if (config.stops.empty() || config.activeStopIndex >= config.stops.size()) config.activeStopIndex = 0;
  config.maxDepartures = std::clamp(static_cast<uint8_t>(doc["max_departures"] | 6), MIN_DEPARTURES, MAX_DEPARTURES);
  config.columnCount = std::clamp(static_cast<uint8_t>(doc["column_count"] | 1), MIN_COLUMNS, MAX_COLUMNS);
  config.boardRows =
      std::clamp(static_cast<uint8_t>(doc["board_rows"] | DEFAULT_BOARD_ROWS), MIN_BOARD_ROWS, MAX_BOARD_ROWS);
  config.stopSymbols = doc["stop_symbols"] | true;
  config.destinationSymbols = doc["destination_symbols"] | true;
  config.wheelchair = doc["wheelchair"] | true;
  config.darkTheme = doc["dark_theme"] | true;
  config.refreshSeconds =
      std::clamp(static_cast<uint16_t>(doc["refresh_seconds"] | 60), MIN_REFRESH_SECONDS, MAX_REFRESH_SECONDS);
  return true;
}

const WienerLinienStop* WienerLinienStore::getStop(const size_t index) const {
  return index < config.stops.size() ? &config.stops[index] : nullptr;
}

const WienerLinienStop* WienerLinienStore::getActiveStop() const { return getStop(config.activeStopIndex); }

bool WienerLinienStore::addStop(const WienerLinienStop& stop) {
  if (config.stops.size() >= WIENER_MAX_STOPS) return false;
  config.stops.push_back(stop);
  config.activeStopIndex = static_cast<uint8_t>(config.stops.size() - 1);
  return saveToFile();
}

bool WienerLinienStore::updateStop(const size_t index, const WienerLinienStop& stop) {
  if (index >= config.stops.size()) return false;
  config.stops[index] = stop;
  return saveToFile();
}

bool WienerLinienStore::removeStop(const size_t index) {
  if (index >= config.stops.size()) return false;
  const uint8_t previousActive = config.activeStopIndex;
  config.stops.erase(config.stops.begin() + static_cast<ptrdiff_t>(index));
  if (config.stops.empty()) {
    config.activeStopIndex = 0;
  } else if (index < previousActive) {
    config.activeStopIndex = static_cast<uint8_t>(previousActive - 1);
  } else if (index == previousActive && config.activeStopIndex >= config.stops.size()) {
    config.activeStopIndex = static_cast<uint8_t>(config.stops.size() - 1);
  }
  return saveToFile();
}

bool WienerLinienStore::setActiveStop(const size_t index) {
  if (index >= config.stops.size()) return false;
  config.activeStopIndex = static_cast<uint8_t>(index);
  return saveToFile();
}

bool WienerLinienStore::setMaxDepartures(const uint8_t value) {
  config.maxDepartures = std::clamp(value, MIN_DEPARTURES, MAX_DEPARTURES);
  return saveToFile();
}

bool WienerLinienStore::setColumnCount(const uint8_t value) {
  config.columnCount = std::clamp(value, MIN_COLUMNS, MAX_COLUMNS);
  return saveToFile();
}

bool WienerLinienStore::setBoardRows(const uint8_t value) {
  config.boardRows = std::clamp(value, MIN_BOARD_ROWS, MAX_BOARD_ROWS);
  return saveToFile();
}

bool WienerLinienStore::setDarkTheme(const bool value) {
  config.darkTheme = value;
  return saveToFile();
}

bool WienerLinienStore::setStopSymbols(const bool value) {
  config.stopSymbols = value;
  return saveToFile();
}

bool WienerLinienStore::setDestinationSymbols(const bool value) {
  config.destinationSymbols = value;
  return saveToFile();
}

bool WienerLinienStore::setWheelchair(const bool value) {
  config.wheelchair = value;
  return saveToFile();
}

bool WienerLinienStore::setRefreshSeconds(const uint16_t value) {
  config.refreshSeconds = std::clamp(value, MIN_REFRESH_SECONDS, MAX_REFRESH_SECONDS);
  return saveToFile();
}
