#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_3_x/libs/encode/cryptopp.h"
using testing::_;
using testing::Return;
namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Test {
namespace Libs {
namespace Encode {
class MockCryptopp : public SrhinoPluginFramework::v1_3_x::Libs::Encode::Cryptopp {
public:
  MockCryptopp()
      : mock_des_(std::make_shared<MockDes>()), mock_triple_des_(std::make_shared<MockTripleDes>()),
        mock_aes_(std::make_shared<MockAes>()), mock_base64_(std::make_shared<MockBase64>()),
        mock_rsa_(std::make_shared<MockRsa>()), mock_md5_(std::make_shared<MockMd5>()),
        mock_sha256_(std::make_shared<MockSha256>()), mock_sha512_(std::make_shared<MockSha512>()),
        mock_sha1_(std::make_shared<MockSha1>()), mock_sha3_(std::make_shared<MockSha3>()),
        mock_sm3_(std::make_shared<MockSm3>()) {
    EXPECT_CALL(*this, des()).WillRepeatedly(testing::Return(mock_des_));
    EXPECT_CALL(*this, triple_des()).WillRepeatedly(testing::Return(mock_triple_des_));
    EXPECT_CALL(*this, aes()).WillRepeatedly(testing::Return(mock_aes_));
    EXPECT_CALL(*this, base64()).WillRepeatedly(testing::Return(mock_base64_));
    EXPECT_CALL(*this, rsa()).WillRepeatedly(testing::Return(mock_rsa_));
    EXPECT_CALL(*this, md5()).WillRepeatedly(testing::Return(mock_md5_));
    EXPECT_CALL(*this, sha256()).WillRepeatedly(testing::Return(mock_sha256_));
    EXPECT_CALL(*this, sha1()).WillRepeatedly(testing::Return(mock_sha1_));
    EXPECT_CALL(*this, sha3()).WillRepeatedly(testing::Return(mock_sha3_));
    EXPECT_CALL(*this, sm3()).WillRepeatedly(testing::Return(mock_sm3_));
  }

public:
  class MockDes : public Des {
  public:
    MockDes() {
      EXPECT_CALL(*this, encode(_, _, _)).WillRepeatedly(testing::Return(std::string("Des")));
      EXPECT_CALL(*this, decode(_, _, _)).WillRepeatedly(testing::Return(std::string("Des")));
    }

  public:
    MOCK_METHOD(std::string, encode, (const std::string&, const std::string&, Mode), (const));
    MOCK_METHOD(std::string, decode, (const std::string&, const std::string&, Mode), (const));
  };

public:
  class MockTripleDes : public TripleDes {
  public:
    MockTripleDes() {
      EXPECT_CALL(*this, encode(_, _, _)).WillRepeatedly(testing::Return(std::string("TripleDes")));
      EXPECT_CALL(*this, decode(_, _, _)).WillRepeatedly(testing::Return(std::string("TripleDes")));
    }

  public:
    MOCK_METHOD(std::string, encode, (const std::string&, const std::string&, Mode), (const));
    MOCK_METHOD(std::string, decode, (const std::string&, const std::string&, Mode), (const));
  };

public:
  class MockAes : public Aes {
  public:
    MockAes() {
      EXPECT_CALL(*this, encode(_, _, _)).WillRepeatedly(testing::Return(std::string("Aes")));
      EXPECT_CALL(*this, decode(_, _, _)).WillRepeatedly(testing::Return(std::string("Aes")));
    }

  public:
    MOCK_METHOD(std::string, encode, (const std::string&, const std::string&, Mode), (const));
    MOCK_METHOD(std::string, decode, (const std::string&, const std::string&, Mode), (const));
  };

  class MockBase64 : public Base64 {
  public:
    MockBase64() {
      EXPECT_CALL(*this, encode(_)).WillRepeatedly(testing::Return(std::string("Base64")));
      EXPECT_CALL(*this, decode(_)).WillRepeatedly(testing::Return(std::string("Base64")));
    }

  public:
    MOCK_METHOD(std::string, encode, (const std::string&), (const));
    MOCK_METHOD(std::string, decode, (const std::string&), (const));
  };

public:
  class MockRsa : public RSA {
  public:
    MockRsa() {
      EXPECT_CALL(*this, generateRSAKeyPair(_, _, _)).WillRepeatedly(testing::Return());
      EXPECT_CALL(*this, encode(_, _, _)).WillRepeatedly(testing::Return(std::string("rsa")));
      EXPECT_CALL(*this, decode(_, _, _)).WillRepeatedly(testing::Return(std::string("rsa")));
    }

  public:
    MOCK_METHOD(void, generateRSAKeyPair, (std::string&, std::string&, uint32_t));
    MOCK_METHOD(std::string, encode, (const std::string&, const std::string&, RsaMode), (const));
    MOCK_METHOD(std::string, decode, (const std::string&, const std::string&, RsaMode), (const));
  };

public:
  class MockMd5 : public Md5 {
  public:
    MockMd5() { EXPECT_CALL(*this, encode(_)).WillRepeatedly(testing::Return(std::string("Md5"))); }

  public:
    MOCK_METHOD(std::string, encode, (const std::string&), (const));
  };

public:
  class MockSha256 : public Sha256 {
  public:
    MockSha256() {
      EXPECT_CALL(*this, encode(_)).WillRepeatedly(testing::Return(std::string("Sha256")));
    }

  public:
    MOCK_METHOD(std::string, encode, (const std::string&), (const));
  };

public:
  class MockSha512 : public Sha512 {
  public:
    MockSha512() {
      EXPECT_CALL(*this, encode(_)).WillRepeatedly(testing::Return(std::string("Sha512")));
    }

  public:
    MOCK_METHOD(std::string, encode, (const std::string&), (const));
  };

public:
  class MockSha1 : public Sha1 {
  public:
    MockSha1() {
      EXPECT_CALL(*this, encode(_)).WillRepeatedly(testing::Return(std::string("Sha1")));
    }

  public:
    MOCK_METHOD(std::string, encode, (const std::string&), (const));
  };

public:
  class MockSha3 : public Sha3 {
  public:
    MockSha3() {
      EXPECT_CALL(*this, encode(_, _)).WillRepeatedly(testing::Return(std::string("Sha3")));
    }

  public:
    MOCK_METHOD(std::string, encode, (const std::string&, SHA3Type), (const));
  };

public:
  class MockSm3 : public Sm3 {
  public:
    MockSm3() { EXPECT_CALL(*this, encode(_)).WillRepeatedly(testing::Return(std::string("Sm3"))); }

  public:
    MOCK_METHOD(std::string, encode, (const std::string&), (const));
  };

public:
  MOCK_METHOD(DesSharedPtr, des, (), (const));
  MOCK_METHOD(TripleDesSharedPtr, triple_des, (), (const));
  MOCK_METHOD(AesSharedPtr, aes, (), (const));
  MOCK_METHOD(Base64SharedPtr, base64, (), (const));
  MOCK_METHOD(RSASharedPtr, rsa, (), (const));
  MOCK_METHOD(Md5SharedPtr, md5, (), (const));
  MOCK_METHOD(Sha256SharedPtr, sha256, (), (const));
  MOCK_METHOD(Sha512SharedPtr, Sha512, (), (const));
  MOCK_METHOD(Sha1SharedPtr, sha1, (), (const));
  MOCK_METHOD(Sha3SharedPtr, sha3, (), (const));
  MOCK_METHOD(Sm3SharedPtr, sm3, (), (const));

public:
  std::shared_ptr<MockDes> mock_des_;
  std::shared_ptr<MockTripleDes> mock_triple_des_;
  std::shared_ptr<MockAes> mock_aes_;
  std::shared_ptr<MockBase64> mock_base64_;
  std::shared_ptr<MockRsa> mock_rsa_;
  std::shared_ptr<MockMd5> mock_md5_;
  std::shared_ptr<MockSha256> mock_sha256_;
  std::shared_ptr<MockSha512> mock_sha512_;
  std::shared_ptr<MockSha1> mock_sha1_;
  std::shared_ptr<MockSha3> mock_sha3_;
  std::shared_ptr<MockSm3> mock_sm3_;
};

} // namespace Encode
} // namespace Libs
} // namespace Test
} // namespace v1_3_x
} // namespace SrhinoPluginFramework