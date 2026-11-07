# Game-Maker OS

Game-Maker OS turns a Raspberry Pi into a Game-Maker machine. It boots straight to a list of games you can play on a TV with a gamepad or remote. It is the Sandbox Edition of RSD Game-Maker 3.0, running as an operating system of its own.

It runs on [sandbox-kernel](https://github.com/lewismoten/sandbox-kernel), a small MIT-licensed bare-metal kernel for Raspberry Pi. The kernel owns the hardware and exposes it through a standard, POSIX-style interface. Game-Maker OS is a separate program the kernel loads from the SD card. If the OS fails, the kernel recovers instead of leaving a blank screen.

## Status

Planning. The first target is a Raspberry Pi Zero W v1.1 with a Waveshare 2.13 inch e-Paper HAT (V4) as a status display, connected to a computer by USB for development.

## Plan

- Boot to a games list on HDMI; play with a USB gamepad, the TV remote (HDMI-CEC) or an IR remote.
- Games live in a `games` folder on the SD card that any computer can copy files into; Wi-Fi downloads come later.
- PC speaker sound first, then digital sound (VOC) and FM music.
- Reuse the game player core from [Game-Maker: Sandbox Edition](https://github.com/lewismoten/GameMaker-SandboxEdition) instead of forking it.
- Later: the e-ink display as a desk buddy (games played, badges, the game in play), a private game server for the home network, and other Pi models from the same SD card.

## License

MIT. See [LICENSE](LICENSE).
