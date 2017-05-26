#include <QString>
#include <chrono>

#include "AbstractTimeRuler.hpp"
#include "AbstractTimeRulerView.hpp"
#include <Process/TimeValue.hpp>
namespace Scenario
{
AbstractTimeRuler::AbstractTimeRuler(
    AbstractTimeRulerView& view, QObject* parent)
    : QObject{parent}, m_view{view}
{
  m_view.setPresenter(this);

  m_graduationsSpacing.push_back({0.1, std::chrono::seconds(120)});
  m_graduationsSpacing.push_back({0.2, std::chrono::seconds(60)});
  m_graduationsSpacing.push_back({0.5, std::chrono::seconds(30)});

  m_graduationsSpacing.push_back({1, std::chrono::seconds(10)});
  m_graduationsSpacing.push_back({2, std::chrono::seconds(5)});
  m_graduationsSpacing.push_back({5, std::chrono::seconds(2)});

  m_graduationsSpacing.push_back({10, std::chrono::seconds(1)});
  m_graduationsSpacing.push_back(
      {20, std::chrono::milliseconds(500)});
  m_graduationsSpacing.push_back(
      {40, std::chrono::milliseconds(250)});
  m_graduationsSpacing.push_back(
      {80, std::chrono::milliseconds(150)});

  m_graduationsSpacing.push_back(
      {100, std::chrono::milliseconds(100)});
  m_graduationsSpacing.push_back(
      {200, std::chrono::milliseconds(50)});
  m_graduationsSpacing.push_back(
      {500, std::chrono::milliseconds(20)});

  m_graduationsSpacing.push_back(
      {1000, std::chrono::milliseconds(10)});
  m_graduationsSpacing.push_back(
      {2000, std::chrono::milliseconds(5)});
  m_graduationsSpacing.push_back(
      {5000, std::chrono::milliseconds(2)});

  m_graduationsSpacing.push_back(
      {10000, std::chrono::microseconds(1000)});
  m_graduationsSpacing.push_back(
      {20000, std::chrono::microseconds(500)});
  m_graduationsSpacing.push_back(
      {50000, std::chrono::microseconds(200)});

  m_graduationsSpacing.push_back(
      {100000, std::chrono::microseconds(100)});
  m_graduationsSpacing.push_back(
      {200000, std::chrono::microseconds(50)});
  m_graduationsSpacing.push_back(
      {500000, std::chrono::microseconds(20)});
}

AbstractTimeRuler::~AbstractTimeRuler() = default;

void AbstractTimeRuler::setStartPoint(TimeVal dur)
{
  if (m_startPoint != dur)
  {
    m_startPoint = dur;
    computeGraduationSpacing();
  }
}

void AbstractTimeRuler::setPixelPerMillis(double factor)
{
  if (factor != m_pixelPerMillis)
  {
    m_pixelPerMillis = factor;
    computeGraduationSpacing();
  }
}

void AbstractTimeRuler::computeGraduationSpacing()
{
  double pixPerSec = 1000. * m_pixelPerMillis;
  double gradSpace = pixPerSec;

  double deltaTime = 100.;
  auto format = QStringLiteral("m:ss");
  int loop = 5;

  int i = 0;
  const int n = m_graduationsSpacing.size();
  for (i = 0; i < n - 1; i++)
  {
    if (pixPerSec > m_graduationsSpacing[i].first
        && pixPerSec < m_graduationsSpacing[i + 1].first)
    {
      deltaTime = m_graduationsSpacing[i].second.count() / 1000.;
      gradSpace = pixPerSec * m_graduationsSpacing[i].second.count() / 1000000.;
      break;
    }
  }
  if (i > 7)
  {
    format = QStringLiteral("m:ss.z");
    loop = 10;
  }

  m_view.setGraduationsStyle(gradSpace, deltaTime, format, loop);
}

const QVector<QPair<double, std::chrono::microseconds>>&
AbstractTimeRuler::graduationsSpacing() const
{
  return m_graduationsSpacing;
}
}
