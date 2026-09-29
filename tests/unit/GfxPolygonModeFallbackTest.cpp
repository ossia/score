// A raw raster's "POLYGON_MODE": "line" on a backend without
// QRhi::NonFillPolygonMode falls back to fill and warns once per process.
// Its own binary: the warning is a process-wide once flag, so any earlier
// fallback in the same process would swallow the one asserted here.
#include <Gfx/Graph/PipelineStateHelpers.hpp>

#include <QStringList>

#include <catch2/catch_test_macros.hpp>

namespace
{
QStringList g_warnings;
QtMessageHandler g_previous{};
void collect(QtMsgType t, const QMessageLogContext& c, const QString& msg)
{
  if(t == QtWarningMsg)
    g_warnings.push_back(msg);
  if(g_previous)
    g_previous(t, c, msg);
}
}

TEST_CASE(
    "an unsupported non-fill polygon mode falls back to fill and warns once",
    "[gfx][polygon-mode]")
{
  using PM = QRhiGraphicsPipeline::PolygonMode;
  g_previous = qInstallMessageHandler(collect);
  const PM supported = score::gfx::supportedPolygonMode(PM::Line, true);
  const PM fillAlways = score::gfx::supportedPolygonMode(PM::Fill, false);
  const int warningsBefore = int(g_warnings.size());
  const PM first = score::gfx::supportedPolygonMode(PM::Line, false);
  const PM second = score::gfx::supportedPolygonMode(PM::Line, false);
  qInstallMessageHandler(g_previous);

  CHECK(supported == PM::Line);
  CHECK(fillAlways == PM::Fill);
  CHECK(warningsBefore == 0);
  CHECK(first == PM::Fill);
  CHECK(second == PM::Fill);
  REQUIRE(g_warnings.size() == 1);
  CHECK(g_warnings.front().contains("POLYGON_MODE"));
}
