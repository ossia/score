#pragma once
// Golden-image comparison for C++ gfx tests.
//
// This header does NOT implement a tolerance. It shells out to
// tests/integration/golden-render/compare.py, which is the single place any
// golden threshold is defined, and turns its JSON verdict into a Catch2
// assertion.
//
// That indirection is the point. Before it, there were three tolerance models
// in the tree for the same job: compare.py's PSNR/SSIM/max_abs profiles, the
// meanAbs<4.0 / fracFar<0.02 pair open-coded in ThreedimRenderTest.cpp, and
// text-render.sh's choice of which compare.py profile to pass. They disagreed
// about what "the same picture" means -- the C++ pair, in particular, allowed
// 2% of the frame to be off by more than 24 codes, which on a 1280x720 grab is
// eighteen thousand pixels, a 135x135 block of arbitrary garbage, passing as a
// match. Reimplementing the metrics here in C++ would have made a fourth.
//
// The cost is a python3 + numpy/PIL/scipy dependency in a C++ test. It is the
// same dependency golden-render.sh and text-render.sh already carry, the tests
// that use this header already drive the real ossia-score binary as a
// subprocess, and a missing interpreter SKIPs rather than fails.

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <string>

namespace score::testing
{

//! Where compare.py lives. CMake defines GOLDEN_COMPARE_PY; the fallback keeps
//! the header usable from a build that has not been reconfigured yet.
inline QString goldenComparePath()
{
#if defined(GOLDEN_COMPARE_PY)
  return QStringLiteral(GOLDEN_COMPARE_PY);
#else
  return {};
#endif
}

//! Mirror the prerequisite check golden-render.sh and text-render.sh already
//! make, so a machine without the scientific stack SKIPs the golden gate
//! instead of failing it. Probed once: the answer cannot change mid-run.
inline bool goldenComparatorUsable()
{
  static const bool ok = [] {
    QProcess p;
    p.start("python3", QStringList{"-c", "import numpy, PIL, scipy"});
    if(!p.waitForStarted(10000))
      return false;
    p.waitForFinished(60000);
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
  }();
  return ok;
}

struct GoldenVerdict
{
  bool ran{false};        //!< the comparator was found and executed
  bool pass{false};
  QString reason;         //!< why it failed, from compare.py
  QString artifacts;      //!< directory holding golden/actual/diff PNGs
  QString metrics;        //!< the one-line metric summary, for the test log
};

//! Compare `actual` against the golden for `caseName`.
//!
//! `refsDir` holds one golden per case (refs/<case>.png) -- NOT one per
//! backend. On failure the actual, the golden and a diff image are written to
//! `artifactDir` named after the case, so a CI failure carries the evidence
//! needed to tell a real regression from a driver difference without a local
//! reproduction.
//!
//! `channels` narrows which colour channels the metrics see, and defaults to
//! all of them. It is NOT a tolerance knob and compare.py's docstring states
//! the only bar that justifies moving it: a channel may be dropped when the
//! renderer cannot reproduce it against ITSELF -- measured, two runs on one
//! machine -- so that comparing it states nothing about correctness. Exactly
//! one case narrows it today (obj-cube, "rg": its blue channel carries a
//! specular term whose light position is sin(TIME)/cos(TIME)); that case
//! asserts the dropped channel structurally instead of dropping the coverage.
inline GoldenVerdict compareToGolden(
    const QImage& actual, const QString& caseName, const QString& refsDir,
    const QString& artifactDir, const QString& channels = QStringLiteral("rgb"))
{
  GoldenVerdict v;

  const QString py = goldenComparePath();
  if(py.isEmpty() || !QFileInfo::exists(py) || !goldenComparatorUsable())
    return v; // caller SKIPs: comparator not available

  const QString golden = refsDir + "/" + caseName + ".png";
  if(!QFileInfo::exists(golden))
  {
    v.ran = true;
    v.reason = "no golden at " + golden;
    return v;
  }

  QDir().mkpath(artifactDir);
  // The grab has to reach the comparator as a file; write it beside the
  // artifacts so a failure leaves the exact bytes that were judged.
  const QString actualPath = artifactDir + "/" + caseName + ".rendered.png";
  if(!actual.save(actualPath))
  {
    v.ran = true;
    v.reason = "could not write " + actualPath;
    return v;
  }

  QProcess p;
  p.start(
      "python3",
      QStringList{py, golden, actualPath, "--json", "--profile", "shared",
                  "--diff-dir", artifactDir, "--name", caseName, "--channels",
                  channels});
  if(!p.waitForStarted(10000))
    return v; // no python3: caller SKIPs
  p.waitForFinished(120000);

  const QByteArray out = p.readAllStandardOutput().trimmed();
  const auto doc = QJsonDocument::fromJson(out);
  if(!doc.isObject())
  {
    // Exit code 2 is a usage/IO error inside the comparator, not a verdict.
    v.ran = true;
    v.reason = "comparator produced no verdict: "
               + QString::fromUtf8(p.readAllStandardError().trimmed());
    return v;
  }

  const auto o = doc.object();
  v.ran = true;
  v.pass = o.value("verdict").toString() == "PASS";
  v.reason = o.value("reason").toString();
  v.artifacts = artifactDir;
  v.metrics = QStringLiteral(
                  "psnr=%1 ssim=%2 max_abs=%3 mean_abs=%4 pixels_over=%5")
                  .arg(o.value("psnr").toDouble())
                  .arg(o.value("ssim").toDouble())
                  .arg(o.value("max_abs").toDouble())
                  .arg(o.value("mean_abs").toDouble())
                  .arg(o.value("pixels_over").toInt());
  if(channels != QStringLiteral("rgb"))
    v.metrics += " channels=" + channels;
  return v;
}

} // namespace score::testing
