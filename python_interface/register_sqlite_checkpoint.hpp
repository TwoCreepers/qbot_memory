#pragma once
#include <pybind11/pybind11.h>
#include <sqlite.hpp>
namespace py = pybind11;
void register_sqlite_checkpoint(py::module_& m)
{
    py::enum_<memory::sqlite::checkpoint::checkpoint>(m, "checkpoint_mode")
        .value("PASSIVE", memory::sqlite::checkpoint::checkpoint::PASSIVE)
        .value("FULL", memory::sqlite::checkpoint::checkpoint::FULL)
        .value("RESTART", memory::sqlite::checkpoint::checkpoint::RESTART)
        .value("TRUNCATE", memory::sqlite::checkpoint::checkpoint::TRUNCATE)
        .export_values();
}