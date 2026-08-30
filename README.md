# SRPC C++ Client

SRPC Client is a prebuilt C++20 library for Windows applications that control
an Xbox 360 running XBDM. It communicates directly with the console over TCP
port 730, so a client application does not need the Xbox 360 SDK, `xbdm.lib`,
or XDevkit COM.

The library supports console discovery, memory access, SRPC/JRPC2 calls, file
transfer, screenshots, console management, and live debugging.

## Requirements

- Windows and a C++20-capable MSVC toolchain.
- An application architecture and configuration matching the downloaded
  library (`x64`/`x86` and `Release`/`Debug`).
- An Xbox 360 with XBDM reachable from the PC on TCP port 730.
- The SRPC or JRPC2 plugin for RPC and plugin-specific helpers. Basic XBDM
  operations remain available without it.

This is a static library. Applications link `srpc_client.lib` in Release or
`srpc_clientd.lib` in Debug; there is no SRPC Client DLL to copy beside the
application.

## Add it to an application

The recommended CMake integration uses the package files included in the
binary release:

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyConsoleTool LANGUAGES CXX)

find_package(SRPCClient CONFIG REQUIRED)

add_executable(my_console_tool main.cpp)
target_compile_features(my_console_tool PRIVATE cxx_std_20)
target_link_libraries(my_console_tool PRIVATE srpc::client)
```

Point `SRPCClient_DIR` at the release's `lib/cmake/SRPCClient` directory when
configuring the application:

```powershell
cmake -S . -B build -DSRPCClient_DIR="C:/SDKs/SRPCClient/lib/cmake/SRPCClient"
cmake --build build --config Release
```

For a Visual Studio project that does not use CMake, add the release's
`include` directory to **C/C++ > Additional Include Directories**, add the
matching library directory to **Linker > Additional Library Directories**, and
link these inputs:

```text
srpc_client.lib
ws2_32.lib
iphlpapi.lib
```

Use `srpc_clientd.lib` for a Debug application. See the
[getting-started guide](docs/getting-started.md) for configuration and ABI
details.

## First connection

```cpp
#include <srpc/client.hpp>
#include <srpc/discovery.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>

int main() {
    try {
        // Pass a known IP to avoid scanning, or call discover_console().
        const auto found = srpc::probe_console("192.168.1.120");
        if (!found) {
            std::cerr << "Console is not reachable\n";
            return 1;
        }

        srpc::Client xbox(found->host);
        xbox.connect();

        std::cout << xbox.console_name() << '\n';
        std::cout << "Title ID: 0x" << std::hex << xbox.title_id() << '\n';

        const auto value = xbox.read<std::uint32_t>(0x82000000);
        std::cout << "Value: 0x" << value << '\n';
    } catch (const srpc::Error& error) {
        std::cerr << "SRPC error: " << error.what() << '\n';
        return 1;
    }
}
```

Typed memory operations default to big-endian values, matching the Xbox 360's
PowerPC CPU. Network discovery is optional; an explicit IP or hostname can be
passed directly to `srpc::Client`.

## Documentation

- [Getting started](docs/getting-started.md) — binary package setup, linking,
  console prerequisites, and a first application.
- [Client guide](docs/client-guide.md) — connections, discovery, options,
  errors, concurrency, console information, and raw commands.
- [Memory and RPC](docs/memory-and-rpc.md) — typed memory, strings, sparse
  reads, function calls, transport differences, and executable helpers.
- [Files and screenshots](docs/files-and-screenshots.md) — in-memory and
  streaming transfers, cancellation, collision policies, and PNG capture.
- [Debugging](docs/debugging.md) — debug sessions, breakpoints, threads,
  contexts, event handling, and safety constraints.
- [Public API map](docs/api-overview.md) — headers and API groups at a glance.

## Important behavior

- A `Client` serializes complete request/reply exchanges and may be shared by
  application threads. A progress callback must not call back into the same
  `Client`.
- Network and protocol failures are reported through the `srpc::Error`
  hierarchy. Local filesystem operations can additionally throw
  `std::filesystem::filesystem_error`.
- Remote file overwrite is delete-then-upload and is not atomic.
- `constant_memory_set()` entries persist until reboot and consume one of the
  plugin's 40 slots.
- `reset_executable_pool()` can overwrite live hooks and requires an explicit
  confirmation value.
- Debug breakpoints can halt a title. Read the [debugging safety
  notes](docs/debugging.md#safety-rules) before using `srpc::DebugSession`.
