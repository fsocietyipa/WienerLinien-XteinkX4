#pragma once

// Release-tag comparison for the OTA update check.
//
// GitHub release tags in this project are short ("0.1", "0.2"), while
// CROSSPOINT_VERSION is a full triple ("0.1.0") and development builds carry a
// branch/SHA suffix ("0.1.0-develop-0eb832b"). Missing segments read as 0, so
// "0.1" and "0.1.0" compare equal instead of leaving the patch field
// uninitialized, and a leading "v" is accepted on either side.

namespace firmware_version {

struct Semver {
  int major = 0;
  int minor = 0;
  int patch = 0;
};

// Never fails: text that parses no leading integer yields 0.0.0.
Semver parse(const char* text);

// True when `latest` names a strictly newer release than `current`. Equal
// triples count as newer only when `current` is a release candidate ("-rc"),
// so an RC upgrades to the final build of the same version.
bool isNewer(const char* latest, const char* current);

}  // namespace firmware_version
