export module memory:exception;

import std;

export namespace memory::exception
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