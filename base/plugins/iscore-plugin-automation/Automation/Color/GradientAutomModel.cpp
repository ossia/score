#include <Automation/Color/GradientAutomModel.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <QColor>
namespace Gradient
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id,
                        Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
  m_colors.insert(std::make_pair(0., QColor(Qt::black)));
  m_colors.insert(std::make_pair(1., QColor(Qt::white)));

  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
}

ProcessModel::ProcessModel(
    const ProcessModel& source,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{
        source, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , m_address(source.m_address)
    , m_colors{source.m_colors}
    , m_tween{source.m_tween}
{
}

QString ProcessModel::prettyName() const
{
  return address().toShortString();
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration)
{
  // We only need to change the duration.
  setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration)
{
  setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration)
{
  setDuration(newDuration);
}

bool ProcessModel::contentHasDuration() const
{
  return true;
}

TimeVal ProcessModel::contentDuration() const
{
  // TODO duration() * min(1., last point)
  return duration();
}

::State::AddressAccessor ProcessModel::address() const
{
  return m_address;
}

void ProcessModel::setAddress(const ::State::AddressAccessor& arg)
{
  if (m_address == arg)
  {
    return;
  }

  m_address = arg;
  emit addressChanged(arg);
  emit unitChanged(arg.qualifiers.get().unit);
}

State::Unit ProcessModel::unit() const
{
  return m_address.qualifiers.get().unit;
}

void ProcessModel::setUnit(const State::Unit& u)
{
  if (u != unit())
  {
    m_address.qualifiers.get().unit = u;
    emit addressChanged(m_address);
    emit unitChanged(u);
  }
}
}


template <>
void DataStreamReader::read(
    const Gradient::ProcessModel& autom)
{
  /*
  m_stream << autom.address();
  m_stream << autom.tween();
  m_stream << autom.gradient();
  */
  insertDelimiter();
}


template <>
void DataStreamWriter::write(Gradient::ProcessModel& autom)
{

  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Gradient::ProcessModel& autom)
{
  obj[strings.Address] = toJsonObject(autom.address());
  obj["Tween"] = autom.tween();
}


template <>
void JSONObjectWriter::write(Gradient::ProcessModel& autom)
{
  autom.setAddress(
      fromJsonObject<State::AddressAccessor>(obj[strings.Address]));
  autom.setTween(obj["Tween"].toBool());
}
