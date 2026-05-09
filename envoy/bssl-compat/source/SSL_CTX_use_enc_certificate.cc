#include <openssl/ssl.h>
#include <ossl.h>

extern "C" int SSL_CTX_use_enc_certificate(SSL_CTX *ctx, X509 *x509) {
  return (ossl.ossl_SSL_CTX_use_enc_certificate(ctx, x509) == 1) ? 1 : 0;
}
