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

#include "CrossPointSettings.h"
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
// The four hint buttons sit along the right edge: the board runs in landscape,
// which puts the device's physical front buttons on that side rather than under
// a bottom strip. Half the previous 54x42 bottom-strip sizing.
constexpr int TOOLBAR_WIDTH = 27;
constexpr int BUTTON_WIDTH = 21;
constexpr int BUTTON_HEIGHT = 30;
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

// Half-size icon: each destination pixel covers a 2x2 source block and is drawn
// when any of the four is set. Averaging would drop the single-pixel strokes in
// these 1-bpp assets; OR keeps them at the cost of looking slightly bolder.
void drawIconHalf(const GfxRenderer& renderer, const freeink::Icon& icon, const int x, const int y, const bool ink) {
  const int rowBytes = (icon.w + 7) / 8;
  const int halfWidth = icon.w / 2;
  const int halfHeight = icon.h / 2;
  for (int row = 0; row < halfHeight; ++row) {
    for (int column = 0; column < halfWidth; ++column) {
      bool set = false;
      for (int dy = 0; dy < 2 && !set; ++dy) {
        for (int dx = 0; dx < 2 && !set; ++dx) {
          const int sourceRow = row * 2 + dy;
          const int sourceColumn = column * 2 + dx;
          if (sourceRow >= icon.h || sourceColumn >= icon.w) continue;
          const uint8_t value = icon.bits[sourceRow * rowBytes + sourceColumn / 8];
          if ((value & (0x80U >> (sourceColumn % 8))) == 0) set = true;
        }
      }
      if (set) renderer.drawPixel(x + column, y + row, ink);
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

// Which board action a front key triggers. This is the inverse of
// MappedInputManager::mapButton's Back/Confirm/Left/Right cases, so a user who
// remaps the front buttons gets hints that follow the keys.
//
// Deliberately not MappedInputManager::mapLabels(): that also swaps
// previous/next when frontButtonFollowOrientation is set and the screen is
// rotated, which this board always is. The board's input handling reads raw
// Button::Left/Right with no such swap, so borrowing mapLabels' swap would
// label the keys with the opposite of what they do.
enum class ToolbarAction : uint8_t { None, Settings, Refresh, Previous, Next };

ToolbarAction actionForFrontButton(const uint8_t hardwareIndex) {
  if (hardwareIndex == SETTINGS.frontButtonBack) return ToolbarAction::Settings;
  if (hardwareIndex == SETTINGS.frontButtonConfirm) return ToolbarAction::Refresh;
  if (hardwareIndex == SETTINGS.frontButtonLeft) return ToolbarAction::Previous;
  if (hardwareIndex == SETTINGS.frontButtonRight) return ToolbarAction::Next;
  return ToolbarAction::None;
}

// The same glyph at half scale, to match drawIconHalf in the toolbar.
void drawRefreshIconHalf(const GfxRenderer& renderer, const int x, const int y, const bool ink) {
  renderer.drawLine(x + 3, y + 8, x + 4, y + 5, 1, ink);
  renderer.drawLine(x + 4, y + 5, x + 7, y + 3, 1, ink);
  renderer.drawLine(x + 7, y + 3, x + 10, y + 3, 1, ink);
  renderer.drawLine(x + 10, y + 3, x + 13, y + 5, 1, ink);
  renderer.drawLine(x + 13, y + 5, x + 14, y + 6, 1, ink);
  renderer.drawLine(x + 11, y + 6, x + 14, y + 6, 1, ink);
  renderer.drawLine(x + 14, y + 6, x + 14, y + 3, 1, ink);

  renderer.drawLine(x + 13, y + 9, x + 12, y + 12, 1, ink);
  renderer.drawLine(x + 12, y + 12, x + 10, y + 14, 1, ink);
  renderer.drawLine(x + 10, y + 14, x + 6, y + 14, 1, ink);
  renderer.drawLine(x + 6, y + 14, x + 4, y + 12, 1, ink);
  renderer.drawLine(x + 4, y + 12, x + 3, y + 10, 1, ink);
  renderer.drawLine(x + 3, y + 10, x + 3, y + 13, 1, ink);
  renderer.drawLine(x + 3, y + 10, x + 6, y + 10, 1, ink);
}
}  // namespace

WienerLinienActivity::WienerLinienActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("WienerLinien", renderer, mappedInput) {}

void WienerLinienActivity::setBoardOrientation() {
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  // Called whenever the board takes the screen back — on entry and on every
  // return from Settings or Wi-Fi selection. Those screens paint portrait, so
  // the next board paint must clear their residue rather than draw over it.
  repaintsUntilCleanRefresh = 0;
}

void WienerLinienActivity::onEnter() {
  Activity::onEnter();
  setBoardOrientation();
  errorMessage = nullptr;
  schedules.fill({});

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
  // The first paint after entering (including the return from Settings) clears
  // whatever the previous screen left behind; after that a clean waveform every
  // REPAINTS_PER_CLEAN_REFRESH paints keeps residue from accumulating.
  const bool clean = repaintsUntilCleanRefresh <= 0;
  renderer.displayBuffer(clean ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  repaintsUntilCleanRefresh = clean ? REPAINTS_PER_CLEAN_REFRESH : repaintsUntilCleanRefresh - 1;
}

void WienerLinienActivity::drawBoard() {
  const bool dark = WIENER_LINIEN_STORE.getConfig().darkTheme;
  const bool ink = !dark;
  renderer.clearScreen(dark ? 0x00 : 0xFF);
  drawToolbar(ink);
  const int boardWidth = renderer.getScreenWidth() - TOOLBAR_WIDTH;
  const int boardHeight = renderer.getScreenHeight();

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
  // Stops are assigned to columns starting from the active one and wrapping, so
  // Previous/Next slide the whole visible set by one stop over the cache.
  const auto& config = WIENER_LINIEN_STORE.getConfig();
  const wiener_board::BoardLayout layout =
      wiener_board::planColumns(std::min(config.stops.size(), WIENER_MAX_STOPS), config.activeStopIndex,
                                config.columnCount, config.maxDepartures, config.boardRows);
  const size_t columnCount = layout.columnCount;
  if (columnCount == 0) {
    drawMessage(tr(STR_WL_NO_DEPARTURES), 8, 8, boardWidth - 16, boardHeight - 16, ink);
    return;
  }

  const uint8_t rowsPerStop = std::max<uint8_t>(1, config.maxDepartures);
  for (size_t index = 0; index < columnCount; ++index) {
    const int left = static_cast<int>(boardWidth * index / columnCount);
    const int right = static_cast<int>(boardWidth * (index + 1) / columnCount);
    if (index > 0) renderer.drawLine(left, 0, left, boardHeight - 1, 2, ink);
    drawColumn(layout.columns[index], left + (index > 0 ? 2 : 0), 0, right - left - (index > 0 ? 2 : 0), boardHeight,
               rowsPerStop, ink);
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

void WienerLinienActivity::drawColumn(const wiener_board::ColumnLayout& layout, const int x, const int y,
                                      const int width, const int height, const uint8_t rowsPerStop, const bool ink) {
  if (layout.sectionCount == 0) return;

  // Section chrome is fixed, so the rest of the column belongs to the rows.
  // Splitting that remainder by cumulative row index keeps every row in the
  // column the same height and lands the last section exactly on the bottom.
  const int chrome = static_cast<int>(layout.sectionCount) * HEADER_HEIGHT + LABEL_HEIGHT;
  const int rowsHeight = std::max(1, height - chrome);
  const size_t totalRows = layout.sectionCount * rowsPerStop;

  int top = y;
  size_t rowsPlaced = 0;
  for (size_t index = 0; index < layout.sectionCount; ++index) {
    const size_t rowsAfter = rowsPlaced + rowsPerStop;
    const int sectionRows =
        static_cast<int>(rowsHeight * rowsAfter / totalRows) - static_cast<int>(rowsHeight * rowsPlaced / totalRows);
    const int sectionHeight = HEADER_HEIGHT + (index == 0 ? LABEL_HEIGHT : 0) + sectionRows;
    if (index > 0) renderer.drawLine(x, top, x + width - 1, top, 2, ink);
    drawSection(schedules[layout.stopIndices[index]], x, top, width, sectionHeight, rowsPerStop, ink, index == 0);
    top += sectionHeight;
    rowsPlaced = rowsAfter;
  }
}

void WienerLinienActivity::drawSection(const StopSchedule& schedule, const int x, const int y, const int width,
                                       const int height, const uint8_t rowCount, const bool ink,
                                       const bool drawLabels) {
  char title[112]{};
  WienerDisplayFont::normalize(schedule.title[0] ? schedule.title : tr(STR_WL_TITLE), title, sizeof(title));
  trimToWidth(title, width - 12);
  const int titleScale = WienerDisplayFont::fitScale(title, width - 12, HEADER_HEIGHT - 8, 3);
  WienerDisplayFont::drawCentered(renderer, x + 6, y + 3, width - 12, HEADER_HEIGHT - 6, title, titleScale, ink);
  renderer.drawLine(x, y + HEADER_HEIGHT, x + width - 1, y + HEADER_HEIGHT, ink);

  // The Line/Min labels describe columns that are identical in every section,
  // so they are drawn once, under the first stop title in the column.
  const int labelHeight = drawLabels ? LABEL_HEIGHT : 0;
  if (drawLabels) {
    char lineHeader[32]{};
    char minutesHeader[64]{};
    WienerDisplayFont::normalize(tr(STR_WL_LINE_HEADER), lineHeader, sizeof(lineHeader));
    WienerDisplayFont::normalize(tr(STR_WL_MINUTES_HEADER), minutesHeader, sizeof(minutesHeader));
    trimToWidth(minutesHeader, width - WienerDisplayFont::textWidth(lineHeader, 1) - 18);
    const int labelY = y + HEADER_HEIGHT + (LABEL_HEIGHT - 7) / 2;
    WienerDisplayFont::drawText(renderer, x + 5, labelY, lineHeader, 1, ink);
    WienerDisplayFont::drawText(renderer, x + width - WienerDisplayFont::textWidth(minutesHeader, 1) - 5, labelY,
                                minutesHeader, 1, ink);
  }

  const int rowsTop = y + HEADER_HEIGHT + labelHeight;
  renderer.drawLine(x, rowsTop, x + width - 1, rowsTop, ink);
  const int bodyHeight = std::max(1, height - HEADER_HEIGHT - labelHeight);
  const int lineWidth = std::clamp(width / 5, 46, 105);
  const int minuteWidth = std::clamp(width / 6, 42, 96);
  const int lineRight = x + lineWidth;
  const int minuteLeft = x + width - minuteWidth;
  renderer.drawLine(lineRight, rowsTop, lineRight, y + height - 1, ink);
  renderer.drawLine(minuteLeft, rowsTop, minuteLeft, y + height - 1, ink);

  if (schedule.message) {
    drawMessage(schedule.message, lineRight + 5, rowsTop + 5, minuteLeft - lineRight - 10, bodyHeight - 10, ink);
    return;
  }
  if (schedule.departureCount == 0) {
    drawMessage(tr(STR_WL_NO_DEPARTURES), lineRight + 5, rowsTop + 5, minuteLeft - lineRight - 10, bodyHeight - 10,
                ink);
    return;
  }

  const size_t visibleRows = std::min(schedule.departureCount, static_cast<size_t>(rowCount));
  for (size_t index = 0; index < visibleRows; ++index) {
    const int rowTop = rowsTop + static_cast<int>(bodyHeight * index / rowCount);
    const int rowBottom = rowsTop + static_cast<int>(bodyHeight * (index + 1) / rowCount);
    const int rowHeight = std::max(1, rowBottom - rowTop);
    if (index > 0) renderer.drawLine(x, rowTop, x + width - 1, rowTop, ink);

    const auto& departure = schedule.departures[index];
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
  const int x = renderer.getScreenWidth() - TOOLBAR_WIDTH;
  const int screenHeight = renderer.getScreenHeight();
  const int slotHeight = screenHeight / 4;
  renderer.drawLine(x, 0, x, screenHeight - 1, 2, ink);
  const freeink::Icon settingsIcon{32, 32, 16, Settings2Icon};

  const int buttonX = x + (TOOLBAR_WIDTH - BUTTON_WIDTH) / 2;
  for (int slot = 1; slot < 4; ++slot) {
    renderer.drawLine(x + 4, slot * slotHeight, renderer.getScreenWidth() - 5, slot * slotHeight, ink);
  }

  // Slots run down the screen while the physical keys run up it: the board
  // renders rotated, so front key 0 belongs to the bottom slot.
  for (uint8_t hardwareIndex = 0; hardwareIndex < 4; ++hardwareIndex) {
    const int slot = 3 - hardwareIndex;
    const int centerY = slot * slotHeight + slotHeight / 2;
    const int buttonY = centerY - BUTTON_HEIGHT / 2;
    renderer.drawRect(buttonX, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT, 1, ink);

    // Icons are centred in the button box at half their asset size.
    const int wideX = buttonX + (BUTTON_WIDTH - 16) / 2;
    const int wideY = buttonY + (BUTTON_HEIGHT - 16) / 2;
    const int narrowX = buttonX + (BUTTON_WIDTH - 12) / 2;
    const int narrowY = buttonY + (BUTTON_HEIGHT - 12) / 2;
    switch (actionForFrontButton(hardwareIndex)) {
      case ToolbarAction::Settings:
        drawIconHalf(renderer, settingsIcon, wideX, wideY, ink);
        break;
      case ToolbarAction::Refresh:
        drawRefreshIconHalf(renderer, wideX, wideY, ink);
        break;
      case ToolbarAction::Previous:
        drawIconHalf(renderer, icon_reader_back_24, narrowX, narrowY, ink);
        break;
      case ToolbarAction::Next:
        drawIconHalf(renderer, icon_reader_next_24, narrowX, narrowY, ink);
        break;
      case ToolbarAction::None:
        break;
    }
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
  if (state != State::READY) return false;
  // Derived from the cache rather than from the last fetch's success count: a
  // refresh where every stop fails must not make the board forget what it is
  // already showing and fall back to the loading screen.
  for (const auto& schedule : schedules) {
    if (schedule.message == nullptr && schedule.title[0] != '\0') return true;
  }
  return false;
}

void WienerLinienActivity::launchWifiSelection() {
  state = State::WIFI_SELECTION;
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  // The incoming screen paints with a fast refresh it cannot upgrade itself,
  // and this is a 90-degree rotation, so every pixel changes. Hand its first
  // paint a ghost-clearing waveform.
  renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
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
  renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
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
    // Stops may have been added, removed, or re-pointed; the whole cache is
    // stale, so drop it and refetch (this is the one path that may show
    // Loading again).
    schedules.fill({});
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
  // The loading screen is only acceptable when there is nothing to show yet.
  // Once a schedule is on the panel, refreshes repaint over the stale board.
  const size_t stopCount = std::min(config.stops.size(), WIENER_MAX_STOPS);
  if (!hadSchedule) {
    schedules.fill({});
    state = State::LOADING;
    requestUpdateAndWait();
  }

  size_t successfulStops = 0;
  for (size_t stopIndex = 0; stopIndex < stopCount; ++stopIndex) {
    auto& schedule = schedules[stopIndex];
    StopSchedule freshSchedule;
    const auto* stop = WIENER_LINIEN_STORE.getStop(stopIndex);
    if (!stop || stop->rbl.empty()) {
      if (schedule.title[0] == '\0') schedule.message = tr(STR_WL_CONFIGURE_FIRST);
      continue;
    }
    snprintf(freshSchedule.title, sizeof(freshSchedule.title), "%s",
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
      // Leave a stop that already has departures showing them; only a stop with
      // nothing to display inherits the error text.
      const char* fetchError = parser->hasError() ? tr(STR_WL_PARSE_FAILED) : tr(STR_WL_FETCH_FAILED);
      if (schedule.title[0] == '\0') schedule.message = fetchError;
      if (!errorMessage) errorMessage = fetchError;
      continue;
    }

    parser->finalize();
    freshSchedule.departureCount = parser->getDepartureCount();
    for (size_t index = 0; index < freshSchedule.departureCount; ++index) {
      freshSchedule.departures[index] = parser->getDeparture(index);
    }
    const char* apiTitle = parser->getStopTitle();
    if (apiTitle[0] != '\0') snprintf(freshSchedule.title, sizeof(freshSchedule.title), "%s", apiTitle);
    schedule = freshSchedule;
    ++successfulStops;
  }

  if (successfulStops == 0) {
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
  // Every stop was fetched on the last refresh, so this is a re-layout of data
  // already in memory. Deliberately no fetch and no LOADING state here.
  requestUpdate();
}
