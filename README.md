# Game-Maker OS

Game-Maker OS turns a Raspberry Pi into a Game-Maker machine. It boots straight to a list of games you can play on a TV with a gamepad or remote. It is the Sandbox Edition of RSD Game-Maker 3.0, running as an operating system of its own.

It runs on [sandbox-kernel](https://github.com/lewismoten/sandbox-kernel), a small MIT-licensed bare-metal kernel for Raspberry Pi. The kernel owns the hardware and exposes it through a standard, POSIX-style interface. Game-Maker OS is a separate program the kernel loads from the SD card. If the OS fails, the kernel recovers instead of leaving a blank screen.

## Status

The first program runs on the board. `GMOS.ELF` lists the games in `/sd/GAMES` (each a folder with a `.GAM` file, titled from that file), draws the list on HDMI at 640x480, moves the selection with the arrow keys and logs the game chosen with Enter; Esc or Q quits. Playing a game comes later. Input is the kernel's synthetic device for now: the development computer sends key presses over USB.

The first target is a Raspberry Pi Zero W v1.1 with a Waveshare 2.13 inch e-Paper HAT (V4) as a status display, connected to a computer by USB for development.

![The games list, rendered on the host from the same code; the board's screen checksum matched it](docs/games-list.png)

## Build

`GMOS.ELF` is built only against the sandbox-kernel SDK, vendored in `sdk/` at the kernel commit named in `sdk/PINNED`. `make` builds it with the Arm GNU toolchain; `make test` runs the host tests and renders the list to PNG files in `build/host/`. See [docs/build.md](docs/build.md) for installing it on a card and driving it over USB.

## Plan

- Boot to a games list on HDMI; play with a USB gamepad, the TV remote (HDMI-CEC) or an IR remote.
- Games live in a `games` folder on the SD card that any computer can copy files into; Wi-Fi downloads come later.
- PC speaker sound first, then digital sound (VOC) and FM music.
- Reuse the game player core from [Game-Maker: Sandbox Edition](https://github.com/lewismoten/GameMaker-SandboxEdition) instead of forking it.
- Later: the e-ink display as a desk buddy (games played, badges, the game in play), a private game server for the home network, and other Pi models from the same SD card.

## License

MIT. See [LICENSE](LICENSE).
