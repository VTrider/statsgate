# Statsgate

Super work in progress, only use this if you know what you're doing!

Currently supports freestanding mode where you can collect stats as a player without any map implementation.
In the future it will support hosted mode where a lua or dll mission can implement stats directly.

## Setup

Things you need:
- Visual studio with latest build tools
- vcpkg
- Python maybe
- [Extra Utilities 2](https://steamcommunity.com/sharedfiles/filedetails/?id=3515140097) (the workshop item just needs to be installed)
- BZCC Version >= 2.0.203

Simply run statslauncher.exe with BZCC open and it will start the stat client. Use the `stats` command
in the console for a list of commands. The stat sessions will be saved to the `statsgate` folder in mydocs.

Make sure to run the powershell script to compile new protobuf headers if the schema changes.

## Contributing Stats

Just make a folder in the sessions folder with your known nick and add stat files to it, feel free to send a PR!

The name is a VSR joke from when players stopped playing out of fear of their winrate
decreasing after F9bomber started publishing stats.
