---
type: Decision
title: "Crash traces via backward-cpp: fatal-signal handler in main, dynsym-only names, stderr capture"
description: "Fatal-signal stack traces via backward-cpp (xrepo, header-only): static backward::SignalHandling armed at the top of main() for SIGABRT/SIGSEGV/SIGBUS/SIGILL/SIGFPE (SIGINT/SIGTERM stay default), handler prints to stderr which launch wrappers capture into data/cppisekai.out; frame names come from dynsym alone because the bundled resolver is dladdr-only (BACKWARD_HAS_DWARF/BFD=0), so the on_load -rdynamic export (3550 syms) is mandatory and file:line needs a future libdwarf dep."
tags: [backward-cpp, crash, signals, dynsym, observability, releasedbg]
generated: { by: agent/mcp, at: "2026-11-14T20:46:28Z" }
status: stable
---

## Shape

- `src/main.hpp`: `#include <backward.hpp>` + `<csignal>`.
- `src/main.cpp`: `static backward::SignalHandling crash_signals({SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE});` armed at the very top of `main()` (before logger/config), so early-boot crashes trace too. SIGINT/SIGTERM stay default so shutdown handling is untouched.
- `xmake.lua`: `backward-cpp` in the cppIsekai `reqs` (header-only, no lib to link); `-rdynamic` injected in the `cxx_bin` `on_load` hook (see findings/xmake-ldflags-on-load-hook); `mode.releasedbg` added to `add_rules`.

## Resolver reality (xrepo backward-cpp v1.6)

Compiled config: `BACKWARD_HAS_UNWIND=1`, `BACKWARD_HAS_BACKTRACE_SYMBOL=1`, `BACKWARD_HAS_DW/DWARF/BFD/LIBUNWIND=0` - dladdr is the only symbolizer. Consequences:

- Frame names require dynsym exports: `-rdynamic` (3550 `T` syms) - without it app frames print as bare addresses.
- No `file:line`, until `libdwarf`/`bfd` becomes a dependency and the TU recompiles with `BACKWARD_HAS_DWARF=1`.

## Build mode

`releasedbg` (release optimization + debug info). xmake then splits symbols: `objcopy --only-keep-debug` -> strip -> `--add-gnu-debuglink`, leaving `cppIsekai` (8.4M, no symtab) + `cppIsekai.sym` (~150M). backward does not read `.sym` sidecars; `.dynsym` survives the strip, which is what dladdr uses.

## Verification

SEGV crash test (`kill -SEGV $(pgrep -x cppIsekai)`) appends to `data/cppisekai.out`:

    #6 in main
    #5 in serve(Store&, BS::thread_pool&, ...)
    #4 in crow::Server<crow::Crow<>, crow::TCPAcceptor, crow::SocketAdaptor>::run()
    #3 in std::thread::join()

Daemon relaunched healthy after the test.

## Followup

file:line frames => add `libdwarf` (or elfutils/bfd) dep, then recheck the BACKWARD_HAS_* detection at compile time.

# Related Concepts
- [xmake build system alongside cmake + prod-code gate tasks](xmake-build-system.md): Crash-trace symbolization required build-system changes: the mode.releasedbg rule and the cxx_bin on_load ldflags hook in xmake.lua.
