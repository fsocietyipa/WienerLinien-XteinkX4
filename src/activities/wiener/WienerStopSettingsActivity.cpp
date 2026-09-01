#include "WienerStopSettingsActivity.h"

#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cctype>

#include "activities/reader/QrDisplayActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
// The RBL is the numeric stop id from the Wiener Linien open data, not the
// number on the shelter, so the screen points at a directory for looking it up.
constexpr char RBL_DIRECTORY_URL[] = "https://till.mabe.at/rbl/";

bool validRbl(const std::string& value) {
  if (value.empty()) return false;
  return std::all_of(value.begin(), value.end(),
                     [](const char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
}
}  // namespace

WienerStopSettingsActivity::WienerStopSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                       const int stopIndex)
    : UiListActivity("WienerStopSettings", renderer, mappedInput), stopIndex(stopIndex) {
  static constexpr StrId labels[MENU_ITEMS] = {StrId::STR_WL_STOP_NAME,   StrId::STR_WL_RBL,
                                               StrId::STR_WL_RBL_QR,      StrId::STR_WL_LINE_FILTER,
                                               StrId::STR_WL_MAKE_ACTIVE, StrId::STR_WL_DELETE_STOP};
  for (int index = 0; index < MENU_ITEMS; ++index) {
    rowItems[index].label = I18N.get(labels[index]);
    rowItems[index].actionValue = static_cast<int16_t>(index);
  }
  // The lookup hint rides on the RBL row itself, where the value is entered.
  rowItems[1].subtitle = tr(STR_WL_RBL_HINT);
  if (const auto* stop = WIENER_LINIEN_STORE.getStop(static_cast<size_t>(stopIndex))) editStop = *stop;
}

const char* WienerStopSettingsActivity::headerTitle() const { return tr(STR_WL_EDIT_STOP); }

bool WienerStopSettingsActivity::save() {
  const bool ok = WIENER_LINIEN_STORE.updateStop(static_cast<size_t>(stopIndex), editStop);
  showSaveError = !ok;
  return ok;
}

void WienerStopSettingsActivity::editText(const int field) {
  const bool rbl = field == 1;
  std::string initial = field == 0 ? editStop.name : (rbl ? editStop.rbl : editStop.lineFilter);
  const char* title = field == 0 ? tr(STR_WL_STOP_NAME) : (rbl ? tr(STR_WL_RBL) : tr(STR_WL_LINE_FILTER));
  auto activity =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, title, initial, rbl ? 10 : 63, InputType::Text);
  if (!activity) {
    showSaveError = true;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(activity), [this, field](const ActivityResult& result) {
    if (result.isCancelled) return;
    const auto& value = std::get<KeyboardResult>(result.data).text;
    if (field == 1 && !validRbl(value)) {
      showInvalidRbl = true;
      requestUpdate();
      return;
    }
    if (field == 0)
      editStop.name = value;
    else if (field == 1)
      editStop.rbl = value;
    else
      editStop.lineFilter = value;
    save();
    requestUpdate();
  });
}

void WienerStopSettingsActivity::showRblWebsiteQr() {
  auto activity = makeUniqueNoThrow<QrDisplayActivity>(renderer, mappedInput, std::string(RBL_DIRECTORY_URL));
  if (!activity) {
    showSaveError = true;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(activity), [this](const ActivityResult&) { requestUpdate(); });
}

void WienerStopSettingsActivity::activateIndex(const int index) {
  app.clearTapFlash();
  showSaveError = false;
  showInvalidRbl = false;
  if (index == 2) {
    showRblWebsiteQr();
  } else if (index <= 3) {
    // Name and RBL keep field numbers 0 and 1; the line filter follows the QR
    // row, so it maps back to field 2.
    editText(index == 3 ? 2 : index);
  } else if (index == 4) {
    showSaveError = !WIENER_LINIEN_STORE.setActiveStop(static_cast<size_t>(stopIndex));
    requestUpdate();
  } else if (index == 5) {
    if (WIENER_LINIEN_STORE.removeStop(static_cast<size_t>(stopIndex)))
      finish();
    else {
      showSaveError = true;
      requestUpdate();
    }
  }
}

void WienerStopSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& config = WIENER_LINIEN_STORE.getConfig();
  rowItems[0].value = editStop.name.empty() ? tr(STR_NOT_SET) : editStop.name.c_str();
  rowItems[1].value = editStop.rbl.empty() ? tr(STR_NOT_SET) : editStop.rbl.c_str();
  rowItems[2].value = ">";
  rowItems[3].value = editStop.lineFilter.empty() ? tr(STR_WL_ALL_LINES) : editStop.lineFilter.c_str();
  rowItems[4].value = static_cast<size_t>(stopIndex) == config.activeStopIndex ? tr(STR_WL_ACTIVE) : "";
  rowItems[5].value = "";

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
  syncListViewport(screen, props, /*hasSubtitle=*/true);
  screen.list(props);
}

void WienerStopSettingsActivity::drawFooter() {
  UiListActivity::drawFooter();
  if (showInvalidRbl)
    GUI.drawPopup(renderer, tr(STR_WL_INVALID_RBL));
  else if (showSaveError)
    GUI.drawPopup(renderer, tr(STR_ERROR_GENERAL_FAILURE));
}
