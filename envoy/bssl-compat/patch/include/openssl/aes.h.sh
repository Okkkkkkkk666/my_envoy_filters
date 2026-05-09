set -euo pipefail

uncomment.sh "$1" --comment -h \
  --uncomment-func-decl AES_set_encrypt_key \
  --uncomment-func-decl AES_ecb_encrypt \
  --uncomment-func-decl AES_cbc_encrypt \
  --uncomment-func-decl AES_cfb128_encrypt \
  --uncomment-func-decl AES_ofb128_encrypt \
  --uncomment-func-decl AES_ctr128_encrypt \
  --uncomment-func-decl AES_set_decrypt_key \
  --uncomment-struct aes_key_st \
  --uncomment-typedef AES_KEY \
  --uncomment-macro AES_BLOCK_SIZE\
  --uncomment-macro AES_DECRYPT \
  --uncomment-macro AES_ENCRYPT \
  --uncomment-macro AES_MAXNR