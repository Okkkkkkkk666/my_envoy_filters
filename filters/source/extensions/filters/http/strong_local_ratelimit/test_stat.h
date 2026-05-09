#pragma once

#if !defined(NDEBUG)
#define DECLARE_STAT_BEGIN(name) struct TestStatOf##name {
#define STAT(field) uint32_t field{};
#define DECLARE_STAT_END(name)                                                                     \
  }                                                                                                \
  test_stat_of_##name##_;
#define STAT_INC(name, field) ++test_stat_of_##name##_.field;
#else
#define DECLARE_STAT_BEGIN(name)
#define STAT(field)
#define DECLARE_STAT_END(name)
#define STAT_INC(name, field)
#endif