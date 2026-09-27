#include "Model.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortSerialization.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Audio/Settings/Model.hpp>

#include <score/application/ApplicationContext.hpp>

#include <ossia/detail/pod_vector.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::Step::Model)
namespace Media
{
namespace Step
{
//! A step not drawn yet: the middle of the range.
static constexpr float default_step = 0.5f;

//! Rates, in the convention of ossia::token_request::get_quantification_dates:
//! above 1 subdivides the whole note, 1 and below counts bars, and 0 does not
//! quantize at all. The same choices as Patternist.
static std::vector<std::pair<QString, ossia::value>> quantificationChoices()
{
  return {{QObject::tr("Free"), 0.},     {QObject::tr("8 bars"), 0.125},
          {QObject::tr("4 bars"), 0.25}, {QObject::tr("2 bars"), 0.5},
          {QObject::tr("1 bar"), 1.},    {QObject::tr("1/2"), 2.},
          {QObject::tr("1/4"), 4.},      {QObject::tr("1/8"), 8.},
          {QObject::tr("1/16"), 16.},    {QObject::tr("1/32"), 32.}};
}

static std::unique_ptr<Process::ControlInlet> makeSequenceSelect(QObject* parent)
{
  return std::make_unique<Process::IntSpinBox>(
      0, Model::maxSequences - 1, 0, QObject::tr("Sequence"), Id<Process::Port>(0),
      parent);
}

static std::unique_ptr<Process::ControlInlet> makeSwitchQuantification(QObject* parent)
{
  return std::make_unique<Process::ComboBox>(
      quantificationChoices(), 1., QObject::tr("Quantization"), Id<Process::Port>(1),
      parent);
}

//! A step lasts `seconds`, or when synced a fraction of a whole note.
static std::unique_ptr<Process::ControlInlet>
makeStepDuration(ossia::vec2f value, QObject* parent)
{
  auto p = std::make_unique<Process::TimeChooser>(
      0.001f, 60.f, 0.1f, QObject::tr("Duration"), Id<Process::Port>(2), parent);
  p->setValue(value);
  return p;
}

Model::Model(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, Metadata<ObjectKey_k, Model>::get(), parent}
    , sequenceSelect{makeSequenceSelect(this)}
    , switchQuantification{makeSwitchQuantification(this)}
    , stepDuration{makeStepDuration(ossia::vec2f{1.f / 16.f, 1.f}, this)}
    , outlet{
          std::make_unique<Process::ValueOutlet>("Step Out", Id<Process::Port>(0), this)}
{
  m_sequences = {{0.5f, 0.3f, 0.5f, 0.8f, 1.f, 0.f, 0.5f, 0.1f}};
  m_min = -1.;
  m_max = 1.;
  metadata().setInstanceName(*this);
  init();
}

void Model::init()
{
  m_inlets.push_back(sequenceSelect.get());
  m_inlets.push_back(switchQuantification.get());
  m_inlets.push_back(stepDuration.get());
  m_outlets.push_back(outlet.get());

  // The bars are the whole body: the node shows the controls as ports.
  for(auto* p : {sequenceSelect.get(), switchQuantification.get(), stepDuration.get()})
  {
    p->displayHandledExplicitly = false;
    p->visibleWhenFolded = true;
  }

  if(m_sequences.empty())
    m_sequences.push_back(ossia::float_vector(8, default_step));
  m_currentSequence = std::clamp(m_currentSequence, 0, int(m_sequences.size()) - 1);

  // The port is the only selector: the property follows it, so the layer
  // draws the sequence the port picked. Execution reads the port itself.
  if(ossia::convert<int>(sequenceSelect->value()) != m_currentSequence)
    sequenceSelect->setValue(m_currentSequence);
  connect(
      sequenceSelect.get(), &Process::ControlInlet::valueChanged, this,
      [this](const ossia::value& v) { setCurrentSequence(ossia::convert<int>(v)); });
}

Model::~Model() { }

int Model::stepCount() const
{
  return steps().size();
}

const ossia::float_vector& Model::steps() const
{
  return m_sequences[m_currentSequence];
}

const std::vector<ossia::float_vector>& Model::sequences() const noexcept
{
  return m_sequences;
}

int Model::currentSequence() const noexcept
{
  return m_currentSequence;
}

double Model::min() const
{
  return m_min;
}

double Model::max() const
{
  return m_max;
}

void Model::setStepCount(int s)
{
  s = std::clamp(s, 1, maxSteps);
  auto& seq = m_sequences[m_currentSequence];
  if(s != std::ssize(seq))
  {
    seq.resize(s, default_step);
    stepCountChanged(s);
    // The executor follows the steps: a new count is new steps.
    stepsChanged();
  }
}

void Model::setSteps(ossia::float_vector v)
{
  setSequenceSteps(m_currentSequence, std::move(v));
}

void Model::setSequenceSteps(int sequence, ossia::float_vector v)
{
  if(sequence < 0 || sequence >= std::ssize(m_sequences) || v.empty())
    return;
  auto& seq = m_sequences[sequence];
  if(seq != v)
  {
    const bool resized = seq.size() != v.size();
    seq = std::move(v);
    if(resized && sequence == m_currentSequence)
      stepCountChanged(seq.size());
    stepsChanged();
  }
}

void Model::setSequences(std::vector<ossia::float_vector> v)
{
  std::erase_if(v, [](const auto& s) { return s.empty(); });
  if(v.empty() || v == m_sequences)
    return;
  const int count = stepCount();
  m_sequences = std::move(v);
  if(m_currentSequence >= std::ssize(m_sequences))
  {
    m_currentSequence = std::ssize(m_sequences) - 1;
    currentSequenceChanged(m_currentSequence);
  }
  if(stepCount() != count)
    stepCountChanged(stepCount());
  stepsChanged();
}

void Model::setCurrentSequence(int n)
{
  // The port's own range: a remote-control write reaches setValue without any
  // clamp, and growing the list to meet it would allocate without bound.
  n = std::clamp(n, 0, maxSequences - 1);

  if(n >= std::ssize(m_sequences))
  {
    // New sequences have the length of the one shown, all steps at rest.
    const auto len = steps().size();
    while(n >= std::ssize(m_sequences))
      m_sequences.push_back(ossia::float_vector(len, default_step));
    stepsChanged();
  }

  if(n != m_currentSequence)
  {
    const int count = stepCount();
    m_currentSequence = n;
    currentSequenceChanged(n);
    if(stepCount() != count)
      stepCountChanged(stepCount());
  }

  // The port stays on the sequence actually shown; it writes back here, which
  // stops as soon as the two agree.
  if(ossia::convert<int>(sequenceSelect->value()) != m_currentSequence)
    sequenceSelect->setValue(m_currentSequence);
}

void Model::setMin(double v)
{
  if(m_min != v)
  {
    m_min = v;
    minChanged(v);
  }
}

void Model::setMax(double v)
{
  if(m_max != v)
  {
    m_max = v;
    maxChanged(v);
  }
}
}
}
template <>
void DataStreamReader::read(const Media::Step::Model& proc)
{
  m_stream << *proc.sequenceSelect << *proc.switchQuantification << *proc.stepDuration
           << *proc.outlet << proc.m_sequences << proc.m_currentSequence << proc.m_min
           << proc.m_max;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Step::Model& proc)
{
  proc.sequenceSelect = Process::load_control_inlet(*this, &proc);
  proc.switchQuantification = Process::load_control_inlet(*this, &proc);
  proc.stepDuration = Process::load_control_inlet(*this, &proc);
  proc.outlet = Process::load_value_outlet(*this, &proc);
  m_stream >> proc.m_sequences >> proc.m_currentSequence >> proc.m_min >> proc.m_max;
  checkDelimiter();
}

template <>
void JSONReader::read(const Media::Step::Model& proc)
{
  obj["SequenceSelect"] = *proc.sequenceSelect;
  obj["SwitchQuantification"] = *proc.switchQuantification;
  obj["StepDuration"] = *proc.stepDuration;
  obj["Outlet"] = *proc.outlet;
  obj["Sequences"] = proc.m_sequences;
  obj["Sequence"] = proc.m_currentSequence;
  obj["StepMin"] = proc.m_min;
  obj["StepMax"] = proc.m_max;
}

template <>
void JSONWriter::write(Media::Step::Model& proc)
{
  using namespace Media::Step;
  {
    JSONWriter writer{obj["Outlet"]};
    proc.outlet = Process::load_value_outlet(writer, &proc);
  }

  if(auto port = obj.tryGet("SequenceSelect"))
  {
    JSONWriter writer{*port};
    proc.sequenceSelect = Process::load_control_inlet(writer, &proc);
  }
  else
  {
    proc.sequenceSelect = makeSequenceSelect(&proc);
  }

  if(auto port = obj.tryGet("SwitchQuantification"))
  {
    JSONWriter writer{*port};
    proc.switchQuantification = Process::load_control_inlet(writer, &proc);
  }
  else
  {
    proc.switchQuantification = makeSwitchQuantification(&proc);
  }

  if(auto port = obj.tryGet("StepDuration"))
  {
    JSONWriter writer{*port};
    proc.stepDuration = Process::load_control_inlet(writer, &proc);
  }
  else
  {
    // A document from before the Duration port: the step lasted StepDur
    // samples at the rate the document played at. The same length in seconds,
    // not synced, so that it plays as it did.
    double rate
        = score::AppContext().settings<Audio::Settings::Model>().getRate();
    if(rate <= 0.)
      rate = 48000.;
    const double samples = obj["StepDur"].toInt();
    const float seconds
        = samples > 0. ? std::clamp(float(samples / rate), 0.001f, 60.f) : 0.1f;
    proc.stepDuration = makeStepDuration(ossia::vec2f{seconds, 0.f}, &proc);
  }

  if(auto seqs = obj.tryGet("Sequences"))
  {
    proc.m_sequences <<= *seqs;
    if(auto cur = obj.tryGet("Sequence"))
      proc.m_currentSequence = cur->toInt();
  }
  else
  {
    // Before sequences: the one there was becomes sequence 0.
    ossia::float_vector steps;
    steps <<= obj["Steps"];
    const int count = obj["StepCount"].toInt();
    if(count > 0)
      steps.resize(std::min(count, Model::maxSteps), default_step);
    proc.m_sequences = {std::move(steps)};
    proc.m_currentSequence = 0;
  }
  std::erase_if(proc.m_sequences, [](const auto& s) { return s.empty(); });

  proc.m_min = obj["StepMin"].toDouble();
  proc.m_max = obj["StepMax"].toDouble();
}
