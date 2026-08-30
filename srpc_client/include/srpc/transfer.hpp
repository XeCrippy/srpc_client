#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace srpc {

enum class ExistingFilePolicy {
    fail,
    overwrite,
    skip,
};

enum class TransferPhase {
    scanning,
    transferring,
    file_complete,
    file_skipped,
};

enum class TransferControl {
    continue_,
    cancel,
};

struct TransferProgress {
    TransferPhase phase = TransferPhase::transferring;
    std::filesystem::path local_path;
    std::string remote_path;
    std::uint64_t file_bytes_transferred = 0;
    std::uint64_t file_bytes_total = 0;
    std::uint64_t overall_bytes_transferred = 0;
    std::optional<std::uint64_t> overall_bytes_total;
    std::size_t file_index = 0;
    std::optional<std::size_t> file_count;
};

using TransferCallback =
    std::function<TransferControl(const TransferProgress& progress)>;

struct FileTransferOptions {
    ExistingFilePolicy existing_file = ExistingFilePolicy::fail;
    std::size_t chunk_size = 64 * 1024;
    std::uint64_t maximum_file_size = 512ULL * 1024 * 1024;
    bool remove_partial_file = true;
    TransferCallback progress;
};

struct DirectoryTransferOptions {
    FileTransferOptions files;
    bool create_destination_root = true;
    std::size_t maximum_depth = 64;
    std::size_t maximum_entries = 100000;
    std::optional<std::uint64_t> maximum_total_size;
};

struct TransferResult {
    bool cancelled = false;
    std::size_t files_completed = 0;
    std::size_t files_skipped = 0;
    std::uint64_t bytes_transferred = 0;
};

} // namespace srpc
