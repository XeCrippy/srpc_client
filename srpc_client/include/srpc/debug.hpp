#pragma once

#include "srpc/error.hpp"
#include "srpc/types.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Debug control of a running title over XBDM: breakpoints, thread contexts and
// the notification channel.
//
// This is what xbdm.dll's Dm* debugger calls wrap. The wire formats are
// documented in docs/xbdm-debug-protocol.md and were taken from that DLL's own
// format strings, so nothing here is guessed.
namespace srpc {

// Access that trips a data breakpoint. The names match the wire keywords.
enum class BreakType {
    read,
    write,
    execute,
};

[[nodiscard]] std::string_view to_string(BreakType type);

// What halted the title.
enum class DebugEventType {
    unknown,
    execution,   // the title's run state changed
    breakpoint,  // code breakpoint
    data_break,  // data breakpoint (watchpoint)
    single_step,
    exception,
    assertion,
    rip,
    debug_string,
};

[[nodiscard]] std::string_view to_string(DebugEventType type);

// A PowerPC register file, captured while the title was halted.
//
// Register dumps are only meaningful in that state. Asking a *running* title
// for a context returns idle kernel threads - `iar` somewhere in 0x80xxxxxx and
// every argument register 0xFFFFFFFF. The breakpoint halting the title is what
// makes these real.
struct PpcContext {
    bool valid = false;

    std::uint32_t iar = 0;   // instruction address
    std::uint32_t msr = 0;
    std::uint32_t cr = 0;
    std::uint32_t xer = 0;
    std::uint32_t lr = 0;
    std::uint64_t ctr = 0;

    std::array<std::uint64_t, 32> gpr{};
    std::array<std::uint64_t, 32> fpr{};  // raw bit patterns, not decoded
    std::uint64_t fpscr = 0;

    // PowerPC passes integer and pointer arguments in r3-r10, floats in f1-f8,
    // and does so *in parallel*: an int and a float argument do not compete for
    // the same slot.
    [[nodiscard]] std::uint64_t arg(std::size_t index) const {
        return index < 8 ? gpr[3 + index] : 0;
    }
    [[nodiscard]] double float_arg(std::size_t index) const;
};

// Which register groups to fetch. Fewer groups is a smaller reply and a faster
// round trip, which matters on the breakpoint-hit path.
struct ContextFlags {
    bool control = true;   // iar, msr, cr, xer, lr, ctr
    bool integer = true;   // gpr0-31
    bool floating = false; // fpr0-31, fpscr
    bool vector = false;   // vr0-127, vscr - not decoded by this client
};

// One thread as reported by THREADINFO.
struct ThreadInfo {
    std::uint32_t id = 0;
    std::uint32_t suspend_count = 0;
    std::uint32_t priority = 0;
    std::uint32_t tls_base = 0;
    std::uint32_t stack_base = 0;
    std::uint32_t stack_limit = 0;
    std::uint32_t start_address = 0;
    bool stopped = false;
};

// A halting event delivered on the notification channel.
struct DebugEvent {
    DebugEventType type = DebugEventType::unknown;
    std::uint32_t thread_id = 0;
    std::uint32_t address = 0;       // instruction that triggered the stop
    std::uint32_t data_address = 0;  // address read/written, data breaks only
    std::uint32_t exception_code = 0;
    std::optional<BreakType> access; // data breaks only
    bool first_chance = false;
    std::string text;                // debugstr payload, assert text
    std::string raw;                 // the whole line, for anything undecoded

    // True when the title is halted because of this event and something has to
    // resume it.
    [[nodiscard]] bool halts_title() const;
};

using DebugEventHandler = std::function<void(const DebugEvent&)>;
using DebugLogHandler = std::function<void(std::string_view)>;

// Live debug control of one console.
//
// Owns two connections: one for commands and one carrying notifications. They
// have to be separate because the notification channel is a one-way stream the
// console pushes to, and a command reply cannot be read from it.
//
// Safety property this class holds: it never leaves the console halted.
// Detaching, destruction and every error path disarm the breakpoints and
// resume the title.
class DebugSession final {
public:
    explicit DebugSession(std::string host, ClientOptions options = {});
    ~DebugSession();

    DebugSession(const DebugSession&) = delete;
    DebugSession& operator=(const DebugSession&) = delete;

    // --- session ---------------------------------------------------------

    // Opens the command connection, registers as the debugger, and starts the
    // notification channel. `on_event` is called on an internal worker thread,
    // never on the caller's.
    void attach(DebugEventHandler on_event, DebugLogHandler on_log = {});
    void detach() noexcept;

    [[nodiscard]] bool attached() const noexcept;

    // True when this client owns the debug session. Notifications only arrive
    // when it does; breakpoints can still be set when it does not.
    [[nodiscard]] bool is_debugger_owner() const noexcept;

    // --- title control ---------------------------------------------------

    void stop();  // halt the whole title
    void go();    // clear the global stop

    // Continues every thread halted at a debug event, returning how many were
    // released.
    //
    // This is the step whose absence looks like a working resume: `go` clears
    // only the *global* stop, so the thread that hit the breakpoint stays
    // halted with its exception unconsumed. The title freezes while the console
    // still answers commands.
    //
    // It costs one round trip per thread, so it belongs on an explicit resume
    // and on detach - not on the hit path, where latency decides whether a
    // networked title keeps its session.
    int release_stopped_threads();

    // `deliver_to_title` false consumes the exception; true passes it to the
    // title's own handler, which for a breakpoint means crashing it.
    void continue_thread(std::uint32_t thread_id, bool deliver_to_title = false);
    void halt_thread(std::uint32_t thread_id);
    void resume_thread(std::uint32_t thread_id);
    [[nodiscard]] bool thread_stopped(std::uint32_t thread_id);

    // --- breakpoints -----------------------------------------------------

    void set_breakpoint(std::uint32_t address);
    void clear_breakpoint(std::uint32_t address);
    [[nodiscard]] bool is_breakpoint(std::uint32_t address);

    // `size` is in bytes (1/2/4/8).
    void set_data_breakpoint(std::uint32_t address, std::uint32_t size, BreakType type);
    void clear_data_breakpoint(std::uint32_t address, std::uint32_t size, BreakType type);

    // Removes every breakpoint this console has, including any another
    // debugger set.
    void clear_all_breakpoints();

    // --- threads and registers -------------------------------------------

    [[nodiscard]] std::vector<std::uint32_t> thread_ids();
    [[nodiscard]] ThreadInfo thread_info(std::uint32_t thread_id);
    [[nodiscard]] PpcContext get_context(std::uint32_t thread_id, ContextFlags flags = {});

    // Writes back only the registers named in `flags`.
    void set_context(std::uint32_t thread_id, const PpcContext& context,
                     ContextFlags flags = {});

    // --- escape hatch ----------------------------------------------------

    // Sends a raw command on the debug connection, for anything this API does
    // not cover.
    [[nodiscard]] Response send_command(std::string_view command);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace srpc
