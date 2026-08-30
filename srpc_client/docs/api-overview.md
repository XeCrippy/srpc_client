# Public API map

Include only the headers needed by the application. All public declarations
are in namespace `srpc`.

| Header | Main API |
|---|---|
| `<srpc/client.hpp>` | `Client`, memory, RPC, files, screenshots, console control |
| `<srpc/discovery.hpp>` | `probe_console`, `discover_console(s)`, discovery options |
| `<srpc/debug.hpp>` | `DebugSession`, breakpoints, threads, PPC contexts, events |
| `<srpc/transfer.hpp>` | transfer options, progress, collision policy, result |
| `<srpc/screenshot.hpp>` | raw/decoded screenshot types and options |
| `<srpc/image.hpp>` | `ImageView`, PNG encoding/writing |
| `<srpc/types.hpp>` | common enums, options, response, RPC values, console records |
| `<srpc/error.hpp>` | exception hierarchy |

`<srpc/client.hpp>` already includes the common error, screenshot, transfer,
and type declarations used by `Client`.

## Client groups

| Area | Operations |
|---|---|
| Lifecycle | `connect`, `reconnect`, `close`, `connected`, `host` |
| Transport | `protocol`, `plugin_available`, `send_srpc` |
| Raw XBDM | `send_command`, `send_command_raw`, `send_multiline_command` |
| Memory bytes | `read_memory`, `write_memory`, chunked/sparse reads, fill/zero |
| Typed memory | `read`, `write`, arrays, C strings, UTF-16 strings |
| PowerPC code | `write_branch`, `write_jump`, executable pool APIs |
| RPC | `call`, typed call helpers, module/ordinal calls, `resolve_function` |
| Console info | name/ID/type, CPU key, kernel, temperatures, drives, sign-in |
| Title/process | title ID/path/module, modules, handles, process ID |
| Remote paths | list, test, create, delete, rename |
| File transfer | in-memory files and streaming file/tree transfer |
| Screenshots | raw capture, decoded capture, PNG save |
| Console control | go/stop, launch, modules, time, notify, LEDs, reboot/shutdown |
| Persistent plugin state | `constant_memory_set` |

## Important public value types

- `ClientOptions` configures transport, port, and timeouts.
- `RpcArgument`, `RpcValue`, `ReturnType`, and `CallOptions` describe calls.
- `MemoryRegion`, `ModuleInfo`, `DirectoryEntry`, and `Response` model XBDM
  results.
- `FileTransferOptions`, `DirectoryTransferOptions`, `TransferProgress`, and
  `TransferResult` control local-path transfers.
- `RawScreenshot`, `ScreenshotImage`, and `ScreenshotOptions` control capture
  and decoding.
- `DebugEvent`, `PpcContext`, `ThreadInfo`, and `ContextFlags` model a debug
  session.

The public headers shipped with a release are authoritative for signatures,
defaults, and enum values. These guides focus on correct usage and behavioral
constraints instead of duplicating every declaration.
