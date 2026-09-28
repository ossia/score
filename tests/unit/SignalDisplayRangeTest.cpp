// Signal display: a row spans the values it received, or a fixed range.

#include <Ui/SignalDisplay.hpp>

#include <score_test/App.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Signal display: a fixed range or the values seen", "[ui][signal_display]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    using Ui::SignalDisplay::Node;
    Node node;
    // Off by default: documents made before draw as they did
    CHECK(node.inputs.fixed.value == false);
    CHECK(node.inputs.min.value == 0.f);
    CHECK(node.inputs.max.value == 1.f);

    CHECK(Node::row_range(false, 0.f, 1.f, -3.f, 5.f) == std::pair{-3.f, 5.f});
    CHECK(Node::row_range(true, 0.f, 1.f, -3.f, 5.f) == std::pair{0.f, 1.f});
    // A flat row with a fixed range is still drawn at its value
    CHECK(Node::row_range(true, -1.f, 1.f, 0.5f, 0.5f) == std::pair{-1.f, 1.f});
  });
}
