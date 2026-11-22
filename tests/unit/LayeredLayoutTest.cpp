#include <Scenario/Document/Interval/LayeredLayout.hpp>

#include <QRectF>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>

using Scenario::layeredLayout;
using Scenario::LayoutEdge;

namespace
{
std::vector<QSizeF> boxes(int n, QSizeF s = {100., 40.})
{
  return std::vector<QSizeF>(std::size_t(n), s);
}

int overlaps(const std::vector<QSizeF>& sizes, const std::vector<QPointF>& pos)
{
  int count = 0;
  for(std::size_t i = 0; i < pos.size(); i++)
    for(std::size_t j = i + 1; j < pos.size(); j++)
      if(QRectF{pos[i], sizes[i]}.intersects(QRectF{pos[j], sizes[j]}))
        count++;
  return count;
}

int backwards(
    const std::vector<QSizeF>& sizes, const std::vector<QPointF>& pos,
    const std::vector<LayoutEdge>& edges)
{
  int count = 0;
  for(const auto& e : edges)
    if(pos[e.from].x() + sizes[e.from].width() > pos[e.to].x())
      count++;
  return count;
}
}

TEST_CASE("an empty graph lays out to nothing", "[layout]")
{
  CHECK(layeredLayout({}, {}).empty());
}

TEST_CASE("a chain flows left to right without overlap", "[layout]")
{
  const auto sizes = boxes(4);
  const std::vector<LayoutEdge> edges{{0, 1, 30, 30}, {1, 2, 30, 30}, {2, 3, 30, 30}};
  const auto pos = layeredLayout(sizes, edges);
  REQUIRE(pos.size() == 4);
  CHECK(backwards(sizes, pos, edges) == 0);
  CHECK(overlaps(sizes, pos) == 0);
  CHECK(pos[0] == QPointF{0., 0.});
}

TEST_CASE("long edges and fan-outs keep every column free of overlap", "[layout]")
{
  const auto sizes = boxes(6);
  const std::vector<LayoutEdge> edges{
      {0, 1, 30, 30}, {1, 2, 30, 30}, {2, 3, 30, 30}, {0, 3, 30, 35},
      {0, 4, 30, 30}, {4, 5, 30, 30}, {0, 5, 25, 30}};
  const auto pos = layeredLayout(sizes, edges);
  CHECK(backwards(sizes, pos, edges) == 0);
  CHECK(overlaps(sizes, pos) == 0);
}

TEST_CASE("cycles and self-loops are laid out", "[layout]")
{
  const auto sizes = boxes(6);
  const std::vector<LayoutEdge> edges{
      {0, 1, 30, 30}, {1, 2, 30, 30}, {2, 0, 30, 30},
      {3, 4, 30, 30}, {4, 3, 30, 30}, {5, 5, 30, 30}, {2, 3, 30, 30}};
  const auto pos = layeredLayout(sizes, edges);
  REQUIRE(pos.size() == 6);
  for(const auto& p : pos)
    CHECK((std::isfinite(p.x()) && std::isfinite(p.y())));
  CHECK(overlaps(sizes, pos) == 0);
}

TEST_CASE("the layout is deterministic", "[layout]")
{
  const auto sizes = boxes(8);
  const std::vector<LayoutEdge> edges{
      {0, 2, 30, 30}, {1, 2, 30, 30}, {2, 3, 30, 30}, {2, 4, 30, 30},
      {3, 5, 30, 30}, {4, 5, 30, 30}, {6, 7, 30, 30}};
  CHECK(layeredLayout(sizes, edges) == layeredLayout(sizes, edges));
}

TEST_CASE("edges to unknown nodes are ignored", "[layout]")
{
  const auto sizes = boxes(2);
  const auto pos = layeredLayout(sizes, {{0, 1, 30, 30}, {0, 7, 30, 30}, {-1, 1, 30, 30}});
  REQUIRE(pos.size() == 2);
  CHECK(backwards(sizes, pos, {{0, 1, 30, 30}}) == 0);
}

TEST_CASE("unconnected nodes go in rows below the connected ones", "[layout]")
{
  std::vector<QSizeF> sizes = boxes(2);
  for(int i = 0; i < 40; i++)
    sizes.push_back({150., 50.});
  const std::vector<LayoutEdge> edges{{0, 1, 30, 30}};
  const auto pos = layeredLayout(sizes, edges);
  CHECK(overlaps(sizes, pos) == 0);
  const double connectedBottom = std::max(pos[0].y() + 40., pos[1].y() + 40.);
  for(std::size_t i = 2; i < pos.size(); i++)
  {
    CHECK(pos[i].y() > connectedBottom);
    CHECK(pos[i].x() + sizes[i].width() <= 1600. + 150.);
  }
}

TEST_CASE("a large graph lays out without overlap", "[layout]")
{
  std::mt19937 rng{1234};
  std::vector<QSizeF> sizes;
  std::uniform_real_distribution<double> w{80., 240.}, h{30., 140.};
  for(int i = 0; i < 500; i++)
    sizes.push_back({w(rng), h(rng)});
  std::vector<LayoutEdge> edges;
  std::uniform_int_distribution<int> node{0, 499};
  for(int i = 0; i < 3000; i++)
  {
    int a = node(rng), b = node(rng);
    if(a > b)
      std::swap(a, b);
    edges.push_back({a, b, 25., 25.});
  }

  const auto pos = layeredLayout(sizes, edges);
  CHECK(overlaps(sizes, pos) == 0);
  std::vector<LayoutEdge> forward;
  for(const auto& e : edges)
    if(e.from != e.to)
      forward.push_back(e);
  CHECK(backwards(sizes, pos, forward) == 0);
}
