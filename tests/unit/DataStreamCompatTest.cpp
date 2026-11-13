// Fields added to a binary format after others are read only when the data
// has them: data saved before stops at a delimiter, or at its end.

#include <score/serialization/DataStreamVisitor.hpp>

#include <QBuffer>
#include <QByteArray>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

TEST_CASE("What follows a field is told apart from an added field", "[serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QByteArray old_data, new_data, at_end;
    {
      DataStreamReader r{&old_data};
      r.m_stream << int32_t(7);
      r.insertDelimiter();
    }
    {
      DataStreamReader r{&new_data};
      r.m_stream << int32_t(7) << bool(true) << int32_t(2);
      r.insertDelimiter();
    }
    {
      DataStreamReader r{&at_end};
      r.m_stream << int32_t(7);
    }

    {
      DataStreamWriter w{old_data};
      int32_t v{};
      w.m_stream >> v;
      CHECK(w.atDelimiterOrEnd());
      w.checkDelimiter();
    }
    {
      DataStreamWriter w{new_data};
      int32_t v{};
      w.m_stream >> v;
      REQUIRE(!w.atDelimiterOrEnd());
      bool b{};
      int32_t i{};
      w.m_stream >> b >> i;
      CHECK(b);
      CHECK(i == 2);
      CHECK(w.atDelimiterOrEnd());
      w.checkDelimiter();
    }
    {
      DataStreamWriter w{at_end};
      int32_t v{};
      w.m_stream >> v;
      CHECK(w.atDelimiterOrEnd());
    }
  });
}
