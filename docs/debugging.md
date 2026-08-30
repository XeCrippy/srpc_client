# Debugging

`srpc::DebugSession`, declared in `<srpc/debug.hpp>`, provides breakpoints,
thread control, register contexts, and XBDM debug notifications. It is
separate from `srpc::Client` because a live session needs both a command
connection and a dedicated notification connection.

## Basic session

```cpp
#include <srpc/debug.hpp>

#include <iostream>

srpc::DebugSession debug("192.168.1.120");

debug.attach(
    [&debug](const srpc::DebugEvent& event) {
        if (event.type != srpc::DebugEventType::breakpoint) {
            return;
        }

        try {
            srpc::ContextFlags flags;
            flags.control = true;
            flags.integer = true;
            flags.floating = false;

            const auto context = debug.get_context(event.thread_id, flags);
            std::cout << "Breakpoint at 0x" << std::hex << context.iar
                      << ", r3=0x" << context.arg(0) << '\n';

            debug.clear_breakpoint(event.address); // one-shot example
            debug.continue_thread(event.thread_id);
        } catch (const srpc::Error& error) {
            std::cerr << error.what() << '\n';
        }
    },
    [](std::string_view message) {
        std::cerr << "debug: " << message << '\n';
    });

if (!debug.is_debugger_owner()) {
    std::cerr << "Another debugger owns console notifications\n";
} else {
    debug.set_breakpoint(0x82123456);
}

// Keep the session alive while the application does its work.
// debug.detach() is optional at normal scope exit.
```

The event handler runs on an internal worker thread. It may call
`DebugSession` commands because they use the separate command connection, but
it must synchronize access to application data shared with other threads.
Keep a halting-event handler short; a networked title can drop its session if
it remains halted too long.

`attach()` can succeed without owning the console's debug session when another
debugger is connected. Check `is_debugger_owner()` before arming breakpoints.
Ownership alone does not prove that the notification connection succeeded, so
also surface the optional log callback. Notification-channel failures and
exceptions thrown by the event handler are reported there.

## Events and contexts

`DebugEvent` reports the event type, thread, instruction address, data address,
exception code, access type, first-chance flag, decoded text, and raw event
line where applicable. `halts_title()` identifies events that require a
resume.

Contexts are meaningful only while the relevant title thread is halted:

```cpp
const auto context = debug.get_context(thread_id);
const auto first_integer_argument = context.arg(0); // r3

srpc::ContextFlags with_floats;
with_floats.floating = true;
const auto full = debug.get_context(thread_id, with_floats);
const auto first_float_argument = full.float_arg(0); // f1
```

PowerPC integer/pointer arguments use r3-r10 while floating-point arguments
use f1-f8 in parallel. `set_context()` writes only groups enabled in its
`ContextFlags` argument.

Thread APIs include `thread_ids()`, `thread_info()`, `thread_stopped()`,
`halt_thread()`, `resume_thread()`, and `continue_thread()`.
`continue_thread(id, false)` consumes a breakpoint exception; passing `true`
delivers it to the title's handler and can crash the title.

`go()` clears a global stop but does not consume per-thread debug exceptions.
Use `continue_thread()` for the thread named by an event. On an explicit
whole-title resume, `release_stopped_threads()` can sweep every stopped thread
before `go()`.

## Breakpoints

Code breakpoints:

```cpp
debug.set_breakpoint(address);
const bool armed = debug.is_breakpoint(address);
debug.clear_breakpoint(address);
```

Data breakpoints use a byte size of 1, 2, 4, or 8 and an access type:

```cpp
debug.set_data_breakpoint(address, 4, srpc::BreakType::write);
debug.clear_data_breakpoint(address, 4, srpc::BreakType::write);
```

`BreakType::read`, `write`, and `execute` match the access that triggers the
watchpoint. A hot read/write address can fire continuously, so one-shot
handling is usually safest.

## Safety rules

- Never automatically arm a data breakpoint outside memory known to belong to
  the title. A watchpoint on memory used by XBDM or the kernel can halt the
  monitor itself and require a console power cycle.
- Do not use a simple lower-bound address rule. Title heaps may be below the
  usual `0x82000000` XEX image. Derive a target from the current title, a
  symbol, or a deliberately constrained title-memory scan.
- Continue a thread that triggered a halting event as quickly as practical.
- Pass `false` to `continue_thread()` for a breakpoint unless the title is
  explicitly expected to handle that exception.
- `clear_all_breakpoints()` removes all breakpoints known to the console,
  including ones placed by another debugger.
- Keep the session object alive until all callbacks are finished. Do not
  destroy it from its own event callback.

`detach()` and the destructor stop notifications, clear breakpoints, release
stopped threads, issue `go`, disconnect debugger ownership, and close the
command connection on a best-effort basis. Explicitly call `detach()` when the
application wants that cleanup to occur before normal scope exit.

## Raw debug command

`DebugSession::send_command()` is an escape hatch for an XBDM debug command
not represented by the typed API. It uses the debug command connection and
throws the same `srpc::Error` types as `Client` operations.
