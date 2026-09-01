#include "FirmwareVersion.h"

#include <cstdio>
#include <cstring>

namespace firmware_version {

Semver parse(const char* text) {
  Semver out;
  if (text == nullptr) return out;
  if (*text == 'v' || *text == 'V') ++text;
  // sscanf leaves trailing arguments untouched when it stops early, so the
  // zero-initialised fields are what make "0.1" equal to "0.1.0".
  sscanf(text, "%d.%d.%d", &out.major, &out.minor, &out.patch);
  return out;
}

bool isNewer(const char* latest, const char* current) {
  if (latest == nullptr || current == nullptr || *latest == '\0' || *current == '\0') return false;

  const Semver l = parse(latest);
  const Semver c = parse(current);

  if (l.major != c.major) return l.major > c.major;
  if (l.minor != c.minor) return l.minor > c.minor;
  if (l.patch != c.patch) return l.patch > c.patch;

  return strstr(current, "-rc") != nullptr;
}

}  // namespace firmware_version
