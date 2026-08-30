#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace srpc {

struct DiscoveredConsole {
    std::string host;
    std::string name;
    std::uint16_t port = 730;
};

struct DiscoveryOptions {
    std::uint16_t port = 730;
    std::chrono::milliseconds connect_timeout{250};
    std::chrono::milliseconds io_timeout{500};
    std::size_t max_concurrency = 32;

    // Large networks are reduced to the /24 containing each local address.
    // This cap also bounds the number of probes generated per adapter.
    std::size_t max_candidates_per_adapter = 1024;
};

// Checks one explicit IP address or hostname and returns its XBDM name when
// available. An unreachable host or a non-XBDM service returns std::nullopt.
[[nodiscard]] std::optional<DiscoveredConsole> probe_console(
    std::string_view host,
    DiscoveryOptions options = {});

// Scans the IPv4 subnets of active local network adapters for XBDM port 730.
[[nodiscard]] std::vector<DiscoveredConsole> discover_consoles(
    DiscoveryOptions options = {});

// Returns the first automatically discovered console.
[[nodiscard]] std::optional<DiscoveredConsole> discover_console(
    DiscoveryOptions options = {});

// Tries automatic discovery first, then probes fallback_host only when no
// console was found on a local subnet.
[[nodiscard]] std::optional<DiscoveredConsole> discover_console(
    std::string_view fallback_host,
    DiscoveryOptions options = {});

} // namespace srpc
