#!/bin/sh
# x11-smoke.sh — starts the AppImage on a headless X server (Cosmoe's X11 backend), checks it
# stays up, and saves a screenshot and the app log.
#
#   sh packaging/appimage/x11-smoke.sh <App>-<version>-<arch>.AppImage [OUT_DIR]
#
# Uses Xvfb with Openbox (without a window manager Cosmoe windows get no focus) and
# ImageMagick's import for the screenshot. Ubuntu: apt-get install --no-install-recommends
# xvfb openbox imagemagick. Generic beos-app-platforms template: appimage.yml runs it after
# building the AppImage, on every architecture, so the aarch64 build is started on real
# arm64 hardware too.
set -u
APP=$(readlink -f "$1")
OUT=$(mkdir -p "${2:-x11-smoke}" && cd "${2:-x11-smoke}" && pwd)
DPY=:77
Xvfb $DPY -screen 0 1280x800x24 -nolisten tcp > "$OUT/xvfb.log" 2>&1 &
XPID=$!
for i in $(seq 40); do [ -e /tmp/.X11-unix/X77 ] && break; sleep 0.25; done
[ -e /tmp/.X11-unix/X77 ] || { echo "x11-smoke: Xvfb did not start"; cat "$OUT/xvfb.log"; exit 1; }
DISPLAY=$DPY openbox > "$OUT/wm.log" 2>&1 &
WM=$!
sleep 1

HOME_DIR=$(mktemp -d)
env -u WAYLAND_DISPLAY DISPLAY=$DPY COSMOE_BACKEND=x11 APPIMAGE_EXTRACT_AND_RUN=1 \
	HOME="$HOME_DIR" "$APP" > "$OUT/app.log" 2>&1 &
PID=$!
sleep 15
if ! kill -0 $PID 2>/dev/null; then
	wait $PID; status=$?
	echo "x11-smoke: the app exited with status $status"; cat "$OUT/app.log"
	kill $WM $XPID 2>/dev/null; exit 1
fi
echo "x11-smoke: the app is running under X11 ($(uname -m))"
DISPLAY=$DPY timeout 30 import -window root "$OUT/x11-screenshot.png" || echo "x11-smoke: screenshot failed"
kill $PID 2>/dev/null; sleep 1; kill $WM $XPID 2>/dev/null
ls -l "$OUT"
if grep -qiE 'error|assert|crash|segment|abort|exception' "$OUT/app.log"; then
	echo "x11-smoke: problems in the app log:"; grep -iE 'error|assert|crash|segment|abort|exception' "$OUT/app.log"
	exit 1
fi
echo "x11-smoke: PASS"
