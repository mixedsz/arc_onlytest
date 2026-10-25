Syn Arc — project map (for another agent)

Live source: this workspace (arc-um.sln). Sidecar backups (if present) are not the working tree.

1\. What this repo is

MSVC C++20 external overlay for ARC Raiders (PioneerGame.exe). Single VS project arc-um / output name arc-raiders-anc. Steam + Xbox (WinGDK) offset tables. Memory goes through an Astrum kernel client (NT hook and/or shared-memory ring). Unicorn Engine emulates encrypted GObjects vtable getters.

No README. No AOB/pattern scanner. No DeviceIoControl in source. Mapper/driver binaries are not in the git tree.

2\. Paths

Gitignored locally: unicorn/, Unicorn.hpp, unicorn.lib, ext/, docs/audit-\*.md, x64/, .vs/.

3\. Directory (source that matters)

src/ main.cpp / main.hpp WinMain + DllMain → entry\_sequence() Cheat.hpp menu/ImGui umbrella includes app/ overlay window (AppWindow.hpp), renderer, init config/build\_config.hpp DBG\_LOG / console gating game/ Loop.hpp overlay frame loop (ESP/aim/radar) — 206 KB EntityCache.hpp player cache NT::Store::CState WorldCache.hpp world cache NT::Store::CScene SlowCache.hpp slower polls (health, names, etc.) CacheTypes.hpp / Cache.hpp OffsetHealth.cpp/.hpp launch-time offset validation memory/ driver\_memory.hpp/.cpp c\_memory — kernel transport driver\_types.hpp c\_request, opcodes, copy/query structs interface/ctx.hpp mem\_ctx; global ctx sdk/ Offsets.hpp Steam/Xbox constexpr field offsets Steam.hpp / Xbox.hpp decrypt + GWorld/GObjects/FName Decryptions.hpp platform dispatch SDK.hpp UE helpers, Game, world-to-screen Platform.hpp Steam vs Xbox detect Math.hpp settings/Settings.hpp, Types.h menu/ custom ImGui-style UI + combat/visuals/world/radar/misc tabs docs/ changelog.md, audit-2026-08-06.md (ignored) \_dump/ SDK dump artifacts Unicorn.hpp + unicorn/ + unicorn.lib CPU emu (gitignored) arc-um.sln / arc-um.vcxproj



Largest sources: Loop.hpp 206 KB, SlowCache.hpp 140 KB, SDK.hpp 118 KB, WorldCache.hpp 86 KB, EntityCache.hpp 80 KB.

4\. Offsets

Canonical field table: src/sdk/Offsets.hpp



namespace Offsets { namespace Steam { constexpr uintptr\_t ... } namespace Xbox { ... } }

Packed into struct Offsets::Values

Runtime: const auto\& off = Offsets::Get(); // Xbox vs Steam via g\_PlatformVersion

Examples (Steam, CL-1341255):

UViewPort\_Uworld 0x1A0 UWorld\_PersistentLevel 0x110 ULevel\_ActorArray 0x108 UPawn\_HealthComponent 0xDD8 Arc\_ActorHealthComponent 0x5E0

RVAs / decrypt constants (not UObject fields):



src/sdk/Steam.hpp — #define STEAM\_\* e.g. STEAM\_GWORLD\_RVA 0xE782D78, GObjects/FName pool RVAs

src/sdk/Xbox.hpp — #define XBOX\_\*

Dump files (not compiled): \_dump/sdk\_offsets.txt, sdk\_wanted.txt, ClassesInfo.json.gz, StructsInfo.json.gz.

No offsets.h. Never hardcode hex; use Offsets::Get() or the STEAM\_/XBOX\_ macros.

5\. Driver / memory (no IOCTL in this repo)

DeviceIoControl / CTL\_CODE: not in source.

kdmapper source: not in this repo. Binaries only at Syndikate Arc\\Release 2\\.

Two user-mode transports in c\_memory:



Hooked NT exports: NtQueryAuxiliaryCounterFrequency or NtConvertBetweenAuxiliaryCounterAndPerformanceCounter; c\_request\* passed as extra arg.

Shared-memory ring: OpenFileMappingW + MapViewOfFile, struct astrum\_shared\_region, magic 0xA5720000, ring\_submit().

Opcodes (driver\_types.hpp): ASTRUM\_CMD\_READ/WRITE/QUERY/ALLOC (1/2/3/4) + batch. Command IDs, not IOCTLs.

Wrappers:

Low-level c\_memory: read, read<T>, write, write<T>, read\_batch, scatter\_read, scatter\_write, attach, try\_connect, open\_ring

App-facing mem\_bridge::mem\_ctx (ctx.hpp), global ctx:

read\_physical / write\_physical, read<T> / write<T>, read\_status<T>, read\_stale<T>, ReadChain<T>, read\_array, ReadWString, read\_string

There is no ReadMem<T>. Use ctx->read<T>(addr).

6\. Signatures / AOB

None. No FindPattern, no byte-signature arrays.

Related but not AOB:



OffsetHealth::ScanForViewportClient / ScanForLocalPlayerController — GObject/name resolution with retries

Platform::Steam::ScanForGWorld — image pointer sweep (8-byte candidates + structural checks)

7\. Build

Solution: arc-um.sln (one project)

C++20 (stdcpp20), toolset v145, Windows SDK 10.0

Configs: Debug|x64, Release|x64, Debug|Win32, Release|Win32, Release DLL|x64 (BUILD\_DLL)

ProjectName arc-raiders-anc → arc-raiders-anc.exe / .dll

Link: d3d9.lib d3d11.lib d3dcompiler.lib dxgi.lib unicorn.lib dwmapi.lib

Extra: /FORCE:MULTIPLE /IGNORE:4006,4098,4088

Pre-build: none

Post-build: none

msbuild "arc-um.sln" /p:Configuration=Release /p:Platform=x64 /t:Build /m

Output: x64\\Release\\arc-raiders-anc.exe

8\. Entry

src/main.cpp



EXE: WinMain → entry\_sequence()

DLL: DllMain → thread DeferredEntryProc → entry\_sequence()

Top files by role: main.cpp, SDK.hpp, Offsets.hpp, Steam.hpp, driver\_memory.cpp, ctx.hpp, Loop.hpp, EntityCache.hpp, WorldCache.hpp, Unicorn.hpp. Xbox decrypt path: Xbox.hpp. Settings/UI: Settings.hpp, menu/menu.cpp. Overlay HWND: AppWindow.hpp.

9\. Unicorn

Headers: unicorn/unicorn.h (C API)

Implementation: Unicorn.hpp — class VTableEmulator, uc\_open(UC\_ARCH\_X86, UC\_MODE\_64)

Maps target pages via ctx->read\_array, fake stack + TEB GS\_BASE, uc\_emu\_start (10k insn / 500 ms cap)

Helpers: EmulateVTableCallWithTLS, EmulateVTableCallWithTLS\_R8

Only used to run encrypted GObjects vtable getters to recover the chunks pointer:

Steam.hpp GetGObjectsChunksPtr

Xbox.hpp GetGObjectsChunksPtr (R8 variant)

Not used for pattern scan or general emulation.

10\. Docs already here

docs/changelog.md — 2026-08-06 audit follow-up commits

docs/audit-2026-08-06.md — 43.9 KB, gitignored

.cursor/rules/main.mdc, offsets.mdc, memory-transport.mdc

Conventions: C++20, MSVC, no exceptions, offsets not inlined, no std::cout in Release, memory through ctx.

11\. Coding notes for an agent working this repo

Offsets: Offsets::Get().UWorld.PersistentLevel style, never raw 0x110 in new code

Steam-only RVAs stay as STEAM\_\* macros in Steam.hpp

Reads: check TransportStatus / validpointer / is\_valid\_pointer\_optimized

Platform: g\_PlatformVersion (Steam vs Xbox), detected from exe path (wingdk vs win64)

Do not change astrum\_shared\_region layout unless explicitly asked

Do not add disk logging

Menu UI lives under src/menu/; keep widget style consistent

