#pragma once

#include <exception>
#include <functional>
#include <sstream>
#include <stacktrace>
#include <stdexcept>
#include <string>
#include <utility>

namespace memory::exception
{
    class base_exception : public std::exception
    {
    public:
        using frame_filter_func = std::function<bool(const std::stacktrace_entry&)>;

        base_exception(const char* msg = "未知标准异常 请注意! 该异常不会抛出! 如果你收到这个异常 这是意外情况",
            frame_filter_func filter = default_frame_filter,
            const char* name = "标准异常")
            : m_message(msg),
            m_name(name),
            m_stacktrace(std::stacktrace::current()),
            m_frame_filter(std::move(filter))
        {
            format_what();
        }

        base_exception(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "标准异常")
            : m_message(std::move(msg)),
            m_name(name),
            m_stacktrace(std::stacktrace::current()),
            m_frame_filter(std::move(filter))
        {
            format_what();
        }

        virtual ~base_exception() noexcept = default;

        virtual std::string name() const noexcept { return m_name; }
        virtual std::string msg() const noexcept { return m_message; }
        virtual std::stacktrace stacktrace() const noexcept { return m_stacktrace; }
        virtual const char* what() const noexcept override { return m_what_str.c_str(); }

        static bool default_frame_filter(const std::stacktrace_entry& frame)
        {
            std::string file = frame.source_file();
            std::string func = frame.description();

            // 跳过 exception.hpp 并且 memory::exception 命名空间的帧
            // 默认 所有具有栈追踪的 异常都在这里
            return !(file.find("exception.hpp") != std::string::npos &&
                func.find("memory::exception::") != std::string::npos);
        }

    protected:
        void format_what()
        {
            std::ostringstream oss;
            oss << "[" << m_name << "] " << m_message << "\n"
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

            const size_t frame_to_show = find_first_non_exception_frame(m_stacktrace);
            const auto& frame = m_stacktrace[frame_to_show];

            std::ostringstream oss;
            oss << frame.description();
            if (frame.source_line() != 0)
            {
                oss << " (" << frame.source_file() << ":" << frame.source_line() << ")";
            }
            return oss.str();
        }

        size_t find_first_non_exception_frame(const std::stacktrace& trace) const
        {
            for (size_t i = 0; i < trace.size(); ++i)
            {
                if (m_frame_filter(trace[i]))
                {
                    return i;
                }
            }
            return 0;
        }

        std::string m_name;
        std::string m_message;
        std::stacktrace m_stacktrace;
        std::string m_what_str;
        frame_filter_func m_frame_filter;
    };

    class runtime_error : public base_exception, public std::runtime_error
    {
    public:
        runtime_error(const char* msg = "未知运行时错误",
            frame_filter_func filter = default_frame_filter,
            const char* name = "运行时错误")
            : base_exception(msg, filter, name),
            std::runtime_error(msg)
        {
        }

        runtime_error(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "运行时错误")
            : base_exception(msg, filter, name),
            std::runtime_error(std::move(msg))
        {
        }

        virtual ~runtime_error() = default;
    };

    class bad_exception : public runtime_error
    {
    public:
        bad_exception(const char* msg = "未知错误异常",
            frame_filter_func filter = default_frame_filter,
            const char* name = "错误异常")
            : runtime_error(msg, filter, name)
        {
        }

        bad_exception(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "错误异常")
            : runtime_error(std::move(msg), filter, name)
        {
        }

        virtual ~bad_exception() = default;
    };

    class bad_function_call : public bad_exception
    {
    public:
        bad_function_call(const char* msg = "调用了错误的函数",
            frame_filter_func filter = default_frame_filter,
            const char* name = "函数调用异常")
            : bad_exception(msg, filter, name)
        {
        }

        bad_function_call(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "函数调用异常")
            : bad_exception(std::move(msg), filter, name)
        {
        }

        virtual ~bad_function_call() = default;
    };

    class bad_stmt : public bad_exception
    {
    public:
        bad_stmt(const char* msg = "错误的语句",
            frame_filter_func filter = default_frame_filter,
            const char* name = "预编译SQL语句异常")
            : bad_exception(msg, filter, name)
        {
        }

        bad_stmt(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "预编译SQL语句异常")
            : bad_exception(std::move(msg), filter, name)
        {
        }

        virtual ~bad_stmt() = default;
    };

    class stmt_call_error : public bad_stmt
    {
    public:
        stmt_call_error(const char* msg = "预编译SQL语句调用错误",
            frame_filter_func filter = default_frame_filter,
            const char* name = "预编译SQL语句调用错误")
            : bad_stmt(msg, filter, name)
        {
        }

        stmt_call_error(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "预编译SQL语句调用错误")
            : bad_stmt(std::move(msg), filter, name)
        {
        }

        virtual ~stmt_call_error() = default;
    };

    class stmt_bind_error : public bad_stmt
    {
    public:
        stmt_bind_error(const char* msg = "预编译SQL语句绑定失败",
            frame_filter_func filter = default_frame_filter,
            const char* name = "预编译SQL语句绑定失败")
            : bad_stmt(msg, filter, name)
        {
        }

        stmt_bind_error(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "预编译SQL语句绑定失败")
            : bad_stmt(std::move(msg), filter, name)
        {
        }

        virtual ~stmt_bind_error() = default;
    };

    class stmt_reset_error : public bad_stmt
    {
    public:
        stmt_reset_error(const char* msg = "预编译SQL语句重置失败",
            frame_filter_func filter = default_frame_filter,
            const char* name = "预编译SQL语句重置失败")
            : bad_stmt(msg, filter, name)
        {
        }

        stmt_reset_error(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "预编译SQL语句重置失败")
            : bad_stmt(std::move(msg), filter, name)
        {
        }

        virtual ~stmt_reset_error() = default;
    };

    class bad_transaction : public bad_exception
    {
    public:
        bad_transaction(const char* msg = "错误的事务",
            frame_filter_func filter = default_frame_filter,
            const char* name = "事务错误")
            : bad_exception(msg, filter, name)
        {
        }

        bad_transaction(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "事务错误")
            : bad_exception(std::move(msg), filter, name)
        {
        }

        virtual ~bad_transaction() = default;
    };

    class bad_database : public bad_exception
    {
    public:
        bad_database(const char* msg = "数据库错误",
            frame_filter_func filter = default_frame_filter,
            const char* name = "数据库错误")
            : bad_exception(msg, filter, name)
        {
        }

        bad_database(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "数据库错误")
            : bad_exception(std::move(msg), filter, name)
        {
        }

        virtual ~bad_database() = default;
    };

    class sqlite_call_error : public bad_database
    {
    public:
        sqlite_call_error(const char* msg = "SQLite调用错误",
            frame_filter_func filter = default_frame_filter,
            const char* name = "SQLite调用错误")
            : bad_database(msg, filter, name)
        {
        }

        sqlite_call_error(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "SQLite调用错误")
            : bad_database(std::move(msg), filter, name)
        {
        }

        virtual ~sqlite_call_error() = default;
    };

    class sqlite_extension_error : public sqlite_call_error
    {
    public:
        sqlite_extension_error(const char* msg = "未知SQLite扩展异常",
            frame_filter_func filter = default_frame_filter,
            const char* name = "SQLite扩展异常")
            : sqlite_call_error(msg, filter, name)
        {
        }

        sqlite_extension_error(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "SQLite扩展异常")
            : sqlite_call_error(std::move(msg), filter, name)
        {
        }

        virtual ~sqlite_extension_error() = default;
    };

    class invalid_argument : public bad_exception, public std::invalid_argument
    {
    public:
        invalid_argument(const char* msg = "无效的参数",
            frame_filter_func filter = default_frame_filter,
            const char* name = "无效参数异常")
            : bad_exception(msg, filter, name), std::invalid_argument(msg) {}

        invalid_argument(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "无效参数异常")
            : bad_exception(msg, filter, name), std::invalid_argument(std::move(msg)) {}

        virtual ~invalid_argument() = default;
    };

    class length_error : public bad_exception
    {
    public:
        length_error(const char* msg = "长度错误",
            frame_filter_func filter = default_frame_filter,
            const char* name = "长度异常")
            : bad_exception(msg, filter, name)
        {
        }

        length_error(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "长度异常")
            : bad_exception(std::move(msg), filter, name)
        {
        }

        virtual ~length_error() = default;
    };

    class out_of_range : public bad_exception
    {
    public:
        out_of_range(const char* msg = "超出有效范围",
            frame_filter_func filter = default_frame_filter,
            const char* name = "范围异常")
            : bad_exception(msg, filter, name)
        {
        }

        out_of_range(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "范围异常")
            : bad_exception(std::move(msg), filter, name)
        {
        }

        virtual ~out_of_range() = default;
    };

    class bad_alloc : public bad_exception, public std::bad_alloc
    {
    public:
        bad_alloc(const char* msg = "内存分配失败",
            frame_filter_func filter = default_frame_filter,
            const char* name = "内存分配异常")
            : bad_exception(msg, filter, name)
        {
        }

        bad_alloc(std::string msg,
            frame_filter_func filter = default_frame_filter,
            const char* name = "内存分配异常")
            : bad_exception(std::move(msg), filter, name)
        {
        }

        virtual ~bad_alloc() = default;
    };
}