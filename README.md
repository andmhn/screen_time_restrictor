# Screen Time Restrictor

![Windows](https://img.shields.io/badge/Platform-Windows-blue)
![C](https://img.shields.io/badge/Language-C-green)

A Windows application that enforces daily screen time limits by locking the workstation when the configured usage time is reached. Ideal for productivity, parental controls, or self-discipline.

## Features

- ⏳ **Daily Time Tracking**: Resets usage counter at midnight
- ⚙️ **Configurable Limits**: Set daily time limits, warning thresholds, and idle detection
- 🕒 **Grace Period**: Allows emergency usage after limit is reached
- ⏰ **Warnings**: Notifies user as limit approaches
- 🔒 **Auto-Lock**: Locks workstation when time expires
- 🚫 **Single Instance**: Ensures only one copy runs at a time

## How It Works

The program:
1. Tracks active computer usage time (ignores idle periods)
2. Saves daily usage to a data file
3. Shows warnings when approaching the limit
4. Grants a configurable grace period when first exceeding limits
5. Locks the workstation when the grace period expires

## Installation

1. Download the latest release
2. Place `screen_time_restrictor.exe` in your preferred directory
3. A `config.ini` file will be automatically created on first run

## Configuration

Edit `config.ini` to customize:

```ini
[Settings]
TimeLimitSeconds=3600       # Daily limit in seconds (1 hour)
WarningTimeSeconds=300      # Warn when this many seconds remain (5 mins)
LoginGracePeriodSeconds=600 #  Emergency usage time after limit (10 mins)
```

## Building from Source

Requirements:
- Windows SDK
- MinGW or Visual Studio

Compile with:
```bash
gcc .\screen_time_restrictor.c  -o screen_time_restrictor.exe -luser32 -lshlwapi -lshell32 -mwindows
```

## Usage

Simply run the executable. It will:
- run in background
- Show warnings as time runs out
- Auto-lock when time expires

To make it run at startup:
1. Create a shortcut to the executable
2. Place it in your Startup folder (`shell:startup`)


## Contributing

Pull requests are welcome! For major changes, please open an issue first.

---

**Note**: This is not a sandbox or child-proof solution. Tech-savvy users may bypass it. Best for personal accountability use.