export module memory:thread_pool;

import std;

export namespace memory::thread_pool
{
	template <std::size_t threads = 4, std::size_t max_tasks = 64>
	class thread_pool
	{
	public:
		thread_pool() : m_stop{}, m_tasks_conut(0)
		{
			for (size_t i = 0; i < threads; i++)
			{
				m_workers.emplace_back([this] { worker_func(); });
			}
		}
		~thread_pool()
		{
			m_stop.test_and_set();
			m_condition.notify_all();
		}
		thread_pool(thread_pool&& _That) = delete;

		std::future<void> enqueue(std::function<void(void)> f)
		{
			if (m_tasks_conut >= max_tasks)
				return {};
			auto promise = std::make_shared<std::promise<void>>();
			auto future = promise->get_future();
			auto func = [promise, f]
				{
					try
					{
						f();
						promise->set_value();
					}
					catch (...) { promise->set_exception(std::current_exception()); }
				};
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_tasks_conut++;
				m_tasks.emplace(func);
			}
			m_condition.notify_one();
			return future;
		}

		std::size_t size()
		{
			return m_tasks_conut;
		}
	private:
		std::atomic_flag m_stop;
		std::atomic<std::size_t> m_tasks_conut;
		std::vector<std::jthread> m_workers;
		std::queue<std::function<void()>> m_tasks;

		// 同步原语
		std::mutex m_mutex;
		std::condition_variable m_condition;

		void worker_func(void)
		{
			while (true)
			{
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(this->m_mutex);
					this->m_condition.wait(lock, [this] { return this->m_stop.test() || !this->m_tasks.empty(); });
					if (this->m_stop.test())
						return;
					task = std::move(this->m_tasks.front());
					this->m_tasks.pop();
				}
				try
				{
					task();
				}
				catch (...) {}
				m_tasks_conut--;
			}
		}
	};
}