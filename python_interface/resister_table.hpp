#pragma once
#include <memory.hpp>
#include <pybind11/cast.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <string>
namespace py = pybind11;
void register_table(py::module_& m)
{
    py::class_<memory::table, std::shared_ptr<memory::table>>(m, "table")
        .def(py::init<std::shared_ptr<memory::database>, const std::string&, int, int>(),
            py::arg("db"),
            py::arg("name"),
            py::arg("vector_dimension"),
            py::arg("HNWS_max_connect") = 32
        )
        // 向量生成回调函数
        .def("set_vector", &memory::table::set_vector,
            py::arg("func"))
        .def("set_vectors", &memory::table::set_vectors,
            py::arg("func"))
        // HNSW参数设置
        .def("set_hnsw_efSearch", &memory::table::set_hnsw_efSearch,
            py::arg("efSearch"))
        // 数据操作
        .def("add", &memory::table::add,
            py::arg("data"))
        .def("adds", &memory::table::adds,
            py::arg("datas"))
        // 搜索方法
        .def("search_id", &memory::table::search_id,
            py::arg("id"))
        .def("search_list_uuid", &memory::table::search_list_uuid,
            py::arg("uuid"))
        .def("search_list_uuid_limit", &memory::table::search_list_uuid_limit,
            py::arg("uuid"),
            py::arg("limit"))
        .def("search_list_time_start", &memory::table::search_list_time_start,
            py::arg("start"))
        .def("search_list_time_end", &memory::table::search_list_time_end,
            py::arg("end"))
        .def("search_list_time_start_end", &memory::table::search_list_time_start_end,
            py::arg("start"),
            py::arg("end"))
        // 全文搜索
        .def("search_list_fts_impl", &memory::table::search_list_fts_impl,
            py::arg("fts") = py::none(),
            py::arg("simple_query") = py::none(),
            py::arg("start") = py::none(),
            py::arg("end") = py::none(),
            py::arg("limit") = py::none())
        // 向量搜索
        .def("search_list_vector_text", &memory::table::search_list_vector_text,
            py::arg("message"),
            py::arg("k"))
        .def("search_list_vector_texts", &memory::table::search_list_vector_texts,
            py::arg("messages"),
            py::arg("k"))
        // 索引管理
        .def("forgotten", &memory::table::forgotten)
        .def("rebuild_faiss_index", &memory::table::rebuild_faiss_index)
        .def("full_rebuild_faiss_index", &memory::table::full_rebuild_faiss_index)
        // 表操作
        .def("drop", &memory::table::drop)
        // 自动保存
        .def("save_faiss_index", &memory::table::save_faiss_index);
}