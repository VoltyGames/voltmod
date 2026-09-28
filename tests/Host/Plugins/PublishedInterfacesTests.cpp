#include "Host/Plugins/PublishedInterfaces.hpp"

#include <doctest/doctest.h>
#include <ostream>
#include <string>
#include <vector>

using VoltMod::PublishedInterfaces;

/** Stands in for a plugin: the table only ever compares the pointer. */
static const int First = 1;
static const int Second = 2;

TEST_CASE("Find returns what an owner published, and nothing once it withdraws")
{
    PublishedInterfaces table;
    int implementation = 7;
    table.Publish(&First, "first.api", &implementation);

    CHECK(table.Find("first.api") == &implementation);
    CHECK(table.Find("missing.api") == nullptr);

    table.Unpublish(&First, "first.api");

    CHECK(table.Find("first.api") == nullptr);
}

TEST_CASE("Unpublishing a name another owner holds leaves that entry alone")
{
    PublishedInterfaces table;
    int implementation = 7;
    table.Publish(&First, "first.api", &implementation);

    table.Unpublish(&Second, "first.api");

    CHECK(table.Find("first.api") == &implementation);
}

TEST_CASE("A peer cannot swap out a live pointer, and the owner can refresh it")
{
    PublishedInterfaces table;
    int mine = 7;
    int theirs = 9;
    table.Publish(&First, "first.api", &mine);

    table.Publish(&Second, "first.api", &theirs);
    CHECK(table.Find("first.api") == &mine);

    table.Publish(&First, "first.api", &theirs);
    CHECK(table.Find("first.api") == &theirs);
}

TEST_CASE("Releasing an owner withdraws everything it published and names each one")
{
    PublishedInterfaces table;
    int one = 1;
    int two = 2;
    table.Publish(&First, "first.one", &one);
    table.Publish(&Second, "second.api", &two);
    table.Publish(&First, "first.two", &two);

    CHECK(table.RemoveAll(&First) == std::vector<std::string>{"first.one", "first.two"});
    CHECK(table.Find("second.api") == &two);
}
