#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// 规则定义
typedef struct {
  int start; // 开始位置
  int end;   // 结束位置
} RuleDefinition_t;
// 遮盖

typedef struct {
  int cover_mode;              // 遮盖方式
  RuleDefinition_t rules[10];  // 自定义规则
  int rules_size;              // 自定义规则个数
  const char* cover_character; // 遮盖字符
  int cover_type;              // 遮盖类型
} Cover_t;

// 加密算法
typedef struct {
  int encryption_algorithm;   // 加密方式
  const char* encryption_key; // 密钥
} EncryptAlgorithm_t;
// hash加密
typedef struct {
  int encryption_algorithm; // 加密方式
  const char* salt_value;   // 加盐值
} HashEncrypt_t;
// 替换
typedef struct {
  int rule_type;
  int value_type;
  const char* regex;
  RuleDefinition_t rules[10];
  int rules_size;
  int cover_type;
  const char* replace_value;
} Replace_t;
// 变换
typedef struct {
  int trans_type;
  // 数字取整规则
  int decimal_places;
  // 日期取整规则
  int level;
  // 字符位移规则
  int direction;

  int shift_amount;
} Transform_t;

typedef struct {
  int method;
  Transform_t transform;
  Replace_t replace;
  HashEncrypt_t hash_encrpyt;
  EncryptAlgorithm_t encrpyt_algorithm;
  Cover_t cover;
} rewrite_rule_t;

void* rewriteRule_New(rewrite_rule_t rule);
void rewriteRule_Delete(void* instance);
void rewriteRule_FreeResult(const char* result);
const char* rewriteRule_Rewrite(void* instance, const char* data);
#ifdef __cplusplus
}
#endif