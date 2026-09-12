#include <Gris/Algo/SpeakerSetupIO.hpp>
#include <Gris/Model.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gris::SpatModel)

namespace Gris
{
SpatModel::SpatModel(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "GrisSpat", parent}
{
  m_inlets.push_back(new Process::AudioInlet{Id<Process::Port>(0), this});
  m_inlets.push_back(new SpeakerSetupInlet{tr("Speaker setup"), Id<Process::Port>(1), this});
  m_inlets.push_back(new Process::FloatSlider{
      0.f, 1.f, 0.f, tr("Interpolation"), Id<Process::Port>(2), this});
  m_inlets.push_back(new Process::IntSlider{
      1, maxSourceCount, defaultSourceCount, tr("Sources"), Id<Process::Port>(3), this});

  m_outlets.push_back(new Process::AudioOutlet{Id<Process::Port>(0), this});
  safe_cast<Process::AudioOutlet*>(m_outlets.back())->setPropagate(true);

  rebuildSourcePorts();
  init();
}

SpatModel::~SpatModel() = default;

void SpatModel::init()
{
  // The source count is driven from its control port so that changing it goes
  // through the usual undo stack.
  if(m_inlets.size() > SourceCount)
  {
    connect(
        safe_cast<Process::ControlInlet*>(m_inlets[SourceCount]),
        &Process::ControlInlet::valueChanged, this, [this](const ossia::value& v) {
      setSourceCount(ossia::convert<int>(v));
        });
  }
}

SpeakerSetupInlet& SpatModel::speakerSetupInlet() const noexcept
{
  return *safe_cast<SpeakerSetupInlet*>(m_inlets[SpeakerSetupPort]);
}

SpeakerSetup SpatModel::speakerSetup() const noexcept
{
  return speakerSetupInlet().setup();
}

void SpatModel::setSourceCount(int count)
{
  count = std::clamp(count, 1, maxSourceCount);
  if(count == m_sourceCount)
    return;

  m_sourceCount = count;
  rebuildSourcePorts();
  sourceCountChanged(count);
  inletsChanged();
}

void SpatModel::rebuildSourcePorts()
{
  // Drop the per-source ports and recreate them; the fixed ones stay put.
  while(std::ssize(m_inlets) > FixedInletCount)
  {
    delete m_inlets.back();
    m_inlets.pop_back();
  }

  int nextId = FixedInletCount;
  for(int source = 0; source < m_sourceCount; ++source)
  {
    auto const label = QString::number(source + 1);

    m_inlets.push_back(new Process::XYZSlider{
        ossia::vec3f{-2.f, -2.f, -2.f}, ossia::vec3f{2.f, 2.f, 2.f},
        ossia::vec3f{0.f, 0.f, 0.f}, tr("Source %1 position").arg(label),
        Id<Process::Port>(nextId++), this});
    m_inlets.push_back(new Process::FloatSlider{
        0.f, 1.f, 0.f, tr("Source %1 azimuth span").arg(label),
        Id<Process::Port>(nextId++), this});
    m_inlets.push_back(new Process::FloatSlider{
        0.f, 1.f, 0.f, tr("Source %1 zenith span").arg(label),
        Id<Process::Port>(nextId++), this});
    m_inlets.push_back(new Process::Enum{
        std::vector<std::string>{"Dome (VBAP)", "Cube (MBAP)"}, std::vector<QString>{},
        "Dome (VBAP)", tr("Source %1 mode").arg(label), Id<Process::Port>(nextId++),
        this});
  }
}
} // namespace Gris
