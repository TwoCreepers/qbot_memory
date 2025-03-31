#pragma once
#include <memory.hpp>
#include <pybind11/pybind11.h>
namespace py = pybind11;
void register_data(py::module_& m)
{
    py::class_<memory::insert_data>(m, "insert_data")
        .def(py::init<>())
        .def(py::init<std::size_t, std::string, std::string, std::string, double>(),
            py::arg("time"),
            py::arg("sender"),
            py::arg("sender_uuid"),
            py::arg("message"),
            py::arg("forget_probability"))
        .def_readwrite("time", &memory::insert_data::time)
        .def_readwrite("sender", &memory::insert_data::sender)
        .def_readwrite("sender_uuid", &memory::insert_data::sender_uuid)
        .def_readwrite("message", &memory::insert_data::message)
        .def_readwrite("forget_probability", &memory::insert_data::forget_probability);

    py::class_<memory::select_data>(m, "select_data")
        .def(py::init<>())
        .def(py::init<std::size_t, std::size_t, std::string, std::string, std::string>(),
            py::arg("id"),
            py::arg("time"),
            py::arg("sender"),
            py::arg("sender_uuid"),
            py::arg("message"))
        .def_readwrite("id", &memory::select_data::id)
        .def_readwrite("time", &memory::select_data::time)
        .def_readwrite("sender", &memory::select_data::sender)
        .def_readwrite("sender_uuid", &memory::select_data::sender_uuid)
        .def_readwrite("message", &memory::select_data::message);

    py::class_<memory::select_fts_data>(m, "select_fts_data")
        .def(py::init<>())
        .def(py::init<std::size_t, std::size_t, std::string, std::string, std::string>(),
            py::arg("id"),
            py::arg("time"),
            py::arg("sender"),
            py::arg("sender_uuid"),
            py::arg("message"))
        .def_readwrite("id", &memory::select_fts_data::id)
        .def_readwrite("time", &memory::select_fts_data::time)
        .def_readwrite("sender", &memory::select_fts_data::sender)
        .def_readwrite("sender_uuid", &memory::select_fts_data::sender_uuid)
        .def_readwrite("message", &memory::select_fts_data::message);

    py::class_<memory::select_vector_data>(m, "select_vector_data")
        .def(py::init<>())
        .def(py::init<std::size_t, std::size_t, std::string, std::string, std::string, double>(),
            py::arg("id"),
            py::arg("time"),
            py::arg("sender"),
            py::arg("sender_uuid"),
            py::arg("message"),
            py::arg("distance"))
        .def_readwrite("id", &memory::select_vector_data::id)
        .def_readwrite("time", &memory::select_vector_data::time)
        .def_readwrite("sender", &memory::select_vector_data::sender)
        .def_readwrite("sender_uuid", &memory::select_vector_data::sender_uuid)
        .def_readwrite("message", &memory::select_vector_data::message)
        .def_readwrite("distance", &memory::select_vector_data::distance);
}