#include "disk_copy.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(DiskCopyChecksum, Rotate1Bit) {
  DiskCopyChecksum sum(0);
  uint32_t checksum = sum.UpdateSum(0x00000001);
  EXPECT_EQ(0x80000000, checksum);
  EXPECT_EQ(0x80000000, sum.Sum());
}

TEST(DiskCopyChecksum, Rotate0Bit) {
  DiskCopyChecksum sum(0);
  uint32_t checksum = sum.UpdateSum(0);
  EXPECT_EQ(0, checksum);
  EXPECT_EQ(0, sum.Sum());
}

TEST(DiskCopyChecksum, Rotate1BitMoreComplex) {
  DiskCopyChecksum sum(0);
  uint32_t checksum = sum.UpdateSum(0x2469);
  EXPECT_EQ(0x80001234, checksum);
  EXPECT_EQ(0x80001234, sum.Sum());
}
