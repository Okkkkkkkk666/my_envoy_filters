#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include "gtest/gtest.h"
#include "contrib/common/fpe/fpe.h"
#include "source/common/common/logger.h" // 引入 Envoy 日志系统

namespace Envoy {
namespace Extensions {
namespace Common {
namespace FPE {
namespace {

/**
 * @class FpeLibraryTest
 * @brief FPE 底层密码学引擎的全面单元测试
 */
class FpeLibraryTest : public testing::Test, public Logger::Loggable<Logger::Id::filter> {
protected:
  void SetUp() override {
    fpe_ = &getFPE();
    
    // 初始化标准的 AES-128 密钥 (16字节)
    std::string key = "1234567890123456";
    std::string tweak = "envoy_gateway_tweak_salt";
    
    ASSERT_TRUE(fpe_->initFromKey(key, tweak)) << "FPE Engine failed to initialize.";
    ENVOY_LOG(info, "FPE Test Environment SetUp Complete.");
  }

  void verifyRoundTrip(const std::string& plaintext, DataType type) {
    std::string ciphertext = fpe_->encrypt(plaintext, type);
    std::string decrypted = fpe_->decrypt(ciphertext, type);

    ENVOY_LOG(trace, "FPE RoundTrip: type={}, plain='{}', cipher='{}'", static_cast<int>(type), plaintext, ciphertext);

    EXPECT_NE(plaintext, ciphertext) << "Ciphertext should not be equal to plaintext (unless bypassed).";
    EXPECT_EQ(plaintext, decrypted) << "Decrypted text must match the original plaintext perfectly.";
    EXPECT_EQ(plaintext.length(), ciphertext.length()) << "FPE must preserve the exact length.";
  }

  FPEInterface* fpe_;
};

// ============================================================================
// 1. 核心业务路由测试
// ============================================================================

TEST_F(FpeLibraryTest, EmailFormatPreservation) {
  std::string plaintext = "admin.support@company.com";
  verifyRoundTrip(plaintext, DataType::EMAIL);
  
  std::string ciphertext = fpe_->encrypt(plaintext, DataType::EMAIL);
  size_t at_pos = ciphertext.find('@');
  EXPECT_EQ(ciphertext.substr(at_pos), "@company.com");
}

TEST_F(FpeLibraryTest, IdCardFormatPreservation) {
  std::string plaintext = "11010519900101123X";
  verifyRoundTrip(plaintext, DataType::ID_CARD);
  
  std::string ciphertext = fpe_->encrypt(plaintext, DataType::ID_CARD);
  EXPECT_EQ(ciphertext.back(), 'X');
}

// ============================================================================
// 2. 密钥敏感性与确定性测试 (新增加)
// ============================================================================

TEST_F(FpeLibraryTest, KeyAndTweakSensitivity) {
  std::string plaintext = "SensitiveData_123456";
  
  // 1. 验证相同 Key/Tweak 的确定性
  std::string cipher1 = fpe_->encrypt(plaintext, DataType::RAW_TEXT);
  std::string cipher2 = fpe_->encrypt(plaintext, DataType::RAW_TEXT);
  EXPECT_EQ(cipher1, cipher2);
  ENVOY_LOG(debug, "FPE Deterministic Check: cipher1 == cipher2");

  // 2. 验证 Tweak 变动导致密文不同
  fpe_->initFromKey("1234567890123456", "different_tweak_salt");
  std::string cipher_diff_tweak = fpe_->encrypt(plaintext, DataType::RAW_TEXT);
  EXPECT_NE(cipher1, cipher_diff_tweak);

  // 3. 验证 Key 变动导致密文不同
  fpe_->initFromKey("6543210987654321", "envoy_gateway_tweak_salt");
  std::string cipher_diff_key = fpe_->encrypt(plaintext, DataType::RAW_TEXT);
  EXPECT_NE(cipher1, cipher_diff_key);
  
  ENVOY_LOG(info, "FPE Sensitivity Test Passed: Key/Tweak changes effectively rotate ciphertext.");
}

// ============================================================================
// 3. 畸形输入与边缘测试 (修复并增加)
// ============================================================================

TEST_F(FpeLibraryTest, EmptyAndShortInputs) {
  // 空字符串原样返回
  EXPECT_EQ(fpe_->encrypt("", DataType::RAW_TEXT), "");
  
  // 极短字符串 (1个字符) 触发 Bypass
  std::string single_char = "A";
  EXPECT_EQ(fpe_->encrypt(single_char, DataType::RAW_TEXT), "A");
  
  ENVOY_LOG(debug, "FPE Bypass Test: Short strings handled correctly.");
}

TEST_F(FpeLibraryTest, MaxSafeLengthBoundary) {
  // 匹配 uint64_t 环境下的安全边界：纯数字最大 36 位
  std::string max_safe_digits(36, '1'); 
  std::string ciphertext = fpe_->encrypt(max_safe_digits, DataType::RAW_TEXT);
  
  // 36位以内，必须被正常加密
  EXPECT_NE(max_safe_digits, ciphertext);
  verifyRoundTrip(max_safe_digits, DataType::RAW_TEXT);

  // 超过 36 位 (例如 37 位)，为了防止 uint64_t 溢出，引擎应该触发安全降级，原样放行
  std::string over_limit_digits(37, '1');
  EXPECT_EQ(fpe_->encrypt(over_limit_digits, DataType::RAW_TEXT), over_limit_digits);
}

// ============================================================================
// 4. 并发安全性测试
// ============================================================================

TEST_F(FpeLibraryTest, ThreadSafety) {
  const int num_threads = 8;
  const int iterations = 500;
  const std::string plaintext = "Concurrent_Test@2026";
  
  std::vector<std::thread> threads;
  std::vector<std::string> results(num_threads);

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, i, &results, plaintext]() {
      for (int j = 0; j < iterations; ++j) {
        results[i] = fpe_->encrypt(plaintext, DataType::RAW_TEXT);
      }
    });
  }

  for (auto& t : threads) { t.join(); }

  std::string expected = fpe_->encrypt(plaintext, DataType::RAW_TEXT);
  for (const auto& res : results) {
    EXPECT_EQ(res, expected);
  }
  ENVOY_LOG(info, "FPE ThreadSafety: {} threads verified successfully.", num_threads);
}

// ============================================================================
// 5. 性能基准测试 (新增加)
// ============================================================================

TEST_F(FpeLibraryTest, PerformanceBenchmark) {
  const int iterations = 5000;
  std::string plaintext = "INSERT INTO users (name, email) VALUES ('Alex', 'test@envoy.io')";
  
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    fpe_->encrypt(plaintext, DataType::RAW_TEXT);
  }
  auto end = std::chrono::high_resolution_clock::now();
  
  std::chrono::duration<double, std::milli> elapsed = end - start;
  double avg_ms = elapsed.count() / iterations;
  
  ENVOY_LOG(info, "FPE Performance: Total {}ms for {} ops. Avg: {} ms/op", 
            elapsed.count(), iterations, avg_ms);
  
  // 性能门禁：单次加密不应超过 2ms (dbg 模式下通常较慢)
  EXPECT_LT(avg_ms, 2.0);
}

} // namespace
} // namespace FPE
} // namespace Common
} // namespace Extensions
} // namespace Envoy