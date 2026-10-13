#!/bin/bash
# Installs Qt 6 build deps so CMake can build JustAnimate and run pencil2d_tests in cloud sessions.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

PKGS="qt6-base-dev qt6-multimedia-dev qt6-svg-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools libgl-dev ninja-build ccache"
missing=""
for p in $PKGS; do
  dpkg -s "$p" >/dev/null 2>&1 || missing="$missing $p"
done
if [ -n "$missing" ]; then
  SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"
  $SUDO apt-get update -qq
  DEBIAN_FRONTEND=noninteractive $SUDO apt-get install -y -qq --no-install-recommends $missing
fi

# Tests and the app run headless in the container.
echo 'export QT_QPA_PLATFORM=offscreen' >> "$CLAUDE_ENV_FILE"
echo 'export CMAKE_GENERATOR=Ninja' >> "$CLAUDE_ENV_FILE"
echo 'export CMAKE_CXX_COMPILER_LAUNCHER=ccache' >> "$CLAUDE_ENV_FILE"
