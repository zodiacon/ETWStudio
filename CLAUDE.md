# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ETW Studio is a Windows GUI tool (native C++, WTL/ATL) for capturing and browsing
Event Tracing for Windows (ETW) events — both real-time trace sessions and `.etl`
log files. It enumerates registered providers, lets the user build a session from
kernel event types and/or manifest providers, streams events into a virtual list
view, and supports filtering, property inspection, and saving.

## Building

There is no command-line test suite — this is an MSBuild/Visual Studio solution.

```pwsh
# Build the whole solution (requires VS 2022+, v145 toolset, Windows SDK with tdh/evntrace)
msbuild ETWStudio.sln /p:Configuration=Debug /p:Platform=x64

# Build a single project
msbuild ETWStudio\ETWStudio.vcxproj /p:Configuration=Release /p:Platform=x64
```

Key facts:
- C++20 (`stdcpp20`), Unicode, platform toolset **v145**.
- Real x64/Win32 builds only. The `ARM64` and `ReleaseSigned` solution configs map
  most projects' active config back to x64/Win32 — don't assume ARM64 actually
  cross-compiles the core projects.
- NuGet packages live in `packages/` (WTL 10.0.10320, Detours 4.0.1) and are wired in
  via `.targets` imports in the `.vcxproj` files — restore before first build.
- `WTLHelper/` is a **git submodule** (github.com/zodiacon/WTLHelper). Run
  `git submodule update --init` after cloning or the build will fail.
- Running ETW real-time sessions requires the app to run **elevated** (and
  `SeDebugPrivilege` / profiling privileges); see `SecurityHelper::EnablePrivilege`.

## Project layout & dependency direction

Five projects, dependencies point downward:

- **FilterManager** (static lib) — provider-agnostic event filter engine. No
  dependencies on the rest.
- **EtwCore** (static lib) — the ETW engine. Depends on FilterManager. No UI.
- **WTLHelper** (static lib, submodule) — reusable WTL/ATL UI toolkit (virtual list
  view, dark mode/theming, custom controls, settings, hex control). Shared across the
  author's tools, not specific to ETW.
- **ETWStudio** (the GUI app) — depends on EtwCore + WTLHelper. Includes from
  `..\WTLHelper\WTLHelper`, `..\EtwCore`, `..\FilterManager`.
- **EtwBrowse** (console app) — standalone CLI that dumps providers/events using
  EtwCore (`EtwProvider`). Useful as a minimal, UI-free example of the core API.

When adding cross-project calls, respect this direction: core/filter code must never
include UI or WTLHelper headers.

## Core architecture (the parts that span files)

**TraceSession** (`EtwCore/TraceSession.h`) is the central engine. One instance owns
one ETW session or one opened `.etl` file. It wraps the Win32 ETW APIs
(`StartTrace`/`OpenTrace`/`ProcessTrace`), runs `ProcessTrace` on its own thread
(`Run`), and for each `PEVENT_RECORD` builds an `EventData` and hands it to the
caller via an `EventCallback` (owning) or `EventCallbackNoOwn` (borrowed pointer).
It also maintains a process-id → image-name map (static, shared, lock-guarded) by
watching process start/stop kernel events. Configuration is additive: `AddProvider`,
`AddKernelEventTypes`, `SetKernelEventStacks`, `AddEventsForProvider`, then `Start`.

**EventData** (`EtwCore/EventData.h`) is the per-event model. It lazily decodes via
TDH: properties (`GetProperties`/`GetProperty`/`FormatProperty`) and human-readable
strings (`GetEventStrings`) are computed on demand and cached behind a per-event
mutex, because decoding is expensive and most events are never inspected. `EventData`
uses a custom heap (`HeapCreate`) via overloaded `operator new`/`delete` — events are
allocated in huge numbers, so don't change allocation casually.

**EtwProvider** (`EtwCore/EtwProvider.h`) is the static/metadata side: enumerate
registered providers, their events, properties, and field maps (keywords/levels/
channels/tasks/opcodes). This is what populates the providers UI and EtwBrowse.

**FilterManager / FilterBase** (`FilterManager/`) is an ordered list of `FilterBase`
filters evaluated against a `FilterValue`. Each filter returns a `FilterResult`
(`Passthrough` / `Include` / `Exclude`); the manager walks filters and falls back to
a default result. Filters are cloneable (so a UI-edited filter set can be snapshotted
onto a session) and composable (`CombinedFilter` with And/Or/Xor). A `TraceSession`
owns one `FilterManager`; the GUI keeps its own and clones into the session.

## GUI conventions

The UI is WTL message-map style, not MFC. Patterns to follow when editing or adding views:

- `CMainFrame` hosts a tabbed view (`CNativeCustomTabView` from WTLHelper). Each tab
  is a view object; `CMainFrame::BEGIN_MSG_MAP` forwards `WM_COMMAND` to the active
  page's `ProcessWindowMessage` so the focused view handles commands first.
- Views implement `IMainFrame` (`ETWStudio/Interfaces.h`) to talk back to the frame
  (status bar text/icons, context menus, shared `CUpdateUIBase`).
- `CLogView` is the main event grid: a `CVirtualListView<CLogView>` (owner-data list)
  backed by `std::vector<EventData*>`. New events arrive on the trace thread into
  `m_TempEvents` under `m_EventsLock`, then a `WM_TIMER` tick moves them into the
  displayed `m_Events` — keep the trace-thread and UI-thread split intact; never touch
  the list control from the callback thread.
- Column rendering goes through `GetColumnText`/`GetRowImage`/`DoSort` keyed by the
  view's `ColumnType` enum, not hard-coded indices.
- Dark mode / theming is provided by WTLHelper (`Theme.h`, `DarkMode/`,
  `ThemeHelper.h`); recent commit history shows dark-mode handling is actively churned,
  so prefer the WTLHelper theming hooks over ad-hoc color code.

## Gotchas

- A custom `std::hash<GUID>` specialization is defined in `TraceSession.h` so GUIDs can
  be unordered_map/set keys — be aware it's already there before adding your own.
- `EventData` is non-copyable and heap-custom; pass by pointer/reference.
- Editing WTLHelper means editing the submodule — changes there are a separate repo/commit.
