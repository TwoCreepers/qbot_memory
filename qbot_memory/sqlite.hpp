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

	using exec_callback_func_ptr = int(void*, int, char**, char**);
	using exec_callback_func = std::function<exec_callback_func_ptr>;

	class database
	{
	public:
		database(std::string_view path)
		{
			open(path);
		}
		~database() { close(); }
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
				throw exception::sqlite_runtime_exception(sqlite3_errmsg(m_db));
				return;
			}
		}
		void close()
		{
			if (m_db != nullptr)
			{
				if (sqlite3_close(m_db) != SQLITE_OK)
				{
					throw exception::sqlite_runtime_exception(sqlite3_errmsg(m_db));
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
				auto error = exception::sqlite_runtime_exception(errMsg);
				sqlite3_free(errMsg);
				throw error;
			}
			return res;
		}
		sqlite3* get()
		{
			if (m_db == nullptr)
			{
				throw exception::sqlite_runtime_exception("Database ptr is nullptr");
			}
			return m_db;
		}
		void load_extension(const std::string& extension_path)
		{
			char* errMsg = nullptr;
			int res;
			if (res = sqlite3_load_extension(m_db, extension_path.c_str(), nullptr, &errMsg) != SQLITE_OK)
			{
				auto error = exception::sqlite_runtime_exception(errMsg);
				sqlite3_free(errMsg);
				throw error;
			}
		}
	private:
		sqlite3* m_db;
	};

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
		DEFERRED,
		IMMEDIATE,
		EXCLUSIVE
	};

	class stmt_buffer
	{
	public:
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
	using stmt_step_ret_t = std::unordered_map<std::string, stmt_buffer>;
	using stmt_steps_ret_t_v1 = std::unordered_map<std::string, std::vector<stmt_buffer>>;

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
			catch (const exception::sqlite_runtime_exception&) {}
		}
		void open(transaction_level level = DEFAULT)
		{
			if (m_active)
			{
				throw exception::double_transaction_exception();
			}
			const char* sql = nullptr;
			switch (level)
			{
			case DEFAULT:    sql = "BEGIN;"; break;
			case DEFERRED:  sql = "BEGIN DEFERRED;"; break;
			case IMMEDIATE: sql = "BEGIN IMMEDIATE;"; break;
			case EXCLUSIVE:  sql = "BEGIN EXCLUSIVE;"; break;
			default: sql = "BEGIN;"; break;
			}
			if (m_db->execute(sql) != SQLITE_OK)
			{
				throw exception::sqlite_runtime_exception("Failed to begin transaction");
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
				throw exception::using_close_transaction_exception();
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
				throw exception::using_close_transaction_exception();
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
				throw exception::using_close_transaction_exception();
			}
			return m_db->execute(sql, callback_func, user_ptr);
		}
		std::shared_ptr<database> get()
		{
			if (!m_active)
			{
				throw exception::using_close_transaction_exception();
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
		stmt(std::shared_ptr<database> db, std::string_view sql, unsigned int prepFlags = NULL)
		{
			open(db, sql, prepFlags);
		}
		stmt(std::shared_ptr<transaction> ta, std::string_view sql, unsigned int prepFlags = NULL)
		{
			open(ta->get(), sql, prepFlags);
		}
		~stmt()
		{
			close();
		}
		stmt(const stmt& _That) = delete;
		stmt(stmt&& _That) noexcept
		{
			this->m_stmt = std::move(_That.m_stmt);
			_That.m_stmt = nullptr;
		}
		stmt& operator=(const stmt& _That) = delete;
		stmt& operator=(stmt& _That)
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
				throw exception::sqlite_runtime_exception();
			}
		}
		void close()
		{
			if (m_stmt != nullptr)
			{
				sqlite3_finalize(m_stmt);
				m_stmt = nullptr;
			}
		}
		int reset()
		{
			return sqlite3_reset(m_stmt);
		}
		template <class T>
		int bind(int index, T value)
		{
			static_assert(false, "使用了一个bind函数不支持的类型");
			return sqlite3_bind_null(m_stmt, index);
		}
		template<>
		int bind<int>(int index, int value)
		{
			return sqlite3_bind_int(m_stmt, index, value);
		}
		template<>
		int bind<std::int64_t>(int index, const std::int64_t value)
		{
			return sqlite3_bind_int64(m_stmt, index, value);
		}
		template<>
		int bind<std::uint64_t>(int index, const std::uint64_t value)
		{
			return sqlite3_bind_int64(m_stmt, index, static_cast<std::int64_t>(value));
		}
		template<>
		int bind<double>(int index, const double value)
		{
			return sqlite3_bind_double(m_stmt, index, value);
		}
		template<>
		int bind<const char*>(int index, const char* value)
		{
			return sqlite3_bind_text(m_stmt, index, value, -1, SQLITE_TRANSIENT);
		}

		int bind(int index, std::string_view value, sqlite3_destructor_type flag = SQLITE_TRANSIENT)
		{
			return sqlite3_bind_text(m_stmt, index, value.data(), static_cast<int>(value.size()), flag);
		}

		int bind(int index, const std::string& value, sqlite3_destructor_type flag = SQLITE_TRANSIENT)
		{
			return sqlite3_bind_text(m_stmt, index, value.data(), static_cast<int>(value.size()), flag);
		}

		int get_column_int(int index)
		{
			return sqlite3_column_int(m_stmt, index);
		}

		int64_t get_column_int64(int index)
		{
			return sqlite3_column_int64(m_stmt, index);
		}

		double get_column_double(int index)
		{
			return sqlite3_column_double(m_stmt, index);
		}

		const unsigned char* get_column_str(int index)
		{
			return sqlite3_column_text(m_stmt, index);
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
			int rc = sqlite3_step(m_stmt);
			in = m_stmt;
			return rc;
		}

		int step()
		{
			return sqlite3_step(m_stmt);
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
				throw exception::sqlite_runtime_exception("Stmt ptr is nullptr");
			}
			return m_stmt;
		}
	private:
		sqlite3_stmt* m_stmt;
	};
}