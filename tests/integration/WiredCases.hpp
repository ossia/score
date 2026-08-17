#pragma once

// Shared reporting for the "drive each tester through the graph its `Wire:`
// clause prescribes" fixtures (ShaderSweepWired.cpp, ShaderSweepScene.cpp).
//
// These are not sweeps: there is no baseline and no per-file loop over the
// library. Each case is hand-built, so it asserts directly — and it records its
// measurement whether it passes or not, because "what did the frame actually
// look like" is the evidence, not the boolean.

#include "ShaderSweep.hpp"

#include <score_test/Gfx.hpp>

namespace score::test::wired
{
struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  //! Empty means the case passed.
  std::string failure;
  //! Always printed: what the fixture actually measured.
  std::string note;
};

inline std::vector<std::pair<std::string, Outcome>>& results()
{
  static std::vector<std::pair<std::string, Outcome>> r;
  return r;
}

//! Printed before the case runs, so that a case which takes the process down
//! rather than failing still names itself in the log.
inline void announce(const std::string& name)
{
  qInfo().noquote() << "[wired] >>>" << QString::fromStdString(name);
}

inline void record(const std::string& name, Outcome o)
{
  qInfo().noquote() << "[wired]" << QString::fromStdString(name)
                    << (o.skipped            ? "SKIP"
                        : o.failure.empty()  ? "ok"
                                             : "FAIL")
                    << QString::fromStdString(o.failure.empty() ? o.note : o.failure);
  results().emplace_back(name, std::move(o));
}

//! A frame with no pixels at all is NOT uniform — it is a failure. Reporting it
//! as a pass is how a shader that renders nothing scores green.
inline bool isUniformImage(const score::test::gfx::ReadbackImage& im)
{
  if(!im.valid() || im.bytes.isEmpty())
    return false;
  const auto* p = reinterpret_cast<const quint32*>(im.bytes.constData());
  const auto n = im.bytes.size() / 4;
  for(qsizetype i = 1; i < n; i++)
    if(p[i] != p[0])
      return false;
  return true;
}

inline bool isBlack(const score::test::gfx::ReadbackImage& im)
{
  if(!im.valid() || im.bytes.isEmpty())
    return false;
  const auto* p = reinterpret_cast<const uint8_t*>(im.bytes.constData());
  for(qsizetype i = 0; i < im.bytes.size(); i += 4)
    if(p[i] || p[i + 1] || p[i + 2])
      return false;
  return true;
}

//! Fraction of pixels that are not pure black. A shaded cube covers part of the
//! frame, so "how much got drawn" separates "a few stray fragments" from "the
//! mesh rendered" far better than a centre sample.
inline double coverage(const score::test::gfx::ReadbackImage& im)
{
  if(!im.valid() || im.bytes.isEmpty())
    return 0.;
  const auto* p = reinterpret_cast<const uint8_t*>(im.bytes.constData());
  qsizetype lit = 0, total = 0;
  for(qsizetype i = 0; i < im.bytes.size(); i += 4, ++total)
    if(p[i] || p[i + 1] || p[i + 2])
      ++lit;
  return total ? double(lit) / double(total) : 0.;
}

inline std::string describe(const score::test::gfx::ReadbackImage& im)
{
  if(!im.valid() || im.bytes.isEmpty())
    return "empty readback";
  const auto c = im.center();
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.3f", coverage(im));
  return "center=(" + std::to_string(c[0]) + "," + std::to_string(c[1]) + ","
         + std::to_string(c[2]) + "," + std::to_string(c[3]) + ") cover=" + buf
         + (isUniformImage(im) ? " uniform" : " varied");
}

//! Compares two readbacks of the same size byte-for-byte. Used by the cases
//! whose whole point is that some upstream changed the picture.
inline bool sameImage(
    const score::test::gfx::ReadbackImage& a, const score::test::gfx::ReadbackImage& b)
{
  return a.valid() && b.valid() && a.bytes == b.bytes;
}

//! Folds the two ways a render can fail to produce anything into one string:
//! `skipped` (no usable backend on this machine) stays a skip, everything else
//! is a failure of the fixture.
inline bool harvest(const score::test::gfx::IsfResult& r, Outcome& o)
{
  if(r.skipped)
  {
    o.skipped = true;
    o.skip_reason = r.skip_reason;
    return false;
  }
  if(!r.error.empty())
  {
    o.failure = r.error;
    return false;
  }
  if(r.outputs.empty() || !r.outputs.front().valid())
  {
    o.failure = "no output attachment read back";
    return false;
  }
  return true;
}

//! Turns the recorded cases into Catch2 assertions. Call AFTER run_in_gui_app
//! returns — Catch2 macros in the render lambda would be lost to a crash.
inline void assertAll()
{
  auto& res = results();
  if(res.empty())
    SKIP("no wired case ran");

  int passed = 0, skipped = 0, failed = 0;
  for(const auto& [name, o] : res)
  {
    INFO(name << ": " << (o.note.empty() ? o.failure : o.note));
    if(o.skipped)
    {
      ++skipped;
      continue;
    }
    if(o.failure.empty())
      ++passed;
    else
      ++failed;
    CHECK(o.failure.empty());
  }

  qInfo().noquote() << "[wired] " << passed << "passed," << failed << "failed,"
                    << skipped << "skipped of" << res.size() << "cases";
  if(skipped == (int)res.size())
    SKIP("no usable RHI backend on this machine");
}
}
