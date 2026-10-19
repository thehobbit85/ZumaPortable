#!/usr/bin/env sh
# Launcher for Zuma Deluxe Portable.
#
# Game data is never packaged: look for it in the user's data directory first,
# then fall back to a system-wide copy.

# Prefer Wayland when the session is Wayland and no driver was forced.
if [ -n "$WAYLAND_DISPLAY" ] && [ -z "$SDL_VIDEODRIVER" ]; then
    export SDL_VIDEODRIVER="wayland,x11"
fi

GAME_DATA="${XDG_DATA_HOME:-$HOME/.local/share}/zuma-portable"
if [ -f "$GAME_DATA/properties/resources.xml" ]; then
    RESDIR="$GAME_DATA"
elif [ -f /usr/share/zuma-portable/properties/resources.xml ]; then
    RESDIR=/usr/share/zuma-portable
else
    RESDIR="$GAME_DATA"
fi

exec /usr/share/zuma-portable/zuma-portable -resdir "$RESDIR" "$@"
