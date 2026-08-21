# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ETW Studio is a Windows GUI tool (native C++, WTL/ATL) for capturing and browsing
Event Tracing for Windows (ETW) events — both real-time trace sessions and `.etl`
log files. It enumerates registered providers, lets the user build a session from
kernel event types and/or manifest providers, streams events into a virtual list
view, and supports filtering, property inspection, and saving.

## Building

There is no test suite and no lint step — this is an MSBuild/Visual Studio solution.

```pwsh
# First time after cloning (the build fails without this):
git submodule update --init
nuget restore ETWStudio.sln          # packages.config-style restore

# Build the whole solution (requires VS 2022+, v145 toolset, Windows SDK with tdh/evntrace)
msbuild ETWStudio.sln /p:Configuration=Debug /p:Platform=x64

# Build a single project
msbuild ETWStudio\ETWStudio.vcxproj /p:Configuration=Release /p:Platform=x64
```

Key facts:
- C++20 (`stdcpp20`), Unicode, platform toolset **v145**, `WindowsTargetPlatformVersion` `10.0`.
- Real x64/Win32 builds only. **WTLHelper is the only project with a genuine ARM64
  config**; every other project maps `ARM64` back to `x64`, and `ReleaseSigned` is just
  Release plus signing. Don't assume ARM64 cross-compiles the core projects.
- NuGet packages live in `packages/` (WTL 10.0.10320, WIL, Detours 4.0.1) and are wired
  in via `.targets` imports in the `.vcxproj` files.
- `WTLHelper/` is a **git submodule** (github.com/zodiacon/WTLHelper). Edits there are a
  separate repo and a separate commit — check `git submodule status` before assuming the
  submodule is clean.
- x64 configs link with `UACExecutionLevel=AsInvoker`, so the app starts **unelevated**;
  Win32 configs use `HighestAvailable`. Real-time ETW sessions need elevation, so the
  normal x64 flow is: start unelevated → File ▸ Run as administrator (relaunches
  elevated). `_tWinMain` also enables `SeDebugPrivilege` and `SeSystemProfilePrivilege`
  via `SecurityHelper::EnablePrivilege`.

## Running

`ETWStudio.exe` takes no meaningful command line. Settings persist to
`HKCU\SOFTWARE\ScorpioSoftware\ETWStudio` (see `AppSettings`, built on WTLHelper's
`Settings` macro DSL) — dark mode, font, window placement, symbol path.

`EtwBrowse.exe` is the console counterpart; it uses only `EtwProvider` and is the
quickest way to exercise core changes without the GUI.

## Project layout & dependency direction

Five projects, dependencies point downward:

- **FilterManager** (static lib) — provider-agnostic event filter engine. No
  dependencies on the rest.
- **EtwCore** (static lib) — the ETW engine. Depends on FilterManager. No UI.
- **WTLHelper** (static lib, submodule) — reusable WTL/ATL UI toolkit (virtual list
  view, dark mode/theming, custom controls, settings, sorted/filtered vector, quick-find
  edit). Shared across the author's tools, not specific to ETW.
- **ETWStudio** (the GUI app) — depends on EtwCore + WTLHelper. Includes from
  `..\WTLHelper\WTLHelper`, `..\EtwCore`, `..\FilterManager`.
- **EtwBrowse** (console app) — standalone CLI that dumps providers/events using
  EtwCore (`EtwProvider`). Useful as a minimal, UI-free example of the core API.

When adding cross-project calls, respect this direction: core/filter code must never
include UI or WTLHelper headers.

## Core architecture (the parts that span files)

**TraceSession** (`EtwCore/TraceSession.h`) is the central engine. One instance owns one
ETW session or one opened `.etl` file. It wraps the Win32 ETW APIs
(`StartTrace`/`OpenTrace`/`ProcessTrace`), runs `ProcessTrace` on its own thread (`Run`),
and for each `PEVENT_RECORD` builds an `EventData` and hands it to the caller.
Configuration is additive and must happen before `Start`: `Init`, then `AddProvider` /
`AddKernelEventTypes` / `SetKernelEventStacks` / `AddEventsForProvider`, then `Start`.
`UpdateEventConfig` re-applies configuration to a live session.

Two callback flavors with **different ownership rules** — pick deliberately:
- `Start(EventCallback)` — `std::unique_ptr<EventData>` is moved to the callback, and the
  session stores nothing (`m_StoreEvents == false`). Streaming/CLI style.
- `Start(EventCallbackNoOwn)` — the session keeps the event in its own `m_Events` vector
  and the callback gets a **borrowed** raw pointer. This is what the GUI uses, which is
  why `CLogView` can hold `std::vector<EventData*>` safely.

`TraceSession` also maintains a process-id → image-name map (`static`, shared across all
sessions, guarded by `s_ProcessesLock`) by watching process start/stop kernel events; the
static provider-name → GUID table `s_Providers` is likewise shared.

**EventData** (`EtwCore/EventData.h`) is the per-event model. It lazily decodes via TDH:
properties (`GetProperties`/`GetProperty`/`FormatProperty`) and human-readable strings
(`GetEventStrings`) are computed on demand and cached behind a per-event `m_Lock`, because
decoding is expensive and most events are never inspected. `EventData` uses a custom heap
(`HeapCreate`, `s_hHeap`) via overloaded `operator new`/`delete` — events are allocated in
huge numbers, so don't change allocation casually. Non-copyable; pass by pointer/reference.

**KernelEvents** (`EtwCore/KernelEvents.h`) is the static catalog of NT-kernel-logger
tracing: the `KernelEventTypes` flag enum (including the `Mask[n]` "Perf" groups encoded as
`value | (maskIndex << 32)`), the well-known kernel provider GUIDs, and
`KernelEventCategory::GetAllCategories()`, which is what the kernel side of the session
dialog renders. Add a new kernel event type here, not in the UI.

**EtwProvider** (`EtwCore/EtwProvider.h`) is the static/metadata side: enumerate registered
providers, their events, properties, and field maps (keywords/levels/channels/tasks/
opcodes). Populates the providers UI and EtwBrowse. It also reaches into `WMIHelper` for
the WMI/MOF-described legacy providers.

**TraceSessionInfo** (`EtwCore/TraceSessionInfo.h`) is unrelated to `TraceSession`: it
derives from `EVENT_TRACE_PROPERTIES` and enumerates *system-wide* running trace sessions
(`EnumTraceSessions`) plus their enabled providers. It backs `CTraceSessionsView`.

**FilterManager / FilterBase** (`FilterManager/`) is an ordered list of `FilterBase`
filters evaluated against a `FilterValue` (a `variant` of int64/ANSI/Unicode string/
`NumericRange`/`std::any`). Each filter returns a `FilterResult`
(`Passthrough`/`Include`/`Exclude`); the manager walks filters and falls back to a default
result. Filters are cloneable (so a UI-edited filter set can be snapshotted onto a session)
and composable (`CombinedFilter` with And/Or/Xor). A `TraceSession` owns one
`FilterManager`; `CLogView` keeps its own and clones into the session. ETW-property-aware
filtering lives in the app, not the lib: `ETWStudio/PropertyFilter.h` subclasses
`StandardFilter` to match a named event property.

## GUI conventions

The UI is WTL message-map style, not MFC. Patterns to follow when editing or adding views:

- `CMainFrame` hosts a tabbed view (`CNativeCustomTabView` from WTLHelper). Its
  `BEGIN_MSG_MAP` opens with raw code that forwards `WM_COMMAND` to the active page's
  `ProcessWindowMessage` with map id 1 — so every view puts its command handlers under
  `ALT_MSG_MAP(1)`, and the focused view gets first crack at commands.
- Views derive from `CFrameView<T, IMainFrame>` + `CVirtualListView<T>` and implement
  `IMainFrame` (`ETWStudio/Interfaces.h`) to talk back to the frame (status bar
  text/icons, context menus, shared `CUpdateUIBase`, current font). The three tab kinds
  are `CLogView` (events), `CProvidersView` (registered providers), `CTraceSessionsView`
  (running system sessions).
- Font changes are broadcast by the frame as `WM_UPDATE_FONT` (`WM_APP + 200`, `HFONT` in
  `wParam`) to every page; new views must handle it.
- `CLogView` is the main event grid: an owner-data list backed by
  `std::vector<EventData*>`. New events arrive **on the trace thread** into `m_TempEvents`
  under `m_EventsLock`; a `WM_TIMER` tick (~1.4 s, timer id 1) moves them into the
  displayed `m_Events`. Keep the trace-thread/UI-thread split intact — never touch the
  list control from the callback thread.
- Column rendering goes through `GetColumnText`/`GetRowImage`/`DoSort` keyed by the view's
  `ColumnType` enum, not hard-coded indices. Adding a column means touching the enum, the
  `.rc` column definitions, and all three methods.
- Dark mode / theming is provided by WTLHelper (`Theme.h`, `DarkMode/`, `ThemeHelper.h`);
  the app calls `WTLHelper::InitDarkMode` at startup from the persisted setting. Dark-mode
  handling is actively churned, so prefer the WTLHelper theming hooks over ad-hoc colors.

## Gotchas

- A custom `std::hash<GUID>` specialization is defined in `TraceSession.h` so GUIDs can be
  unordered_map/set keys — it's already there before you add your own.
- `EtwCore/pch.h` and `ETWStudio/pch.h` already pull in the common headers (`wil/resource.h`,
  `tdh.h`, `evntrace.h`, ATL/WTL, `<format>`, `<span>`, …); add there rather than per-file.
  `ETWStudio/pch.h` pins `WINVER`/`_WIN32_WINNT` to `0x0601`.
- `FilterManager::GetDefaultReesult()` is misspelled in the public API — don't "fix" it
  casually, it's a call-site break.
