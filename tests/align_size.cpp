#include "gtest/gtest.h"
#include "bfdr/buffered_fd_reader.h"

TEST(ALIGN_SIZE, TEST) {
  EXPECT_EQ(bfdr::alignSize(0x123456, 0x100), 0x123500);
  EXPECT_EQ(bfdr::alignSize(0x123400, 0x100), 0x123400);
  EXPECT_EQ(bfdr::alignSize(0x123400, 0x800), 0x123800);
}
