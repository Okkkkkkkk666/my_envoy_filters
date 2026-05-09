#pragma once

#define EXPORT __attribute__((visibility("default")))

extern "C" {
EXPORT void* createSqliteDbFactory();
}
