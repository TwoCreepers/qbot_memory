#pragma once

#include <functional>
#include <pybind11/gil.h>

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
				pybind11::gil_scoped_acquire lock;
				return m_func(std::forward<Args>(args)...);
			}
		}
	private:
		std::function<F> m_func;
	};
}