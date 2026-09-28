// Minimal JSON writer into a caller-provided buffer. Strings are escaped, commas are
// placed automatically, and running out of space is reported instead of producing
// truncated JSON.
//
//   JsonWriter j(buf, sizeof buf);
//   j.beginObject();
//   j.field("call", "K1ABC");
//   j.field("dbm", 23);
//   j.endObject();
//   if (j.ok()) send(j.c_str());
#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <type_traits>

class JsonWriter {
 public:
  JsonWriter(char *buf, size_t size) : buf_(buf), size_(size) { buf_[0] = '\0'; }

  void beginObject(const char *key = nullptr) { open(key, '{'); }
  void endObject() { close('}'); }
  void beginArray(const char *key = nullptr) { open(key, '['); }
  void endArray() { close(']'); }

  // `key` is null for array elements.
  void field(const char *key, const char *value) {
    separator(key);
    string(value);
  }
  void field(const char *key, bool value) {
    separator(key);
    append(value ? "true" : "false");
  }
  void field(const char *key, double value, int decimals) {
    separator(key);
    appendf("%.*f", decimals, value);
  }
  template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
  void field(const char *key, T value) {
    separator(key);
    if (std::is_signed<T>::value) appendf("%lld", (long long)value);
    else appendf("%llu", (unsigned long long)value);
  }

  bool ok() const { return !overflow_ && depth_ == 0; }
  const char *c_str() const { return buf_; }
  size_t length() const { return len_; }

 private:
  static constexpr int MAX_DEPTH = 8;

  void open(const char *key, char bracket) {
    separator(key);
    appendChar(bracket);
    if (depth_ < MAX_DEPTH) first_[depth_] = true;
    depth_++;
  }

  void close(char bracket) {
    if (depth_ > 0) depth_--;
    appendChar(bracket);
  }

  void separator(const char *key) {
    if (depth_ > 0 && depth_ <= MAX_DEPTH) {
      if (!first_[depth_ - 1]) appendChar(',');
      first_[depth_ - 1] = false;
    }
    if (key) {
      string(key);
      appendChar(':');
    }
  }

  void string(const char *s) {
    appendChar('"');
    for (; *s; s++) {
      unsigned char c = (unsigned char)*s;
      if (c == '"' || c == '\\') {
        appendChar('\\');
        appendChar((char)c);
      } else if (c < 0x20) {
        appendf("\\u%04x", c);
      } else {
        appendChar((char)c);
      }
    }
    appendChar('"');
  }

  void append(const char *s) {
    while (*s) appendChar(*s++);
  }

  void appendChar(char c) {
    if (len_ + 1 >= size_) {
      overflow_ = true;
      return;
    }
    buf_[len_++] = c;
    buf_[len_] = '\0';
  }

  __attribute__((format(printf, 2, 3))) void appendf(const char *fmt, ...) {
    char tmp[32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, args);
    va_end(args);
    append(tmp);
  }

  char *buf_;
  size_t size_;
  size_t len_ = 0;
  int depth_ = 0;
  bool first_[MAX_DEPTH] = {};
  bool overflow_ = false;
};
