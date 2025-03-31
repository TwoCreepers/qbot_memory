#pragma once

#include "exception.hpp"
#include <functional>
#ifdef ENABLE_GET_GIL_BEFORE_CALL
#include <pybind11/gil.h>
#endif

namespace memory::py
{
	template <class F>
	class function
	{
	public:
		function() = default;
		function(std::function<F> func) : m_func{ func } {}
		~function() = default;
		operator bool()
		{
			return m_func.operator bool();
		}
		template <typename... Args>
		auto operator()(Args&&... args)
		{
			if (m_func)
			{
#ifdef ENABLE_GET_GIL_BEFORE_CALL
				pybind11::gil_scoped_acquire lock;
#endif
				return m_func(std::forward<Args>(args)...);
			}
			else
			{
				throw exception::bad_function_call("调用空的函数");
			}
		}
	private:
		std::function<F> m_func;
	};
}