#!/usr/bin/env bash
# load_driver.sh - (re)load the heart-rate module and create /dev/heart_rate.
# Run this after every reboot / "wsl --shutdown", because kernel modules and
# device nodes do not survive a restart.
#
# Usage:  ./load_driver.sh            (load)
#         ./load_driver.sh unload     (remove module and device node)
set -euo pipefail
cd "$(dirname "$0")"

if [[ "${1:-}" == "unload" ]]; then
    sudo rm -f /dev/heart_rate
    sudo rmmod heart_rate_driver 2>/dev/null || true
    echo "Unloaded."
    exit 0
fi

[[ -f heart_rate_driver.ko ]] || make

# Start from a clean state so re-running the script is always safe.
sudo rmmod heart_rate_driver 2>/dev/null || true
sudo rm -f /dev/heart_rate

sudo insmod heart_rate_driver.ko

# Ask the kernel which major number it gave us, then create the node.
MAJOR=$(awk '$2=="heart_rate" {print $1}' /proc/devices)
sudo mknod /dev/heart_rate c "$MAJOR" 0
sudo chmod 666 /dev/heart_rate

echo "Loaded: /dev/heart_rate (major $MAJOR). Test with:  cat /dev/heart_rate"
