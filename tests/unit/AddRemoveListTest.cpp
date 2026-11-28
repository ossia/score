#include <score/widgets/AddRemoveList.hpp>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

TEST_CASE("A channel list reports a change of its count both ways", "[widgets][audio]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::AddRemoveList list{"out_", {"left", "right", "sub", "rear"}, nullptr};
    std::vector<QStringList> reported;
    QObject::connect(&list, &score::AddRemoveList::changed, &list, [&] {
      reported.push_back(list.content());
    });

    list.setCount(2);
    REQUIRE(!reported.empty());
    CHECK(reported.back() == QStringList{"left", "right"});

    reported.clear();
    list.setCount(3);
    REQUIRE(!reported.empty());
    CHECK(reported.back() == QStringList{"left", "right", "out_2"});

    reported.clear();
    list.setCount(3);
    CHECK(reported.empty());
  });
}
