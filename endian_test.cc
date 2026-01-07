#include "endian.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(Endian, TwoBytes) {
  char b[2];
  const uint16_t val = 0x1234;
  WriteBigEndian2(val, b);
  EXPECT_EQ(b[0], 0x12);
  EXPECT_EQ(b[1], 0x34);
  uint16_t read_val = BigEndian2(b);
  EXPECT_EQ(val, read_val);
}

TEST(Endian, FourBytes) {
  char b[4];
  const uint32_t val = 0x12345678;
  WriteBigEndian4(val, b);
  EXPECT_EQ(b[0], 0x12);
  EXPECT_EQ(b[1], 0x34);
  EXPECT_EQ(b[2], 0x56);
  EXPECT_EQ(b[3], 0x78);
  const uint32_t read_val = BigEndian4(b);
  EXPECT_EQ(val, read_val);
}
