#include "memory.hpp"

#include "exception.hpp"
#include <cpr/api.h>
#include <cpr/body.h>
#include <cpr/cprtypes.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <print>
#include <string>
#include <vector>

using json = nlohmann::json;

std::vector<float> text_to_vec(const std::string& str)
{
	json body_json{
		{"model", "nomic-embed-text"},
		{"prompt", str}
	};
	auto res = cpr::Post(
		cpr::Url{ "http://127.0.0.1:11434/api/embeddings" },
		cpr::Header{ {"Content-Type", "application/json"} },
		cpr::Body{ body_json.dump() }
	);
	if (res.status_code != 200)
		throw memory::exception::runtime_error();
	json res_json = json::parse(res.text);
	if (!res_json["embedding"].is_array())
		throw memory::exception::runtime_error();
	std::vector<float> embeddings;
	for (const auto& i : res_json["embedding"])
	{
		if (i.is_number_float())
			embeddings.emplace_back(i.get<float>());
	}
	return embeddings;
}

std::vector<float> texts_to_vec(const std::vector<std::string>& vec)
{
	return {};
}

int main()
{
	try
	{
		auto db = std::make_shared<memory::database>(R"(C:\game\123\test\02\__test__.db)", R"(C:/game/source/SQLite插件/simple.dll)", R"(C:/game/source/SQLite插件/dict)");
		try
		{
			memory::table test1{ db, "test1", 768 };
			test1.set_hnsw_efSearch(64);
			test1.set_vector(text_to_vec);
			//test1.set_vectors(texts_to_vec);
			test1.add(memory::insert_data{ 1000, "幻日", "幻日", "幻蓝你好！", 0.11 });
			test1.add(memory::insert_data{ 1023, "幻蓝", "幻蓝", "啊！是老爹啊！", 0.9 });
			test1.add(memory::insert_data{ 1000, "幻蓝", "幻蓝", "老爹好！", 0.4 });
			auto i = test1.search_list_vector_text("你好", 2);
			if (!i.empty())
				std::println("sender_uuid:{} msg:{} distance:{}", i[0].sender_uuid, i[0].message, i[0].distance);
			auto i2 = test1.search_list_vector_text("老爹", 1);
			if (!i2.empty())
				std::println("sender_uuid:{} msg:{} distance:{}", i2[0].sender_uuid, i2[0].message, i2[0].distance);
			auto i4 = test1.search_list_vector_text("幻蓝", 1);
			if (!i4.empty())
				std::println("sender_uuid:{} msg:{} distance:{}", i4[0].sender_uuid, i4[0].message, i2[0].distance);
			auto i3 = test1.search_list_fts_impl("幻蓝", {}, {}, {}, {}, {});
			std::println("sender_uuid:{} msg:{}", i3[0].sender_uuid, i3[0].message);
			auto i5 = test1.search_list_fts_impl("幻蓝", {}, {}, "[", "]", {});
			std::println("sender_uuid:{} msg:{}", i5[0].sender_uuid, i5[0].message);
			//auto i6 = test1.search_list_vector_text("幻蓝", -1);
			test1.full_rebuild_faiss_index();
			test1.forgotten();
			test1.save_faiss_index();
			test1.drop();
		}
		catch (const memory::exception::base_exception& e)
		{
			std::println("{}", e.what());
		}
	}
	catch (const memory::exception::base_exception& e)
	{
		std::println("{}", e.what());
	}
}