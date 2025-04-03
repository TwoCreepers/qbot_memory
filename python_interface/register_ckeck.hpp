#pragma once
#include <memory.hpp>
#include <pybind11/cast.h>
#include <pybind11/pybind11.h>
namespace py = pybind11;
void register_ckecks(py::module_& m)
{
    m.def("check_limit", &memory::check_limit, py::arg("limit"));
    m.def("ckeck_k", &memory::ckeck_k, py::arg("k"));
}