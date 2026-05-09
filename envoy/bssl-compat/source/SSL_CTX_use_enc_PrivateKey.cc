#include <openssl/ssl.h>
#include <ossl.h>

extern "C" int SSL_CTX_use_enc_PrivateKey(SSL_CTX *ctx, EVP_PKEY *pkey) {
  return (ossl.ossl_SSL_CTX_use_enc_PrivateKey(ctx, pkey) == 1) ? 1 : 0;
}
