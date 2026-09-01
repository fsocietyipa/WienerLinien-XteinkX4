#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>
#include <string>
#include <vector>

struct WienerLinienStop {
  std::string name;
  std::string rbl;
  std::string lineFilter;
};

// Hard cap on configured stops. The board caches one schedule per stop in a
// fixed array sized from this, so the two must not drift apart.
constexpr size_t WIENER_MAX_STOPS = 8;

struct WienerLinienConfig {
  std::vector<WienerLinienStop> stops;
  uint8_t activeStopIndex = 0;
  uint8_t maxDepartures = 6;
  uint8_t columnCount = 1;
  // Departure rows a column is packed to before it stops pulling in stops.
  uint8_t boardRows = 3;
  bool darkTheme = true;
  uint16_t refreshSeconds = 60;
};

class WienerLinienStore : public PersistableStore<WienerLinienStore> {
 private:
  WienerLinienConfig config;

  WienerLinienStore() = default;

  friend class PersistableStore<WienerLinienStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/wiener_linien.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  const WienerLinienConfig& getConfig() const { return config; }
  const WienerLinienStop* getStop(size_t index) const;
  const WienerLinienStop* getActiveStop() const;
  bool addStop(const WienerLinienStop& stop);
  bool updateStop(size_t index, const WienerLinienStop& stop);
  bool removeStop(size_t index);
  bool setActiveStop(size_t index);
  bool setMaxDepartures(uint8_t value);
  bool setColumnCount(uint8_t value);
  bool setBoardRows(uint8_t value);
  bool setDarkTheme(bool value);
  bool setRefreshSeconds(uint16_t value);
};

#define WIENER_LINIEN_STORE WienerLinienStore::getInstance()
