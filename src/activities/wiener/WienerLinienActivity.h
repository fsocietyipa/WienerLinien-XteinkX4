#pragma once

#include <array>
#include <cstdint>

#include "WienerLinienStore.h"
#include "activities/Activity.h"
#include "wiener/WienerBoardLayout.h"
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
  enum class State : uint8_t { CONFIG_REQUIRED, CHECK_WIFI, WIFI_SELECTION, LOADING, READY, ERROR };

  // One cached schedule per configured stop (1420 bytes each). Every stop is
  // fetched on each refresh so Previous/Next only re-lays-out what is already
  // in memory — switching a stop must never wait on the network.
  struct StopSchedule {
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
  // Indexed by stop index, not by column position.
  std::array<StopSchedule, WIENER_MAX_STOPS> schedules{};
  const char* errorMessage = nullptr;
  unsigned long nextRefreshAt = 0;
  int repaintsUntilCleanRefresh = 0;

  void checkAndConnectWifi();
  void launchWifiSelection();
  void launchSettings();
  void fetchSchedules();
  void switchStop(int direction);
  void drawBoard();
  void drawColumn(const wiener_board::ColumnLayout& layout, int x, int y, int width, int height, uint8_t rowsPerStop,
                  bool ink);
  // One stop inside a column: its title, optionally the shared Line/Min labels,
  // and its departure rows.
  void drawSection(const StopSchedule& schedule, int x, int y, int width, int height, uint8_t rowCount, bool ink,
                   bool drawLabels);
  void drawMessage(const char* message, int x, int y, int width, int height, bool ink);
  void drawToolbar(bool ink);
  bool reconnectWifi();
  bool hasScheduleData() const;
  void setBoardOrientation();
};
