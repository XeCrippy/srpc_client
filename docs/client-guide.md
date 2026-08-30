# Client guide

## Direct connections and discovery

Use a known address when possible:

```cpp
srpc::Client client("192.168.1.120");
client.connect();
```

Discovery helpers are declared in `<srpc/discovery.hpp>`:

```cpp
// Scan active local IPv4 subnets and return every console found.
const auto all = srpc::discover_consoles();

// Return the first discovered console.
const auto first = srpc::discover_console();

// Scan first, then probe this address only if the scan found nothing.
const auto fallback = srpc::discover_console("10.0.0.190");

// Probe one IP or hostname without scanning.
const auto manual = srpc::probe_console("10.0.0.190");
```

An unreachable address or non-XBDM endpoint makes `probe_console()` return
`std::nullopt`; it is not exceptional. Invalid options or a local networking
failure can still throw an `srpc::Error`.

Discovery limits large networks to bounded candidate sets. The defaults can
be tuned when a network or endpoint-security policy prefers a quieter scan:

```cpp
srpc::DiscoveryOptions discovery;
discovery.connect_timeout = std::chrono::milliseconds(150);
discovery.io_timeout = std::chrono::milliseconds(300);
discovery.max_concurrency = 8;
discovery.max_candidates_per_adapter = 256;

const auto consoles = srpc::discover_consoles(discovery);
```

## Client options

```cpp
srpc::ClientOptions options;
options.protocol = srpc::Protocol::automatic;
options.port = 730;
options.connect_timeout = std::chrono::seconds(5);
options.io_timeout = std::chrono::seconds(5);
options.rpc_timeout = std::chrono::seconds(10);
options.poll_interval = std::chrono::milliseconds(50);

srpc::Client client("192.168.1.120", options);
```

`Protocol::automatic` detects native SRPC or JRPC2 on first plugin use. To
require a specific transport, select `Protocol::native_srpc` or
`Protocol::jrpc2`. `protocol()` returns the configured/detected transport;
`plugin_available()` performs a non-throwing availability check for the
supported plugin commands.

## Connection lifecycle

- `connect()` opens the connection and reads the XBDM greeting.
- `connected()` reports the local connection state.
- `reconnect()` closes any current socket and establishes a new exchange.
- `close()` is non-throwing and may be called more than once.
- `host()` returns the configured address.

A timeout or a partially cancelled binary transfer can leave the stream
unsuitable for another request. Reconnect before continuing in those cases.

## Error handling

All library-specific exceptions derive from `srpc::Error`:

| Exception | Meaning |
|---|---|
| `ConnectionError` | Socket setup, connection, send, receive, or disconnect failure |
| `TimeoutError` | A configured connect, I/O, or RPC deadline expired |
| `ProtocolError` | Invalid input or an invalid/unexpected console response |
| `CommandError` | XBDM returned a non-success command status |
| `RpcError` | The selected RPC transport rejected or could not complete a call |

`CommandError::status_code()` exposes the XBDM status code. Local-path file
operations can also throw `std::filesystem::filesystem_error`.

```cpp
try {
    client.delete_file("Hdd:\\missing.bin");
} catch (const srpc::CommandError& error) {
    std::cerr << "XBDM status " << error.status_code()
              << ": " << error.what() << '\n';
} catch (const srpc::TimeoutError& error) {
    client.reconnect();
} catch (const srpc::Error& error) {
    std::cerr << error.what() << '\n';
}
```

Catch derived exceptions before `srpc::Error`.

## Concurrency

One `Client` serializes each complete operation, including multiline/binary
replies and asynchronous RPC polling. Multiple application threads can share
it without interleaving protocol frames, but their operations execute one at a
time.

Transfer and memory progress callbacks execute while that exchange owns the
connection. They must not invoke another operation on the same `Client`; use a
different client connection or communicate progress back to another thread.

## Console and title information

Common queries include:

```cpp
const auto name = client.console_name();
const auto id = client.console_id();
const auto type = client.console_type();
const auto cpu_key = client.cpu_key();
const auto kernel = client.kernel_version();
const auto dashboard = client.dm_version();
const auto title_id = client.title_id();
const auto title_path = client.title_path();
const auto modules = client.module_list();
const auto title = client.title_module();
```

The API also exposes `is_devkit()`, `gamertag()`, `motherboard_type()`,
`temperature()`, `drives()`, `module_handle()`, `process_id()`, and
`sign_in_state()`.

Console actions include notifications, quadrant LEDs, time synchronization,
title launch, module load/unload, debug stop/go, reboot, and shutdown. Treat
the last group as state-changing operations and make them explicit in the
application UI.

## Raw commands

Use the typed APIs when one exists. The raw methods are escape hatches:

```cpp
const auto response = client.send_command("getpid");
const auto rows = client.send_multiline_command("modules");
const auto srpc_text = client.send_srpc("titleid");
```

- `send_command()` throws `CommandError` for a non-2xx status.
- `send_command_raw()` returns every status in an `srpc::Response`.
- `send_multiline_command()` reads a dot-terminated XBDM response.
- `send_srpc()` accepts a command with or without the leading `s360` token and
  chooses native SRPC or the JRPC2 tunnel.

Do not include CR, LF, or NUL characters in user-supplied command fields.
Incorrect raw commands can mutate console state, so validate inputs before
building command strings.
