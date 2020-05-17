// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Dataflow/Port.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/editor/state/destination_qualifiers.hpp>

#include <QColor>

#include <Color/GradientModel.hpp>
#include <Color/GradientPresenter.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Gradient::ProcessModel)
namespace Gradient
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{Process::make_value_outlet(Id<Process::Port>(0), this)}
{
  m_colors.insert(std::make_pair(0.2, QColor(Qt::black)));
  m_colors.insert(std::make_pair(0.8, QColor(Qt::white)));

  metadata().setInstanceName(*this);
  init();
}

ProcessModel::~ProcessModel() { }

void ProcessModel::init()
{
  outlet->setCustomData("Out");
  auto update_invalid_address = [=](const State::AddressAccessor& addr) {
    if (addr.qualifiers.get() != ossia::destination_qualifiers{{}, ossia::argb_u{}})
    {
      State::AddressAccessor copy = addr;
      copy.qualifiers = ossia::destination_qualifiers{{}, ossia::argb_u{}};
      outlet->setAddress(std::move(copy));
    }
  };
  con(*outlet, &Process::Outlet::addressChanged, this, update_invalid_address);
  update_invalid_address(outlet->address());
  m_outlets.push_back(outlet.get());
}

QString ProcessModel::prettyName() const noexcept
{
  auto res = address().toString();
  if (!res.isEmpty())
    return res;
  return "Gradient";
}

const ProcessModel::gradient_colors& ProcessModel::gradient() const
{
  return m_colors;
}

void ProcessModel::setGradient(const ProcessModel::gradient_colors& c)
{
  if (m_colors != c)
  {
    m_colors = c;
    gradientChanged();
  }
}

const ::State::AddressAccessor& ProcessModel::address() const
{
  return outlet->address();
}

bool ProcessModel::tween() const
{
  return m_tween;
}

void ProcessModel::setTween(bool tween)
{
  if (m_tween == tween)
    return;

  m_tween = tween;
  tweenChanged(tween);
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  // We only need to change the duration.
  setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

TimeVal ProcessModel::contentDuration() const noexcept
{
  auto lastPoint = 1.;
  if (!m_colors.empty())
  {
    auto back = m_colors.rbegin()->first;
    lastPoint = std::max(1., back);
  }

  return TimeVal(duration().impl * lastPoint);
}
}

template <>
void DataStreamReader::read(const Gradient::ProcessModel& autom)
{
  m_stream << *autom.outlet;
  m_stream << autom.m_colors << autom.m_tween;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gradient::ProcessModel& autom)
{
  autom.outlet = Process::load_value_outlet(*this, &autom);
  m_stream >> autom.m_colors >> autom.m_tween;

  checkDelimiter();
}

template <>
void JSONReader::read(const Gradient::ProcessModel& autom)
{
  obj["Outlet"] = *autom.outlet;
  obj["Gradient"] = autom.m_colors;
  obj["Tween"] = autom.tween();
}

template <>
void JSONWriter::write(Gradient::ProcessModel& autom)
{
  JSONWriter writer{obj["Outlet"]};
  autom.outlet = Process::load_value_outlet(writer, &autom);

  autom.setTween(obj["Tween"].toBool());
  autom.m_colors <<= obj["Gradient"];
}
