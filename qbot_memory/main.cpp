#include "memory.hpp"

#include <memory>

int main()
{
	auto db = std::make_shared<memory::database>(":memory:", R"(C:/game/source/SQLite插件/simple.dll)", R"(C:/game/source/SQLite插件/dict)");
	{
		memory::table test1{ db, "test1", 384 };
	}
	memory::table test1{ db, "test1", 384 };
}