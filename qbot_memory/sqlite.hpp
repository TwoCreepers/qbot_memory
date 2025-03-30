#pragma once

#include "exception.hpp"
#include <any>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace memory::sqlite
{

	enum SQLite_Ty
	{
		INTEGER = SQLITE_INTEGER,
		DOUBLE = SQLITE_FLOAT,
		TEXT = SQLITE_TEXT,
		BLOB = SQLITE_BLOB,
		NULL_ = SQLITE_NULL
	};

	enum transaction_level
	{
		DEFAULT,
		READ_ONLY,
		DEFERRED,
		IMMEDIATE,
		EXCLUSIVE
	};

	enum synchronous_mode
	{
		OFF = 0,
		NORMAL = 1,
		FULL = 2
	};

	using exec_callback_func_ptr = int(void*, int, char**, char**);
	using exec_callback_func = std::function<exec_callback_func_ptr>;

	using stmt_step_ret_t = std::unordered_map<std::string, stmt_buffer>;
	using stmt_steps_ret_t_v1 = std::unordered_map<std::string, std::vector<stmt_buffer>>;

	class database
	{
	public:
		database(std::string_view path)
		{
			open(path);
		}
		~database()
		{
			try
			{
				close();
			}
			catch (const exception::bad_database&) {}
		}
		database(const database& _That) = delete;
		database(database&& _That) noexcept
		{
			this->m_db = std::move(_That.m_db);
			_That.m_db = nullptr;
		}
		database& operator=(const database& _That) = delete;
		database& operator=(database&& _That) noexcept
		{
			this->m_db = std::move(_That.m_db);
			_That.m_db = nullptr;
			return *this;
		}
		void open(std::string_view path)
		{
			if (sqlite3_open(path.data(), &m_db) != SQLITE_OK)
			{
				close();
				auto errMsg = errmsg();
				throw exception::bad_database(std::format("数据库连接打开时: {}", errMsg));
				return;
			}
		}
		void close()
		{
			if (m_db != nullptr)
			{
				if (sqlite3_close(m_db) != SQLITE_OK)
				{
					auto errMsg = errmsg();
					throw exception::bad_database(std::format("数据库连接关闭时: {}", errMsg));
					return;
				}
				m_db = nullptr;
			}
		}
		int execute(const std::string& sql, exec_callback_func callback_func = nullptr, void* user_ptr = nullptr) const
		{
			char* errMsg = nullptr;
			int res;
			if (res = sqlite3_exec(m_db, sql.c_str(), *(callback_func.target<exec_callback_func_ptr>()), user_ptr, &errMsg) != SQLITE_OK)
			{
				auto error = exception::sqlite_call_error(errMsg);
				sqlite3_free(errMsg);
				throw error;
			}
			return res;
		}
		sqlite3* get()
		{
			if (m_db == nullptr)
			{
				throw exception::bad_database("数据库连接未开启");
			}
			return m_db;
		}
		void load_extension(const std::string& extension_path)
		{
			char* errMsg = nullptr;
			int res;
			if (res = sqlite3_load_extension(m_db, extension_path.c_str(), nullptr, &errMsg) != SQLITE_OK)
			{
				auto error = exception::sqlite_extension_error(errMsg);
				sqlite3_free(errMsg);
				throw error;
			}
		}
		void enable_load_extension(int onoff = 1)
		{
			const int res = sqlite3_enable_load_extension(m_db, onoff);
			if (res != SQLITE_OK)
			{
				auto errMsg = errmsg();
				auto error = exception::sqlite_extension_error(std::format("启用扩展加载时: {}", errMsg));
				throw error;
			}
		}
		sqlite_int64 last_insert_rowid()
		{
			return sqlite3_last_insert_rowid(m_db);
		}
		const char* errmsg()
		{
			return sqlite3_errmsg(m_db);
		}
		int extended_errcode()
		{
			return sqlite3_extended_errcode(m_db);
		}
		void set_synchronous(const synchronous_mode synchronous)
		{
			const char* sql = nullptr;
			switch (synchronous)
			{
			case OFF:
				sql = "PRAGMA synchronous=OFF;";
				break;
			case NORMAL:
				sql = "PRAGMA synchronous=NORMAL;";
				break;
			case FULL:
				sql = "PRAGMA synchronous=FULL;";
				break;
			default:
				sql = nullptr;
				break;
			}
			if (sql == nullptr)
			{
				throw exception::invalid_argument("未知的同步模式");
			}
			char* errMsg = nullptr;
			auto res = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
			if (res != SQLITE_OK)
			{
				throw exception::bad_database(std::format("无法设置同步模式: {} SQL: {}", errMsg, sql));
			}
		}
		void set_wal_autocheckpoint(const std::size_t wal_autocheckpoint)
		{
			auto sql = std::format("PRAGMA wal_autocheckpoint={};", wal_autocheckpoint);
			char* errMsg = nullptr;
			auto res = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);
			if (res != SQLITE_OK)
			{
				throw exception::bad_database(std::format("无法设置同步模式: {} SQL: {}", errMsg, sql));
			}
		}
	private:
		sqlite3* m_db;
	};

	class stmt_buffer
	{
	public:
		stmt_buffer() : m_any{}, m_column_type{ SQLite_Ty::NULL_ } {}
		stmt_buffer(sqlite3_stmt* stmt, int i) :m_column_type{ static_cast<SQLite_Ty>(sqlite3_column_type(stmt, i)) }, m_any{}
		{
			switch (m_column_type)
			{
			case SQLite_Ty::INTEGER:
			{
				auto res = sqlite3_column_int64(stmt, i);
				m_any = res;
				break;
			}
			case SQLite_Ty::DOUBLE:
			{
				auto res = sqlite3_column_double(stmt, i);
				m_any = res;
				break;
			}
			case SQLite_Ty::TEXT:
			{
				std::string res = (const char*)sqlite3_column_text(stmt, i);
				m_any = res;
				break;
			}
			case SQLite_Ty::BLOB:
			{
				throw std::runtime_error("");
				break;
			}
			case SQLite_Ty::NULL_:
			{
				auto res = nullptr;
				m_any = res;
				break;
			}
			default:
			{
				m_any = nullptr;
			}
			}
		}
		~stmt_buffer() = default;
		stmt_buffer(const stmt_buffer& _That)
		{
			this->m_any = _That.m_any;
			this->m_column_type = _That.m_column_type;
		}
		stmt_buffer(stmt_buffer&& _That) noexcept
		{
			this->m_any = std::move(_That.m_any);
			this->m_column_type = std::move(_That.m_column_type);
		}
		stmt_buffer& operator=(const stmt_buffer& _That)
		{
			this->m_any = _That.m_any;
			this->m_column_type = _That.m_column_type;
			return *this;
		}
		stmt_buffer& operator=(stmt_buffer&& _That) noexcept
		{
			this->m_any = std::move(_That.m_any);
			this->m_column_type = std::move(_That.m_column_type);
			return *this;
		}
		template <typename _Ty>
		_Ty as()
		{
			return std::any_cast<_Ty>(m_any);
		}
		bool is_int64() const
		{
			return m_column_type == SQLite_Ty::INTEGER;
		}
		bool is_double() const
		{
			return m_column_type == SQLite_Ty::DOUBLE;
		}
		bool is_string() const
		{
			return m_column_type == SQLite_Ty::TEXT;
		}
		bool is_blob() const
		{
			return m_column_type == SQLite_Ty::BLOB;
		}
		bool is_null() const
		{
			return m_column_type == SQLite_Ty::NULL_;
		}
	private:
		std::any m_any;
		SQLite_Ty m_column_type;
	};

	class transaction
	{
	public:
		transaction(std::shared_ptr<database> db, transaction_level level = DEFAULT) :m_db{ db }
		{
			open(level);
		}
		~transaction()
		{
			try
			{
				if (m_active) { rollback(); }
			}
			catch (const exception::stmt_call_error&) {}
		}
		void open(transaction_level level = DEFAULT)
		{
			if (m_active)
			{
				throw exception::bad_transaction("重复开启事务");
			}
			const char* sql = nullptr;
			switch (level)
			{
			case DEFAULT:    sql = "BEGIN;"; break;
			case READ_ONLY:    sql = "EGIN TRANSACTION READ ONLY;"; break;
			case DEFERRED:  sql = "BEGIN DEFERRED;"; break;
			case IMMEDIATE: sql = "BEGIN IMMEDIATE;"; break;
			case EXCLUSIVE:  sql = "BEGIN EXCLUSIVE;"; break;
			default: sql = "BEGIN;"; break;
			}
			if (m_db->execute(sql) != SQLITE_OK)
			{
				throw exception::bad_transaction("开启事务失败");
			}
			m_active = true; // 事务成功启动
		}
		transaction(const transaction& _That) = delete;
		transaction(transaction&& _That) = delete;
		transaction& operator=(const transaction& _That) = delete;
		transaction& operator=(transaction&& _That) = delete;
		int commit()
		{
			if (!m_active)
			{
				throw exception::bad_transaction("提交失败, 因为事务已关闭");
			}
			int rc = m_db->execute("COMMIT;");
			{
				if (rc == SQLITE_OK) m_active = false;
			}
			return rc;


		}
		int rollback()
		{
			if (!m_active)
			{
				throw exception::bad_transaction("回滚失败, 因为事务已关闭");
			}
			int rc = m_db->execute("ROLLBACK;");
			{
				if (rc == SQLITE_OK) m_active = false;
			}
			return rc;

		}
		int execute(const std::string& sql, exec_callback_func callback_func = nullptr, void* user_ptr = nullptr) const
		{
			if (!m_active)
			{
				throw exception::bad_transaction("执行失败, 因为事务已关闭");
			}
			return m_db->execute(sql, callback_func, user_ptr);
		}
		std::shared_ptr<database> get()
		{
			if (!m_active)
			{
				throw exception::bad_transaction("获取一个已关闭的事务");
			}
			return m_db;
		}
	private:
		std::shared_ptr<database> m_db;
		std::atomic_bool m_active;
	};

	class stmt
	{
	public:
		stmt() = default;
		stmt(std::shared_ptr<database> db, std::string_view sql, unsigned int prepFlags = NULL) : m_db(db)
		{
			open(db, sql, prepFlags);
		}
		stmt(transaction& ta, std::string_view sql, unsigned int prepFlags = NULL) : m_db(ta.get())
		{
			open(ta.get(), sql, prepFlags);
		}
		~stmt()
		{
			try
			{
				close();
			}
			catch (const exception::bad_stmt&) {}
		}
		stmt(const stmt& _That) = delete;
		stmt(stmt&& _That) noexcept
		{
			this->m_stmt = std::move(_That.m_stmt);
			_That.m_stmt = nullptr;
		}
		stmt& operator=(const stmt& _That) = delete;
		stmt& operator=(stmt&& _That) noexcept
		{
			this->m_stmt = std::move(_That.m_stmt);
			_That.m_stmt = nullptr;
			return *this;
		}
		void open(std::shared_ptr<database> db, std::string_view sql, unsigned int prepFlags = NULL)
		{
			auto res = sqlite3_prepare_v3(db->get(), sql.data(), static_cast<int>(sql.size()), prepFlags, &m_stmt, nullptr);
			if (res != SQLITE_OK)
			{
				throw exception::bad_stmt(std::format("开启预编译SQL语句失败: ", m_db->errmsg()));
			}
		}
		void close()
		{
			if (m_stmt != nullptr)
			{
				auto res = sqlite3_finalize(m_stmt);
				if (res != SQLITE_OK)
				{
					throw exception::bad_stmt(std::format("关闭预编译SQL语句失败: ", m_db->errmsg()));
				}
				m_stmt = nullptr;
			}
		}
		void reset()
		{
			int rc = sqlite3_reset(m_stmt);
			if (rc != SQLITE_OK)
			{
				throw exception::stmt_reset_error(
					std::format("重置语句失败: {}", m_db->errmsg())
				);
			}
		}

		template <class T>
		void bind(int index, T value)
		{
			static_assert(false, "使用了一个bind函数不支持的类型");
			int rc = sqlite3_bind_null(m_stmt, index);
			if (rc != SQLITE_OK)
			{
				throw exception::stmt_bind_error(
					std::format("绑定参数失败(不支持的类型): {}", m_db->errmsg())
				);
			}
		}

		template <>
		void bind<nullptr_t>(int index, nullptr_t value)
		{
			int rc = sqlite3_bind_null(m_stmt, index);
			if (rc != SQLITE_OK)
			{
				throw exception::stmt_bind_error(
					std::format("绑定null参数失败: {}", m_db->errmsg())
				);
			}
		}

		template<>
		void bind<int>(int index, int value)
		{
			int rc = sqlite3_bind_int(m_stmt, index, value);
			if (rc != SQLITE_OK)
			{
				throw exception::stmt_bind_error(
					std::format("绑定int参数失败: {}", m_db->errmsg())
				);
			}
		}

		template<>
		void bind<std::int64_t>(int index, const std::int64_t value)
		{
			int rc = sqlite3_bind_int64(m_stmt, index, value);
			if (rc != SQLITE_OK)
			{
				throw exception::stmt_bind_error(
					std::format("绑定int64参数失败: {}", m_db->errmsg())
				);
			}
		}

		template<>
		void bind<std::uint64_t>(int index, const std::uint64_t value)
		{
			int rc = sqlite3_bind_int64(m_stmt, index, static_cast<std::int64_t>(value));
			if (rc != SQLITE_OK)
			{
				throw exception::stmt_bind_error(
					std::format("绑定uint64参数失败: {}", m_db->errmsg())
				);
			}
		}

		template<>
		void bind<double>(int index, const double value)
		{
			int rc = sqlite3_bind_double(m_stmt, index, value);
			if (rc != SQLITE_OK)
			{
				throw exception::stmt_bind_error(
					std::format("绑定double参数失败: {}", m_db->errmsg())
				);
			}
		}

		template<>
		void bind<const char*>(int index, const char* value)
		{
			int rc = sqlite3_bind_text(m_stmt, index, value, -1, SQLITE_TRANSIENT);
			if (rc != SQLITE_OK)
			{
				throw exception::stmt_bind_error(
					std::format("绑定const char*参数失败: {}", m_db->errmsg())
				);
			}
		}

		void bind(int index, std::string_view value, sqlite3_destructor_type flag = SQLITE_TRANSIENT)
		{
			int rc = sqlite3_bind_text(m_stmt, index, value.data(), static_cast<int>(value.size()), flag);
			if (rc != SQLITE_OK)
			{
				throw exception::stmt_bind_error(
					std::format("绑定string_view参数失败: {}", m_db->errmsg())
				);
			}
		}

		void bind(int index, const std::string& value, sqlite3_destructor_type flag = SQLITE_TRANSIENT)
		{
			int rc = sqlite3_bind_text(m_stmt, index, value.data(), static_cast<int>(value.size()), flag);
			if (rc != SQLITE_OK)
			{
				throw exception::stmt_bind_error(
					std::format("绑定string参数失败: {}", m_db->errmsg())
				);
			}
		}

		int get_column_int(int index)
		{
			return sqlite3_column_int(m_stmt, index);
		}

		int64_t get_column_int64(int index)
		{
			return sqlite3_column_int64(m_stmt, index);
		}

		std::uint64_t get_column_uint64(int index)
		{
			return static_cast<std::uint64_t>(sqlite3_column_int64(m_stmt, index));
		}

		double get_column_double(int index)
		{
			return sqlite3_column_double(m_stmt, index);
		}

		const char* get_column_str(int index)
		{
			return (const char*)(sqlite3_column_text(m_stmt, index));
		}

		int step(stmt_step_ret_t& in)
		{
			int rc = sqlite3_step(m_stmt);

			int num_columns = sqlite3_column_count(m_stmt);
			for (int i = 0; i < num_columns; i++)
			{
				std::string columnName = sqlite3_column_name(m_stmt, i);
				in[columnName] = std::move(stmt_buffer{ m_stmt, i });
			}

			return rc;
		}

		int step(sqlite3_stmt*& in)
		{
			in = m_stmt;
			const auto res = sqlite3_step(m_stmt);
			switch (res)
			{
			case SQLITE_ROW:
				break;
			case SQLITE_DONE:
				break;
			default:
				throw exception::stmt_call_error(std::format("执行step时: ", m_db->errmsg()));
				break;
			}
			return res;
		}

		int step()
		{
			const auto res = sqlite3_step(m_stmt);
			switch (res)
			{
			case SQLITE_ROW:
				break;
			case SQLITE_DONE:
				break;
			default:
				throw exception::stmt_call_error(std::format("执行step时: ", m_db->errmsg()));
				break;
			}
			return res;
		}

		int steps(stmt_steps_ret_t_v1& in)
		{
			int rc;
			while (rc = sqlite3_step(m_stmt) == SQLITE_ROW)
			{
				int num_columns = sqlite3_column_count(m_stmt);
				for (int i = 0; i < num_columns; i++)
				{
					std::string columnName = sqlite3_column_name(m_stmt, i);
					in[columnName].emplace_back(m_stmt, i);
				}
			}
			return rc;
		}

		sqlite3_stmt* get()
		{
			if (m_stmt == nullptr)
			{
				throw exception::bad_stmt("获取一个已关闭的预编译SQL语句");
			}
			return m_stmt;
		}
	private:
		std::shared_ptr<database> m_db;
		sqlite3_stmt* m_stmt;
	};
}