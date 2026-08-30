#pragma once

#include "srpc/error.hpp"
#include "srpc/screenshot.hpp"
#include "srpc/transfer.hpp"
#include "srpc/types.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace srpc {

using MemoryProgressCallback =
    std::function<bool(std::size_t processed, std::size_t total)>;

class Client final {
public:
    explicit Client(std::string host, ClientOptions options = {});
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;

    void connect();
    void reconnect();
    void close() noexcept;

    [[nodiscard]] bool connected() const noexcept;
    [[nodiscard]] std::string_view host() const noexcept;

    // Returns the configured transport, probing the console on first use when
    // Protocol::automatic was selected.
    [[nodiscard]] Protocol protocol();
    [[nodiscard]] bool plugin_available();

    // Raw XBDM commands. send_command throws CommandError for 4xx replies;
    // send_command_raw returns every status code to the caller.
    [[nodiscard]] Response send_command(std::string_view command);
    [[nodiscard]] Response send_command_raw(std::string_view command);
    [[nodiscard]] std::vector<std::string> send_multiline_command(
        std::string_view command);

    // Runs an SRPC command over the native `s360` command or the JRPC2 type-100
    // tunnel. `command` may include or omit the leading `s360` word.
    [[nodiscard]] std::string send_srpc(std::string_view command);

    [[nodiscard]] ByteBuffer read_memory(
        std::uint32_t address,
        std::size_t size);
    void write_memory(
        std::uint32_t address,
        std::span<const std::uint8_t> data);
    [[nodiscard]] ByteBuffer read_memory_chunked(
        std::uint32_t address,
        std::size_t size,
        std::size_t chunk_size = 1024 * 1024);
    // Returns exactly `size` bytes. Bytes outside `readable_regions` are zero;
    // failures while reading a declared region are not hidden. Progress is
    // monotonic across both reads and gaps. Returning false cancels by throwing
    // Error, so a partially populated buffer is never returned.
    [[nodiscard]] ByteBuffer read_memory_sparse(
        std::uint32_t address,
        std::size_t size,
        std::span<const MemoryRegion> readable_regions,
        std::size_t chunk_size = 1024 * 1024,
        MemoryProgressCallback progress = {});
    // Discovers the readable regions with walkmem before performing the same
    // sparse read described above.
    [[nodiscard]] ByteBuffer read_memory_sparse(
        std::uint32_t address,
        std::size_t size,
        std::size_t chunk_size = 1024 * 1024,
        MemoryProgressCallback progress = {});
    [[nodiscard]] std::vector<MemoryRegion> memory_regions();
    [[nodiscard]] bool is_valid_address(std::uint32_t address);
    void fill_memory(
        std::uint32_t address,
        std::size_t size,
        std::uint8_t value);
    void zero_memory(std::uint32_t address, std::size_t size);

    [[nodiscard]] std::string read_cstring(
        std::uint32_t address,
        std::size_t maximum_length = 256);
    void write_cstring(
        std::uint32_t address,
        std::string_view value,
        bool null_terminate = true);
    [[nodiscard]] std::u16string read_utf16_string(
        std::uint32_t address,
        std::size_t maximum_characters = 256,
        Endian endian = Endian::big);
    void write_utf16_string(
        std::uint32_t address,
        std::u16string_view value,
        Endian endian = Endian::big,
        bool null_terminate = true);

    template <typename T>
    [[nodiscard]] T read(std::uint32_t address, Endian endian = Endian::big) {
        static_assert(std::is_trivially_copyable_v<T>);
        static_assert(!std::is_pointer_v<T>);
        static_assert(
            std::is_arithmetic_v<T> || std::is_enum_v<T>,
            "read<T> is for scalar values; use read_array<T> or read_memory for aggregates");

        auto bytes = read_memory(address, sizeof(T));
        if constexpr (sizeof(T) > 1) {
            const bool reverse =
                (endian == Endian::big && std::endian::native == std::endian::little) ||
                (endian == Endian::little && std::endian::native == std::endian::big);
            if (reverse) {
                std::reverse(bytes.begin(), bytes.end());
            }
        }

        T value{};
        std::memcpy(&value, bytes.data(), sizeof(T));
        return value;
    }

    template <typename T>
    void write(std::uint32_t address, T value, Endian endian = Endian::big) {
        static_assert(std::is_trivially_copyable_v<T>);
        static_assert(!std::is_pointer_v<T>);
        static_assert(
            std::is_arithmetic_v<T> || std::is_enum_v<T>,
            "write<T> is for scalar values; use write_array<T> or write_memory for aggregates");

        std::array<std::uint8_t, sizeof(T)> bytes{};
        std::memcpy(bytes.data(), &value, sizeof(T));
        if constexpr (sizeof(T) > 1) {
            const bool reverse =
                (endian == Endian::big && std::endian::native == std::endian::little) ||
                (endian == Endian::little && std::endian::native == std::endian::big);
            if (reverse) {
                std::reverse(bytes.begin(), bytes.end());
            }
        }
        write_memory(address, std::span<const std::uint8_t>(bytes));
    }

    template <typename T>
    [[nodiscard]] std::vector<T> read_array(
        std::uint32_t address,
        std::size_t count,
        Endian endian = Endian::big) {
        static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>);
        static_assert(!std::is_same_v<std::remove_cv_t<T>, bool>);
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw ProtocolError("Array memory read size overflow");
        }

        auto bytes = read_memory(address, count * sizeof(T));
        std::vector<T> result(count);
        const bool reverse = sizeof(T) > 1 &&
            ((endian == Endian::big && std::endian::native == std::endian::little) ||
             (endian == Endian::little && std::endian::native == std::endian::big));
        for (std::size_t index = 0; index < count; ++index) {
            auto first = bytes.begin() + static_cast<std::ptrdiff_t>(index * sizeof(T));
            if (reverse) {
                std::reverse(first, first + sizeof(T));
            }
            std::memcpy(&result[index], bytes.data() + index * sizeof(T), sizeof(T));
        }
        return result;
    }

    template <typename T>
    void write_array(
        std::uint32_t address,
        std::span<const T> values,
        Endian endian = Endian::big) {
        static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>);
        static_assert(!std::is_same_v<std::remove_cv_t<T>, bool>);
        if (values.size() > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw ProtocolError("Array memory write size overflow");
        }

        ByteBuffer bytes(values.size() * sizeof(T));
        const bool reverse = sizeof(T) > 1 &&
            ((endian == Endian::big && std::endian::native == std::endian::little) ||
             (endian == Endian::little && std::endian::native == std::endian::big));
        for (std::size_t index = 0; index < values.size(); ++index) {
            auto* output = bytes.data() + index * sizeof(T);
            std::memcpy(output, &values[index], sizeof(T));
            if (reverse) {
                std::reverse(output, output + sizeof(T));
            }
        }
        write_memory(address, bytes);
    }

    void write_branch(
        std::uint32_t address,
        std::uint32_t destination,
        bool linked = false);
    void write_jump(
        std::uint32_t address,
        std::uint32_t destination,
        std::uint8_t scratch_register = 11,
        bool linked = false);
    [[nodiscard]] std::uint32_t allocate_executable(std::uint32_t size);
    [[nodiscard]] ExecutablePoolInfo executable_pool_info();
    // Rewinds the plugin's bump allocator without clearing memory or removing
    // hooks. The confirmation argument makes accidental reuse of live hook
    // addresses harder.
    void reset_executable_pool(ExecutablePoolReset confirmation);

    [[nodiscard]] RpcValue call(
        std::uint32_t address,
        ReturnType return_type = ReturnType::int32,
        const std::vector<RpcArgument>& arguments = {},
        CallOptions options = {});

    [[nodiscard]] RpcValue call(
        std::string_view module,
        std::uint32_t ordinal,
        ReturnType return_type = ReturnType::int32,
        const std::vector<RpcArgument>& arguments = {},
        CallOptions options = {});

    [[nodiscard]] std::uint32_t call_uint32(
        std::uint32_t address,
        const std::vector<RpcArgument>& arguments = {},
        CallOptions options = {});
    [[nodiscard]] std::uint64_t call_uint64(
        std::uint32_t address,
        const std::vector<RpcArgument>& arguments = {},
        CallOptions options = {});
    [[nodiscard]] float call_float(
        std::uint32_t address,
        const std::vector<RpcArgument>& arguments = {},
        CallOptions options = {});
    [[nodiscard]] std::string call_string(
        std::uint32_t address,
        const std::vector<RpcArgument>& arguments = {},
        CallOptions options = {});
    void call_void(
        std::uint32_t address,
        const std::vector<RpcArgument>& arguments = {},
        CallOptions options = {});

    [[nodiscard]] std::uint32_t resolve_function(
        std::string_view module,
        std::uint32_t ordinal);

    [[nodiscard]] std::string console_name();
    [[nodiscard]] std::string console_id();
    [[nodiscard]] std::string console_type();
    [[nodiscard]] bool is_devkit();
    [[nodiscard]] std::string cpu_key();
    [[nodiscard]] std::string gamertag();
    [[nodiscard]] std::uint32_t title_id();
    [[nodiscard]] std::string title_path();
    [[nodiscard]] std::uint16_t kernel_version();
    [[nodiscard]] std::string motherboard_type();
    [[nodiscard]] float temperature(TemperatureSensor sensor);
    [[nodiscard]] std::string dm_version();
    [[nodiscard]] std::vector<std::string> drives();
    // Raw `modules` lines. Prefer module_list() unless a field this client
    // does not model is needed.
    [[nodiscard]] std::vector<std::string> modules();

    // Parsed module table, including each module's base, size and dll flag.
    [[nodiscard]] std::vector<ModuleInfo> module_list();

    // The running title's image range, or nullopt when no title is loaded.
    //
    // Worth reading rather than assuming: a tool that guesses the code range
    // stops recognising anything above its guess, and images vary by tens of
    // megabytes between games.
    [[nodiscard]] std::optional<ModuleInfo> title_module();
    [[nodiscard]] std::uint32_t module_handle(std::string_view module_name);
    [[nodiscard]] std::uint32_t process_id();
    [[nodiscard]] SignInState sign_in_state(std::uint32_t user_index = 0);

    [[nodiscard]] std::vector<DirectoryEntry> directory_contents(
        std::string_view remote_path);
    [[nodiscard]] bool is_directory(std::string_view remote_path);
    void create_directory(std::string_view remote_path);
    void delete_file(std::string_view remote_path);
    void delete_directory(std::string_view remote_path);
    void rename_path(std::string_view old_path, std::string_view new_path);
    [[nodiscard]] ByteBuffer download_file(
        std::string_view remote_path,
        std::size_t maximum_size = 512 * 1024 * 1024);
    void upload_file(
        std::string_view remote_path,
        std::span<const std::uint8_t> data,
        bool overwrite = false);
    // Streaming local-path transfers. Progress callbacks run while the binary
    // exchange is reserved and must not invoke another operation on this
    // Client. Returning TransferControl::cancel closes an in-progress wire
    // exchange and reports cancellation without throwing.
    [[nodiscard]] TransferResult download_file_to(
        std::string_view remote_path,
        const std::filesystem::path& local_path,
        FileTransferOptions options = {});
    [[nodiscard]] TransferResult upload_file_from(
        const std::filesystem::path& local_path,
        std::string_view remote_path,
        FileTransferOptions options = {});
    [[nodiscard]] TransferResult download_directory_to(
        std::string_view remote_root,
        const std::filesystem::path& local_root,
        DirectoryTransferOptions options = {});
    [[nodiscard]] TransferResult upload_directory_from(
        const std::filesystem::path& local_root,
        std::string_view remote_root,
        DirectoryTransferOptions options = {});

    // XBDM sends screenshot metadata followed directly by a tiled
    // framebuffer. Raw capture is available for custom image pipelines;
    // capture_screenshot decodes it and save_screenshot writes a PNG to the
    // exact caller-selected path.
    [[nodiscard]] RawScreenshot capture_raw_screenshot(
        std::size_t maximum_framebuffer_size =
            default_maximum_screenshot_size);
    [[nodiscard]] ScreenshotImage capture_screenshot(
        ScreenshotOptions options = {});
    void save_screenshot(
        const std::filesystem::path& output_path,
        ScreenshotOptions options = {});

    void debug_go();
    void debug_stop();
    void reboot();
    void shutdown();
    void launch_xex(
        std::string_view title_path,
        std::string_view working_directory);
    [[nodiscard]] std::string load_module(std::string_view remote_path);
    void unload_module(std::string_view module_name);
    void set_system_time(std::chrono::system_clock::time_point value);
    void synchronize_time();

    // JRPC-backed on every supported SRPC installation, including consoles
    // where automatic selection chooses the native `s360` command. Entries
    // persist until reboot; the current plugin has 40 slots and no remove
    // operation. Avoid duplicates and do not unload the plugin after adding
    // entries because its worker thread remains active.
    void constant_memory_set(
        std::uint32_t address,
        std::uint32_t value,
        std::optional<std::uint32_t> if_value = std::nullopt,
        std::optional<std::uint32_t> title_id = std::nullopt);

    void notify(std::string_view text, std::uint32_t type = 34);
    void notify(std::string_view text, NotificationType type);
    void set_leds(LedColor q1, LedColor q2, LedColor q3, LedColor q4);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace srpc
