#include "WienerLinienParser.h"

#include <strings.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {
void safeCopy(char* destination, const size_t destinationSize, const char* source, const size_t sourceLength) {
  const size_t count = std::min(sourceLength, destinationSize - 1);
  memcpy(destination, source, count);
  destination[count] = '\0';
}

bool keyEquals(const char* key, const size_t length, const char* expected) {
  const size_t expectedLength = strlen(expected);
  return length == expectedLength && memcmp(key, expected, length) == 0;
}
}  // namespace

WienerLinienParser::WienerLinienParser(const char* filter, const size_t resultLimit)
    : parser(JsonCallbacks{this, sOnKey, sOnString, sOnNumber, sOnBool, sOnNull, sOnObjectStart, sOnObjectEnd,
                           sOnArrayStart, sOnArrayEnd}),
      limit(std::clamp(resultLimit, size_t{1}, MAX_DEPARTURES)),
      lineFilter(filter) {}

void WienerLinienParser::feed(const char* data, const size_t len) { parser.feed(data, len); }

void WienerLinienParser::finalize() {
  std::sort(departures.begin(), departures.begin() + static_cast<ptrdiff_t>(departureCount),
            [](const WienerDeparture& left, const WienerDeparture& right) { return left.countdown < right.countdown; });
}

WienerLinienParser::Context WienerLinienParser::currentContext() const {
  return depth == 0 ? Context::OTHER : stack[depth - 1].context;
}

WienerLinienParser::Context WienerLinienParser::childContext(const bool isArray) const {
  const Context parent = currentContext();
  if (depth == 0 && !isArray) return Context::ROOT;
  if (parent == Context::ROOT && lastKey == Key::DATA && !isArray) return Context::DATA;
  if (parent == Context::DATA && lastKey == Key::MONITORS && isArray) return Context::MONITORS_ARRAY;
  if (parent == Context::MONITORS_ARRAY && !isArray) return Context::MONITOR;
  if (parent == Context::MONITOR && lastKey == Key::LOCATION_STOP && !isArray) return Context::LOCATION_STOP;
  if (parent == Context::LOCATION_STOP && lastKey == Key::PROPERTIES && !isArray) return Context::PROPERTIES;
  if (parent == Context::MONITOR && lastKey == Key::LINES && isArray) return Context::LINES_ARRAY;
  if (parent == Context::LINES_ARRAY && !isArray) return Context::LINE;
  if (parent == Context::LINE && lastKey == Key::DEPARTURES && !isArray) return Context::DEPARTURES;
  if (parent == Context::DEPARTURES && lastKey == Key::DEPARTURE && isArray) return Context::DEPARTURE_ARRAY;
  if (parent == Context::DEPARTURE_ARRAY && !isArray) return Context::DEPARTURE;
  if (parent == Context::DEPARTURE && lastKey == Key::DEPARTURE_TIME && !isArray) return Context::DEPARTURE_TIME;
  if (parent == Context::DEPARTURE && lastKey == Key::VEHICLE && !isArray) return Context::VEHICLE;
  return Context::OTHER;
}

void WienerLinienParser::pushContext(const Context context, const bool isArray) {
  if (depth < stack.size()) stack[depth++] = Frame{context, isArray};
  lastKey = Key::NONE;

  if (context == Context::LINE) {
    lineName[0] = '\0';
    lineDestination[0] = '\0';
  } else if (context == Context::DEPARTURE) {
    currentDeparture = {};
    safeCopy(currentDeparture.line, sizeof(currentDeparture.line), lineName, strlen(lineName));
    safeCopy(currentDeparture.destination, sizeof(currentDeparture.destination), lineDestination,
             strlen(lineDestination));
    currentDeparture.countdown = -1;
  }
}

void WienerLinienParser::popContext() {
  if (depth > 0) --depth;
  lastKey = Key::NONE;
}

bool WienerLinienParser::matchesFilter(const char* line) const {
  if (!lineFilter || lineFilter[0] == '\0') return true;

  const char* cursor = lineFilter;
  while (*cursor) {
    while (*cursor == ',' || std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
    const char* start = cursor;
    while (*cursor && *cursor != ',') ++cursor;
    const char* end = cursor;
    while (end > start && std::isspace(static_cast<unsigned char>(end[-1]))) --end;
    const size_t tokenLength = static_cast<size_t>(end - start);
    if (tokenLength == strlen(line) && strncasecmp(start, line, tokenLength) == 0) return true;
  }
  return false;
}

void WienerLinienParser::commitDeparture() {
  if (currentDeparture.countdown < 0 || currentDeparture.line[0] == '\0' || !matchesFilter(currentDeparture.line)) {
    return;
  }

  for (size_t index = 0; index < departureCount; ++index) {
    const auto& existing = departures[index];
    if (existing.countdown == currentDeparture.countdown && strcmp(existing.line, currentDeparture.line) == 0 &&
        strcmp(existing.destination, currentDeparture.destination) == 0) {
      return;
    }
  }

  if (departureCount < limit) {
    departures[departureCount++] = currentDeparture;
    return;
  }

  size_t latestIndex = 0;
  for (size_t index = 1; index < departureCount; ++index) {
    if (departures[index].countdown > departures[latestIndex].countdown) latestIndex = index;
  }
  if (currentDeparture.countdown < departures[latestIndex].countdown) departures[latestIndex] = currentDeparture;
}

void WienerLinienParser::sOnKey(void* ctx, const char* key, const size_t len) {
  auto* self = static_cast<WienerLinienParser*>(ctx);
  if (keyEquals(key, len, "data"))
    self->lastKey = Key::DATA;
  else if (keyEquals(key, len, "monitors"))
    self->lastKey = Key::MONITORS;
  else if (keyEquals(key, len, "locationStop"))
    self->lastKey = Key::LOCATION_STOP;
  else if (keyEquals(key, len, "properties"))
    self->lastKey = Key::PROPERTIES;
  else if (keyEquals(key, len, "title"))
    self->lastKey = Key::TITLE;
  else if (keyEquals(key, len, "lines"))
    self->lastKey = Key::LINES;
  else if (keyEquals(key, len, "name"))
    self->lastKey = Key::NAME;
  else if (keyEquals(key, len, "towards"))
    self->lastKey = Key::TOWARDS;
  else if (keyEquals(key, len, "departures"))
    self->lastKey = Key::DEPARTURES;
  else if (keyEquals(key, len, "departure"))
    self->lastKey = Key::DEPARTURE;
  else if (keyEquals(key, len, "departureTime"))
    self->lastKey = Key::DEPARTURE_TIME;
  else if (keyEquals(key, len, "countdown"))
    self->lastKey = Key::COUNTDOWN;
  else if (keyEquals(key, len, "vehicle"))
    self->lastKey = Key::VEHICLE;
  else
    self->lastKey = Key::NONE;
}

void WienerLinienParser::sOnString(void* ctx, const char* value, const size_t len) {
  auto* self = static_cast<WienerLinienParser*>(ctx);
  const Context context = self->currentContext();
  if (context == Context::PROPERTIES && self->lastKey == Key::TITLE && self->stopTitle[0] == '\0') {
    safeCopy(self->stopTitle, sizeof(self->stopTitle), value, len);
  } else if (context == Context::LINE && self->lastKey == Key::NAME) {
    safeCopy(self->lineName, sizeof(self->lineName), value, len);
  } else if (context == Context::LINE && self->lastKey == Key::TOWARDS) {
    safeCopy(self->lineDestination, sizeof(self->lineDestination), value, len);
  } else if (context == Context::VEHICLE && self->lastKey == Key::NAME) {
    safeCopy(self->currentDeparture.line, sizeof(self->currentDeparture.line), value, len);
  } else if (context == Context::VEHICLE && self->lastKey == Key::TOWARDS) {
    safeCopy(self->currentDeparture.destination, sizeof(self->currentDeparture.destination), value, len);
  }
  self->lastKey = Key::NONE;
}

void WienerLinienParser::sOnNumber(void* ctx, const char* value, size_t) {
  auto* self = static_cast<WienerLinienParser*>(ctx);
  if (self->currentContext() == Context::DEPARTURE_TIME && self->lastKey == Key::COUNTDOWN) {
    const long countdown = strtol(value, nullptr, 10);
    if (countdown >= 0 && countdown <= INT16_MAX) self->currentDeparture.countdown = static_cast<int16_t>(countdown);
  }
  self->lastKey = Key::NONE;
}

void WienerLinienParser::sOnBool(void* ctx, bool) { static_cast<WienerLinienParser*>(ctx)->lastKey = Key::NONE; }
void WienerLinienParser::sOnNull(void* ctx) { static_cast<WienerLinienParser*>(ctx)->lastKey = Key::NONE; }

void WienerLinienParser::sOnObjectStart(void* ctx) {
  auto* self = static_cast<WienerLinienParser*>(ctx);
  self->pushContext(self->childContext(false), false);
}

void WienerLinienParser::sOnObjectEnd(void* ctx) {
  auto* self = static_cast<WienerLinienParser*>(ctx);
  if (self->currentContext() == Context::DEPARTURE) self->commitDeparture();
  self->popContext();
}

void WienerLinienParser::sOnArrayStart(void* ctx) {
  auto* self = static_cast<WienerLinienParser*>(ctx);
  self->pushContext(self->childContext(true), true);
}

void WienerLinienParser::sOnArrayEnd(void* ctx) { static_cast<WienerLinienParser*>(ctx)->popContext(); }
