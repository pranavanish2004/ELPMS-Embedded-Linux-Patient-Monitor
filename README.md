# ELPMS - Embedded Linux Patient Monitor System

Software-simulated patient monitor: kernel driver -> SQLite logger -> Qt dashboard.

```
elpms/
  driver/     heart_rate_driver.c, Makefile, load_driver.sh   (Phase 1, C kernel module)
  logger/     logger.cpp                                       (Phase 2, C++ daemon + SQLite)
  dashboard/  main.cpp, mainwindow.h, mainwindow.cpp, dashboard.pro   (Phase 3, Qt)
```
(`dashboard.pro` is the Qt build file and `load_driver.sh` is a helper; they are not counted among the 6 source files.)

## 0. Install packages (Ubuntu on WSL2)

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential gcc g++ make git sqlite3 libsqlite3-dev \
    qtcreator qtbase5-dev qt5-qmake libqt5sql5-sqlite \
    flex bison libssl-dev libelf-dev bc dwarves cpio python3
```

## 1. One-time: kernel headers for WSL2

```bash
uname -r                                  # e.g. 6.6.87.2-microsoft-standard-WSL2
cd ~
git clone --depth 1 -b linux-msft-wsl-$(uname -r | cut -d- -f1) \
    https://github.com/microsoft/WSL2-Linux-Kernel.git
cd WSL2-Linux-Kernel
cp Microsoft/config-wsl .config           # or: zcat /proc/config.gz > .config
make olddefconfig LOCALVERSION=
make modules_prepare LOCALVERSION= -j$(nproc)
```
`LOCALVERSION=` matters: without it a git clone names the kernel `...WSL2+`
(with a plus), and `insmod` then fails with "Invalid module format".

## 2. Phase 1 - driver

```bash
cd ~/elpms/driver
make
./load_driver.sh          # insmod + mknod + chmod (rerun after every wsl --shutdown)
sudo dmesg | tail -n 5
cat /dev/heart_rate       # prints a number between 60 and 100
```
Unload: `./load_driver.sh unload`

## 3. Phase 2 - logger

```bash
cd ~/elpms/logger
g++ -std=c++17 -Wall -Wextra -O2 logger.cpp -o logger -lsqlite3
cd ~/elpms
./logger/logger                      # foreground, Ctrl+C stops it
./logger/logger --daemon             # background; log in patient_records.db.log
sqlite3 patient_records.db "SELECT * FROM readings ORDER BY id DESC LIMIT 5;"
pkill logger                         # stop the daemon
```
No driver loaded? Test with a fake device: `mkfifo /tmp/fake; while true; do echo $((60+RANDOM%41)) > /tmp/fake; done &`
then `./logger/logger --device /tmp/fake`.

## 4. Phase 3 - dashboard

```bash
cd ~/elpms/dashboard
qmake dashboard.pro && make
cd ~/elpms
./dashboard/elpms_dashboard patient_records.db
```
WSL2 on Windows 11 (WSLg) shows the window automatically. Or open `dashboard.pro` in Qt Creator.
Keep the logger running, then click **Refresh Patient Vitals**.
