# Tested hardware

Hardware tried with Game-Maker OS on [sandbox-kernel](https://github.com/lewismoten/sandbox-kernel).
Only what a log or a person's test confirmed is marked as working. Everything else says "not
tested" or "not recorded".

## Boards

| Board                    | What works                                                                                                                           | Notes                                                                                                                        | Date       | Tested by |
| ------------------------ | ------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------- | ---------- | --------- |
| Raspberry Pi Zero W v1.1 | Boots the kernel and `GMOS.ELF`; games list, details and play on HDMI; USB keyboards through the host role; e-ink status on the HAT. | The first target. Connected to a computer by USB for development, or on PWR IN with a keyboard on the data port (host role). | 2026-11-12 | Lewis     |

## Displays

| Display                              | Connection  | What works                                                                 | Notes                                                                                                                                         | Date       | Tested by |
| ------------------------------------ | ----------- | -------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- | ---------- | --------- |
| Desk monitor                         | HDMI        | Games list, menus and play.                                                | Make and model not recorded.                                                                                                                  | 2026-11-11 | Lewis     |
| Living-room TV                       | HDMI        | Games list, menus and play.                                                | Make and model not recorded. HDMI sound and the TV remote (CEC): see the kernel's notes.                                                      | 2026-11-11 | Lewis     |
| Insignia NS-50F301NA22 (50 inch)     | HDMI        | Picture.                                                                   | TV remote over HDMI-CEC did not respond on PI-2. Music stopped after a few seconds (cause not known yet). Kernel and GMOS build not recorded. | 2026-11-12 | Lewis     |
| Waveshare 2.13 inch e-Paper HAT (V4) | GPIO header | The kernel's status lines show on the panel during start-up and USB setup. | Game-Maker OS's own e-ink pages (loading, library count, game title, `SAVING`) not tested.                                                    | 2026-11-11 | Lewis     |

## USB hubs

| Hub                   | USB VID:PID | Powered? | What works                                                                   | Notes                                                                                  | Date       | Tested by |
| --------------------- | ----------- | -------- | ---------------------------------------------------------------------------- | -------------------------------------------------------------------------------------- | ---------- | --------- |
| Four-port USB 2.0 hub | 1A40:0101   | No       | Two keyboards at once behind it (split transactions for full and low speed). | Make and model not recorded. Unplugging and re-plugging while running: not tested yet. | 2026-11-11 | Lewis     |

## Keyboards

The kernel reads boot keyboards only. Trackpads and media keys are not supported yet.

| Keyboard                                         | Receiver / wired | USB VID:PID  | Speed        | Through a hub?                     | Kernel                                                           | What works                                                                                                                                                              | Notes                                                                                                                   | Date       | Tested by        |
| ------------------------------------------------ | ---------------- | ------------ | ------------ | ---------------------------------- | ---------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------- | ---------- | ---------------- |
| Rii mini X2 RT-MWK01                             | 2.4 GHz receiver | 1997:2433    | Full         | Yes, hub 1A40:0101 (split)         | 139d140 (feature/adv-fixes-1111), host companion gen 31, on PI-2 | Keys, menus, games, high-score name entry on a desk monitor and a living-room TV. Arrows, F-keys, LEDs: not tested separately. Trackpad, media keys: not supported yet. | Interface 0 is a boot keyboard (accepted); interface 1 (mouse protocol: trackpad, system and media control) is skipped. | 2026-11-11 | Lewis            |
| Unknown keyboard (second device on the same hub) | Wired or unknown | 1A2C:2124    | Low          | Yes, hub 1A40:0101, port 4 (split) | same                                                             | Enumerated and mounted; interface 0 accepted as a boot keyboard; first report received. Keys, arrows, F-keys, LEDs: not tested.                                         | Interface 1 (report protocol) is skipped. Make and model not recorded.                                                  | 2026-11-11 | Lewis (log only) |
| Rii mini X1                                      | 2.4 GHz receiver | not recorded | not recorded | not recorded                       | not recorded                                                     | Works, per a person's test. Which keys were tried: not recorded. Trackpad, media keys: not supported yet.                                                               | Add the USB.TXT lines to fill in the IDs.                                                                               | 2026-11-12 | Lewis            |
| Rii RK907                                        | Wired            | not recorded | not recorded | not recorded                       | not recorded                                                     | Works, per a person's test. Which keys were tried: not recorded.                                                                                                        | Add the USB.TXT lines to fill in the IDs.                                                                               | 2026-11-12 | Lewis            |

Sources: the Rii receiver's descriptors from a computer (2026-11-10), the `USB.TXT` from the
2026-11-11 TV test, and hands-on tests on 2026-11-12.

## Adding a row

Run sandbox-kernel's `usb_kernel.py <port> logs` and copy the device's `USB.TXT` lines (the
`Dn ... VID:PID`, `hidN ... ACCEPTED` or `skipped` and `first report` lines), then note the
kernel version, hub use and what you tried (keys, arrows, F-keys, LEDs). Write "not tested" for
the rest.
