# Wiener Linien departure board for Xteink X4

A dedicated Wiener Linien departure-board firmware for the ESP32-C3 based
[Xteink X4](https://www.xteink.com/products/xteink-x4). It uses CrossPoint
Reader's X4 hardware drivers underneath, but boots directly into the departure
board and does not expose the reader interface.

The firmware reads the official realtime monitor endpoint directly and keeps
only the configured number of departures in memory while the JSON response is
streamed — the ESP32-C3 has no PSRAM and roughly 380KB of usable RAM, so nothing
is buffered that does not have to be.

## Features

- Landscape departure board with a high-contrast dot-matrix display.
- One to three stops side by side, one to ten departures per stop.
- Destinations wrap onto two lines when that produces a more legible fit.
- Departures due in one minute or less use the two-square arrival symbol.
- Per-stop line filters, for example `1, D, U2`.
- Dark or Light board theme.
- Icon toolbar with Settings, Refresh, Previous, and Next.
- Wi-Fi stays associated with modem sleep disabled. If a refresh loses
  connectivity the firmware reconnects, keeps the last valid schedule on screen,
  and retries after 15 seconds.
- Over-the-air updates: **Settings > Check for updates** pulls the latest
  GitHub release and flashes it to the inactive OTA slot.
- Firmware updates from the SD card.

Settings are stored on the SD card in `/.crosspoint/wiener_linien.json`.

## Install

Download `wiener-linien-x4.bin` from the
[Releases](https://github.com/fsocietyipa/WienerLinien-XteinkX4/releases) page.

For the first installation, connect the X4 over USB and select the image with
the CrossPoint web flasher's **Custom .bin** option at
https://crosspointreader.com/#flash-tools.

A command-line installation can write the same application image at offset
`0x10000`:

```sh
pip install esptool
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 \
  write_flash 0x10000 wiener-linien-x4.bin
```

Adjust the serial port for your computer.

> The image is an ESP32-C3 application image, not a vendor `update.bin` package.
> Do not rename it to `update.bin` for the Chinese stock updater.

If the X4 already runs a compatible CrossPoint-based firmware with an SD
firmware updater, copy `wiener-linien-x4.bin` to the SD card and select it
there.

Some Xteink units bought from third-party stores ship with USB flashing locked
from the factory. If the browser's serial device picker never shows the device,
see the unlock tool at https://crosspointreader.com/#unlock-tool.

## Updating

Once this firmware is installed, **Settings > Check for updates** connects to
Wi-Fi, reads the latest release from this repository, and — if the release tag
is newer than the running build — downloads `wiener-linien-x4.bin` straight into
the inactive OTA partition and reboots into it. The image is rejected before it
can become the boot target if its chip ID or embedded board tag does not match
the device.

`.bin` files on the SD card can also be flashed from **Settings > SD Card
Firmware Update**.

> The release asset name `wiener-linien-x4.bin` is part of the update contract.
> Renaming it in a future release leaves installed devices unable to find the
> update.

## Configure

1. Press **Settings** on the departure board.
2. Open **Stops**, choose **Add stop**, and enter a name and numeric RBL ID.
3. Optionally set a comma-separated line filter, for example `1, D, U2`.
4. Add more stops as needed and mark one active.
5. Configure Wi-Fi, **Schedules per stop** (1-10), **Stop columns** (1-3), the
   **Board theme** (Dark or Light), and the refresh interval.
6. Return to the board. **Previous** and **Next** move the first visible stop;
   following configured stops fill the remaining columns and wrap around.

The RBL ID is the numeric stop identifier from the Wiener Linien realtime data
set, not the stop number printed on the shelter.

## Build

```sh
git clone --recursive https://github.com/fsocietyipa/WienerLinien-XteinkX4
cd WienerLinien-XteinkX4

# if cloned without --recursive:
git submodule update --init --recursive
```

Requires [pioarduino](https://github.com/pioarduino/pioarduino) (or VS Code with
the pioarduino plugin), Python 3.8+, and `clang-format` 21 for the format check.

```sh
pio run                  # development build  -> .pio/build/default/firmware.bin
pio run -e gh_release    # release build      -> .pio/build/gh_release/firmware.bin
pio run --target upload  # build and flash
```

Nix/NixOS users can enter the development shell with `nix develop -f nix` or
`nix-shell nix`.

Pre-commit checks:

```sh
./bin/clang-format-fix
pio check -e default
pio run -e default
```

For serial logs:

```sh
python3 -m pip install pyserial colorama matplotlib
python3 scripts/debugging_monitor.py
```

## Documentation

- [Troubleshooting](./docs/troubleshooting.md)
- [Contributing docs](./docs/contributing/README.md)
- [Activity manager](./docs/activity-manager.md)
- [File formats](./docs/file-formats.md)
- [Internationalization](./docs/i18n.md)
- [Recovering a bricked device](./docs/fix-bricked-xteink.md)

## Credits and license

This project is a fork of
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) and
keeps its hardware abstraction layer, renderer, UI framework, and networking
stack. All credit for that foundation goes to the CrossPoint contributors.
Low-level hardware support comes from the
[FreeInk SDK](https://github.com/Free-Ink/freeink-sdk).

Released under the [MIT License](./LICENSE).

Not affiliated with Wiener Linien, Xteink, or any device manufacturer. Realtime
data is retrieved from the Wiener Linien open data endpoint and is subject to
its terms of use.
