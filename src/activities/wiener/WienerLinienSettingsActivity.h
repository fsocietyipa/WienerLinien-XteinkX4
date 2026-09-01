#pragma once

#include "activities/UiListActivity.h"

class WienerLinienSettingsActivity final : public UiListActivity {
 public:
  explicit WienerLinienSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

 private:
  static constexpr int MENU_ITEMS = 9;
  freeink::ui::ListItem rowItems[MENU_ITEMS]{};
  char activeStopValue[96]{};
  char wifiValue[64]{};
  char departureCountValue[8]{};
  char columnCountValue[8]{};
  char boardRowsValue[8]{};
  const char* themeValue = nullptr;
  char refreshValue[24]{};
  bool showError = false;

  int listCount() const override { return MENU_ITEMS; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  // Overridden to put the firmware version in the header's trailing slot, the
  // same place the reader's settings screen shows it.
  void drawChrome() override;
  void drawFooter() override;
  void cycleDepartureCount();
  void cycleColumnCount();
  void cycleBoardRows();
  void cycleTheme();
  void cycleRefreshInterval();
};
