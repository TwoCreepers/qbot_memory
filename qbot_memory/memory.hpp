#pragma once

#include "exception.hpp"
#include "faiss.hpp"
#include "py.hpp"
#include "sqlite.hpp"
#include <faiss/index_io.h>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <new>
#include <random>
#include <ranges>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace memory
{
	inline void check_limit(const faiss::idx_t limit)
	{
		if (limit <= 0)
		{
			throw exception::invalid_argument(std::format("limit不能小于0, 但实际值为: {}", limit));
		}
	}
	inline void ckeck_k(const faiss::idx_t k)
	{
		if (k <= 0)
		{
			throw exception::invalid_argument(std::format("k不能小于0, 但实际值为: {}", k));
		}
	}
	inline void ckeck_id(const std::size_t id)
	{
		if (id <= 1)
		{
			throw exception::invalid_argument(std::format("id不能小于0, 但实际值为: {}", id));
		}
	}

	struct insert_data
	{
		std::size_t time;
		std::string sender;
		std::string sender_uuid;
		std::string message;
		double forget_probability;
	};

	struct select_data
	{
		std::size_t id;
		std::size_t time;
		std::string sender;
		std::string sender_uuid;
		std::string message;
	};

	struct select_fts_data
	{
		std::size_t id;
		std::size_t time;
		std::string sender;
		std::string sender_uuid;
		std::string message;
	};

	struct select_vector_data
	{
		std::size_t id;
		std::size_t time;
		std::string sender;
		std::string sender_uuid;
		std::string message;
		double distance;
	};

	class database
	{
	public:
		database(const fs::path& db_file_path, const fs::path& simple_path, const fs::path& simple_dict_path)
			: m_db{ new sqlite::database(db_file_path.string()) },
			m_db_file_path{ db_file_path }
		{
			m_db->enable_load_extension();
			m_db->load_extension(simple_path.string());
			sqlite::stmt jieba_dict_sql{ m_db, "SELECT jieba_dict(?);" };
			jieba_dict_sql.bind(1, simple_dict_path.string(), SQLITE_STATIC);
			jieba_dict_sql.step();

			m_db->execute(R"(
				CREATE TABLE IF NOT EXISTS __TABLE_MANAGE__ (
				id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
				tablename TEXT NOT NULL,
				vector_dimension INTEGER NOT NULL,
				HNWS_max_connect INTEGER NOT NULL,
				faiss_fullpath TEXT NOT NULL,
				faiss_new_id INTEGER NOT NULL
				);
				)");
			m_db->execute("PRAGMA journal_mode=WAL;");
		}
		~database() = default;

		const fs::path db_file_path() const noexcept
		{
			return m_db_file_path;
		}

		std::shared_ptr<sqlite::database> get() noexcept
		{
			return m_db;
		}

		void set_synchronous(const sqlite::synchronous_mode synchronous)
		{
			m_db->set_synchronous(synchronous);
		}
		void set_wal_autocheckpoint(const std::size_t wal_autocheckpoint)
		{
			m_db->set_wal_autocheckpoint(wal_autocheckpoint);
		}
		void wal_checkpoint(sqlite::checkpoint::checkpoint moed, std::string_view db_name, int* log, int* ckpt)
		{
			m_db->wal_checkpoint(moed, db_name, log, ckpt);
		}
	private:
		const fs::path m_db_file_path;
		std::shared_ptr<sqlite::database> m_db;
	};

	class table
	{
	public:
		table(std::shared_ptr<database> db, const std::string& name, const int vector_dimension, const int HNWS_max_connect = 32)
			: m_db(db->get()),
			m_name(name)
		{
			sqlite::transaction ts(db->get(), sqlite::transaction_level::EXCLUSIVE);

			init(ts, db, name, vector_dimension, HNWS_max_connect);

			try_create_table(ts);

			load_faiss_index();
			ts.commit();

			init_stmt();
		}
		~table()
		{
			save_faiss_index();
		}
		void save_faiss_index()
		{
			if (!m_faiss_index)
				return;
			sqlite::stmt update_faiss_new_id{ m_db, R"(UPDATE __TABLE_MANAGE__ SET faiss_new_id = ? WHERE tablename = ?;)" };
			update_faiss_new_id.bind(1, m_faiss_index_new_id);
			update_faiss_new_id.bind(2, m_name);
			update_faiss_new_id.step();
			if (!fs::exists(m_faiss_fullpath.parent_path())) // write_index 不会自动创建目录
				fs::create_directories(m_faiss_fullpath.parent_path());
			faiss::write_index(m_faiss_index.get(), m_faiss_fullpath.string().c_str());
		}
		void set_vector(std::function<std::vector<float>(std::string)> func)
		{
			m_generate_vector_callback = func;
		}
		void set_vectors(std::function < std::vector<float>(std::vector<std::string>)> func)
		{
			m_generate_vectors_callback = func;
		}
		void set_hnsw_efSearch(const int efSearch)
		{
			m_faiss_index->hnsw.efSearch = efSearch;
		}
		void add(const insert_data& data)
		{
			auto vector = m_generate_vector_callback(data.message);
			m_faiss_index->add(1, vector.data());

			m_insert_fts_data.reset();
			m_insert_main_data.reset();

			m_insert_main_data.bind(6, m_faiss_index_new_id++);
			m_insert_main_data.bind(1, data.time);
			if (data.sender.empty()) { m_insert_main_data.bind(2, ""); }
			else { m_insert_main_data.bind(2, data.sender); }
			m_insert_main_data.bind(3, data.sender_uuid);
			m_insert_main_data.bind(4, data.message);
			m_insert_main_data.bind(5, data.forget_probability);
			sqlite::transaction ts{ m_db };
			m_insert_main_data.step();
			m_insert_fts_data.bind(1, m_db->last_insert_rowid());
			m_insert_fts_data.bind(2, data.message);
			m_insert_fts_data.step();
			ts.commit();
		}
		void adds(const std::vector<insert_data>& datas)
		{
			auto vector = insert_data_generate_vectors(datas);
			m_faiss_index->add(datas.size(), vector.data());

			m_insert_fts_data.reset();
			m_insert_main_data.reset();

			sqlite::transaction ts{ m_db };
			for (const auto& i : datas)
			{
				m_insert_fts_data.reset();
				m_insert_main_data.reset();
				m_insert_main_data.bind(6, m_faiss_index_new_id++);
				m_insert_main_data.bind(1, i.time);
				if (i.sender.empty()) { m_insert_main_data.bind(2, ""); }
				else { m_insert_main_data.bind(2, i.sender); }
				m_insert_main_data.bind(3, i.sender_uuid);
				m_insert_main_data.bind(4, i.message);
				m_insert_main_data.bind(5, i.forget_probability);
				m_insert_main_data.step();
				m_insert_fts_data.bind(1, m_db->last_insert_rowid());
				m_insert_fts_data.bind(2, i.message);
				m_insert_fts_data.step();
			}
			ts.commit();
		}
		select_data search_id(const std::size_t id)
		{
			ckeck_id(id);

			m_select_main_data_id.reset();
			m_select_main_data_id.bind(1, id);
			m_select_main_data_id.step();
			return { m_select_main_data_id.get_column_uint64(0),
				m_select_main_data_id.get_column_uint64(1),
				m_select_main_data_id.get_column_str(2),
				m_select_main_data_id.get_column_str(3) ,
				m_select_main_data_id.get_column_str(4) };
		}
		std::vector<select_data> search_list_uuid(std::string_view uuid)
		{
			sqlite::transaction ts(m_db);

			m_select_main_sender_uuid.reset();
			m_select_main_sender_uuid.bind(1, uuid);
			std::vector<select_data> res;
			while (m_select_main_sender_uuid.step())
			{
				res.emplace_back(m_select_main_sender_uuid.get_column_uint64(0),
					m_select_main_sender_uuid.get_column_uint64(1),
					m_select_main_sender_uuid.get_column_str(2),
					m_select_main_sender_uuid.get_column_str(3),
					m_select_main_sender_uuid.get_column_str(4)
				);
			}

			ts.commit();

			return res;
		}
		std::vector<select_data> search_list_uuid_limit(std::string_view uuid, const std::size_t limit)
		{
			sqlite::transaction ts(m_db);

			m_select_main_sender_uuid_limit.reset();
			m_select_main_sender_uuid_limit.bind(1, uuid);
			m_select_main_sender_uuid_limit.bind(2, limit);
			std::vector<select_data> res;
			while (m_select_main_sender_uuid_limit.step())
			{
				res.emplace_back(m_select_main_sender_uuid_limit.get_column_uint64(0),
					m_select_main_sender_uuid_limit.get_column_uint64(1),
					m_select_main_sender_uuid_limit.get_column_str(2),
					m_select_main_sender_uuid_limit.get_column_str(3),
					m_select_main_sender_uuid_limit.get_column_str(4)
				);
			}

			ts.commit();

			return res;
		}
		std::vector<select_data> search_list_time_start(const std::size_t start)
		{
			sqlite::transaction ts(m_db);

			m_select_main_data_time_start.reset();
			m_select_main_data_time_start.bind(1, start);
			std::vector<select_data> res;
			while (m_select_main_data_time_start.step())
			{
				res.emplace_back(m_select_main_data_time_start.get_column_uint64(0),
					m_select_main_data_time_start.get_column_uint64(1),
					m_select_main_data_time_start.get_column_str(2),
					m_select_main_data_time_start.get_column_str(3),
					m_select_main_data_time_start.get_column_str(4)
				);
			}

			ts.commit();

			return res;
		}
		std::vector<select_data> search_list_time_end(const std::size_t end)
		{
			sqlite::transaction ts(m_db);

			m_select_main_data_time_end.reset();
			m_select_main_data_time_end.bind(1, end);
			std::vector<select_data> res;
			while (m_select_main_data_time_end.step())
			{
				res.emplace_back(m_select_main_data_time_end.get_column_uint64(0),
					m_select_main_data_time_end.get_column_uint64(1),
					m_select_main_data_time_end.get_column_str(2),
					m_select_main_data_time_end.get_column_str(3),
					m_select_main_data_time_end.get_column_str(4)
				);
			}

			ts.commit();

			return res;
		}
		std::vector<select_data> search_list_time_start_end(const std::size_t start, const std::size_t end)
		{
			sqlite::transaction ts(m_db);

			m_select_main_data_time_start_end.reset();
			m_select_main_data_time_start_end.bind(1, start);
			m_select_main_data_time_start_end.bind(2, end);
			std::vector<select_data> res;
			while (m_select_main_data_time_start_end.step())
			{
				res.emplace_back(m_select_main_data_time_start_end.get_column_uint64(0),
					m_select_main_data_time_start_end.get_column_uint64(1),
					m_select_main_data_time_start_end.get_column_str(2),
					m_select_main_data_time_start_end.get_column_str(3),
					m_select_main_data_time_start_end.get_column_str(4)
				);
			}

			ts.commit();

			return res;
		}

		std::vector<select_fts_data> search_list_fts(std::string_view fts)
		{
			sqlite::transaction ts(m_db);

			std::vector<select_fts_data> res;
			m_select_fts_message.reset();
			m_select_fts_message.bind(1, fts, SQLITE_STATIC);
			while (m_select_fts_message.step() == SQLITE_ROW)
			{
				m_select_main_data_id_no_message.reset();
				m_select_main_data_id_no_message.bind(1, m_select_fts_message.get_column_uint64(0));
				m_select_main_data_id_no_message.step();
				res.emplace_back(
					m_select_main_data_id_no_message.get_column_uint64(0),
					m_select_main_data_id_no_message.get_column_uint64(1),
					m_select_main_data_id_no_message.get_column_str(2),
					m_select_main_data_id_no_message.get_column_str(3),
					m_select_fts_message.get_column_str(1)
				);
			}

			ts.commit();

			return res;
		}
		std::vector<select_fts_data> search_list_fts_limit(std::string_view fts, const std::size_t limit)
		{
			sqlite::transaction ts(m_db);

			std::vector<select_fts_data> res;
			m_select_fts_message_limit.reset();
			m_select_fts_message_limit.bind(1, fts, SQLITE_STATIC);
			m_select_fts_message_limit.bind(2, limit);
			while (m_select_fts_message_limit.step() == SQLITE_ROW)
			{
				m_select_main_data_id_no_message.reset();
				m_select_main_data_id_no_message.bind(1, m_select_fts_message_limit.get_column_uint64(0));
				m_select_main_data_id_no_message.step();
				res.emplace_back(
					m_select_main_data_id_no_message.get_column_uint64(0),
					m_select_main_data_id_no_message.get_column_uint64(1),
					m_select_main_data_id_no_message.get_column_str(2),
					m_select_main_data_id_no_message.get_column_str(3),
					m_select_fts_message_limit.get_column_str(1)
				);
			}

			ts.commit();

			return res;
		}

		std::vector<select_fts_data> search_list_highlight_fts(std::string_view fts, std::string_view start, std::string_view end)
		{
			sqlite::transaction ts(m_db);

			std::vector<select_fts_data> res;
			m_select_fts_highlight_message.reset();
			m_select_fts_highlight_message.bind(3, fts, SQLITE_STATIC);
			m_select_fts_highlight_message.bind(1, start, SQLITE_STATIC);
			m_select_fts_highlight_message.bind(2, end, SQLITE_STATIC);
			while (m_select_fts_highlight_message.step() == SQLITE_ROW)
			{
				m_select_main_data_id_no_message.reset();
				m_select_main_data_id_no_message.bind(1, m_select_fts_highlight_message.get_column_uint64(0));
				m_select_main_data_id_no_message.step();
				res.emplace_back(
					m_select_main_data_id_no_message.get_column_uint64(0),
					m_select_main_data_id_no_message.get_column_uint64(1),
					m_select_main_data_id_no_message.get_column_str(2),
					m_select_main_data_id_no_message.get_column_str(3),
					m_select_fts_highlight_message.get_column_str(1)
				);
			}

			ts.commit();

			return res;
		}
		std::vector<select_fts_data> search_list_highlight_fts_limit(std::string_view fts, std::string_view start, std::string_view end, const std::size_t limit)
		{
			sqlite::transaction ts(m_db);

			std::vector<select_fts_data> res;
			m_select_fts_highlight_message_limit.reset();
			m_select_fts_highlight_message_limit.bind(3, fts, SQLITE_STATIC);
			m_select_fts_highlight_message_limit.bind(1, start, SQLITE_STATIC);
			m_select_fts_highlight_message_limit.bind(2, end, SQLITE_STATIC);
			m_select_fts_highlight_message_limit.bind(4, limit);
			while (m_select_fts_highlight_message_limit.step() == SQLITE_ROW)
			{
				m_select_main_data_id_no_message.reset();
				m_select_main_data_id_no_message.bind(1, m_select_fts_highlight_message_limit.get_column_uint64(0));
				m_select_main_data_id_no_message.step();
				res.emplace_back(
					m_select_main_data_id_no_message.get_column_uint64(0),
					m_select_main_data_id_no_message.get_column_uint64(1),
					m_select_main_data_id_no_message.get_column_str(2),
					m_select_main_data_id_no_message.get_column_str(3),
					m_select_fts_highlight_message_limit.get_column_str(1)
				);
			}

			ts.commit();

			return res;
		}

		std::vector<select_vector_data> search_list_vector_text(std::string_view message, const faiss::idx_t k)
		{
			ckeck_k(k);

			constexpr faiss::idx_t limit = 1;
			sqlite::transaction ts(m_db);

			auto vector = m_generate_vector_callback(message.data());
			std::vector<faiss::idx_t> indices(k * limit); // 索引结果
			std::vector<float> distances(k * limit);        // 距离结果
			m_faiss_index->search(limit, vector.data(), k, distances.data(), indices.data());

			std::vector<select_vector_data> res;
			res.reserve(indices.size());
			for (std::size_t i = 0; i < indices.size(); i++)
			{
				if (indices[i] < 0)
				{
					continue;
				}
				m_select_main_faiss_index_id.reset();
				m_select_main_faiss_index_id.bind(1, indices[i]);
				if (m_select_main_faiss_index_id.step() != SQLITE_ROW)
				{
					continue;
				}
				res.emplace_back(
					m_select_main_faiss_index_id.get_column_uint64(0),
					m_select_main_faiss_index_id.get_column_uint64(1),
					m_select_main_faiss_index_id.get_column_str(2),
					m_select_main_faiss_index_id.get_column_str(3),
					m_select_main_faiss_index_id.get_column_str(4),
					distances[i]
				);

			}

			ts.commit();

			return res;

		}
		std::vector<select_vector_data> search_list_vector_text_limit(const std::vector<std::string>& message, const faiss::idx_t k, const faiss::idx_t limit)
		{
			check_limit(limit);
			ckeck_k(k);

			sqlite::transaction ts(m_db);

			auto vector = string_generate_vectors(message);
			std::vector<faiss::idx_t> indices(k * limit); // 索引结果
			std::vector<float> distances(k * limit);        // 距离结果
			m_faiss_index->search(limit, vector.data(), k, distances.data(), indices.data());

			std::vector<select_vector_data> res;
			res.reserve(indices.size());
			for (std::size_t i = 0; i < indices.size(); i++)
			{
				if (indices[i] < 0)
				{
					continue;
				}
				m_select_main_faiss_index_id.reset();
				m_select_main_faiss_index_id.bind(1, indices[i]);
				if (m_select_main_faiss_index_id.step() != SQLITE_ROW)
				{
					continue;
				}
				res.emplace_back(
					m_select_main_faiss_index_id.get_column_uint64(0),
					m_select_main_faiss_index_id.get_column_uint64(1),
					m_select_main_faiss_index_id.get_column_str(2),
					m_select_main_faiss_index_id.get_column_str(3),
					m_select_main_faiss_index_id.get_column_str(4),
					distances[i]
				);

			}

			ts.commit();

			return res;
		}

		void forgotten()
		{
			std::vector<std::size_t> ids;
			std::random_device rd;
			std::mt19937 generator(rd());
			std::uniform_real_distribution<double> distribution(0.0, 1.0);

			sqlite::transaction ts(m_db, sqlite::IMMEDIATE);

			m_select_main_id_forget_probability.reset();

			constexpr double k_almost_one = 1.0 - std::numeric_limits<double>::epsilon();
			constexpr double k_almost_zero = std::numeric_limits<double>::epsilon();
			while (m_select_main_id_forget_probability.step() == SQLITE_ROW)
			{
				auto main_id = m_select_main_id_forget_probability.get_column_uint64(0);
				auto forget_probability = m_select_main_id_forget_probability.get_column_double(1);
				if (forget_probability <= k_almost_zero)
				{
					continue;
				}
				if (forget_probability >= k_almost_one
					|| distribution(generator) < forget_probability
					)
				{
					ids.emplace_back(main_id);
					continue;
				}
			}
			for (const auto& i : ids)
			{
				m_del_main_id.reset();
				m_del_main_id.bind(1, i);
				m_del_main_id.step();
				m_del_fts_id.reset();
				m_del_fts_id.bind(1, i);
				m_del_fts_id.step();
			}
			m_select_main_count.reset();
			m_select_main_count.step();

			auto faiss_index_size = m_select_main_count.get_column_uint64(0);
			std::vector<float> vec(faiss_index_size * m_vector_dimension);
			auto vec_begin = vec.begin();
			ids.clear();
			m_select_main_id_faiss_index.reset();
			while (m_select_main_id_faiss_index.step() == SQLITE_ROW)
			{
				auto main_id = m_select_main_id_faiss_index.get_column_uint64(0);
				auto faiss_id = m_select_main_id_faiss_index.get_column_uint64(1);
				m_faiss_index->reconstruct(faiss_id, vec_begin._Ptr);
				vec_begin += m_vector_dimension;
				ids.emplace_back(main_id);
			}
			auto  new_faiss_index = std::make_shared<f::faiss_index>(m_vector_dimension, m_HNSW_max_connect);
			new_faiss_index->add(faiss_index_size, vec.data());
			m_faiss_index_new_id = 0;
			for (const auto& i : ids)
			{
				m_update_main_id_to_faiss_index.reset();
				m_update_main_id_to_faiss_index.bind(2, i);
				m_update_main_id_to_faiss_index.bind(1, m_faiss_index_new_id++);
				m_update_main_id_to_faiss_index.step();
			}
			m_faiss_index = new_faiss_index;
			ts.commit();
		}

		void rebuild_faiss_index()
		{
			std::vector<std::size_t> ids;
			sqlite::transaction ts(m_db, sqlite::IMMEDIATE);

			m_select_main_count.reset();
			m_select_main_count.step();

			auto faiss_index_size = m_select_main_count.get_column_uint64(0);
			std::vector<float> vec(faiss_index_size * m_vector_dimension);
			auto vec_begin = vec.begin();
			ids.clear();
			m_select_main_id_faiss_index.reset();
			while (m_select_main_id_faiss_index.step() == SQLITE_ROW)
			{
				auto main_id = m_select_main_id_faiss_index.get_column_uint64(0);
				auto faiss_id = m_select_main_id_faiss_index.get_column_uint64(1);
				m_faiss_index->reconstruct(faiss_id, vec_begin._Ptr);
				vec_begin += m_vector_dimension;
				ids.emplace_back(main_id);
			}
			auto  new_faiss_index = std::make_shared<f::faiss_index>(m_vector_dimension, m_HNSW_max_connect);
			new_faiss_index->add(faiss_index_size, vec.data());
			m_faiss_index_new_id = 0;
			for (const auto& i : ids)
			{
				m_update_main_id_to_faiss_index.reset();
				m_update_main_id_to_faiss_index.bind(2, i);
				m_update_main_id_to_faiss_index.bind(1, m_faiss_index_new_id++);
				m_update_main_id_to_faiss_index.step();
			}
			m_faiss_index = new_faiss_index;
			ts.commit();
		}
		void full_rebuild_faiss_index()
		{
			sqlite::transaction ts(m_db, sqlite::IMMEDIATE);

			m_select_main_count.reset();
			m_select_main_count.step();
			auto faiss_index_size = m_select_main_count.get_column_uint64(0);
			std::vector<std::size_t> ids;
			std::vector<std::string> messages;
			m_select_main_id_message.reset();
			while (m_select_main_id_message.step() == SQLITE_ROW)
			{
				ids.emplace_back(m_select_main_id_message.get_column_uint64(0));
				messages.emplace_back(m_select_main_id_message.get_column_str(1));
			}
			auto vec = string_generate_vectors(messages);
			auto new_faiss_index = std::make_shared<f::faiss_index>(m_vector_dimension, m_HNSW_max_connect);
			new_faiss_index->add(faiss_index_size, vec.data());
			m_faiss_index_new_id = 0;
			for (const auto& i : ids)
			{
				m_update_main_id_to_faiss_index.reset();
				m_update_main_id_to_faiss_index.bind(1, m_faiss_index_new_id++);
				m_update_main_id_to_faiss_index.bind(2, i);
				m_update_main_id_to_faiss_index.step();
			}
			m_faiss_index = new_faiss_index;
			ts.commit();
		}

		void drop()
		{
			m_insert_main_data.close();
			m_insert_fts_data.close();

			m_select_main_data_id.close();
			m_select_main_data_id_no_message.close();
			m_select_main_faiss_index_id.close();

			m_select_main_sender_uuid.close();
			m_select_main_sender_uuid_limit.close();

			m_select_main_data_time_start.close();
			m_select_main_data_time_end.close();
			m_select_main_data_time_start_end.close();

			m_select_fts_message.close();
			m_select_fts_message_limit.close();

			m_select_fts_highlight_message.close();
			m_select_fts_highlight_message_limit.close();

			m_select_main_count.close();

			m_select_main_id_forget_probability.close();
			m_select_main_id_faiss_index.close();
			m_select_main_id_message.close();

			m_update_main_id_to_faiss_index.close();

			m_del_main_id.close();
			m_del_fts_id.close();

			sqlite::transaction ts(m_db, sqlite::EXCLUSIVE);
			m_db->execute(std::format("DROP TABLE IF EXISTS {}", m_name));
			m_db->execute(std::format("DROP TABLE IF EXISTS {}_fts", m_name));
			sqlite::stmt delete_table(m_db, "DELETE FROM __TABLE_MANAGE__ WHERE tablename = ?");
			delete_table.bind(1, m_name);
			delete_table.step();
			m_faiss_index.reset();
			ts.commit();
		}
	private:
		std::string m_name;
		std::shared_ptr<sqlite::database> m_db;

		int m_HNSW_max_connect;
		int m_vector_dimension;

		std::size_t m_faiss_index_new_id = 0;
		std::shared_ptr<f::faiss_index> m_faiss_index;
		fs::path m_faiss_fullpath;

		py::function<std::vector<float>(std::string)> m_generate_vector_callback;
		py::function<std::vector<float>(std::vector<std::string>)> m_generate_vectors_callback;

		sqlite::stmt m_insert_main_data;
		sqlite::stmt m_insert_fts_data;

		sqlite::stmt m_select_main_data_id;
		sqlite::stmt m_select_main_data_id_no_message;
		sqlite::stmt m_select_main_faiss_index_id;

		sqlite::stmt m_select_main_sender_uuid;
		sqlite::stmt m_select_main_sender_uuid_limit;

		sqlite::stmt m_select_main_data_time_start;
		sqlite::stmt m_select_main_data_time_end;
		sqlite::stmt m_select_main_data_time_start_end;

		sqlite::stmt m_select_fts_message;
		sqlite::stmt m_select_fts_message_limit;

		sqlite::stmt m_select_fts_highlight_message;
		sqlite::stmt m_select_fts_highlight_message_limit;

		sqlite::stmt m_select_main_count;

		sqlite::stmt m_select_main_id_forget_probability;
		sqlite::stmt m_select_main_id_faiss_index;
		sqlite::stmt m_select_main_id_message;

		sqlite::stmt m_update_main_id_to_faiss_index;

		sqlite::stmt m_del_main_id;
		sqlite::stmt m_del_fts_id;

		std::vector<float> insert_data_generate_vectors(const std::vector<insert_data>& datas)
		{
			if (this->m_generate_vectors_callback)
			{
				return m_generate_vectors_callback(
					datas |
					std::views::transform([](const insert_data& data) { return data.message; }) |
					std::ranges::to<std::vector<std::string>>()
				);
			}
			else
			{
				std::vector<float> vector;
				vector.reserve(datas.size());
				for (const auto i : datas | std::views::transform([](const insert_data& data) { return data.message; }))
				{
					for (const auto& i : this->m_generate_vector_callback(i))
					{
						vector.emplace_back(i);
					}
				}
				return vector;
			}
		}
		std::vector<float> string_generate_vectors(const std::vector<std::string>& datas)
		{
			if (this->m_generate_vectors_callback)
			{
				return m_generate_vectors_callback(
					datas
				);
			}
			else
			{
				std::vector<float> vector;
				vector.reserve(datas.size());
				for (const auto i : datas)
				{
					for (const auto& i : this->m_generate_vector_callback(i))
					{
						vector.emplace_back(i);
					}
				}
				return vector;
			}
		}
		void try_create_table(sqlite::transaction& ts)
		{
			ts.execute(std::format(R"(
				CREATE TABLE IF NOT EXISTS {} (
				id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
				timestamp INTEGER NOT NULL,
				sender TEXT,
				sender_uuid TEXT NOT NULL,
				message TEXT NOT NULL,
				forget_probability REAL NOT NULL DEFAULT 0.0 CHECK (forget_probability >= 0.0 AND forget_probability <= 1.0),
				faiss_index_id INTEGER NOT NULL);
			)", m_name));
			ts.execute(std::format(R"(
				CREATE VIRTUAL TABLE IF NOT EXISTS {}_fts USING fts5(message, tokenize = 'simple');
			)", m_name));
		}
		void load_faiss_index()
		{
			if (fs::exists(m_faiss_fullpath))
			{
				auto faiss_index = faiss::read_index(m_faiss_fullpath.string().c_str());
				std::shared_ptr<f::faiss_index> index_HNSW(dynamic_cast<f::faiss_index*>(faiss_index));
				if (index_HNSW == nullptr)
					throw exception::runtime_error();
				m_faiss_index = index_HNSW;
			}
			else
			{
				m_faiss_index = std::make_shared<f::faiss_index>(m_vector_dimension, m_HNSW_max_connect);
				m_faiss_index_new_id = 0;
			}
		}
		void init(sqlite::transaction& ts, std::shared_ptr<memory::database>& db, const std::string& name, const int vector_dimension, const int HNWS_max_connect)
		{
			sqlite::stmt where_table{ ts, R"(
				SELECT COUNT(*) FROM __TABLE_MANAGE__ WHERE tablename = ?;
			)" };
			where_table.bind(1, name);
			where_table.step();
			if (where_table.get_column_int(0))
			{
				sqlite::stmt get_table_info{ ts, R"(
					SELECT vector_dimension, faiss_fullpath, HNWS_max_connect, faiss_new_id FROM __TABLE_MANAGE__ WHERE tablename = ?;
				)" };
				get_table_info.bind(1, name);
				get_table_info.step();
				m_faiss_index_new_id = get_table_info.get_column_uint64(3);
				m_HNSW_max_connect = get_table_info.get_column_int(2);
				m_faiss_fullpath = (const char*)(get_table_info.get_column_str(1));
				m_vector_dimension = get_table_info.get_column_int(0);
			}
			else
			{
				m_faiss_fullpath = db->db_file_path().parent_path() / db->db_file_path().stem() / std::format("{}.faiss", m_name);
				m_vector_dimension = vector_dimension;
				m_HNSW_max_connect = HNWS_max_connect;
				sqlite::stmt insert_table_info{ ts, R"(
					INSERT INTO __TABLE_MANAGE__ (tablename, vector_dimension, faiss_fullpath, HNWS_max_connect, faiss_new_id) VALUES (?, ?, ?, ?, ?);
				)" };
				insert_table_info.bind(1, m_name);
				insert_table_info.bind(2, m_vector_dimension);
				insert_table_info.bind(3, m_faiss_fullpath.string().c_str());
				insert_table_info.bind(4, m_HNSW_max_connect);
				insert_table_info.bind(5, m_faiss_index_new_id);
				insert_table_info.step();
			}
		}
		void init_stmt()
		{
			m_insert_main_data = sqlite::stmt(m_db, std::format(R"(INSERT INTO {} 
			(timestamp, sender, sender_uuid, message, forget_probability, faiss_index_id) 
			VALUES (?, ?, ?, ?, ?, ?);)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);
			m_insert_fts_data = sqlite::stmt(m_db, std::format(R"(INSERT INTO {}_fts (rowid, message) VALUES (?, ?);)", m_name), SQLITE_PREPARE_PERSISTENT);

			m_select_main_data_id = sqlite::stmt(m_db, std::format(R"(SELECT id, timestamp, sender, sender_uuid, message FROM {} WHERE id = ?;)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);
			m_select_main_faiss_index_id = sqlite::stmt(m_db, std::format(R"(SELECT id, timestamp, sender, sender_uuid, message FROM {} WHERE faiss_index_id = ?;)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);

			m_select_main_sender_uuid = sqlite::stmt(m_db, std::format(R"(SELECT id, timestamp, sender, sender_uuid, message FROM {} WHERE sender_uuid = ? ORDER BY id DESC;)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);
			m_select_main_sender_uuid_limit = sqlite::stmt(m_db, std::format(R"(SELECT id, timestamp, sender, sender_uuid, message FROM {} WHERE sender_uuid = ? ORDER BY id DESC LIMIT ?;)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);

			m_select_main_data_id_no_message = sqlite::stmt(m_db, std::format(R"(SELECT id, timestamp, sender, sender_uuid FROM {} WHERE id = ?;)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);

			m_select_main_data_time_start = sqlite::stmt(m_db, std::format(R"(SELECT id, timestamp, sender, sender_uuid, message FROM {} WHERE timestamp >= ? ORDER BY timestamp DESC;)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);
			m_select_main_data_time_end = sqlite::stmt(m_db, std::format(R"(SELECT id, timestamp, sender, sender_uuid, message FROM {} WHERE timestamp <= ? ORDER BY timestamp DESC;)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);
			m_select_main_data_time_start_end = sqlite::stmt(m_db, std::format(R"(SELECT id, timestamp, sender, sender_uuid, message FROM {} WHERE timestamp >= ? AND timestamp <= ? ORDER BY timestamp DESC;)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);

			m_select_fts_message = sqlite::stmt(m_db, std::format(R"(SELECT rowid, message FROM {}_fts WHERE message MATCH ? ORDER BY rowid DESC;)", m_name), SQLITE_PREPARE_PERSISTENT);
			m_select_fts_message_limit = sqlite::stmt(m_db, std::format(R"(SELECT rowid, message FROM {}_fts WHERE message MATCH ? ORDER BY rowid DESC LIMIT ?;)", m_name), SQLITE_PREPARE_PERSISTENT);

			m_select_fts_highlight_message = sqlite::stmt(m_db, std::format(R"(SELECT rowid, simple_highlight({}_fts, 0 , ?, ?) AS text FROM {}_fts WHERE message MATCH ? ORDER BY rowid DESC;)", m_name, m_name), SQLITE_PREPARE_PERSISTENT);
			m_select_fts_highlight_message_limit = sqlite::stmt(m_db, std::format(R"(SELECT rowid, simple_highlight({}_fts, 0, ?, ?) AS text FROM {}_fts WHERE message MATCH ? ORDER BY rowid DESC;)", m_name, m_name), SQLITE_PREPARE_PERSISTENT);

			m_select_main_count = sqlite::stmt(m_db, std::format(R"(SELECT count(*) FROM {};)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);

			m_select_main_id_forget_probability = sqlite::stmt(m_db, std::format(R"(SELECT id, forget_probability FROM {};)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);
			m_select_main_id_faiss_index = sqlite::stmt(m_db, std::format(R"(SELECT id, faiss_index_id FROM {};)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);
			m_select_main_id_message = sqlite::stmt(m_db, std::format(R"(SELECT id, message FROM {};)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);

			m_update_main_id_to_faiss_index = sqlite::stmt(m_db, std::format(R"(UPDATE {} SET faiss_index_id = ? WHERE id = ?;)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);

			m_del_main_id = sqlite::stmt(m_db, std::format(R"(DELETE FROM {} WHERE id = ?;)", m_name), SQLITE_PREPARE_NO_VTAB | SQLITE_PREPARE_PERSISTENT);
			m_del_fts_id = sqlite::stmt(m_db, std::format(R"(DELETE FROM {}_fts WHERE rowid = ?;)", m_name), SQLITE_PREPARE_PERSISTENT);
		}
	};
}