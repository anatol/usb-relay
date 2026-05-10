#!/usr/bin/env bash
set -euo pipefail

RULE=99-usb-relay.rules
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

sudo cp "$SRC_DIR/$RULE" "/etc/udev/rules.d/$RULE"
sudo udevadm control --reload-rules
sudo udevadm trigger

echo "Installed /etc/udev/rules.d/$RULE"
