#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace srpc {

enum class Protocol {
    automatic,
    native_srpc,
    jrpc2,
};

enum class ReturnType : std::uint32_t {
    void_ = 0,
    int32 = 1,
    string = 2,
    float32 = 3,
    byte = 4,
    int32_array = 5,
    float_array = 6,
    byte_array = 7,
    uint64 = 8,
};

enum class Endian {
    big,
    little,
};

enum class TemperatureSensor : std::uint8_t {
    cpu = 0,
    gpu = 1,
    memory = 2,
    motherboard = 3,
};

enum class LedColor : std::uint8_t {
    off = 0,
    red = 8,
    green = 128,
    orange = 136,
};

enum class SignInState : std::uint32_t {
    not_signed_in = 0,
    signed_in_locally = 1,
    signed_in_to_xbox_live = 2,
    guest_account_locally = 3,
    guest_account_xbox_live = 4,
};

// XNotify toast styles. The gaps match the values used by the Xbox 360
// notification API; names such as `achievement` select an icon/style only.
enum class NotificationType : std::uint32_t {
    friend_online = 0,
    game_invite = 1,
    friend_request = 2,
    generic = 3,
    multi_pending = 4,
    personal_message = 5,
    signed_out = 6,
    signed_in = 7,
    signed_in_live = 8,
    signed_in_need_pass = 9,
    chat_request = 10,
    connection_lost = 11,
    download_complete = 12,
    song_playing = 13,
    preferred_review = 14,
    avoid_review = 15,
    complaint = 16,
    chat_callback = 17,
    removed_mu = 18,
    removed_gamepad = 19,
    chat_join = 20,
    chat_leave = 21,
    game_invite_sent = 22,
    cancel_persistent = 23,
    chat_callback_sent = 24,
    multi_friend_online = 25,
    one_friend_online = 26,
    achievement = 27,
    hybrid_disc = 28,
    mailbox = 29,
    video_chat_invite = 30,
    download_completed_ready_to_play = 31,
    cannot_download = 32,
    download_stopped = 33,
    console_message = 34,
    game_message = 35,
    device_full = 36,
    chat_message = 38,
    multi_achievements = 39,
    nudge = 40,
    messenger_connection_lost = 41,
    messenger_sign_in_failed = 43,
    messenger_conversation_missed = 44,
    family_timer_remaining = 45,
    connection_lost_reconnect = 46,
    excessive_play_time = 47,
    party_join_request = 49,
    party_invite_sent = 50,
    party_game_invite_sent = 51,
    party_kicked = 52,
    party_disconnected = 53,
    party_cannot_connect = 56,
    party_someone_joined = 57,
    party_someone_left = 58,
    gamer_picture_unlocked = 59,
    avatar_award_unlocked = 60,
    party_joined = 61,
    removed_usb = 62,
    player_muted = 63,
    player_unmuted = 64,
    chat_message2 = 65,
    kinect_connected = 66,
    kinect_break = 67,
    ethernet = 68,
    kinect_player_recognized = 69,
    console_shutting_down_soon_alert = 70,
    profile_signed_in_elsewhere = 71,
    last_sign_in_elsewhere = 73,
    kinect_device_unsupported = 74,
    wireless_device_turn_off = 75,
    updating = 76,
    smartglass_available = 77,
};

struct DirectoryEntry {
    std::string name;
    std::uint64_t size = 0;
    std::optional<std::uint64_t> created_filetime;
    std::optional<std::uint64_t> changed_filetime;
    bool is_directory = false;
};

// One loaded module, from the `modules` command.
//
// The title is the module that is *not* flagged `dll` and sits at or above
// 0x82000000 - the kernel is not a dll either, but it loads below that.
struct ModuleInfo {
    std::string name;
    std::uint32_t base = 0;
    std::uint32_t size = 0;
    std::uint32_t checksum = 0;
    std::uint32_t timestamp = 0;
    bool is_dll = false;

    [[nodiscard]] constexpr std::uint64_t end() const noexcept {
        return static_cast<std::uint64_t>(base) + size;
    }
};

struct MemoryRegion {
    std::uint32_t base = 0;
    std::uint32_t size = 0;
    std::uint32_t protection = 0;

    [[nodiscard]] constexpr std::uint64_t end() const noexcept {
        return static_cast<std::uint64_t>(base) + size;
    }
};

struct ExecutablePoolInfo {
    std::uint32_t used = 0;
    std::uint32_t free = 0;
};

enum class ExecutablePoolReset {
    confirm_live_allocations_may_be_overwritten,
};

struct Response {
    int status_code = 0;
    std::string message;

    [[nodiscard]] bool ok() const noexcept {
        return status_code >= 200 && status_code < 300;
    }
};

struct ClientOptions {
    Protocol protocol = Protocol::automatic;
    std::uint16_t port = 730;
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::milliseconds io_timeout{5000};
    std::chrono::milliseconds rpc_timeout{10000};
    std::chrono::milliseconds poll_interval{50};
};

struct CallOptions {
    bool system_thread = true;
    bool virtual_machine = false;
    std::uint32_t array_size = 0;
};

using ByteBuffer = std::vector<std::uint8_t>;

class RpcArgument {
public:
    using Storage = std::variant<
        bool,
        std::int32_t,
        std::uint32_t,
        std::int64_t,
        std::uint64_t,
        float,
        std::string,
        ByteBuffer>;

    RpcArgument(bool value) : value_(value) {}
    RpcArgument(std::int32_t value) : value_(value) {}
    RpcArgument(std::uint32_t value) : value_(value) {}
    RpcArgument(std::int64_t value) : value_(value) {}
    RpcArgument(std::uint64_t value) : value_(value) {}
    RpcArgument(float value) : value_(value) {}
    RpcArgument(std::string value) : value_(std::move(value)) {}
    RpcArgument(std::string_view value) : value_(std::string(value)) {}
    RpcArgument(const char* value)
        : value_(value == nullptr ? std::string{} : std::string(value)) {}
    RpcArgument(ByteBuffer value) : value_(std::move(value)) {}
    RpcArgument(std::nullptr_t) : value_(std::uint32_t{0}) {}

    [[nodiscard]] const Storage& storage() const noexcept {
        return value_;
    }

private:
    Storage value_;
};

using RpcValue = std::variant<
    std::monostate,
    std::uint32_t,
    std::uint64_t,
    float,
    std::string,
    std::vector<std::uint32_t>,
    std::vector<float>,
    ByteBuffer>;

} // namespace srpc
