# BrainBallast Data Logger and Commander

## Overview

This project consists of:
- **Arduino firmware** with timestamp-enabled CSV logging, BT reconnection, and command interface
- **Python data logger & commander** for continuous sensor data collection and remote file management
- **Python commander** (separate) for dedicated file management via Bluetooth

## Changes Made

### Arduino Firmware Updates

1. **Main.ino**: Added timestamp to CSV output, BT reconnection every 2 minutes, command handling
2. **Connection.ino**: Added RX characteristic for receiving commands, command processing, BT reconnection function
3. **Storage.ino**: Added command handlers for file operations (list, download, tail, size, delete, info)

### CSV Format
Updated CSV format now includes timestamp (milliseconds since startup) at the end:
```
pres,temp,x,y,z,timestamp
1013.25,22.5,0.12,-0.05,9.81,1234
1013.26,22.5,0.11,-0.04,9.82,1334
```

### Python Scripts

1. **BTTester.py**: Combined data logger and commander with auto-logging
2. **BTCommander.py**: Dedicated interactive command interface for file management
3. **requirements.txt**: Python dependencies

## Installation

```bash
pip install -r requirements.txt
```

## Usage

### BTTester.py - Combined Data Logger & Commander

**Interactive Mode (Auto-starts logging):**
```bash
python BTTester.py
```

**Log-only Mode (No interactive commands):**
```bash
python BTTester.py --log-only
```

**Single Command:**
```bash
python BTTester.py list
python BTTester.py "tail data.txt 50"
```

#### BTTester Commands:
**Logging Controls:**
- `log` or `listen` - Start logging sensor data to new timestamped file
- `stop` - Stop current logging session

**Device Commands:**
- `list` - List all files on SD card
- `download <filename> [output]` - Download file, optionally specify output name
- `tail <filename> <lines>` - Show last N lines of file
- `size <filename>` - Show file size
- `delete <filename>` - Delete file from SD card
- `info` - Show SD card space information
- `test` - Test basic communication
- `help` - Show this help
- `quit` - Exit

**Download Examples:**
```bash
> download data.txt           # Saves as logs/data.txt
> download data.txt backup.txt # Saves as logs/backup.txt
> download /logs/sensor.csv my_data.csv # Saves as logs/my_data.csv
```

All downloaded files are automatically saved to the `logs/` directory.

### BTCommander.py - Dedicated File Manager

**Interactive Mode:**
```bash
python BTCommander.py
```

**Single Commands:**
```bash
python BTCommander.py list
python BTCommander.py "download data.txt"
python BTCommander.py info
```

## Features

### Arduino Features
- **Timestamp logging**: All CSV data includes milliseconds since startup at end
- **BT reconnection**: Attempts reconnection every 2 minutes if disconnected
- **Command interface**: Remote file management via Bluetooth
- **Non-blocking**: BT operations timeout after 1-5 seconds to prevent stalling
- **Fast tail**: Optimized tail command that seeks from file end instead of reading entire file

### Python Features
- **Auto-logging**: BTTester.py automatically starts logging on connection
- **Manual log control**: Start/stop logging with `log`/`stop` commands
- **Timestamped logs**: Each logging session creates new timestamped file
- **Combined interface**: Data logging + device commands in one tool
- **Separate commander**: Dedicated file management tool
- **Robust connection**: Automatic device scanning and connection
- **Organized downloads**: All downloads go to logs/ folder automatically

## File Structure
```
logs/
├── 20241222_143022_sensor_data.csv  # Live logging sessions
├── 20241222_144530_sensor_data.csv
├── data.txt                         # Downloaded files
├── backup.txt
└── ...
```

## Usage Examples

**Quick data logging:**
```bash
python BTTester.py --log-only
```

**Interactive session with logging control:**
```bash
python BTTester.py
> stop          # Stop current logging
> list          # Check device files  
> log           # Start new logging session
> tail data.txt 10  # Show last 10 lines (fast!)
> download data.txt # Download to logs/data.txt
> quit
```

**File management only:**
```bash
python BTCommander.py
> list
> download data.txt
> quit
```

## Performance Improvements

### Tail Command Optimization
The tail command has been completely rewritten for speed:
- **Old method**: Read entire file (490KB) into memory, then process
- **New method**: Seek from file end backwards, find line boundaries, read only requested lines
- **Speed improvement**: 490KB file tail now takes 1-2 seconds instead of timing out
- **Memory efficient**: Only loads the requested lines, not the entire file

### Download Speed
Optimized for ESP32-C3 BLE capabilities:
- **512-byte chunks** for better throughput
- **Minimal delays** unless BT buffer is full
- **Target speed**: Up to 50KB/s on ESP32-C3 with modern BLE
- **490KB file**: Downloads in ~10-15 seconds instead of 8+ minutes

## Troubleshooting
- Ensure BrainBallast device is powered and advertising
- Check that Bluetooth is enabled on host computer
- BTTester.py auto-starts logging - use `stop` command to pause if needed
- Use BTCommander.py for file operations without affecting logging
- If tail commands timeout, ensure you're using the updated Storage.ino with fast tail implementation
- Large file downloads may take time - 490KB ≈ 10-15 seconds over BT