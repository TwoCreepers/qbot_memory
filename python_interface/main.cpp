#define ENABLE_GET_GIL_BEFORE_CALL
#include <memory.hpp>
#include <pybind11/pybind11.h>
namespace py = pybind11;
#include <memory>
#include <string>
#include <pybind11/cast.h>
#include <pybind11/detail/common.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
namespace me = memory;
PYBIND11_MODULE(qbot_memory, m)
{
    py::class_<me::insert_data>(m, "insert_data")
        .def(py::init<>())
        .def_readwrite("time", &me::insert_data::time)
        .def_readwrite("sender", &me::insert_data::sender)
        .def_readwrite("sender_uuid", &me::insert_data::sender_uuid)
        .def_readwrite("message", &me::insert_data::message)
        .def_readwrite("forget_probability", &me::insert_data::forget_probability)
        ;
    py::class_<me::select_data>(m, "select_data")
        .def(py::init<>())
        .def_readwrite("id", &me::select_data::id)
        .def_readwrite("time", &me::select_data::time)
        .def_readwrite("sender", &me::select_data::sender)
        .def_readwrite("sender_uuid", &me::select_data::sender_uuid)
        .def_readwrite("message", &me::select_data::message)
        ;
    py::class_<me::select_fts_data>(m, "select_fts_data")
        .def(py::init<>())
        .def_readwrite("id", &me::select_fts_data::id)
        .def_readwrite("time", &me::select_fts_data::time)
        .def_readwrite("sender", &me::select_fts_data::sender)
        .def_readwrite("sender_uuid", &me::select_fts_data::sender_uuid)
        .def_readwrite("message", &me::select_fts_data::message)
        ;
    py::class_<me::select_vector_data>(m, "select_vector_data")
        .def(py::init<>())
        .def_readwrite("id", &me::select_vector_data::id)
        .def_readwrite("time", &me::select_vector_data::time)
        .def_readwrite("sender", &me::select_vector_data::sender)
        .def_readwrite("sender_uuid", &me::select_vector_data::sender_uuid)
        .def_readwrite("message", &me::select_vector_data::message)
        .def_readwrite("distance", &me::select_vector_data::distance)
        ;
    
    py::class_<me::database, std::shared_ptr<me::database>>(m, "database")
        .def(py::init<const std::string&, const std::string&, const std::string&>(),
            py::arg("db_file_path"), py::arg("simple_path"), py::arg("simple_dict_path"))
        .def("db_file_path", &me::database::db_file_path)
        ;
    py::class_<me::table>(m, "table")
        .def(py::init<std::shared_ptr<me::database>, const std::string&, const int, const int>(),
            py::arg("db"), py::arg("name"), py::arg("vector_dimension"), py::arg("HNWS_max_connect") = 32,
            py::keep_alive<1, 2>()
        )
        .def("set_vector", &me::table::set_vector)
        .def("set_vectors", &me::table::set_vectors)
        .def("add", &me::table::add)
        .def("adds", &me::table::adds)
        .def("search_id", &me::table::search_id)
        .def("search_list_uuid", &me::table::search_list_uuid)
        .def("search_list_uuid_limit", &me::table::search_list_uuid_limit)
        .def("search_list_time_start", &me::table::search_list_time_start)
        .def("search_list_time_end", &me::table::search_list_time_end)
        .def("search_list_time_start_end", &me::table::search_list_time_start_end)
        .def("search_list_fts", &me::table::search_list_fts)
        .def("search_list_fts_limit", &me::table::search_list_fts_limit)
        .def("search_list_vector_text", &me::table::search_list_vector_text)
        ;
}