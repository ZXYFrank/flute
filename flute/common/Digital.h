#ifndef FLUTE_COMMON_DIGITAL_H
#define FLUTE_COMMON_DIGITAL_H

#include <algorithm>

namespace flute {

const char digits[] = "9876543210123456789";
const char* zero = digits + 9;
static_assert(sizeof(digits) == 20, "wrong number of digits");

const char digits_hex[] = "0123456789ABCDEF";
static_assert(sizeof(digits_hex) == 17, "wrong number of digitsHex");

// Efficient Integer to String Conversions, by Matthew Wilson.
template <typename T>
size_t integer_to_string(char buf[], T value) {
  T i = value;
  char* p = buf;

  do {
    int lsd = static_cast<int>(i % 10);
    i /= 10;
    *p++ = zero[lsd];
  } while (i != 0);

  if (value < 0) {
    *p++ = '-';
  }
  *p = '\0';
  std::reverse(buf, p);

  return p - buf;
}

size_t hex_to_string(char buf[], uintptr_t value) {
  uintptr_t i = value;
  char* p = buf;

  do {
    int lsd = static_cast<int>(i % 16);
    i /= 16;
    *p++ = digits_hex[lsd];
  } while (i != 0);

  *p = '\0';
  std::reverse(buf, p);

  return p - buf;
}

}  // namespace flute

#endif  // FLUTE_COMMON_DIGITAL_H