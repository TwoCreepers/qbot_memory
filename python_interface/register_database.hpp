#pragma once
#include <memory.hpp>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
namespace py = pybind11;
void register_database(py::module& m)
{
    py::class_<memory::database, std::shared_ptr<memory::database>>(m, "database")
        .def(py::init<const fs::path&, const fs::path&>(),
            py::arg("db_file_path"),
            py::arg("simple_path"))
        .def("db_file_path", &memory::database::db_file_path)
        .def("set_synchronous", &memory::database::set_synchronous,
            py::arg("synchronous"))
        .def("set_wal_autocheckpoint", &memory::database::set_wal_autocheckpoint,
            py::arg("wal_autocheckpoint"))
        .def("wal_checkpoint", &memory::database::wal_checkpoint,
            py::arg("mode"),
            py::arg("db_name"),
            py::arg("log") = nullptr,
            py::arg("ckpt") = nullptr);
}