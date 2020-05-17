// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioStyle.hpp"

#include <score/model/Skin.hpp>
namespace Process
{
Style::Style(score::Skin& s) noexcept : skin{s}
{
  update(s);
  QObject::connect(&s, &score::Skin::changed, [&] { this->update(s); });
}

void Style::setIntervalWidth(double w)
{
  /*
  IntervalSolidPen.setWidthF(3. * w);
  IntervalDashPen.setWidthF(3. * w);
  IntervalPlayPen.setWidthF(3. * w);
  IntervalPlayDashPen.setWidthF(3. * w);
  IntervalWaitingDashPen.setWidthF(3. * w);
  */
}

Style& Style::instance() noexcept
{
  static Style s(score::Skin::instance());
  return s;
}

Style::Style() noexcept : skin{score::Skin::instance()}
{
  update(skin);
}

Style::~Style() { }

void Style::update(const score::Skin&) { }
}
