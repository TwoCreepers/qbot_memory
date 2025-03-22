#include "exception.hpp"
#include "faiss.hpp"
#include "sqlite.hpp"
#include <faiss/Index.h>
#include <faiss/index_io.h>
#include <faiss/IndexHNSW.h>
#include <filesystem>
#include <format>
#include <memory>
#include <new>
#include <sqlite3.h>
#include <string>

namespace fs = std::filesystem;

namespace memory
{
	struct insert_data
	{
		std::size_t time;
		std::string sender;
		std::string sender_uuid;
		std::string message;
		double forget_probability;
	};

	class database
	{
	public:
		database(const std::string& db_file_path, const std::string& simple_path, const std::string& simple_dict_path) : m_db{ new sqlite::database(db_file_path) }
		{
			sqlite3_enable_load_extension(m_db->get(), 1);
			m_db->load_extension(simple_path);
			sqlite::stmt jieba_dict_sql{ m_db, "SELECT jieba_dict(?);" };
			jieba_dict_sql.bind(1, simple_dict_path, SQLITE_STATIC);
			jieba_dict_sql.step();

			m_db->execute(R"(
				CREATE TABLE IF NOT EXISTS __TABLE_MANAGE__ (
				id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
				tablename TEXT NOT NULL,
				vector_dimension INTEGER NOT NULL,
				HNWS_max_connect INTEGER NOT NULL,
				faiss_fullpath TEXT NOT NULL
				);
				)");
		}
		~database() = default;
		database(database&&) = delete;

		fs::path db_file_path()
		{
			return m_db_file_path;
		}

		std::shared_ptr<sqlite::database> get()
		{
			return m_db;
		}
	private:
		fs::path m_db_file_path;
		std::shared_ptr<sqlite::database> m_db;
	};

	class table
	{
	public:
		table(std::shared_ptr<database> db, const std::string& name, const int vector_dimension, const int HNWS_max_connect = 32) : m_db(db->get()), m_name(name)
		{
			sqlite::transaction ts(db->get(), sqlite::transaction_level::EXCLUSIVE);

			init(ts, db, name, vector_dimension, HNWS_max_connect);

			try_create_table(ts);

			load_faiss_index();
			ts.commit();
		}
		~table()
		{
			faiss::write_index(m_faiss_index.get(), m_faiss_fullpath.string().c_str());
		}
	private:
		std::string m_name;
		std::shared_ptr<sqlite::database> m_db;

		int m_HNSW_max_connect;
		int m_vector_dimension;

		std::shared_ptr<f::faiss_index> m_faiss_index;
		fs::path m_faiss_fullpath;

		void try_create_table(sqlite::transaction& ts)
		{
			ts.execute(std::format(R"(
				CREATE TABLE IF NOT EXISTS {} (
				id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
				timestamp INTEGER NOT NULL,
				sender TEXT,
				sender_uuid TEXT NOT NULL,
				message TEXT NOT NULL,
				forget_probability REAL NOT NULL,
				faiss_index_id INTEGER NOT NULL);
			)", m_name));
			ts.execute(std::format(R"(
				CREATE VIRTUAL TABLE IF NOT EXISTS {}-fts USING fts5(message, tokenize = 'simple');
			)", m_name));
		}
		void load_faiss_index()
		{
			if (fs::exists(m_faiss_fullpath))
			{
				auto faiss_index = faiss::read_index(m_faiss_fullpath.string().c_str());
				std::shared_ptr<f::faiss_index> index_HNSW(dynamic_cast<f::faiss_index*>(faiss_index));
				if (index_HNSW == nullptr)
					throw exception::runtime_exception();
				m_faiss_index = index_HNSW;
			}
			else
			{
				m_faiss_index = std::make_shared<f::faiss_index>(m_vector_dimension, m_HNSW_max_connect);
			}
		}
		void init(sqlite::transaction& ts, std::shared_ptr<memory::database>& db, const std::string& name, const int vector_dimension, const int HNWS_max_connect)
		{
			sqlite::stmt where_table{ ts, R"(
				SELECT COUNT(*) FROM __TABLE_MANAGE__ WHERE tablename = ?;
			)"};
			where_table.bind(1, name);
			where_table.step();
			if (where_table.get_column_int(0))
			{
				sqlite::stmt get_table_info{ ts, R"(
					SELECT vector_dimension, faiss_fullpath, HNWS_max_connect FROM __TABLE_MANAGE__ WHERE tablename = ?;
				)"};
				get_table_info.bind(1, name);
				get_table_info.step();
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
					INSERT INTO __TABLE_MANAGE__ (tablename, vector_dimension, faiss_fullpath, HNWS_max_connect) VALUES (?, ?, ?, ?);
				)" };
				insert_table_info.bind(1, m_name);
				insert_table_info.bind(2, m_vector_dimension);
				insert_table_info.bind(3, m_faiss_fullpath.string().c_str());
				insert_table_info.bind(4, m_HNSW_max_connect);
				insert_table_info.step();
			}
		}
	};
}