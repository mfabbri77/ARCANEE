#pragma once

#include "common/Types.h"
#include <optional>
#include <ostream>
#include <string>
#include <variant>

namespace arcanee {

// Status Code Enumeration
enum class StatusCode {
  Ok = 0,
  Cancelled = 1,
  Unknown = 2,
  InvalidArgument = 3,
  DeadlineExceeded = 4,
  NotFound = 5,
  AlreadyExists = 6,
  PermissionDenied = 7,
  ResourceExhausted = 8,
  FailedPrecondition = 9,
  Aborted = 10,
  OutOfRange = 11,
  Unimplemented = 12,
  Internal = 13,
  Unavailable = 14,
  DataLoss = 15,
  Unauthenticated = 16
};

// Simple Status class (like absl::Status)
class Status {
public:
  Status() : m_code(StatusCode::Ok) {}
  Status(StatusCode code, const std::string &msg) : m_code(code), m_msg(msg) {}

  static Status Ok() { return Status(); }
  static Status InternalError(const std::string &msg) {
    return Status(StatusCode::Internal, msg);
  }
  static Status InvalidArgument(const std::string &msg) {
    return Status(StatusCode::InvalidArgument, msg);
  }
  static Status NotFound(const std::string &msg) {
    return Status(StatusCode::NotFound, msg);
  }
  static Status Unimplemented(const std::string &msg) {
    return Status(StatusCode::Unimplemented, msg);
  }

  bool ok() const { return m_code == StatusCode::Ok; }
  StatusCode code() const { return m_code; }
  const std::string &message() const { return m_msg; }

  std::string toString() const {
    if (ok())
      return "OK";
    return std::string(codeToString(m_code)) + ": " + m_msg;
  }

private:
  static const char *codeToString(StatusCode code) {
    switch (code) {
    case StatusCode::Ok:
      return "Ok";
    case StatusCode::Cancelled:
      return "Cancelled";
    case StatusCode::Unknown:
      return "Unknown";
    case StatusCode::InvalidArgument:
      return "InvalidArgument";
    case StatusCode::DeadlineExceeded:
      return "DeadlineExceeded";
    case StatusCode::NotFound:
      return "NotFound";
    case StatusCode::AlreadyExists:
      return "AlreadyExists";
    case StatusCode::PermissionDenied:
      return "PermissionDenied";
    case StatusCode::ResourceExhausted:
      return "ResourceExhausted";
    case StatusCode::FailedPrecondition:
      return "FailedPrecondition";
    case StatusCode::Aborted:
      return "Aborted";
    case StatusCode::OutOfRange:
      return "OutOfRange";
    case StatusCode::Unimplemented:
      return "Unimplemented";
    case StatusCode::Internal:
      return "Internal";
    case StatusCode::Unavailable:
      return "Unavailable";
    case StatusCode::DataLoss:
      return "DataLoss";
    case StatusCode::Unauthenticated:
      return "Unauthenticated";
    default:
      return "UnknownCode";
    }
  }

  StatusCode m_code;
  std::string m_msg;
};

inline std::ostream &operator<<(std::ostream &os, const Status &s) {
  return os << s.toString();
}

// StatusOr<T> - Result type
template <typename T> class StatusOr {
public:
  // Implicit construction from T
  StatusOr(const T &value) : m_impl(value) {}
  StatusOr(T &&value) : m_impl(std::move(value)) {}

  // Implicit construction from Status
  StatusOr(const Status &status) : m_impl(status) {
    if (status.ok()) {
      // Logic error: StatusOr constructed with OK status but no value?
      // Conventionally OK status implies value presence.
      // Here we allow it but value is undefined if we don't handle it.
      // Let's assume OK status construction is forbidden for semantic safety or
      // handle it. Simplest: if status is OK, abort or force Internal error.
      m_impl = Status(StatusCode::Internal,
                      "StatusOr constructed with Status::Ok()");
    }
  }

  bool ok() const { return std::holds_alternative<T>(m_impl); }

  const T &value() const {
    if (!ok()) {
      // Crash or throw? Abort for now.
      std::abort();
    }
    return std::get<T>(m_impl);
  }

  T &value() {
    if (!ok()) {
      std::abort();
    }
    return std::get<T>(m_impl);
  }

  const Status &status() const {
    if (ok()) {
      static Status okStatus;
      return okStatus;
    }
    return std::get<Status>(m_impl);
  }

  // Pointer-like access
  const T *operator->() const { return &value(); }
  T *operator->() { return &value(); }
  const T &operator*() const { return value(); }
  T &operator*() { return value(); }

private:
  std::variant<Status, T> m_impl;
};

// Helper macro for return error propagation
#define ARC_RETURN_IF_ERROR(expr)                                              \
  do {                                                                         \
    const auto &_arc_status = (expr);                                          \
    if (!_arc_status.ok())                                                     \
      return _arc_status;                                                      \
  } while (0)

#define ARC_ASSIGN_OR_RETURN(lhs, expr)                                        \
  auto lhs##_status_or = (expr);                                               \
  if (!lhs##_status_or.ok())                                                   \
    return lhs##_status_or.status();                                           \
  lhs = std::move(lhs##_status_or.value());

} // namespace arcanee
