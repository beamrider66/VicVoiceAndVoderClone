#pragma once

#include <stddef.h>

// Collect a complete USB line. Never execute a truncated command or sentence.
template <size_t Capacity> class LineBuffer {
public:
  enum class Result { Pending, Ready, Overflow };

  Result append(char c) {
    if (c == '\r' || c == '\n') {
      buffer_[length_] = 0;
      Result result = overflow_ ? Result::Overflow : (length_ ? Result::Ready : Result::Pending);
      length_ = 0;
      overflow_ = false;
      return result;
    }
    if (overflow_) {
      return Result::Pending;
    }
    if (c == '\b') {
      if (length_) {
        --length_;
      }
    } else if (c != 0) {
      if (length_ < Capacity) {
        buffer_[length_++] = c;
      } else {
        overflow_ = true;
      }
    }
    return Result::Pending;
  }

  const char *line() const { return buffer_; }

private:
  char buffer_[Capacity + 1] = {};
  size_t length_ = 0;
  bool overflow_ = false;
};
