#include "WienerLinienActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Icon.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "MappedInputManager.h"
#include "WienerLinienStore.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/wiener/WienerLinienSettingsActivity.h"
#include "components/icons/readerToolbarIcons.h"
#include "components/icons/settings2.h"
#include "network/HttpDownloader.h"
#include "wiener/WienerDisplayFont.h"

namespace {
constexpr const char* API_URL =
    "https://www.wienerlinien.at/ogd_realtime/monitor?activateTrafficInfo=stoerunglang&rbl=";
constexpr int HEADER_HEIGHT = 36;
constexpr int LABEL_HEIGHT = 20;
constexpr int TOOLBAR_HEIGHT = 54;
constexpr unsigned long WIFI_RETRY_MS = 15000;

void trimToWidth(char* text, const int width, const int scale = 1) {
  if (!text || width <= 0) return;
  const size_t length = strlen(text);
  const size_t maxChars = static_cast<size_t>((width / std::max(scale, 1) + 1) / 6);
  if (length <= maxChars) return;
  if (maxChars == 0) {
    text[0] = '\0';
    return;
  }
  text[maxChars] = '\0';
  if (maxChars >= 2) {
    text[maxChars - 2] = '.';
    text[maxChars - 1] = '.';
  }
}

void drawArrivalIndicator(const GfxRenderer& renderer, const int x, const int y, const int width, const int height,
                          const bool ink) {
  const int square = std::max(4, std::min(width, height) / 5);
  const int gap = std::max(2, square / 2);
  const int totalWidth = square * 2 + gap;
  const int left = x + (width - totalWidth) / 2;
  const int top = y + (height - (square * 2 + gap)) / 2;
  renderer.fillRect(left, top, square, square, ink);
  renderer.fillRect(left + square + gap, top + square + gap, square, square, ink);
}

void drawIcon(const GfxRenderer& renderer, const freeink::Icon& icon, const int x, const int y, const bool ink) {
  const int rowBytes = (icon.w + 7) / 8;
  for (int row = 0; row < icon.h; ++row) {
    for (int column = 0; column < icon.w; ++column) {
      const uint8_t value = icon.bits[row * rowBytes + column / 8];
      if ((value & (0x80U >> (column % 8))) == 0) renderer.drawPixel(x + column, y + row, ink);
    }
  }
}

void drawRefreshIcon(const GfxRenderer& renderer, const int x, const int y, const bool ink) {
  constexpr int width = 2;
  renderer.drawLine(x + 6, y + 15, x + 8, y + 9, width, ink);
  renderer.drawLine(x + 8, y + 9, x + 13, y + 5, width, ink);
  renderer.drawLine(x + 13, y + 5, x + 20, y + 5, width, ink);
  renderer.drawLine(x + 20, y + 5, x + 25, y + 9, width, ink);
  renderer.drawLine(x + 25, y + 9, x + 27, y + 12, width, ink);
  renderer.drawLine(x + 21, y + 12, x + 27, y + 12, width, ink);
  renderer.drawLine(x + 27, y + 12, x + 27, y + 6, width, ink);

  renderer.drawLine(x + 26, y + 17, x + 24, y + 23, width, ink);
  renderer.drawLine(x + 24, y + 23, x + 19, y + 27, width, ink);
  renderer.drawLine(x + 19, y + 27, x + 12, y + 27, width, ink);
  renderer.drawLine(x + 12, y + 27, x + 7, y + 23, width, ink);
  renderer.drawLine(x + 7, y + 23, x + 5, y + 20, width, ink);
  renderer.drawLine(x + 5, y + 20, x + 5, y + 26, width, ink);
  renderer.drawLine(x + 5, y + 20, x + 11, y + 20, width, ink);
}
}  // namespace

WienerLinienActivity::WienerLinienActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("WienerLinien", renderer, mappedInput) {}

void WienerLinienActivity::setBoardOrientation() {
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
}

void WienerLinienActivity::onEnter() {
  Activity::onEnter();
  setBoardOrientation();
  errorMessage = nullptr;
  visibleColumnCount = 0;

  const auto* stop = WIENER_LINIEN_STORE.getActiveStop();
  if (!stop || stop->rbl.empty()) {
    state = State::CONFIG_REQUIRED;
    requestUpdate();
    launchSettings();
    return;
  }
  checkAndConnectWifi();
}

void WienerLinienActivity::onExit() {
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  Activity::onExit();
}

void WienerLinienActivity::loop() {
  if (state == State::WIFI_SELECTION) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    launchSettings();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    switchStop(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    switchStop(1);
    return;
  }

  if (state == State::ERROR || state == State::CONFIG_REQUIRED) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      const auto* stop = WIENER_LINIEN_STORE.getActiveStop();
      if (!stop || stop->rbl.empty())
        launchSettings();
      else
        checkAndConnectWifi();
    }
    return;
  }
  if (state == State::CHECK_WIFI || state == State::LOADING) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    fetchSchedules();
    return;
  }
  if (state == State::READY && static_cast<long>(millis() - nextRefreshAt) >= 0) fetchSchedules();
}

void WienerLinienActivity::render(RenderLock&&) {
  drawBoard();
  renderer.displayBuffer();
}

void WienerLinienActivity::drawBoard() {
  const bool dark = WIENER_LINIEN_STORE.getConfig().darkTheme;
  const bool ink = !dark;
  renderer.clearScreen(dark ? 0x00 : 0xFF);
  drawToolbar(ink);
  const int boardWidth = renderer.getScreenWidth();
  const int boardHeight = renderer.getScreenHeight() - TOOLBAR_HEIGHT;

  const char* message = nullptr;
  switch (state) {
    case State::CONFIG_REQUIRED:
      message = tr(STR_WL_CONFIGURE_FIRST);
      break;
    case State::CHECK_WIFI:
    case State::WIFI_SELECTION:
      message = tr(STR_CHECKING_WIFI);
      break;
    case State::LOADING:
      message = tr(STR_LOADING);
      break;
    case State::ERROR:
      message = errorMessage ? errorMessage : tr(STR_ERROR_GENERAL_FAILURE);
      break;
    case State::READY:
      break;
  }
  if (message) {
    drawMessage(message, 8, 8, boardWidth - 16, boardHeight - 16, ink);
    return;
  }
  if (visibleColumnCount == 0) {
    drawMessage(tr(STR_WL_NO_DEPARTURES), 8, 8, boardWidth - 16, boardHeight - 16, ink);
    return;
  }

  const uint8_t rowCount = WIENER_LINIEN_STORE.getConfig().maxDepartures;
  for (size_t index = 0; index < visibleColumnCount; ++index) {
    const int left = static_cast<int>(boardWidth * index / visibleColumnCount);
    const int right = static_cast<int>(boardWidth * (index + 1) / visibleColumnCount);
    if (index > 0) renderer.drawLine(left, 0, left, boardHeight - 1, 2, ink);
    drawColumn(columns[index], left + (index > 0 ? 2 : 0), 0, right - left - (index > 0 ? 2 : 0), boardHeight, rowCount,
               ink);
  }
}

void WienerLinienActivity::drawMessage(const char* message, const int x, const int y, const int width, const int height,
                                       const bool ink) {
  char normalized[160]{};
  WienerDisplayFont::normalize(message, normalized, sizeof(normalized));
  trimToWidth(normalized, width);
  const int scale = WienerDisplayFont::fitScale(normalized, width, height, 5);
  WienerDisplayFont::drawCentered(renderer, x, y, width, height, normalized, scale, ink);
}

void WienerLinienActivity::drawColumn(const StopColumn& column, const int x, const int y, const int width,
                                      const int height, const uint8_t rowCount, const bool ink) {
  char title[112]{};
  WienerDisplayFont::normalize(column.title[0] ? column.title : tr(STR_WL_TITLE), title, sizeof(title));
  trimToWidth(title, width - 12);
  const int titleScale = WienerDisplayFont::fitScale(title, width - 12, HEADER_HEIGHT - 8, 3);
  WienerDisplayFont::drawCentered(renderer, x + 6, y + 3, width - 12, HEADER_HEIGHT - 6, title, titleScale, ink);
  renderer.drawLine(x, y + HEADER_HEIGHT, x + width - 1, y + HEADER_HEIGHT, ink);

  char lineHeader[32]{};
  char minutesHeader[64]{};
  WienerDisplayFont::normalize(tr(STR_WL_LINE_HEADER), lineHeader, sizeof(lineHeader));
  WienerDisplayFont::normalize(tr(STR_WL_MINUTES_HEADER), minutesHeader, sizeof(minutesHeader));
  trimToWidth(minutesHeader, width - WienerDisplayFont::textWidth(lineHeader, 1) - 18);
  const int labelY = y + HEADER_HEIGHT + (LABEL_HEIGHT - 7) / 2;
  WienerDisplayFont::drawText(renderer, x + 5, labelY, lineHeader, 1, ink);
  WienerDisplayFont::drawText(renderer, x + width - WienerDisplayFont::textWidth(minutesHeader, 1) - 5, labelY,
                              minutesHeader, 1, ink);

  const int rowsTop = y + HEADER_HEIGHT + LABEL_HEIGHT;
  renderer.drawLine(x, rowsTop, x + width - 1, rowsTop, ink);
  const int bodyHeight = std::max(1, height - HEADER_HEIGHT - LABEL_HEIGHT);
  const int lineWidth = std::clamp(width / 5, 46, 105);
  const int minuteWidth = std::clamp(width / 6, 42, 96);
  const int lineRight = x + lineWidth;
  const int minuteLeft = x + width - minuteWidth;
  renderer.drawLine(lineRight, rowsTop, lineRight, y + height - 1, ink);
  renderer.drawLine(minuteLeft, rowsTop, minuteLeft, y + height - 1, ink);

  if (column.message) {
    drawMessage(column.message, lineRight + 5, rowsTop + 5, minuteLeft - lineRight - 10, bodyHeight - 10, ink);
    return;
  }
  if (column.departureCount == 0) {
    drawMessage(tr(STR_WL_NO_DEPARTURES), lineRight + 5, rowsTop + 5, minuteLeft - lineRight - 10, bodyHeight - 10,
                ink);
    return;
  }

  const size_t visibleRows = std::min(column.departureCount, static_cast<size_t>(rowCount));
  for (size_t index = 0; index < visibleRows; ++index) {
    const int rowTop = rowsTop + static_cast<int>(bodyHeight * index / rowCount);
    const int rowBottom = rowsTop + static_cast<int>(bodyHeight * (index + 1) / rowCount);
    const int rowHeight = std::max(1, rowBottom - rowTop);
    if (index > 0) renderer.drawLine(x, rowTop, x + width - 1, rowTop, ink);

    const auto& departure = column.departures[index];
    char line[20]{};
    WienerDisplayFont::normalize(departure.line, line, sizeof(line));
    trimToWidth(line, lineWidth - 8);
    const int lineScale = WienerDisplayFont::fitScale(line, lineWidth - 8, rowHeight - 6, 8);
    WienerDisplayFont::drawCentered(renderer, x + 4, rowTop + 3, lineWidth - 8, rowHeight - 6, line, lineScale, ink);

    const int destinationWidth = minuteLeft - lineRight - 10;
    char destination[112]{};
    char firstLine[112]{};
    char secondLine[112]{};
    WienerDisplayFont::normalize(departure.destination, destination, sizeof(destination));
    const int singleScale = WienerDisplayFont::fitScale(destination, destinationWidth, rowHeight - 6, 5);
    WienerDisplayFont::splitBalanced(destination, firstLine, sizeof(firstLine), secondLine, sizeof(secondLine));
    int splitScale = 0;
    if (secondLine[0] != '\0' && rowHeight >= 17) {
      splitScale = std::min(WienerDisplayFont::fitScale(firstLine, destinationWidth, (rowHeight - 3) / 2, 5),
                            WienerDisplayFont::fitScale(secondLine, destinationWidth, (rowHeight - 3) / 2, 5));
    }
    if (splitScale >= singleScale && splitScale > 0) {
      trimToWidth(firstLine, destinationWidth, splitScale);
      trimToWidth(secondLine, destinationWidth, splitScale);
      const int blockHeight = splitScale * 15;
      const int textTop = rowTop + std::max(0, (rowHeight - blockHeight) / 2);
      WienerDisplayFont::drawCentered(renderer, lineRight + 5, textTop, destinationWidth, 7 * splitScale, firstLine,
                                      splitScale, ink);
      WienerDisplayFont::drawCentered(renderer, lineRight + 5, textTop + 8 * splitScale, destinationWidth,
                                      7 * splitScale, secondLine, splitScale, ink);
    } else {
      trimToWidth(destination, destinationWidth, singleScale);
      WienerDisplayFont::drawCentered(renderer, lineRight + 5, rowTop + 3, destinationWidth, rowHeight - 6, destination,
                                      singleScale, ink);
    }

    if (departure.countdown >= 0 && departure.countdown <= 1) {
      drawArrivalIndicator(renderer, minuteLeft + 3, rowTop + 3, minuteWidth - 6, rowHeight - 6, ink);
    } else {
      char countdown[8]{};
      if (departure.countdown < 0)
        snprintf(countdown, sizeof(countdown), "--");
      else
        snprintf(countdown, sizeof(countdown), "%d", departure.countdown);
      const int countdownScale = WienerDisplayFont::fitScale(countdown, minuteWidth - 8, rowHeight - 6, 8);
      WienerDisplayFont::drawCentered(renderer, minuteLeft + 4, rowTop + 3, minuteWidth - 8, rowHeight - 6, countdown,
                                      countdownScale, ink);
    }
  }
}

void WienerLinienActivity::drawToolbar(const bool ink) {
  const int y = renderer.getScreenHeight() - TOOLBAR_HEIGHT;
  const int slotWidth = renderer.getScreenWidth() / 4;
  renderer.drawLine(0, y, renderer.getScreenWidth() - 1, y, 2, ink);
  const freeink::Icon settingsIcon{32, 32, 16, Settings2Icon};

  for (int index = 0; index < 4; ++index) {
    const int centerX = index * slotWidth + slotWidth / 2;
    renderer.drawRect(centerX - 30, y + 6, 60, 42, 2, ink);
    if (index > 0) renderer.drawLine(index * slotWidth, y + 7, index * slotWidth, y + TOOLBAR_HEIGHT - 7, ink);
    if (index == 0)
      drawIcon(renderer, settingsIcon, centerX - 16, y + 11, ink);
    else if (index == 1)
      drawRefreshIcon(renderer, centerX - 16, y + 11, ink);
    else if (index == 2)
      drawIcon(renderer, icon_reader_back_24, centerX - 12, y + 15, ink);
    else
      drawIcon(renderer, icon_reader_next_24, centerX - 12, y + 15, ink);
  }
}

void WienerLinienActivity::checkAndConnectWifi() {
  state = State::CHECK_WIFI;
  requestUpdate();
  if (reconnectWifi()) {
    fetchSchedules();
    return;
  }
  launchWifiSelection();
}

bool WienerLinienActivity::reconnectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) return true;

  LOG_INF("WL", "WiFi disconnected, attempting reconnect");
  WiFi.reconnect();
  const unsigned long deadline = millis() + 10000;
  while (static_cast<long>(millis() - deadline) < 0) {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
      WiFi.setSleep(false);
      LOG_INF("WL", "WiFi reconnected");
      return true;
    }
    delay(100);
  }
  LOG_ERR("WL", "WiFi reconnect timed out");
  return false;
}

bool WienerLinienActivity::hasScheduleData() const {
  if (state != State::READY || visibleColumnCount == 0) return false;
  for (size_t index = 0; index < visibleColumnCount; ++index) {
    if (columns[index].message == nullptr && columns[index].title[0] != '\0') return true;
  }
  return false;
}

void WienerLinienActivity::launchWifiSelection() {
  state = State::WIFI_SELECTION;
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  auto activity = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput);
  if (!activity) {
    setBoardOrientation();
    state = State::ERROR;
    errorMessage = tr(STR_MEMORY_ERROR);
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(activity), [this](const ActivityResult& result) {
    setBoardOrientation();
    if (result.isCancelled) {
      state = State::ERROR;
      errorMessage = tr(STR_WIFI_CONN_FAILED);
      requestUpdate();
      return;
    }
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    fetchSchedules();
  });
}

void WienerLinienActivity::launchSettings() {
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  auto activity = makeUniqueNoThrow<WienerLinienSettingsActivity>(renderer, mappedInput);
  if (!activity) {
    setBoardOrientation();
    state = State::ERROR;
    errorMessage = tr(STR_MEMORY_ERROR);
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(activity), [this](const ActivityResult&) {
    setBoardOrientation();
    const auto* stop = WIENER_LINIEN_STORE.getActiveStop();
    if (!stop || stop->rbl.empty()) {
      state = State::CONFIG_REQUIRED;
      requestUpdate();
      return;
    }
    visibleColumnCount = 0;
    for (auto& column : columns) column = {};
    checkAndConnectWifi();
  });
}

void WienerLinienActivity::fetchSchedules() {
  const auto& config = WIENER_LINIEN_STORE.getConfig();
  if (config.stops.empty()) {
    state = State::CONFIG_REQUIRED;
    requestUpdate();
    return;
  }

  const bool hadSchedule = hasScheduleData();
  if (!reconnectWifi()) {
    if (hadSchedule) {
      state = State::READY;
      nextRefreshAt = millis() + WIFI_RETRY_MS;
      requestUpdate();
    } else {
      state = State::ERROR;
      errorMessage = tr(STR_WIFI_CONN_FAILED);
      requestUpdate();
    }
    return;
  }

  errorMessage = nullptr;
  const size_t requestedColumns = std::min({static_cast<size_t>(config.columnCount), config.stops.size(), MAX_COLUMNS});
  if (!hadSchedule || requestedColumns != visibleColumnCount) {
    visibleColumnCount = requestedColumns;
    for (auto& column : columns) column = {};
    state = State::LOADING;
    requestUpdateAndWait();
  }

  size_t successfulColumns = 0;
  for (size_t columnIndex = 0; columnIndex < visibleColumnCount; ++columnIndex) {
    auto& column = columns[columnIndex];
    StopColumn freshColumn;
    const size_t stopIndex = (static_cast<size_t>(config.activeStopIndex) + columnIndex) % config.stops.size();
    const auto* stop = WIENER_LINIEN_STORE.getStop(stopIndex);
    if (!stop || stop->rbl.empty()) {
      if (!hadSchedule) column.message = tr(STR_WL_CONFIGURE_FIRST);
      continue;
    }
    snprintf(freshColumn.title, sizeof(freshColumn.title), "%s",
             stop->name.empty() ? stop->rbl.c_str() : stop->name.c_str());

    auto parser = makeUniqueNoThrow<WienerLinienParser>(stop->lineFilter.c_str(), config.maxDepartures);
    if (!parser) {
      state = State::ERROR;
      errorMessage = tr(STR_MEMORY_ERROR);
      requestUpdate();
      return;
    }

    std::string url = API_URL;
    url += stop->rbl;
    LOG_DBG("WL", "Fetching RBL %s", stop->rbl.c_str());
    const bool fetched = HttpDownloader::fetchUrl(url, [&parser](const uint8_t* data, const size_t len) {
      parser->feed(reinterpret_cast<const char*>(data), len);
      return !parser->hasError();
    });
    if (!fetched) {
      const char* fetchError = parser->hasError() ? tr(STR_WL_PARSE_FAILED) : tr(STR_WL_FETCH_FAILED);
      if (!hadSchedule) column.message = fetchError;
      if (!errorMessage) errorMessage = fetchError;
      continue;
    }

    parser->finalize();
    freshColumn.departureCount = parser->getDepartureCount();
    for (size_t index = 0; index < freshColumn.departureCount; ++index) {
      freshColumn.departures[index] = parser->getDeparture(index);
    }
    const char* apiTitle = parser->getStopTitle();
    if (apiTitle[0] != '\0') snprintf(freshColumn.title, sizeof(freshColumn.title), "%s", apiTitle);
    column = freshColumn;
    ++successfulColumns;
  }

  if (successfulColumns == 0) {
    if (hadSchedule) {
      state = State::READY;
      errorMessage = nullptr;
      nextRefreshAt = millis() + WIFI_RETRY_MS;
      LOG_ERR("WL", "Refresh failed; retaining previous schedule and retrying");
    } else {
      state = State::ERROR;
      if (!errorMessage) errorMessage = tr(STR_WL_FETCH_FAILED);
    }
  } else {
    state = State::READY;
    errorMessage = nullptr;
    nextRefreshAt = millis() + static_cast<unsigned long>(config.refreshSeconds) * 1000UL;
  }
  requestUpdate();
}

void WienerLinienActivity::switchStop(const int direction) {
  const auto& config = WIENER_LINIEN_STORE.getConfig();
  const size_t count = config.stops.size();
  if (count < 2) return;
  int next = static_cast<int>(config.activeStopIndex) + direction;
  if (next < 0) next = static_cast<int>(count) - 1;
  if (next >= static_cast<int>(count)) next = 0;
  if (!WIENER_LINIEN_STORE.setActiveStop(static_cast<size_t>(next))) return;
  visibleColumnCount = 0;
  for (auto& column : columns) column = {};
  checkAndConnectWifi();
}
