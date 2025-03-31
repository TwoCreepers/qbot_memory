#define ENABLE_GET_GIL_BEFORE_CALL
#include "register_ckeck.hpp"
#include "register_data.hpp"
#include "register_database.hpp"
#include "register_exceptions.hpp"
#include "register_sqlite_checkpoint.hpp"
#include "resister_table.hpp"
#include <py.hpp>
#include <pybind11/detail/common.h>
#include <pybind11/stl.h>
namespace py = pybind11;
namespace me = memory;
PYBIND11_MODULE(qbot_memory, m)
{
	register_exceptions(m);
	register_ckecks(m);
	register_data(m);
	register_sqlite_checkpoint(m);
	register_database(m);
	register_table(m);
}