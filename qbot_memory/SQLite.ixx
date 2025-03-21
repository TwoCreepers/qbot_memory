export module memory:sqlite;
import std;
import <sqlite3.h>;

/*
* 不应导出符号的命名空间
*/
namespace sqlite
{

}

export namespace sqlite
{
	namespace exception
	{
		class exception : public std::exception
		{
		public:
			exception(const char* msg = "Unknown exception.") :m_message(msg), m_stacktrace{ std::stacktrace::current() }
			{
				std::ostringstream oss;
				oss << "{\n"
					<< R"("message": ")" << m_message << R"(",)" << '\n'
					<< R"("stacktrace":")" << '\n' << m_stacktrace << "\"\n"
					<< '}';
				m_what_str = oss.str();
			}
			virtual ~exception() noexcept = default;

			virtual std::string msg() const noexcept { return m_message; }
			virtual std::stacktrace stacktrace() const noexcept { return m_stacktrace; }
			virtual const char* what() const noexcept override { return m_what_str.c_str(); }
		private:
			std::string m_message;
			std::stacktrace m_stacktrace;
			std::string m_what_str;
		};
		class runtime_exception : public exception
		{
		public:
			runtime_exception(const char* msg = "Unknown runtime exception.") : exception(msg) {}
			virtual ~runtime_exception() = default;
		};
		class sqlite_runtime_exception :public runtime_exception
		{
		public:
			sqlite_runtime_exception(const char* msg = "Unknown sqlite runtime exception.") : runtime_exception(msg) {}
			virtual ~sqlite_runtime_exception() = default;
		};
		class transaction_exception : public sqlite_runtime_exception
		{
		public:
			transaction_exception(const char* msg = "Unknown transaction exception.") :sqlite_runtime_exception(msg) {}
			virtual ~transaction_exception() = default;
		};
		class double_transaction_exception : public transaction_exception
		{
		public:
			double_transaction_exception(const char* msg = "Double transaction exception.") :transaction_exception(msg) {}
			virtual ~double_transaction_exception() = default;
		};
		class using_close_transaction_exception : public transaction_exception
		{
		public:
			using_close_transaction_exception(const char* msg = "Using close transaction exception.") :transaction_exception(msg) {}
			virtual ~using_close_transaction_exception() = default;
		};
	}

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
		database(database&& _That)
		{
			this->m_db = std::move(_That.m_db);
			_That.m_db = nullptr;
		}
		database& operator=(const database& _That) = delete;
		database& operator=(database&& _That)
		{
			this->m_db = std::move(_That.m_db);
			_That.m_db = nullptr;
			return *this;
		}
		void open(std::string_view path)
		{
			if (sqlite3_open(path.data(), &m_db) != SQLITE_OK) {
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
			if (res = sqlite3_exec(m_db, sql.c_str(), *(callback_func.target<exec_callback_func_ptr>()), user_ptr, &errMsg) != SQLITE_OK) {
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
		stmt_buffer() = default;
		stmt_buffer(sqlite3_stmt* stmt, int i) :m_column_type{ static_cast<SQLite_Ty>(sqlite3_column_type(stmt, i)) }
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
		stmt_buffer(stmt_buffer&& _That)
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
		stmt_buffer& operator=(stmt_buffer&& _That)
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
		transaction(database& db, transaction_level level = DEFAULT) :m_db{ db }
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
			switch (level) {
			case DEFAULT:    sql = "BEGIN;"; break;
			case DEFERRED:  sql = "BEGIN DEFERRED;"; break;
			case IMMEDIATE: sql = "BEGIN IMMEDIATE;"; break;
			case EXCLUSIVE:  sql = "BEGIN EXCLUSIVE;"; break;
			default: sql = "BEGIN;"; break;
			}
			if (m_db.execute(sql) != SQLITE_OK) {
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
			int rc = m_db.execute("COMMIT;");
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
			int rc = m_db.execute("ROLLBACK;");
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
			return m_db.execute(sql, callback_func, user_ptr);
		}
		database& get()
		{
			if (!m_active)
			{
				throw exception::using_close_transaction_exception();
			}
			return m_db;
		}
	private:
		database& m_db;
		std::atomic_bool m_active;
	};

	class stmt
	{
	public:
		stmt(database& db, std::string_view sql, unsigned int prepFlags = NULL)
		{
			open(db, sql, prepFlags);
		}
		stmt(transaction& ta, std::string_view sql, unsigned int prepFlags = NULL)
		{
			open(ta.get(), sql, prepFlags);
		}
		~stmt()
		{
			close();
		}
		stmt(const stmt& _That) = delete;
		stmt(stmt&& _That)
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
		void open(database& db, std::string_view sql, unsigned int prepFlags = NULL)
		{
			auto res = sqlite3_prepare_v3(db.get(), sql.data(), static_cast<int>(sql.size()), prepFlags, &m_stmt, nullptr);
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
		template <class T>
		int bind(int index, T&& value)
		{
			static_assert(false, "使用了一个bind函数不支持的类型");
			return sqlite3_bind_null(m_stmt, index);
		}
		template <class T>
		int bind(int index, const T&& value)
		{
			static_assert(false, "使用了一个bind函数不支持的类型");
			return sqlite3_bind_null(m_stmt, index);
		}
		template<>
		int bind<int>(int index, const int&& value)
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
		int bind<std::string_view>(int index, std::string_view value)
		{
			return sqlite3_bind_text(m_stmt, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
		}
		template<>
		int bind<std::string>(int index, const std::string&& value)
		{
			return sqlite3_bind_text(m_stmt, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
		}
		template<>
		int bind<const char*>(int index, const char* value)
		{
			return sqlite3_bind_text(m_stmt, index, value, -1, SQLITE_TRANSIENT);
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