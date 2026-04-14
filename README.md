# Statsgate

Super work in progress, only use this if you know what you're doing!

Currently supports freestanding mode where you can collect stats as a player without any map implementation.
In the future it will support hosted mode where a lua or dll mission can implement stats directly.

Simply run statslauncher.exe with BZCC open and it will start the stat client. Use the `stats` command
in the console for a list of commands. The stat sessions will be saved to the `statsgate` folder in mydocs.

Note that if you want to compile protobuf headers from the schema you must use the protoc.exe that is installed
in the vcpkg folder in this project in order for it to be compatible with the stat client.

The name is a VSR joke from when players stopped playing out of fear of their winrate
decreasing after F9bomber started publishing stats.
