#include <cmath>
#include <bits/stdint-uintn.h>

#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/strong_local_ratelimit/impl/quota.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {

class QuotaTest : public testing::Test {
public:
  QuotaTest() = default;

  void Consume(uint32_t duration, uint32_t max_count, int step) {
    Impl::Quota quota(duration, max_count);

    // 第0个窗口，此时等同于固定窗口
    uint64_t now = quota.createdTick();
    for (size_t i = 0; i < quota.maxCount(); i++) {
      ASSERT_TRUE(quota.consume(now));
      ASSERT_EQ(quota.maxCount() - quota.passCount(), quota.maxCount() - i - 1);
      now += 1;
    }
    ASSERT_FALSE(quota.consume(now));
    ASSERT_EQ(quota.passCount(), max_count);

    // 第1个窗口，此时为滑动窗口。每次前进一小段时间，验证是否正确
    now = quota.createdTick() + quota.duration() * 1000;
    double remain = 0;
    for (int i = 0; i < step; i++) {
      if (i == 0) {
        now += (quota.duration() * 1000) / step - 1;
      } else if (i == step - 1) {
        now = quota.createdTick() + quota.duration() * 1000 * 2 - 1;
      } else {
        now += (quota.duration() * 1000) / step;
      }
      double count = static_cast<double>(quota.maxCount()) / step;
      if (count >= 1) {
        for (size_t j = 0; j < std::floor(count); j++) {
          ASSERT_TRUE(quota.consume(now));
        }
      }

      remain += count - std::floor(count);
      if (remain > 0) {
        size_t compensation = std::ceil(remain);
        for (size_t i = 0; i < compensation; i++) {
          if (quota.consume(now)) {
            remain -= 1;
            remain = remain < 0 ? 0 : remain;
          } else {
            break;
          }
        }
      }
    }
    EXPECT_EQ(quota.passCount(), max_count);
    ASSERT_FALSE(quota.consume(now));

    // 第3个窗口，此时等同于固定窗口，因为第2个窗口没有消费
    now = quota.createdTick() + quota.duration() * 1000 * 3;
    for (size_t i = 0; i < quota.maxCount(); i++) {
      ASSERT_TRUE(quota.consume(now));
      ASSERT_EQ(quota.maxCount() - quota.passCount(), quota.maxCount() - i - 1);
      now += 1;
    }
    ASSERT_FALSE(quota.consume(now));
    ASSERT_EQ(quota.passCount(), max_count);
  }
};

// 验证每秒100限额，滑动窗口每次步进1/4时，是否正确
TEST_F(QuotaTest, Consume0) { Consume(1, 100, 4); }

// 验证每秒100限额，滑动窗口每次步进1/8时，是否正确
TEST_F(QuotaTest, Consume1) { Consume(1, 100, 8); }

// 验证每秒100限额，滑动窗口每次步进1/16时，是否正确
TEST_F(QuotaTest, Consume2) { Consume(1, 100, 16); }

// 验证每秒100限额，滑动窗口每次步进1/32时，是否正确
TEST_F(QuotaTest, Consume3) { Consume(1, 100, 32); }

// 验证每秒100限额，滑动窗口每次步进1/64时，是否正确
TEST_F(QuotaTest, Consume4) { Consume(1, 100, 64); }

// 验证每秒100限额，滑动窗口每次步进1/128时，是否正确
TEST_F(QuotaTest, Consume5) { Consume(1, 100, 128); }

// 验证每秒100限额，滑动窗口每次步进1/256时，是否正确
TEST_F(QuotaTest, Consume6) { Consume(1, 100, 256); }

// 验证每秒100限额，滑动窗口每次步进1/512时，是否正确
TEST_F(QuotaTest, Consume7) { Consume(1, 100, 512); }

// 验证每7秒99限额是否正确
TEST_F(QuotaTest, Consume8) { Consume(7, 99, 4); }
TEST_F(QuotaTest, Consume9) { Consume(7, 99, 8); }
TEST_F(QuotaTest, Consume10) { Consume(7, 99, 16); }
TEST_F(QuotaTest, Consume11) { Consume(7, 99, 32); }
TEST_F(QuotaTest, Consume12) { Consume(7, 99, 64); }
TEST_F(QuotaTest, Consume13) { Consume(7, 99, 128); }
TEST_F(QuotaTest, Consume14) { Consume(7, 99, 256); }
TEST_F(QuotaTest, Consume15) { Consume(7, 99, 512); }

// 验证每188秒1999限额是否正确
TEST_F(QuotaTest, Consume16) { Consume(188, 1999, 4); }
TEST_F(QuotaTest, Consume17) { Consume(188, 1999, 8); }
TEST_F(QuotaTest, Consume18) { Consume(188, 1999, 16); }
TEST_F(QuotaTest, Consume19) { Consume(188, 1999, 32); }
TEST_F(QuotaTest, Consume20) { Consume(188, 1999, 64); }
TEST_F(QuotaTest, Consume21) { Consume(188, 1999, 128); }
TEST_F(QuotaTest, Consume22) { Consume(188, 1999, 256); }
TEST_F(QuotaTest, Consume23) { Consume(188, 1999, 512); }

// 验证每60秒100限额，随机步进时，是否正确
TEST_F(QuotaTest, Consume24) {
  srand(time(nullptr));
  const size_t test_count = 100000;
  for (size_t i = 0; i < test_count; i++) {
    int step = rand() % 10000;
    Consume(60, 100, step);
  }
}

} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy