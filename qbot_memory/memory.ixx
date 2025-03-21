export module memory;

import :faiss;
import :sqlite;
import :thread_pool;
import std;

import <sqlite3.h>;

namespace fs = std::filesystem;

export namespace memory
{
	class database
	{
	public:
		database(const std::string& db_file_path, const std::string& simple_path, const std::string& simple_dict_path) : m_db{ new sqlite::database(db_file_path) }, m_db_parent_path{ fs::path{db_file_path}.parent_path() }
		{
			m_db->load_extension(simple_path);
			sqlite::stmt jieba_dict_sql{ m_db, "SELECT jieba_dict(?);"};
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
	private :
		fs::path m_db_parent_path;
		std::shared_ptr<sqlite::database> m_db;
	};
}