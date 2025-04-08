#pragma once

#include <pybind11/pybind11.h>

import version;
namespace py = pybind11;
void set_module_info(py::module_& m)
{
	m.attr("__version__") = version::VERSION;
	m.attr("git_hash") = version::GIT_HASH;
	m.attr("git_tag") = version::GIT_TAG;
	m.attr("git_branch") = version::GIT_BRANCH;
	m.attr("git_commit_time") = version::GIT_COMMIT_TIME;
	m.attr("build_time") = version::BUILD_TIME;
	m.attr("build_python_version") = version::PYTHON_VERSION;
	m.attr("build_pybind11_version") = version::PYBIND11_VERSION;
}