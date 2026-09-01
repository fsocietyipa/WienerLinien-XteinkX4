#include "WienerStopsActivity.h"

#include <I18n.h>
#include <Memory.h>

#include <cstdio>

#include "WienerLinienStore.h"
#include "activities/wiener/WienerStopSettingsActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

WienerStopsActivity::WienerStopsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("WienerStops", renderer, mappedInput) {}

int WienerStopsActivity::listCount() const {
  const int stopCount = static_cast<int>(WIENER_LINIEN_STORE.getConfig().stops.size());
  return stopCount < 8 ? stopCount + 1 : stopCount;
}

const char* WienerStopsActivity::headerTitle() const { return tr(STR_WL_STOPS); }

void WienerStopsActivity::activateIndex(const int index) {
  app.clearTapFlash();
  const size_t stopCount = WIENER_LINIEN_STORE.getConfig().stops.size();
  int editIndex = index;
  if (static_cast<size_t>(index) == stopCount && stopCount < 8) {
    if (!WIENER_LINIEN_STORE.addStop(WienerLinienStop{})) return;
    editIndex = static_cast<int>(WIENER_LINIEN_STORE.getConfig().stops.size() - 1);
  }

  auto activity = makeUniqueNoThrow<WienerStopSettingsActivity>(renderer, mappedInput, editIndex);
  if (!activity) return;
  startActivityForResult(std::move(activity), [this](const ActivityResult&) {
    const int count = listCount();
    if (count > 0 && nav.selected >= count) nav.selected = count - 1;
    requestUpdate();
  });
}

void WienerStopsActivity::buildScreen(UiScreen& screen) {
  const auto& config = WIENER_LINIEN_STORE.getConfig();
  const size_t stopCount = config.stops.size();
  for (size_t index = 0; index < stopCount; ++index) {
    const auto& stop = config.stops[index];
    snprintf(stopLabels[index], sizeof(stopLabels[index]), "%s",
             stop.name.empty() ? stop.rbl.c_str() : stop.name.c_str());
    rowItems[index] = {};
    rowItems[index].label = stopLabels[index];
    rowItems[index].value = index == config.activeStopIndex ? tr(STR_WL_ACTIVE) : stop.rbl.c_str();
    rowItems[index].actionValue = static_cast<int16_t>(index);
  }
  if (stopCount < 8) {
    rowItems[stopCount] = {};
    rowItems[stopCount].label = tr(STR_WL_ADD_STOP);
    rowItems[stopCount].value = "+";
    rowItems[stopCount].actionValue = static_cast<int16_t>(stopCount);
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                  static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                  static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height) + metrics.buttonHintsHeight),
                  static_cast<int16_t>(safe.x)});

  fui::ListProps props;
  props.items = rowItems;
  props.count = static_cast<uint16_t>(listCount());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  syncListViewport(screen, props);
  screen.list(props);
}
