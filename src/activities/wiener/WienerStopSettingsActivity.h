#pragma once

#include "WienerLinienStore.h"
#include "activities/UiListActivity.h"

class WienerStopSettingsActivity final : public UiListActivity {
 public:
  explicit WienerStopSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int stopIndex);

 private:
  static constexpr int MENU_ITEMS = 5;
  int stopIndex;
  WienerLinienStop editStop;
  freeink::ui::ListItem rowItems[MENU_ITEMS]{};
  bool showSaveError = false;
  bool showInvalidRbl = false;

  int listCount() const override { return MENU_ITEMS; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawFooter() override;
  bool save();
  void editText(int field);
};
