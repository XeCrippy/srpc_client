# Getting started

## Choose the correct binary

Use a library built for the same architecture and configuration as the
application:

| Application | Library |
|---|---|
| x64 Release | x64 `srpc_client.lib` |
| x64 Debug | x64 `srpc_clientd.lib` |
| x86 Release | x86 `srpc_client.lib` |
| x86 Debug | x86 `srpc_clientd.lib` |

The distributed binaries use the dynamic MSVC runtime: `/MD` for Release and
`/MDd` for Debug. Keep the application's runtime library setting compatible.
Because the public API contains standard-library types, also use the MSVC
toolset identified by the release and do not mix Debug and Release binaries.

The archive contains public headers under `include/srpc`, one or more static
libraries, and (when supplied for that asset) a CMake package under
`lib/cmake/SRPCClient`.

## CMake package integration

Linking the imported target is preferred because it supplies the header path,
the selected configuration's library, and the required Windows system
libraries.

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyConsoleTool LANGUAGES CXX)

find_package(SRPCClient CONFIG REQUIRED)

add_executable(my_console_tool main.cpp)
target_compile_features(my_console_tool PRIVATE cxx_std_20)
target_link_libraries(my_console_tool PRIVATE srpc::client)
```

Configure with the directory containing `SRPCClientConfig.cmake`:

```powershell
cmake -S . -B build -A x64 `
  -DSRPCClient_DIR="C:/SDKs/SRPCClient/lib/cmake/SRPCClient"
cmake --build build --config Release
```

The `-A` value and `--config` value must match an included binary. An error
that says an imported configuration or library file is missing usually means
the wrong release archive or configuration was selected.

## Manual Visual Studio integration

For each applicable project configuration:

1. Select C++20 in **C/C++ > Language > C++ Language Standard**.
2. Add `<release-root>/include` to **C/C++ > General > Additional Include
   Directories**.
3. Add the directory containing the matching `.lib` to **Linker > General >
   Additional Library Directories**.
4. Add `srpc_client.lib`, `ws2_32.lib`, and `iphlpapi.lib` to **Linker > Input
   > Additional Dependencies**. Use `srpc_clientd.lib` in Debug.
5. Match the application platform (`x64` or `Win32`) and runtime library
   (`/MD` or `/MDd`) to the selected binary.

An unresolved external symbol from `srpc::` generally means the SRPC library
is missing or the wrong architecture/configuration was selected. Unresolved
WinSock or adapter-enumeration symbols usually mean `ws2_32.lib` or
`iphlpapi.lib` was omitted.

## Console prerequisites

The console must run XBDM and be reachable from the PC on TCP port 730. Verify
that the PC and console can communicate through any host firewall, VLAN, VPN,
or guest-network isolation. Automatic discovery probes active local IPv4
subnets; an explicit IP is more predictable on large or segmented networks.

RPC calls and helpers implemented by the console plugin require native SRPC or
JRPC2. XBDM-only features such as raw commands, ordinary memory access, file
operations, and screenshots do not require an RPC transport.

## Minimal application

```cpp
#include <srpc/client.hpp>
#include <srpc/discovery.hpp>

#include <iostream>

int main() {
    try {
        const auto console = srpc::probe_console("192.168.1.120");
        if (!console) {
            std::cerr << "No XBDM console found\n";
            return 2;
        }

        srpc::Client client(console->host);
        client.connect();

        std::cout << "Connected to " << client.console_name() << '\n';
        std::cout << "Plugin available: "
                  << (client.plugin_available() ? "yes" : "no") << '\n';
    } catch (const srpc::Error& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

`Client` is move-only and closes its connection when destroyed. Call
`reconnect()` after a lost or deliberately cancelled binary exchange; call
`close()` when an earlier shutdown is useful.

Next: [connections, options, and error handling](client-guide.md).
