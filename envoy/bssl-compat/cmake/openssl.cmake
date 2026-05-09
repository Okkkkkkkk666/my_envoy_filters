include(ExternalProject)
set(OPENSSL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Tongsuo)
set(OPENSSL_CONFIG_CMD ${OPENSSL_SOURCE_DIR}/config)
set(OPENSSL_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/openssl/install)
set(OPENSSL_INCLUDE_DIR ${OPENSSL_INSTALL_DIR}/include)
set(OPENSSL_LIBRARY_DIR ${OPENSSL_INSTALL_DIR}/lib)
ExternalProject_Add(OpenSSL
    SOURCE_DIR ${OPENSSL_SOURCE_DIR}
    CONFIGURE_COMMAND ${OPENSSL_CONFIG_CMD} --prefix=${OPENSSL_INSTALL_DIR} --libdir=lib
    enable-ntls
    enable-weak-ssl-ciphers
    enable-zlib
    enable-zlib-dynamic
    enable-cert-compression
    enable-evp-cipher-api-compat
    TEST_COMMAND ""
    INSTALL_COMMAND make install_sw
)
