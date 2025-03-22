#include "faiss.hpp"
#include "memory.h"
#include "sqlite.hpp"
#include <filesystem>
#include <memory>
#include <new>
#include <string>

namespace fs = std::filesystem;

namespace memory
{
	class database
	{
	public:
		database(const std::string& db_file_path, const std::string& simple_path, const std::string& simple_dict_path) : m_db{ new sqlite::database(db_file_path) }
		{
			m_db->load_extension(simple_path);
			sqlite::stmt jieba_dict_sql{ m_db, "SELECT jieba_dict(?);" };
			jieba_dict_sql.bind(1, simple_dict_path, SQLITE_STATIC);
			jieba_dict_sql.step();

			m_db->execute(R"(
				CREATE TABLE IF NOT EXISTS __TABLE_MANAGE__ (
				 id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
				 tablename TEXT NOT NULL,
				 vector_dimension INTEGER NOT NULL,
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
		table(std::shared_ptr<database> db, const std::string& name, const std::size_t vector_dimension) : m_db(db), m_name(name)
		{
			init(db, name, vector_dimension);

			//faiss::indexHNSW
			
		}
		~table() = default;
	private:
		std::string m_name;
		std::shared_ptr<database> m_db;
		std::size_t m_vector_dimension;
		fs::path m_faiss_fullpath;

		void init(std::shared_ptr<memory::database>& db, const std::string& name, const size_t vector_dimension)
		{
			auto ts = std::make_shared<sqlite::transaction>(db->get(), sqlite::transaction_level::EXCLUSIVE);
			static sqlite::stmt where_table{ ts, R"(
				SELECT COUNT(*)
				FROM __TABLE_MANAGE__
				WHERE tablename = ?;
			)",SQLITE_PREPARE_PERSISTENT };
			where_table.bind(1, name);
			where_table.step();
			if (where_table.get_column_int(0))
			{
				static sqlite::stmt get_table_info{ ts, R"(
					SELECT vector_dimension, faiss_fullpath
					FROM __TABLE_MANAGE__
					WHERE tablename = ?;
				)",SQLITE_PREPARE_PERSISTENT };
				get_table_info.bind(1, name);
				get_table_info.step();
				m_faiss_fullpath = (const char*)(get_table_info.get_column_str(1));
				m_vector_dimension = get_table_info.get_column_int64(0);
				get_table_info.reset();
			}
			else
			{
				m_faiss_fullpath = db->db_file_path().parent_path() / m_name;
				m_vector_dimension = vector_dimension;
				static sqlite::stmt insert_table_info{ ts, R"(
					INSERT INFO __TABLE_MANAGE__ (tablename, vector_dimension, faiss_fullpath) VALUES (?, ?, ?);
				)",SQLITE_PREPARE_PERSISTENT };
				insert_table_info.bind(1, m_name);
				insert_table_info.bind(2, m_vector_dimension);
				insert_table_info.bind(3, m_faiss_fullpath.c_str());
				insert_table_info.step();
				insert_table_info.reset();
			}
			ts->commit();
			where_table.reset();
		}
	};
}