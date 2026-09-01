#pragma once

#include "activities/UiListActivity.h"

class WienerStopsActivity final : public UiListActivity {
 public:
  explicit WienerStopsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

 private:
  static constexpr int MAX_ROWS = 9;
  freeink::ui::ListItem rowItems[MAX_ROWS]{};
  char stopLabels[8][96]{};

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
};
