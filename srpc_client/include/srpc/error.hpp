#pragma once

#include <stdexcept>
#include <string>

namespace srpc {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ConnectionError : public Error {
public:
    using Error::Error;
};

class ProtocolError : public Error {
public:
    using Error::Error;
};

class CommandError : public Error {
public:
    CommandError(int status_code, std::string message);

    [[nodiscard]] int status_code() const noexcept;

private:
    int status_code_;
};

class RpcError : public Error {
public:
    using Error::Error;
};

class TimeoutError : public Error {
public:
    using Error::Error;
};

} // namespace srpc
