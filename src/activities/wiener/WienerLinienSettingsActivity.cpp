#include "WienerLinienSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>
#include <WiFi.h>

#include <cstdio>

#include "WienerLinienStore.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/settings/SdFirmwareUpdateActivity.h"
#include "activities/wiener/WienerStopsActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

WienerLinienSettingsActivity::WienerLinienSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("WienerLinienSettings", renderer, mappedInput) {
  static constexpr StrId labels[MENU_ITEMS] = {
      StrId::STR_WL_STOPS,           StrId::STR_WIFI_NETWORKS,  StrId::STR_WL_DEPARTURE_COUNT,
      StrId::STR_WL_STOP_COLUMNS,    StrId::STR_WL_BOARD_THEME, StrId::STR_WL_REFRESH_INTERVAL,
      StrId::STR_SD_FIRMWARE_UPDATE,
  };
  for (int index = 0; index < MENU_ITEMS; ++index) {
    rowItems[index].label = I18N.get(labels[index]);
    rowItems[index].actionValue = static_cast<int16_t>(index);
  }
}

const char* WienerLinienSettingsActivity::headerTitle() const { return tr(STR_WL_SETTINGS); }

void WienerLinienSettingsActivity::activateIndex(const int index) {
  app.clearTapFlash();
  showError = false;
  if (index == 0) {
    auto activity = makeUniqueNoThrow<WienerStopsActivity>(renderer, mappedInput);
    if (!activity) {
      showError = true;
      requestUpdate();
      return;
    }
    startActivityForResult(std::move(activity), [this](const ActivityResult&) { requestUpdate(); });
  } else if (index == 1) {
    auto activity = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput, false);
    if (!activity) {
      showError = true;
      requestUpdate();
      return;
    }
    startActivityForResult(std::move(activity), [this](const ActivityResult&) { requestUpdate(); });
  } else if (index == 2) {
    cycleDepartureCount();
  } else if (index == 3) {
    cycleColumnCount();
  } else if (index == 4) {
    cycleTheme();
  } else if (index == 5) {
    cycleRefreshInterval();
  } else if (index == 6) {
    auto activity = makeUniqueNoThrow<SdFirmwareUpdateActivity>(renderer, mappedInput);
    if (!activity) {
      showError = true;
      requestUpdate();
      return;
    }
    startActivityForResult(std::move(activity), [this](const ActivityResult&) { requestUpdate(); });
  }
}

void WienerLinienSettingsActivity::cycleDepartureCount() {
  const uint8_t current = WIENER_LINIEN_STORE.getConfig().maxDepartures;
  const uint8_t next = current >= 10 ? 1 : static_cast<uint8_t>(current + 1);
  showError = !WIENER_LINIEN_STORE.setMaxDepartures(next);
  requestUpdate();
}

void WienerLinienSettingsActivity::cycleColumnCount() {
  const uint8_t current = WIENER_LINIEN_STORE.getConfig().columnCount;
  const uint8_t next = current >= 3 ? 1 : static_cast<uint8_t>(current + 1);
  showError = !WIENER_LINIEN_STORE.setColumnCount(next);
  requestUpdate();
}

void WienerLinienSettingsActivity::cycleTheme() {
  showError = !WIENER_LINIEN_STORE.setDarkTheme(!WIENER_LINIEN_STORE.getConfig().darkTheme);
  requestUpdate();
}

void WienerLinienSettingsActivity::cycleRefreshInterval() {
  const uint16_t current = WIENER_LINIEN_STORE.getConfig().refreshSeconds;
  uint16_t next = 30;
  if (current < 60)
    next = 60;
  else if (current < 120)
    next = 120;
  else if (current < 300)
    next = 300;
  showError = !WIENER_LINIEN_STORE.setRefreshSeconds(next);
  requestUpdate();
}

void WienerLinienSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& config = WIENER_LINIEN_STORE.getConfig();
  const auto* stop = WIENER_LINIEN_STORE.getActiveStop();
  if (stop && (!stop->name.empty() || !stop->rbl.empty())) {
    snprintf(activeStopValue, sizeof(activeStopValue), "%s",
             stop->name.empty() ? stop->rbl.c_str() : stop->name.c_str());
  } else {
    snprintf(activeStopValue, sizeof(activeStopValue), "%s", tr(STR_NOT_SET));
  }

  if (WiFi.status() == WL_CONNECTED)
    snprintf(wifiValue, sizeof(wifiValue), "%s", WiFi.SSID().c_str());
  else
    snprintf(wifiValue, sizeof(wifiValue), "%s", tr(STR_WL_NOT_CONNECTED));
  snprintf(departureCountValue, sizeof(departureCountValue), "%u", config.maxDepartures);
  snprintf(columnCountValue, sizeof(columnCountValue), "%u", config.columnCount);
  themeValue = config.darkTheme ? tr(STR_DARK) : tr(STR_LIGHT);
  snprintf(refreshValue, sizeof(refreshValue), tr(STR_WL_SECONDS_FORMAT), config.refreshSeconds);

  rowItems[0].value = activeStopValue;
  rowItems[1].value = wifiValue;
  rowItems[2].value = departureCountValue;
  rowItems[3].value = columnCountValue;
  rowItems[4].value = themeValue;
  rowItems[5].value = refreshValue;
  rowItems[6].value = ">";

  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                  static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                  static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height) + metrics.buttonHintsHeight),
                  static_cast<int16_t>(safe.x)});

  fui::ListProps props;
  props.items = rowItems;
  props.count = MENU_ITEMS;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}

void WienerLinienSettingsActivity::drawFooter() {
  UiListActivity::drawFooter();
  if (showError) GUI.drawPopup(renderer, tr(STR_ERROR_GENERAL_FAILURE));
}
