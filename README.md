# Reverso Linux

A Linux desktop translator using the [Reverso](https://www.reverso.net) translation API. Select text, press a hotkey, get translations with usage examples.

## Features

- **Auto-detect source language** — just select text, no need to specify source
- **Persistent target language** — saved to `~/.config/reverso-linux/config`
- **Translation options** for short text (1–3 words) with frequency counts, e.g. `бежать [6074]`
- **Usage examples** — click a translation option to see sentence examples side by side
- **16 supported languages**: english, russian, ukrainian, french, german, spanish, italian, portuguese, polish, dutch, arabic, hebrew, japanese, turkish, chinese, romanian

## Dependencies

- `libcurl` — HTTP requests
- `json-c` — JSON parsing
- `gtk3` — GUI
- `pkg-config` — build flags
- `wl-clipboard` (optional, for Wayland clipboard integration)

## Build & Install

```sh
make
sudo make install
```

To override the version (printed by `-v`):

```sh
make VERSION=1.0.1
sudo make install
```

## Usage

### CLI

```sh
reverso-linux hello world              # translate from stdin
wl-paste -p | reverso-linux           # pipe from clipboard
reverso-linux -t french hello world   # set target language
reverso-linux -h                      # help
reverso-linux -v                      # version
```

### Compositor keybinding (Hyprland)

Add to `~/.config/hypr/hyprland.conf`:

```
bind = ALT, R, exec, wl-paste -p | reverso-linux
```

Press **Alt+R** to translate the current clipboard selection.

### Changing target language

Use the dropdown in the GUI, or set it from the command line:

```sh
reverso-linux -t french
```

The target persists across sessions.
