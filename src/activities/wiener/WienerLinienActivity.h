#pragma once

#include <array>
#include <cstdint>

#include "activities/Activity.h"
#include "wiener/WienerLinienParser.h"

class WienerLinienActivity final : public Activity {
 public:
  explicit WienerLinienActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  static constexpr size_t MAX_COLUMNS = 3;

  enum class State : uint8_t { CONFIG_REQUIRED, CHECK_WIFI, WIFI_SELECTION, LOADING, READY, ERROR };

  struct StopColumn {
    std::array<WienerDeparture, WienerLinienParser::MAX_DEPARTURES> departures{};
    size_t departureCount = 0;
    char title[112]{};
    const char* message = nullptr;
  };

  // Repaints between ghost-clearing waveforms. The board repaints on a timer
  // for as long as the device is on, so fast refreshes would otherwise stack
  // residue indefinitely; the reader solves the same problem with a countdown
  // (ReaderUtils::displayWithRefreshCycle). Zero means the next paint is clean.
  static constexpr int REPAINTS_PER_CLEAN_REFRESH = 10;

  State state = State::CONFIG_REQUIRED;
  std::array<StopColumn, MAX_COLUMNS> columns{};
  size_t visibleColumnCount = 0;
  const char* errorMessage = nullptr;
  unsigned long nextRefreshAt = 0;
  int repaintsUntilCleanRefresh = 0;

  void checkAndConnectWifi();
  void launchWifiSelection();
  void launchSettings();
  void fetchSchedules();
  void switchStop(int direction);
  void drawBoard();
  void drawColumn(const StopColumn& column, int x, int y, int width, int height, uint8_t rowCount, bool ink);
  void drawMessage(const char* message, int x, int y, int width, int height, bool ink);
  void drawToolbar(bool ink);
  bool reconnectWifi();
  bool hasScheduleData() const;
  void setBoardOrientation();
};
