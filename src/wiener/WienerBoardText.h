#pragma once

class GfxRenderer;

// Board text with inline interchange badges.
//
// Wiener Linien names carry interchanges as bare letters -- "Praterstern S U",
// "Schottenring U" -- and the platform displays draw those as the S-Bahn and
// U-Bahn logos rather than as letters. A standalone "S" or "U" is rendered as
// a badge here; every other character goes through WienerDisplayFont
// unchanged, so a name containing the word "US" or a line like "S45" is
// untouched.
namespace WienerBoardText {

// `markers` off renders every character as text, for the settings that turn
// the interchange symbols off in stop names or destinations.
int width(const char* text, int scale, bool markers = true);
int fitScale(const char* text, int maxWidth, int maxHeight, int maxScale = 8, bool markers = true);
void draw(const GfxRenderer& renderer, int x, int y, const char* text, int scale, bool state, bool markers = true);
void drawCentered(const GfxRenderer& renderer, int x, int y, int width, int height, const char* text, int scale,
                  bool state, bool markers = true);
// Shortens `text` in place, ellipsis included, until it renders within
// `maxWidth` at `scale`.
void trimToWidth(char* text, int maxWidth, int scale, bool markers = true);

// True when `text` contains an interchange letter. The markers are taller than
// a text row, so a line holding one needs extra leading above and below.
bool hasMarker(const char* text);
// Rows a marker adds beyond the seven of a text line.
int markerOverhangRows();

}  // namespace WienerBoardText
