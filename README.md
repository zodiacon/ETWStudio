# ETW Studio

A Windows GUI tool for capturing and browsing [Event Tracing for Windows (ETW)](https://learn.microsoft.com/windows/win32/etw/about-event-tracing) events, live from trace sessions or from `.etl` log files.

## Features

- **Live trace sessions** from any mix of manifest, MOF and TraceLogging providers and NT kernel event types, with per-provider settings:
  - level, keywords (picked from the provider's manifest), event IDs
  - call stacks (symbolized in the properties view)
- **Log files**: open `.etl` files, pause and resume reading them.
- **Optional `.etl` output** for a live session while still displaying events.
- **Session presets**: save a session definition to a `.etwsession` file and load it later; *File ▸ Save Session* saves the active session.
- **Session summary** before starting: everything the session will enable, plus warnings (not elevated, name in use, file will be overwritten, ...).
- **Buffer settings** (buffer size, minimum/maximum buffers, flush timer).
- **Event grid**: a virtual list that handles millions of events, with sortable columns, a property summary column and a properties view per event (all properties, arrays and structs included, plus the call stack).
- **Filters** on standard fields (process, thread, event ID, level, provider, ...) or any event property; include/exclude rules, saved to `.etwfilter` files, optionally re-applied to events already captured.
- **Export** to CSV (the displayed columns) or JSON (all event data, properties and stacks).
- **Registered providers** view, and a view of the trace sessions running on the system.
- Dark mode.

## Requirements

- Windows 10 or later, x64.
- Live trace sessions need administrator rights: start ETW Studio elevated, or use *File ▸ Run as Administrator*. Opening `.etl` files does not.
- Call stacks are symbolized with DbgHelp. For function names, set a symbol path in *Options ▸ Symbol Path* (for example `srv*C:\Symbols*https://msdl.microsoft.com/download/symbols`; empty uses `_NT_SYMBOL_PATH`). Downloading symbols requires `symsrv.dll` next to `dbghelp.dll`.

## Building

Visual Studio 2022 or later with the C++ desktop workload (toolset v145) and a Windows 10/11 SDK.

```pwsh
git clone --recursive https://github.com/zodiacon/ETWStudio.git
cd ETWStudio
nuget restore ETWStudio.sln
msbuild ETWStudio.sln /p:Configuration=Release /p:Platform=x64
```

If the repository was cloned without `--recursive`, run `git submodule update --init` first: [WTLHelper](https://github.com/zodiacon/WTLHelper) is a submodule.

## Projects

| Project | |
|---|---|
| `ETWStudio` | The GUI application (WTL) |
| `EtwCore` | The ETW engine: sessions, log files, event decoding (TDH), provider metadata |
| `FilterManager` | Event filter engine |
| `EtwBrowse` | Console tool that lists providers and their events using `EtwCore` |
| `WTLHelper` | Reusable WTL UI toolkit (submodule) |

## License

[MIT](LICENSE.txt)
