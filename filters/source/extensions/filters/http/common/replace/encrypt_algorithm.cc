#include "encrypt_algorithm.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
EncryptAlgorithmRewrite::EncryptAlgorithmRewrite(const EncryptAlgorithm& EncryptAlgorithm)
    : encryption_algorithm_(EncryptAlgorithm.encryption_algorithm),
      encryption_key_(EncryptAlgorithm.encryption_key) {}

std::string EncryptAlgorithmRewrite::encryptAlgorithm(const std::string& data) const {
  switch (encryption_algorithm_) {
  case EncryptionAlgorithm::DES:
    return DES(data, encryption_key_);
    break;
  case EncryptionAlgorithm::TRIPLE_DES:
    return TRIPLE_DES(data, encryption_key_);
    break;
  case EncryptionAlgorithm::AES:
    return AES(data, encryption_key_);
    break;
  default:
    break;
  }
  return data;
}

std::string EncryptAlgorithmRewrite::TRIPLE_DES(const std::string& data,
                                                const std::string& encryption_key) const {

  std::string cipher, encoded;
  CryptoPP::DES_EDE3::Encryption des_encryption((CryptoPP::byte*)encryption_key.data(),
                                               CryptoPP::DES_EDE3::DEFAULT_KEYLENGTH);
  CryptoPP::ECB_Mode_ExternalCipher::Encryption ecb_encryption(des_encryption);
  CryptoPP::StringSource(data, true,
                         new CryptoPP::StreamTransformationFilter(
                             ecb_encryption,
                             new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded)),
                             CryptoPP::StreamTransformationFilter::PKCS_PADDING));

  return encoded;
}

std::string EncryptAlgorithmRewrite::AES(const std::string& data,
                                         const std::string& encryption_key) const {

  std::string cipher, encoded;
  CryptoPP::AES::Encryption aes_encryption((CryptoPP::byte*)encryption_key.data(),
                                          CryptoPP::AES::DEFAULT_KEYLENGTH);
  CryptoPP::ECB_Mode_ExternalCipher::Encryption ecb_encryption(aes_encryption);
  CryptoPP::StringSource ss(
      data, true,
      new CryptoPP::StreamTransformationFilter(ecb_encryption, new CryptoPP::StringSink(cipher),
                                               CryptoPP::StreamTransformationFilter::PKCS_PADDING));

  CryptoPP::StringSource encode(cipher, true,
                                new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded), false));

  return encoded;
}

std::string EncryptAlgorithmRewrite::DES(const std::string& data,
                                         const std::string& encryption_key) const {

  std::string encoded;
  CryptoPP::DES::Encryption des_encryption((CryptoPP::byte*)encryption_key.data(),
                                          CryptoPP::DES::DEFAULT_KEYLENGTH);
  CryptoPP::ECB_Mode_ExternalCipher::Encryption ecb_encryption(des_encryption);
  CryptoPP::StringSource(data, true,
                         new CryptoPP::StreamTransformationFilter(
                             ecb_encryption,
                             new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded)),
                             CryptoPP::StreamTransformationFilter::PKCS_PADDING));

  return encoded;
}

} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy