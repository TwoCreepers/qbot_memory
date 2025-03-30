#pragma once

#include <exception>
#include <sstream>
#include <stacktrace>
#include <string>
#include <utility>

namespace memory::exception
{
    class exception : public std::exception
    {
    public:
        exception(const char* msg = "未知标准异常 请注意! 该异常不会抛出! 如果你收到这个异常 这是意外情况")
            : m_message(msg),
            m_stacktrace(std::stacktrace::current())
        {
            format_what();
        }

        exception(std::string msg)
            : m_message(std::move(msg)),
            m_stacktrace(std::stacktrace::current())
        {
            format_what();
        }

        virtual ~exception() noexcept = default;

        virtual std::string msg() const noexcept { return m_message; }
        virtual std::stacktrace stacktrace() const noexcept { return m_stacktrace; }
        virtual const char* what() const noexcept override { return m_what_str.c_str(); }

    protected:
        void format_what()
        {
            std::ostringstream oss;
            oss << "[" << exception_type() << "] " << m_message << "\n"
                << "位置: " << get_most_recent_frame() << "\n"
                << "调用栈:\n" << format_stacktrace();
            m_what_str = oss.str();
        }

        std::string format_stacktrace() const
        {
            std::ostringstream oss;
            for (const auto& frame : m_stacktrace)
            {
                oss << "    在 " << frame.description();
                if (frame.source_line() != 0)
                {
                    oss << " (" << frame.source_file() << ":" << frame.source_line() << ")";
                }
                oss << "\n";
            }
            return oss.str();
        }

        std::string get_most_recent_frame() const
        {
            if (m_stacktrace.empty()) return "未知位置";

            // 跳过第一帧（是构造函数本身）
            const size_t frame_to_show = m_stacktrace.size() > 1 ? 1 : 0;
            const auto& frame = m_stacktrace[frame_to_show];

            std::ostringstream oss;
            oss << frame.description();
            if (frame.source_line() != 0)
            {
                oss << " (" << frame.source_file() << ":" << frame.source_line() << ")";
            }
            return oss.str();
        }

        virtual const char* exception_type() const noexcept { return "标准异常"; }

        std::string m_message;
        std::stacktrace m_stacktrace;
        std::string m_what_str;
    };

    class runtime_error : public exception
    {
    public:
        runtime_error(const char* msg = "未知运行时错误") : exception(msg) {}
        runtime_error(std::string msg) : exception(std::move(msg)) {}
        virtual ~runtime_error() = default;
    protected:
        const char* exception_type() const noexcept override { return "运行时错误"; }
    };

    class bad_exception : public runtime_error
    {
    public:
        bad_exception(const char* msg = "未知错误异常") : runtime_error(msg) {}
        bad_exception(std::string msg) : runtime_error(std::move(msg)) {}
        virtual ~bad_exception() = default;
    protected:
        const char* exception_type() const noexcept override { return "错误异常"; }
    };

    class bad_function_call : public bad_exception
    {
    public:
        bad_function_call(const char* msg = "调用了错误的函数") : bad_exception(msg) {}
        bad_function_call(std::string msg) : bad_exception(std::move(msg)) {}
        virtual ~bad_function_call() = default;
    protected:
        const char* exception_type() const noexcept override { return "函数调用异常"; }
    };

    class bad_stmt : public bad_exception
    {
    public:
        bad_stmt(const char* msg = "错误的语句") : bad_exception(msg) {}
        bad_stmt(std::string msg) : bad_exception(std::move(msg)) {}
        virtual ~bad_stmt() = default;
    protected:
        const char* exception_type() const noexcept override { return "预编译SQL语句异常"; }
    };

    class stmt_call_error : public bad_stmt
    {
    public:
        stmt_call_error(const char* msg = "语句调用错误") : bad_stmt(msg) {}
        stmt_call_error(std::string msg) : bad_stmt(std::move(msg)) {}
        virtual ~stmt_call_error() = default;
    protected:
        const char* exception_type() const noexcept override { return "语句调用错误"; }
    };

    class stmt_bind_error : public bad_stmt
    {
    public:
        stmt_bind_error(const char* msg = "语句绑定失败") : bad_stmt(msg) {}
        stmt_bind_error(std::string msg) : bad_stmt(std::move(msg)) {}
        virtual ~stmt_bind_error() = default;
    protected:
        const char* exception_type() const noexcept override { return "语句绑定失败"; }
    };

    class stmt_reset_error : public bad_stmt
    {
    public:
        stmt_reset_error(const char* msg = "语句重置失败") : bad_stmt(msg) {}
        stmt_reset_error(std::string msg) : bad_stmt(std::move(msg)) {}
        virtual ~stmt_reset_error() = default;
    protected:
        const char* exception_type() const noexcept override { return "语句重置失败"; }
    };

    class bad_transaction : public bad_exception
    {
    public:
        bad_transaction(const char* msg = "错误的事务") : bad_exception(msg) {}
        bad_transaction(std::string msg) : bad_exception(std::move(msg)) {}
        virtual ~bad_transaction() = default;
    protected:
        const char* exception_type() const noexcept override { return "事务错误"; }
    };

    class bad_database : public bad_exception
    {
    public:
        bad_database(const char* msg = "数据库错误") : bad_exception(msg) {}
        bad_database(std::string msg) : bad_exception(std::move(msg)) {}
        virtual ~bad_database() = default;
    protected:
        const char* exception_type() const noexcept override { return "数据库错误"; }
    };

    class sqlite_call_error : public bad_database
    {
    public:
        sqlite_call_error(const char* msg = "SQLite调用错误") : bad_database(msg) {}
        sqlite_call_error(std::string msg) : bad_database(std::move(msg)) {}
        virtual ~sqlite_call_error() = default;
    protected:
        const char* exception_type() const noexcept override { return "SQLite调用错误"; }
    };

    class sqlite_extension_error : public sqlite_call_error
    {
    public:
        sqlite_extension_error(const char* msg = "未知SQLite扩展异常")
            : sqlite_call_error(msg)
        {
        }
        sqlite_extension_error(std::string msg) : sqlite_call_error(std::move(msg)) {}
        virtual ~sqlite_extension_error() = default;
    protected:
        const char* exception_type() const noexcept override { return "SQLite扩展异常"; }
    };

    class invalid_argument : public bad_exception
    {
    public:
        invalid_argument(const char* msg = "无效的参数") : bad_exception(msg) {}
        invalid_argument(std::string msg) : bad_exception(std::move(msg)) {}
        virtual ~invalid_argument() = default;
    protected:
        const char* exception_type() const noexcept override { return "无效参数异常"; }
    };

    class length_error : public bad_exception
    {
    public:
        length_error(const char* msg = "长度错误") : bad_exception(msg) {}
        length_error(std::string msg) : bad_exception(std::move(msg)) {}
        virtual ~length_error() = default;
    protected:
        const char* exception_type() const noexcept override { return "长度异常"; }
    };

    class out_of_range : public bad_exception
    {
    public:
        out_of_range(const char* msg = "超出有效范围") : bad_exception(msg) {}
        out_of_range(std::string msg) : bad_exception(std::move(msg)) {}
        virtual ~out_of_range() = default;
    protected:
        const char* exception_type() const noexcept override { return "范围异常"; }
    };

    class bad_alloc : public bad_exception
    {
    public:
        bad_alloc(const char* msg = "内存分配失败") : bad_exception(msg) {}
        bad_alloc(std::string msg) : bad_exception(std::move(msg)) {}
        virtual ~bad_alloc() = default;
    protected:
        const char* exception_type() const noexcept override { return "内存分配异常"; }
    };
}