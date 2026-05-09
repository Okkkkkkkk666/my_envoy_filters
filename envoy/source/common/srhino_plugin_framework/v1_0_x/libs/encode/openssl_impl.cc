#include "source/common/srhino_plugin_framework/v1_0_x/libs/encode/openssl_impl.h"
#include "source/common/common/assert.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace Encode {
OpenSslImpl::OpenSslImpl()
    : aes_(std::make_shared<OpenSslImpl::AesImpl>()),
      md5_(std::make_shared<OpenSslImpl::Md5Impl>()) {}

std::string OpenSslImpl::AesImpl::encode(const void* data, size_t size, const std::string& key,
                                         Mode mode) const {
  AES_KEY aes_key;
  std::string normal_key = normalizeKey(key);
  ASSERT(normal_key.length() == bits128_ || normal_key.length() == bits192_ ||
         normal_key.length() == bits256_);

  AES_set_encrypt_key(reinterpret_cast<const uint8_t*>(normal_key.c_str()), normal_key.length() * 8,
                      &aes_key);

  std::string padding_data = padding(data, size);
  ASSERT(padding_data.length() % AES_BLOCK_SIZE == 0);

  std::string encode_data;
  encode_data.resize(padding_data.length());
  State state;
  const uint8_t* in = reinterpret_cast<const uint8_t*>(padding_data.c_str());
  uint8_t* out = reinterpret_cast<uint8_t*>(encode_data.data());
  switch (mode) {
  case Mode::ECB:
    AES_ecb_encrypt(in, out, &aes_key, AES_ENCRYPT);
    break;
  case Mode::CBC:
    AES_cbc_encrypt(in, out, encode_data.length(), &aes_key, state.ivec_, AES_ENCRYPT);
    break;
  case Mode::CFB:
    AES_cfb128_encrypt(in, out, encode_data.length(), &aes_key, state.ivec_, &state.num_,
                       AES_ENCRYPT);
    break;
  case Mode::OFB:
    AES_ofb128_encrypt(in, out, encode_data.length(), &aes_key, state.ivec_, &state.num_);
    break;
  case Mode::CTR:
    AES_ctr128_encrypt(in, out, encode_data.length(), &aes_key, state.ivec_, state.ecount_,
                       reinterpret_cast<unsigned int*>(&state.num_));
    break;
  default:
    break;
  }

  return encode_data;
}

std::string OpenSslImpl::AesImpl::decode(const void* data, size_t size, const std::string& key,
                                         Mode mode) const {
  AES_KEY aes_key;
  std::string normal_key = normalizeKey(key);
  ASSERT(normal_key.length() == bits128_ || normal_key.length() == bits192_ ||
         normal_key.length() == bits256_);

  AES_set_decrypt_key(reinterpret_cast<const uint8_t*>(normal_key.c_str()), normal_key.length() * 8,
                      &aes_key);

  std::string decode_data;
  decode_data.resize(size);
  State state;
  const uint8_t* in = reinterpret_cast<const uint8_t*>(data);
  uint8_t* out = reinterpret_cast<uint8_t*>(decode_data.data());
  switch (mode) {
  case Mode::ECB:
    AES_ecb_encrypt(in, out, &aes_key, AES_DECRYPT);
    break;
  case Mode::CBC:
    AES_cbc_encrypt(in, out, decode_data.length(), &aes_key, state.ivec_, AES_DECRYPT);
    break;
  case Mode::CFB:
    AES_cfb128_encrypt(in, out, decode_data.length(), &aes_key, state.ivec_, &state.num_,
                       AES_DECRYPT);
    break;
  case Mode::OFB:
    AES_ofb128_encrypt(in, out, decode_data.length(), &aes_key, state.ivec_, &state.num_);
    break;
  case Mode::CTR:
    AES_ctr128_encrypt(in, out, decode_data.length(), &aes_key, state.ivec_, state.ecount_,
                       reinterpret_cast<unsigned int*>(&state.num_));
    break;
  default:
    break;
  }

  // 去除填充字符
  size_t padding_count = static_cast<size_t>(*(decode_data.rbegin()));
  ASSERT(padding_count > 0 && padding_count <= AES_BLOCK_SIZE);
  if (padding_count > 0 && padding_count <= AES_BLOCK_SIZE) {
    decode_data.resize(decode_data.length() - padding_count);
  }

  return decode_data;
}

void OpenSslImpl::AesImpl::decode(const std::vector<unsigned char>& encrypted_data,
                                  std::vector<unsigned char>& decrypted_data,
                                  const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv,
                                  Mode mode) const {
  State state;
  std::memcpy(state.ivec_, iv.data(), AES_BLOCK_SIZE);
  AES_KEY aes_key;
  AES_set_decrypt_key(key.data(), key.size() * 8, &aes_key);
  decrypted_data.resize(encrypted_data.size());
  switch (mode) {
  case Mode::ECB:
    AES_ecb_encrypt(encrypted_data.data(), decrypted_data.data(), &aes_key, AES_DECRYPT);
    break;
  case Mode::CBC:
    AES_cbc_encrypt(encrypted_data.data(), decrypted_data.data(), encrypted_data.size(), &aes_key,
                    state.ivec_, AES_DECRYPT);
    break;
  case Mode::CFB:
    AES_KEY decrypt_key;
    AES_set_encrypt_key(key.data(), key.size() * 8, &decrypt_key);
    AES_cfb128_encrypt(encrypted_data.data(), decrypted_data.data(), encrypted_data.size(),
                       &decrypt_key, state.ivec_, &state.num_, AES_DECRYPT);
    break;
  case Mode::OFB:
    AES_ofb128_encrypt(encrypted_data.data(), decrypted_data.data(), encrypted_data.size(),
                       &aes_key, state.ivec_, &state.num_);
    break;
  case Mode::CTR:
    AES_ctr128_encrypt(encrypted_data.data(), decrypted_data.data(), encrypted_data.size(),
                       &aes_key, state.ivec_, state.ecount_,
                       reinterpret_cast<unsigned int*>(&state.num_));
    break;
  default:
    break;
  }
}

// 将key的长度修改为128(bits)、192(bits)、256(bits)
std::string OpenSslImpl::AesImpl::normalizeKey(const std::string& key) {
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

// 采用PKCS7标准将待加密数据长度修改为AES_BLOCK_SIZE的整数倍
std::string OpenSslImpl::AesImpl::padding(const void* data, size_t size) {
  std::string padding_data;
  if ((size % AES_BLOCK_SIZE) != 0) {
    const size_t block_num = size / AES_BLOCK_SIZE + 1;
    const size_t normal_size = block_num * AES_BLOCK_SIZE;
    padding_data.reserve(normal_size);
    padding_data.append(static_cast<const char*>(data), size);
    padding_data.append(normal_size - size, normal_size - size);
  } else {
    padding_data.reserve(size + AES_BLOCK_SIZE);
    padding_data.append(static_cast<const char*>(data), size);
    padding_data.append(AES_BLOCK_SIZE, AES_BLOCK_SIZE);
  }

  return padding_data;
}

OpenSslImpl::AesImpl::State::State() {
  memset(this, 0, sizeof(State));
  memcpy(ivec_, "[www.srhino.com]", sizeof(ivec_));
}
} // namespace Encode
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework
