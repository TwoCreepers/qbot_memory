#pragma once
#include <exception.hpp>
#include <pybind11/pybind11.h>
#include <pyerrors.h>
namespace py = pybind11;

void register_exceptions(py::module_& m)
{
    // 注册基础异常
    py::register_exception<memory::exception::base_exception>(m, "base_exception", PyExc_Exception);

    // 运行时错误相关
    py::register_exception<memory::exception::runtime_error>(m, "runtime_error", m.attr("base_exception"));
    py::register_exception<memory::exception::bad_exception>(m, "bad_exception", m.attr("runtime_error"));

    // 函数调用相关
    py::register_exception<memory::exception::bad_function_call>(m, "bad_function_call", m.attr("bad_exception"));

    // SQL语句相关
    py::register_exception<memory::exception::bad_stmt>(m, "bad_stmt", m.attr("bad_exception"));
    py::register_exception<memory::exception::stmt_call_error>(m, "stmt_call_error", m.attr("bad_stmt"));
    py::register_exception<memory::exception::stmt_bind_error>(m, "stmt_bind_error", m.attr("bad_stmt"));
    py::register_exception<memory::exception::stmt_reset_error>(m, "stmt_reset_error", m.attr("bad_stmt"));

    // 数据库事务相关
    py::register_exception<memory::exception::bad_transaction>(m, "bad_transaction", m.attr("bad_exception"));

    // 数据库相关
    py::register_exception<memory::exception::bad_database>(m, "bad_database", m.attr("bad_exception"));
    py::register_exception<memory::exception::wal_error>(m, "wal_error", m.attr("bad_database"));
    py::register_exception<memory::exception::sqlite_call_error>(m, "sqlite_call_error", m.attr("bad_database"));
    py::register_exception<memory::exception::sqlite_extension_error>(m, "sqlite_extension_error", m.attr("sqlite_call_error"));

    // 参数相关
    py::register_exception<memory::exception::invalid_argument>(m, "invalid_argument", m.attr("runtime_error"));

    // 长度和范围相关
    py::register_exception<memory::exception::length_error>(m, "length_error", m.attr("runtime_error"));
    py::register_exception<memory::exception::range_error>(m, "range_error", m.attr("runtime_error"));
    py::register_exception<memory::exception::out_of_range>(m, "out_of_range", m.attr("range_error"));

    // 内存和溢出相关
    py::register_exception<memory::exception::bad_alloc>(m, "bad_alloc", m.attr("bad_exception"));
    py::register_exception<memory::exception::overflow_error>(m, "overflow_error", m.attr("runtime_error"));
}