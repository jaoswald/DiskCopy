// Construct big-endian numbers from bytes.
//
#include <cstdint>

inline uint16_t BigEndian2(const char b[2]) {
  const uint8_t* c = (const uint8_t*)b;
  return c[0] << 8 | c[1];
}

inline uint32_t BigEndian4(const char b[4]) {
  const uint8_t* c = (const uint8_t*)b;
  return c[0] << 24 | c[1] << 16 | c[2] << 8 | c[3];
}

inline void WriteBigEndian2(uint16_t value, char bytes[2]) {
  bytes[0] = value >> 8;
  bytes[1] = value & 0xff;
}

inline void WriteBigEndian4(uint32_t value, char bytes[4]) {
  bytes[0] = (value >> 24);
  bytes[1] = (value >> 16) & 0xff;
  bytes[2] = (value >> 8) & 0xff;
  bytes[3] = value & 0xff;
}
