#include "source/common/srhino_plugin_framework/v1_0_x/libs/encode/cryptopp_impl.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace Encode {

static std::string iv;
CryptoppImpl::CryptoppImpl() {}

std::string CryptoppImpl::padData(const std::string& data, size_t block_size) {
  std::string padding_data;
  if ((data.size() % block_size) != 0) {
    const size_t block_num = data.size() / block_size + 1;
    const size_t normal_size = block_num * block_size;
    padding_data.reserve(normal_size);
    padding_data.append(data.data(), data.size());
    padding_data.append(normal_size - data.size(), normal_size - data.size());
  } else {
    padding_data.reserve(data.size() + block_size);
    padding_data.append(data.data(), data.size());
    padding_data.append(block_size, block_size);
  }

  return padding_data;
}

std::string CryptoppImpl::DesImpl::encode(const std::string& data,
                                          const std::string& encryption_key, Mode mode) const {
  std::string encoded;
  // 将key的长度修改为64(bits)
  std::string normal_key = normalizeKey(encryption_key);
  // 生成随机IV值
  CryptoPP::AutoSeededRandomPool prng;
  CryptoPP::byte ivBytes[CryptoPP::DES::BLOCKSIZE];
  prng.GenerateBlock(ivBytes, sizeof(ivBytes));
  iv.assign(reinterpret_cast<const char*>(ivBytes), sizeof(ivBytes));
  // 填充数据
  std::string padded_data = padData(data, CryptoPP::DES::BLOCKSIZE);

  switch (mode) {
  case Mode::ECB: {
    CryptoPP::ECB_Mode<CryptoPP::DES>::Encryption encryptor;
    encryptor.SetKey(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()), normal_key.size());
    CryptoPP::StringSource(data, true,
                           new CryptoPP::StreamTransformationFilter(
                               encryptor,
                               new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded)),
                               CryptoPP::StreamTransformationFilter::PKCS_PADDING));
    break;
  }
  case Mode::CFB: {
    CryptoPP::CFB_Mode<CryptoPP::DES>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), ivBytes);

    CryptoPP::StringSource(
        padded_data, true,
        new CryptoPP::StreamTransformationFilter(
            encryptor, new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded))));
    break;
  }
  case Mode::CTR: {
    CryptoPP::CTR_Mode<CryptoPP::DES>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), ivBytes);

    CryptoPP::StringSource(
        padded_data, true,
        new CryptoPP::StreamTransformationFilter(
            encryptor, new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded))));
    break;
  }
  case Mode::CBC: {
    CryptoPP::CBC_Mode<CryptoPP::DES>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), ivBytes);
    CryptoPP::StringSource(data, true,
                           new CryptoPP::StreamTransformationFilter(
                               encryptor,
                               new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded)),
                               CryptoPP::StreamTransformationFilter::PKCS_PADDING));
    break;
  }
  case Mode::OFB: {
    CryptoPP::OFB_Mode<CryptoPP::DES>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), ivBytes);

    CryptoPP::StringSource(
        padded_data, true,
        new CryptoPP::StreamTransformationFilter(
            encryptor, new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded))));
    break;
  }
  default:
    break;
  }

  return encoded;
}

std::string CryptoppImpl::DesImpl::decode(const std::string& data,
                                          const std::string& encryption_key, Mode mode) const {
  std::string decoded;
  std::string normal_key = normalizeKey(encryption_key);
  switch (mode) {
  case Mode::ECB: {
    CryptoPP::ECB_Mode<CryptoPP::DES>::Decryption decryptor;
    decryptor.SetKey(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()), normal_key.size());
    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded),
                               CryptoPP::StreamTransformationFilter::PKCS_PADDING)));
    break;
  }
  case Mode::CFB: {
    CryptoPP::CFB_Mode<CryptoPP::DES>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));

    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded))));
    break;
  }
  case Mode::CTR: {
    CryptoPP::CTR_Mode<CryptoPP::DES>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));

    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded))));
    break;
  }
  case Mode::CBC: {
    CryptoPP::CBC_Mode<CryptoPP::DES>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));
    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded),
                               CryptoPP::StreamTransformationFilter::PKCS_PADDING)));
    break;
  }
  case Mode::OFB: {
    CryptoPP::OFB_Mode<CryptoPP::DES>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));

    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded))));
  }
  default:
    break;
  }
  return decoded;
}

std::string CryptoppImpl::DesImpl::normalizeKey(const std::string& key) {
  std::string normal_key = key;

  size_t length = key.length();
  if (length < bits64_) {
    // 填充0补齐64位
    normal_key.reserve(bits64_);
    normal_key.append(bits64_ - length, 0);
  } else {
    // 截断，保留64位
    normal_key.resize(bits64_);
  }

  return normal_key;
}

std::string CryptoppImpl::TripleDesImpl::encode(const std::string& data,
                                                const std::string& encryption_key,
                                                Mode mode) const {
  std::string encoded;
  // 将key的长度修改为192(bits)、256(bits)
  std::string normal_key = normalizeKey(encryption_key);
  // 生成随机IV值
  CryptoPP::AutoSeededRandomPool prng;
  CryptoPP::byte ivBytes[CryptoPP::DES_EDE3::BLOCKSIZE];
  prng.GenerateBlock(ivBytes, sizeof(ivBytes));
  iv.assign(reinterpret_cast<const char*>(ivBytes), sizeof(ivBytes));
  // 填充数据
  std::string padded_data = padData(data, CryptoPP::DES_EDE3::BLOCKSIZE);

  switch (mode) {
  case Mode::ECB: {
    CryptoPP::ECB_Mode<CryptoPP::DES_EDE3>::Encryption encryptor;
    encryptor.SetKey(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()), normal_key.size());
    CryptoPP::StringSource(data, true,
                           new CryptoPP::StreamTransformationFilter(
                               encryptor,
                               new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded)),
                               CryptoPP::StreamTransformationFilter::PKCS_PADDING));
    break;
  }
  case Mode::CFB: {
    CryptoPP::CFB_Mode<CryptoPP::DES_EDE3>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), ivBytes);

    CryptoPP::StringSource(
        padded_data, true,
        new CryptoPP::StreamTransformationFilter(
            encryptor, new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded))));
    break;
  }
  case Mode::CTR: {
    CryptoPP::CTR_Mode<CryptoPP::DES_EDE3>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), ivBytes);

    CryptoPP::StringSource(
        padded_data, true,
        new CryptoPP::StreamTransformationFilter(
            encryptor, new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded))));
    break;
  }
  case Mode::CBC: {
    CryptoPP::CBC_Mode<CryptoPP::DES_EDE3>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), ivBytes);
    CryptoPP::StringSource(data, true,
                           new CryptoPP::StreamTransformationFilter(
                               encryptor,
                               new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded)),
                               CryptoPP::StreamTransformationFilter::PKCS_PADDING));
    break;
  }
  case Mode::OFB: {
    CryptoPP::OFB_Mode<CryptoPP::DES_EDE3>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), ivBytes);

    CryptoPP::StringSource(
        padded_data, true,
        new CryptoPP::StreamTransformationFilter(
            encryptor, new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded))));
    break;
  }
  default:
    break;
  }
  return encoded;
}

std::string CryptoppImpl::TripleDesImpl::decode(const std::string& data,
                                                const std::string& encryption_key,
                                                Mode mode) const {
  std::string decoded;
  std::string normal_key = normalizeKey(encryption_key);
  switch (mode) {
  case Mode::ECB: {
    CryptoPP::ECB_Mode<CryptoPP::DES_EDE3>::Decryption decryptor;
    decryptor.SetKey(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()), normal_key.size());
    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded),
                               CryptoPP::StreamTransformationFilter::PKCS_PADDING)));
    break;
  }
  case Mode::CFB: {
    CryptoPP::CFB_Mode<CryptoPP::DES_EDE3>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));

    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded))));
    break;
  }
  case Mode::CTR: {
    CryptoPP::CTR_Mode<CryptoPP::DES_EDE3>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));

    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded))));
    break;
  }
  case Mode::CBC: {
    CryptoPP::CBC_Mode<CryptoPP::DES_EDE3>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));
    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded),
                               CryptoPP::StreamTransformationFilter::PKCS_PADDING)));
    break;
  }
  case Mode::OFB: {
    CryptoPP::OFB_Mode<CryptoPP::DES_EDE3>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));

    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded))));

    break;
  }
  default:
    break;
  }
  return decoded;
}

std::string CryptoppImpl::TripleDesImpl::normalizeKey(const std::string& key) {
  std::string normal_key = key;

  size_t length = key.length();
  if (length < bits192_) {
    // 填充0补齐192位
    normal_key.reserve(bits192_);
    normal_key.append(bits192_ - length, 0);
  } else if (length < bits256_) {
    // 填充0补齐256位
    normal_key.reserve(bits256_);
    normal_key.append(bits256_ - length, 0);
  } else {
    // 截断，保留256位
    normal_key.resize(bits256_);
  }

  return normal_key;
}

std::string CryptoppImpl::AesImpl::encode(const std::string& data,
                                          const std::string& encryption_key, Mode mode) const {
  std::string encoded;
  // 将key的长度修改为128(bits)、192(bits)、256(bits)
  std::string normal_key = normalizeKey(encryption_key);
  // 生成随机IV值
  CryptoPP::AutoSeededRandomPool prng;
  CryptoPP::byte ivBytes[CryptoPP::AES::BLOCKSIZE];
  prng.GenerateBlock(ivBytes, sizeof(ivBytes));
  iv.assign(reinterpret_cast<const char*>(ivBytes), sizeof(ivBytes));
  // 填充数据
  std::string padded_data = padData(data, CryptoPP::AES::BLOCKSIZE);

  switch (mode) {
  case Mode::ECB: {
    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption encryptor;
    encryptor.SetKey(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()), normal_key.size());
    CryptoPP::StringSource(data, true,
                           new CryptoPP::StreamTransformationFilter(
                               encryptor,
                               new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded)),
                               CryptoPP::StreamTransformationFilter::PKCS_PADDING));
    break;
  }
  case Mode::CFB: {
    CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), ivBytes);

    CryptoPP::StringSource(
        padded_data, true,
        new CryptoPP::StreamTransformationFilter(
            encryptor, new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded))));
    break;
  }
  case Mode::CTR: {
    CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), ivBytes);

    CryptoPP::StringSource(
        padded_data, true,
        new CryptoPP::StreamTransformationFilter(
            encryptor, new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded))));
    break;
  }
  case Mode::CBC: {
    CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), ivBytes);
    CryptoPP::StringSource(data, true,
                           new CryptoPP::StreamTransformationFilter(
                               encryptor,
                               new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded)),
                               CryptoPP::StreamTransformationFilter::PKCS_PADDING));
    break;
  }
  case Mode::OFB: {
    CryptoPP::OFB_Mode<CryptoPP::AES>::Encryption encryptor;
    encryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), ivBytes);

    CryptoPP::StringSource(
        padded_data, true,
        new CryptoPP::StreamTransformationFilter(
            encryptor, new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded))));
    break;
  }
  default:
    break;
  }
  return encoded;
}

std::string CryptoppImpl::AesImpl::decode(const std::string& data,
                                          const std::string& encryption_key, Mode mode) const {
  std::string decoded;
  std::string normal_key = normalizeKey(encryption_key);
  switch (mode) {
  case Mode::ECB: {
    CryptoPP::ECB_Mode<CryptoPP::AES>::Decryption decryptor;
    decryptor.SetKey(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()), normal_key.size());
    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded),
                               CryptoPP::StreamTransformationFilter::PKCS_PADDING)));
    break;
  }
  case Mode::CFB: {
    CryptoPP::CFB_Mode<CryptoPP::AES>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));

    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded))));
    break;
  }
  case Mode::CTR: {
    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));

    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded))));
    break;
  }
  case Mode::CBC: {
    CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));
    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded),
                               CryptoPP::StreamTransformationFilter::PKCS_PADDING)));
    break;
  }
  case Mode::OFB: {
    CryptoPP::OFB_Mode<CryptoPP::AES>::Decryption decryptor;
    decryptor.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(normal_key.data()),
                           normal_key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));

    CryptoPP::StringSource(data, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StreamTransformationFilter(
                               decryptor, new CryptoPP::StringSink(decoded))));

    break;
  }
  default:
    break;
  }
  return decoded;
}

std::string CryptoppImpl::AesImpl::normalizeKey(const std::string& key) {
  std::string normal_key = key;

  size_t length = key.length();
  if (length < bits128_) {
    // 填充0补齐128位
    normal_key.reserve(bits128_);
    normal_key.append(bits128_ - length, 0);
  } else if (length < bits192_) {
    // 填充0补齐192位
    normal_key.reserve(bits192_);
    normal_key.append(bits192_ - length, 0);
  } else if (length < bits256_) {
    // 填充0补齐256位
    normal_key.reserve(bits256_);
    normal_key.append(bits256_ - length, 0);
  } else {
    // 截断，保留256位
    normal_key.resize(bits256_);
  }

  return normal_key;
}

std::string CryptoppImpl::Base64Impl::encode(const std::string& data) const {
  std::string encoded;
  CryptoPP::StringSource(data, true,
                         new CryptoPP::Base64Encoder(new CryptoPP::StringSink(encoded), false));
  return encoded;
}

std::string CryptoppImpl::Base64Impl::decode(const std::string& data) const {
  std::string decoded;
  CryptoPP::StringSource(data, true,
                         new CryptoPP::Base64Decoder(new CryptoPP::StringSink(decoded)));
  return decoded;
}

std::string CryptoppImpl::Md5Impl::encode(const std::string& data) const {
  CryptoPP::Weak1::MD5 md5;

  CryptoPP::byte digest[CryptoPP::Weak1::MD5::DIGESTSIZE];
  md5.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(data.c_str()), data.length());

  std::stringstream ss;
  for (int i = 0; i < CryptoPP::Weak1::MD5::DIGESTSIZE; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }

  return ss.str();
}

std::string CryptoppImpl::Sha256Impl::encode(const std::string& data) const {
  CryptoPP::SHA256 sha256;
  CryptoPP::byte digest[CryptoPP::SHA256::DIGESTSIZE];
  sha256.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(data.c_str()),
                         data.length());
  std::stringstream ss;
  for (int i = 0; i < CryptoPP::SHA256::DIGESTSIZE; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }

  return ss.str();
}

std::string CryptoppImpl::Sha512Impl::encode(const std::string& data) const {
  CryptoPP::SHA512 sha512;
  CryptoPP::byte digest[CryptoPP::SHA512::DIGESTSIZE];
  sha512.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(data.c_str()),
                         data.length());
  std::stringstream ss;
  for (int i = 0; i < CryptoPP::SHA512::DIGESTSIZE; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }

  return ss.str();
}

std::string CryptoppImpl::Sha1Impl::encode(const std::string& data) const {
  CryptoPP::SHA1 sha1;
  CryptoPP::byte digest[CryptoPP::SHA1::DIGESTSIZE];
  sha1.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(data.c_str()),
                       data.length());
  std::stringstream ss;
  for (int i = 0; i < CryptoPP::SHA1::DIGESTSIZE; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }

  return ss.str();
}

std::string CryptoppImpl::Sha3Impl::encode(const std::string& data, SHA3Type type) const {
  std::string hash;

  switch (type) {
  case SHA3Type::SHA3_224: {
    CryptoPP::SHA3_224 sha3;
    CryptoPP::byte digest[CryptoPP::SHA3_224::DIGESTSIZE];
    sha3.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(data.c_str()),
                         data.length());
    hash.assign(reinterpret_cast<const char*>(digest), CryptoPP::SHA3_224::DIGESTSIZE);
    break;
  }
  case SHA3Type::SHA3_256: {
    CryptoPP::SHA3_256 sha3;
    CryptoPP::byte digest[CryptoPP::SHA3_256::DIGESTSIZE];
    sha3.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(data.c_str()),
                         data.length());
    hash.assign(reinterpret_cast<const char*>(digest), CryptoPP::SHA3_256::DIGESTSIZE);
    break;
  }
  case SHA3Type::SHA3_384: {
    CryptoPP::SHA3_384 sha3;
    CryptoPP::byte digest[CryptoPP::SHA3_384::DIGESTSIZE];
    sha3.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(data.c_str()),
                         data.length());
    hash.assign(reinterpret_cast<const char*>(digest), CryptoPP::SHA3_384::DIGESTSIZE);
    break;
  }
  case SHA3Type::SHA3_512: {
    CryptoPP::SHA3_512 sha3;
    CryptoPP::byte digest[CryptoPP::SHA3_512::DIGESTSIZE];
    sha3.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(data.c_str()),
                         data.length());
    hash.assign(reinterpret_cast<const char*>(digest), CryptoPP::SHA3_512::DIGESTSIZE);
    break;
  }
  }

  std::stringstream ss;
  for (auto c : hash) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(static_cast<unsigned char>(c));
  }

  return ss.str();
}

std::string CryptoppImpl::Sm3Impl::encode(const std::string& data) const {
  CryptoPP::SM3 sm3;
  CryptoPP::byte digest[CryptoPP::SM3::DIGESTSIZE];
  sm3.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(data.c_str()), data.length());
  std::stringstream ss;
  for (int i = 0; i < CryptoPP::SM3::DIGESTSIZE; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }

  return ss.str();
}

} // namespace Encode
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework