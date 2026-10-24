#include <doctest/doctest.h> // for ResultBuilder, CHECK_EQ, TEST_CASE

#include <sqlite_orm/sqlite_orm.h> // for make_storage, make_table, make_column
#include <string>                  // for basic_string, string

namespace {

struct Row
{
    int id;
    std::string name;
};

} // namespace

TEST_CASE("smoke.cc: sqlite_orm in-memory round-trip")
{
    using namespace sqlite_orm;
    auto storage = make_storage(":memory:",
        make_table("rows",
            make_column("id", &Row::id, primary_key()),
            make_column("name", &Row::name)));
    storage.sync_schema();

    storage.replace(Row{ 1, "catalan" });
    CHECK_EQ(storage.get<Row>(1).name, "catalan");
}
