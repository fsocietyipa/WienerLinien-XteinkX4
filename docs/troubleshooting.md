# Troubleshooting

Common issues while running the departure board.

- [Board shows no departures](#board-shows-no-departures)
- [Wi-Fi connection drops or times out](#wi-fi-connection-drops-or-times-out)
- [Saved password not working](#saved-password-not-working)
- [Settings are not retained](#settings-are-not-retained)

### Board shows no departures

**Problem:** A stop column stays empty, or the board keeps showing a stale
schedule.

**Solutions:**

1. Check the RBL ID. It is the numeric stop identifier from the Wiener Linien
   realtime data set — not the stop number printed on the shelter. A wrong RBL
   returns a valid but empty response.
2. Check the line filter for that stop. A filter such as `1, D` hides every
   other line at the stop; clear it to see all departures.
3. Confirm Wi-Fi is up. Without connectivity the firmware keeps the last valid
   schedule on screen and retries every 15 seconds.
4. Some stops publish no realtime data outside service hours.

### Wi-Fi connection drops or times out

**Problem:** Refreshes fail intermittently.

**Solutions:**

1. Move the device closer to the router — the X4 has a small onboard antenna.
2. Check signal strength on the device (should be at least `||`).
3. The board keeps Wi-Fi associated with modem sleep disabled. A router that
   aggressively de-authenticates idle clients can still drop it; the firmware
   reconnects on the next refresh.
4. Try a 2.4GHz network. The ESP32-C3 has no 5GHz radio.

### Saved password not working

**Problem:** The device fails to connect with saved credentials.

**Solutions:**

1. When the connection fails, you are prompted to "Forget Network".
2. Select **Yes** to remove the saved password.
3. Reconnect and enter the password again, then choose to save it.

### Settings are not retained

**Problem:** Stops or Wi-Fi credentials are lost after a power cycle.

**Solutions:**

1. Settings live on the SD card at `/.crosspoint/wiener_linien.json`. Confirm an
   SD card is inserted and writable.
2. A card formatted as exFAT with a very large cluster size can fail to mount;
   reformat as FAT32.
