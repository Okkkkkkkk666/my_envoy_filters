#pragma once
#ifdef __cplusplus
extern "C"
{
#endif
// 结构体定义

typedef struct {
    int   identify_type;
    int   recognition_logic;
    const char* field_name;
    const char* data_content;
} identify_rule_t;

  void* identifyRule_New(identify_rule_t rule);
  void identifyRule_Delete(void* instance);
  void identifyRule_FreeResult(const char* result);
  const char* identifyRule_Rewrite(void* instance, const char* data);

#ifdef __cplusplus
}
#endif
