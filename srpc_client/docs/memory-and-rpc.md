# Memory and RPC

Memory addresses are 32-bit Xbox virtual addresses. Validate or derive an
address for the currently loaded title rather than assuming that an address is
stable between title versions.

## Typed memory

Xbox 360 PowerPC values are normally big-endian, which is the default:

```cpp
const auto health = client.read<std::uint32_t>(0x82001000);
client.write<float>(0x82001010, 100.0F);

const auto little = client.read<std::uint32_t>(
    0x82002000, srpc::Endian::little);
```

`read<T>()` and `write<T>()` accept arithmetic and enum scalar types. Arrays
are converted element by element:

```cpp
const auto values = client.read_array<std::uint16_t>(0x82003000, 32);

const std::array<float, 3> position{1.0F, 2.0F, 3.0F};
client.write_array<float>(0x82003100, position);
```

Use byte operations for opaque structures:

```cpp
const srpc::ByteBuffer bytes = client.read_memory(0x82004000, 64);
client.write_memory(0x82004100, bytes);
```

String helpers enforce a maximum read size and can include or omit a trailing
terminator on write:

```cpp
const auto label = client.read_cstring(0x82005000, 128);
client.write_cstring(0x82005100, "ready");

const auto wide = client.read_utf16_string(0x82005200, 128);
client.write_utf16_string(0x82005300, u"Ready");
```

## Large and sparse reads

`read_memory_chunked()` divides a large contiguous read into bounded XBDM
requests. `read_memory_sparse()` first obtains the memory map, returns exactly
the requested output size, and fills unmapped gaps with zero:

```cpp
const auto dump = client.read_memory_sparse(
    0x80000000,
    0x02000000,
    1024 * 1024,
    [](std::size_t completed, std::size_t total) {
        std::cout << completed << " / " << total << '\n';
        return true; // false cancels by throwing srpc::Error
    });
```

A second overload accepts a caller-provided list of `MemoryRegion` values.
Failures inside a region declared readable are reported rather than silently
replaced with zero. `memory_regions()` returns the current XBDM map and
`is_valid_address()` checks whether a single address belongs to a readable
region.

`fill_memory()` and `zero_memory()` provide bounded repeated-byte writes.

## Branches and executable memory

`write_branch()` writes a validated relative PowerPC branch and reports a
destination outside its signed range. `write_jump()` writes a longer absolute
jump sequence using a selected scratch register.

The plugin also exposes a fixed 64 KiB, four-byte-aligned executable pool:

```cpp
const auto address = client.allocate_executable(64);
const auto usage = client.executable_pool_info();
```

Allocations cannot be individually freed. `reset_executable_pool()` only
rewinds the allocator; it neither removes hooks nor clears code. It can make a
later allocation overwrite live code and therefore requires this explicit
argument:

```cpp
client.reset_executable_pool(
    srpc::ExecutablePoolReset::confirm_live_allocations_may_be_overwritten);
```

Only reset the pool when the application can prove that no allocation remains
in use.

## RPC calls

Resolve an exported ordinal, then use a convenience method for its result:

```cpp
const auto address = client.resolve_function("xam.xex", 0x290);

const auto result = client.call_uint32(
    address,
    {std::uint32_t{0}, std::string{"argument"}, 1.0F});
```

Arguments may be `bool`, signed or unsigned 32/64-bit integers, `float`,
`std::string`, `srpc::ByteBuffer`, or `nullptr` (encoded as a null 32-bit
value). Convenience result methods are:

- `call_uint32()`
- `call_uint64()`
- `call_float()`
- `call_string()`
- `call_void()`

The general `call()` method returns `srpc::RpcValue`, a variant containing no
value, a scalar, string, integer array, float array, or byte buffer:

```cpp
srpc::CallOptions options;
options.array_size = 16;

const auto value = client.call(
    address,
    srpc::ReturnType::int32_array,
    {},
    options);

const auto& values = std::get<std::vector<std::uint32_t>>(value);
```

There is also a `call(module, ordinal, ...)` overload when a separate resolve
step is not useful. `CallOptions` selects the system/title thread behavior,
virtual-machine mode, and returned array length.

## Transport capabilities

Native SRPC captures the function's 32-bit `r3` result. It supports void,
32-bit integer/byte, and raw-word float results. Strings, 64-bit results,
arrays, virtual-machine calls, and thread-selection options require JRPC2.
Unsupported combinations produce an error before a command is sent.

Automatic mode detects the available transport. Pin JRPC2 when an operation
requires it:

```cpp
srpc::ClientOptions options;
options.protocol = srpc::Protocol::jrpc2;
srpc::Client client("192.168.1.120", options);
client.connect();
```

Native SRPC can also use the plugin's JRPC2 type-100 tunnel for helpers that
are implemented only by JRPC2.

## Persistent memory sets

`constant_memory_set()` registers a persistent 32-bit write, optionally
conditioned on a current value or title ID:

```cpp
client.constant_memory_set(
    0x82001000,
    999,
    std::nullopt,
    client.title_id());
```

The plugin currently has 40 slots and no remove command. Entries last until
reboot. Avoid duplicates, and do not unload the plugin after registering an
entry because its memory-set worker remains active.
