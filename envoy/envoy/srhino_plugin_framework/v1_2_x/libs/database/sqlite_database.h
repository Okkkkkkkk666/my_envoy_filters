#pragma once

#include <memory>
#include <string>

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Database {

using SqliteQueryCb = int(*)(void* p_data, int num_fields, char** p_fields, char** p_col_names);

// 本类实现了对sqlite3数据库的各种操作。
class SqliteDatabase {
public:
  SqliteDatabase() {}
  SqliteDatabase(const SqliteDatabase&) = delete;
  virtual ~SqliteDatabase() = default;

public:
  /**
   * 执行一条sql语句
   * @param sql sql语句
   * @param err_msg 执行失败时错误信息
   * @return 
   *      true 执行成功
   *      false 执行失败
  */
  virtual bool execute(const std::string& sql, std::string& err_msg) = 0;

  /**
   * 对数据库执行插入操作
   * @param sql 插入sql语句
   * @param err_msg 执行失败时错误信息
   * @return 
   *      true 执行成功
   *      false 执行失败
  */
  virtual bool insert(const std::string& sql, std::string& err_msg) = 0;

  /**
   * 对数据库执行查询操作
   * @param sql 查询sql语句
   * @param p_res 指针指向查询结果
   * @param cb  查询回调函数。
   *    当查询结果不为空时，将对查询结果迭代调用cb回调函数。
   *    每次迭代，将使用p_res作为第一个参数调用一次cb回调函数。
   *    当cb返回值不为0时，停止迭代。
   * @param err_msg 执行失败时错误信息
   * @return 
   *      true 执行成功
   *      false 执行失败
  */
  virtual bool query(const std::string& sql, void* p_res, SqliteQueryCb cb, std::string& err_msg) = 0;
  /**
   * 删除表操作
   * @param table_name 要删除的表名称
   * @param err_msg 执行失败时错误信息
   * @return 
   *      true 执行成功
   *      false 执行失败
  */
  virtual bool dropTable(const std::string& table_name, std::string& err_msg) = 0;

public:
  /**
   * 返回当前数据库名称
  */
  virtual const std::string& db_name() = 0;
};
using SqliteDatabaseSharedPtr = std::shared_ptr<SqliteDatabase>;

} // namespace Database
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework