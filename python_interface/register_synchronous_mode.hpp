#pragma once
#include <pybind11/pybind11.h>
#include <sqlite.hpp>
namespace py = pybind11;
void register_sqlite_synchronous_mode(py::module& m)
{
    py::enum_<memory::sqlite::synchronous_mode>(m, "synchronous_mode")
        .value("OFF", memory::sqlite::synchronous_mode::OFF)
        .value("NORMAL", memory::sqlite::synchronous_mode::NORMAL)
        .value("FULL", memory::sqlite::synchronous_mode::FULL)
        .export_values();
}