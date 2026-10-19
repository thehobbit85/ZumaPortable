# Arch Linux packaging

Builds the engine and installs it under `/usr/share/zuma-portable`, with a launcher at
`/usr/bin/zuma-portable`. **No game assets are packaged** — see the legal note below.

## Legal note

This package contains **engine code only**. You must own a legal copy of *Zuma Deluxe*
(Steam app 37400, or the EA release) to play. Nothing in this directory redistributes
PopCap/EA files.

## Install from the AUR

```bash
yay -S zuma-portable
```

## Build from a local checkout

```bash
cd archlinux
makepkg -si
```

## Provide your game data

The launcher looks for `properties/resources.xml` in this order:

1. `~/.local/share/zuma-portable/`
2. `/usr/share/zuma-portable/`

Copy the six folders from your Zuma Deluxe installation into the first path:

```bash
mkdir -p ~/.local/share/zuma-portable
cp -r "/path/to/Zuma Deluxe"/{properties,images,sounds,levels,music,fonts} \
      ~/.local/share/zuma-portable/
zuma-portable
```

On Steam the game folder is reachable via **Zuma Deluxe → Manage → Browse local files**.

## Notes

- The engine keeps the original 4:3 aspect ratio, so widescreen displays get pillarboxing.
- Wayland works natively; the launcher falls back to X11 when `WAYLAND_DISPLAY` is unset.
- Save data lives under the per-user application data directory; pass `-savedir` to override.
