#pragma once
#include <Process/Dataflow/Port.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <ossia/network/domain/domain.hpp>

namespace Process
{
struct FloatSlider;
struct FloatKnob;
struct LogFloatSlider;
struct IntSlider;
struct IntRangeSlider;
struct FloatRangeSlider;
struct IntSpinBox;
struct FloatSpinBox;
struct TimeChooser;
struct Toggle;
struct ChooserToggle;
struct ComboBox;
struct LineEdit;
struct ProgramEdit;
struct FileChooser;
struct AudioFileChooser;
struct Enum;
struct Button;
struct ImpulseButton;
struct HSVSlider;
struct XYSlider;
struct XYZSlider;
struct XYSpinboxes;
struct XYZSpinboxes;
struct MultiSlider;
struct Bargraph;
} // namespace Process
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::FloatSlider,
    "af2b4fc3-aecb-4c15-a5aa-1c573a239925")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::FloatKnob,
    "82427d27-084a-4ab6-9c4e-db83929a1200")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::LogFloatSlider,
    "5554eb67-bcc8-45ab-8ec2-37a3f191aa64")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::IntSlider,
    "348b80a4-45dc-4f70-8f5f-6546c85089a2")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::IntRangeSlider,
    "0c1902bc-e282-11ec-8fea-0242ac120002")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::FloatRangeSlider,
    "73ae3e85-0c91-497e-b612-b1391f87ac72")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::IntSpinBox,
    "238399a0-7e81-47e3-896f-08e8856e2973")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::FloatSpinBox,
    "10d62b0d-5bc9-4ac9-9540-9e8ac0c24947")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::Toggle,
    "fb27e4cb-ea7f-41e2-ad92-2354498c1b6b")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::ChooserToggle,
    "27d488b6-784b-4bfc-8e7f-e28ef030c248")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::TimeChooser,
    "b631d9b7-cbe3-4d9c-b470-f139e348aecb")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::LineEdit,
    "9ae797ea-d94c-4792-acec-9ec1932bae5d")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::FileChooser,
    "40833147-4c42-4b8b-bb80-0b1d15dae129")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::AudioFileChooser,
    "c347b510-927a-4924-9da1-c76871623567")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::ProgramEdit,
    "de15c0da-429b-49d3-bb07-7c41f5f205c8")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::Enum,
    "8b1d76c4-3838-4ac0-9b9c-c12bc5db8e8a")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::ComboBox,
    "485680cc-b8b9-4a01-acc7-3e8334bdc016")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::Button,
    "feb87e84-e0d2-428f-96ff-a123ac964f59")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::ImpulseButton,
    "7cd210d3-ebd1-4f71-9de6-cccfb639cbc3")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::HSVSlider,
    "8f38638e-9f9f-48b0-ae36-1cba86ef5703")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::XYSlider,
    "8093743c-584f-4bb9-97d4-6c7602f87116")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::XYZSlider,
    "bae00244-cd93-4893-a4ad-71489adb3fa1")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::XYSpinboxes,
    "0adbbdda-fda4-451e-91cc-1da731bde9d5")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::XYZSpinboxes,
    "377e8205-b442-4d54-8832-3761def522b2")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::MultiSlider,
    "25de6d71-1554-4fe1-bf3f-9cbf12bdadeb")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::Bargraph,
    "f6d740ce-acc0-44c0-932a-0a03345af84f")

namespace Process
{
struct SCORE_LIB_PROCESS_EXPORT FloatSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(FloatSlider)
  FloatSlider(
      float min, float max, float init, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~FloatSlider();

  auto getMin() const noexcept { return domain().get().convert_min<float>(); }
  auto getMax() const noexcept { return domain().get().convert_max<float>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;
  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT FloatKnob : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(FloatKnob)
  FloatKnob(
      float min, float max, float init, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~FloatKnob();

  auto getMin() const noexcept { return domain().get().convert_min<float>(); }
  auto getMax() const noexcept { return domain().get().convert_max<float>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT LogFloatSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(LogFloatSlider)
  LogFloatSlider(
      float min, float max, float init, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~LogFloatSlider();

  auto getMin() const noexcept { return domain().get().convert_min<float>(); }
  auto getMax() const noexcept { return domain().get().convert_max<float>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT IntSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(IntSlider)
  IntSlider(
      int min, int max, int init, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~IntSlider();

  auto getMin() const noexcept { return domain().get().convert_min<int>(); }
  auto getMax() const noexcept { return domain().get().convert_max<int>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT IntRangeSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(IntRangeSlider)
  IntRangeSlider(
      int min, int max, ossia::vec2f init, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~IntRangeSlider();

  auto getMin() const noexcept { return domain().get().convert_min<int>(); }
  auto getMax() const noexcept { return domain().get().convert_max<int>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT FloatRangeSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(FloatRangeSlider)
  FloatRangeSlider(
      float min, float max, ossia::vec2f init, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~FloatRangeSlider();

  auto getMin() const noexcept { return domain().get().convert_min<float>(); }
  auto getMax() const noexcept { return domain().get().convert_max<float>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT IntSpinBox : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(IntSpinBox)
  IntSpinBox(
      int min, int max, int init, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~IntSpinBox();

  auto getMin() const noexcept { return domain().get().convert_min<int>(); }
  auto getMax() const noexcept { return domain().get().convert_max<int>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT FloatSpinBox : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(FloatSpinBox)
  FloatSpinBox(
      float min, float max, float init, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~FloatSpinBox();

  auto getMin() const noexcept { return domain().get().convert_min<float>(); }
  auto getMax() const noexcept { return domain().get().convert_max<float>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};
struct SCORE_LIB_PROCESS_EXPORT TimeChooser : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(TimeChooser)
  TimeChooser(
      float min, float max, float init, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~TimeChooser();

  auto getMin() const noexcept { return domain().get().convert_min<float>(); }
  auto getMax() const noexcept { return domain().get().convert_max<float>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;
  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT Toggle : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(Toggle)
  Toggle(bool init, const QString& name, Id<Process::Port> id, QObject* parent);
  ~Toggle();

  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT ChooserToggle : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(ChooserToggle)
  ChooserToggle(
      QStringList alternatives, bool init, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~ChooserToggle();

  QStringList alternatives() const noexcept;
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT LineEdit : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(LineEdit)
  LineEdit(QString init, const QString& name, Id<Process::Port> id, QObject* parent);
  ~LineEdit();

  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT FileChooserBase : public Process::ControlInlet
{
  W_OBJECT(FileChooserBase)
public:
  FileChooserBase(
      QString init, QString filters, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~FileChooserBase();
  using Process::ControlInlet::ControlInlet;

  void setupExecution(ossia::inlet& inl) const noexcept override;
  const QString& filters() const noexcept { return m_filters; };
  void setFilters(QString nf) { m_filters = std::move(nf); }

  void enableFileWatch();
  void destroying() W_SIGNAL(destroying);

private:
  QString m_filters;
};
struct SCORE_LIB_PROCESS_EXPORT FileChooser : public FileChooserBase
{
  MODEL_METADATA_IMPL(FileChooser)
  W_OBJECT(FileChooser)
public:
  FileChooser(
      QString init, QString filters, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~FileChooser();
  using Process::FileChooserBase::FileChooserBase;
};

struct SCORE_LIB_PROCESS_EXPORT AudioFileChooser : public FileChooserBase
{
  MODEL_METADATA_IMPL(AudioFileChooser)
  W_OBJECT(AudioFileChooser)
public:
  AudioFileChooser(
      QString init, QString filters, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~AudioFileChooser();
  using Process::FileChooserBase::FileChooserBase;
};

struct SCORE_LIB_PROCESS_EXPORT ProgramEdit : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(ProgramEdit)
  ProgramEdit(QString init, const QString& name, Id<Process::Port> id, QObject* parent);
  ~ProgramEdit();

  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT ComboBox : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(ComboBox)
  std::vector<std::pair<QString, ossia::value>> alternatives;
  ComboBox(
      std::vector<std::pair<QString, ossia::value>> values, ossia::value init,
      const QString& name, Id<Process::Port> id, QObject* parent);
  ~ComboBox();

  const auto& getValues() const noexcept { return alternatives; }
  auto count() const noexcept { return alternatives.size(); }

  ComboBox(DataStream::Deserializer& vis, QObject* parent);
  ComboBox(JSONObject::Deserializer& vis, QObject* parent);
  ComboBox(DataStream::Deserializer&& vis, QObject* parent);
  ComboBox(JSONObject::Deserializer&& vis, QObject* parent);

  void setupExecution(ossia::inlet& inl) const noexcept override;
};

struct SCORE_LIB_PROCESS_EXPORT Enum : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(Enum)
  std::vector<QString> values;
  std::vector<QString> pixmaps;
  Enum(
      const std::vector<std::string>& dom, std::vector<QString> pixmaps,
      std::string init, const QString& name, Id<Process::Port> id, QObject* parent);

  Enum(
      const QStringList& values, std::vector<QString> pixmaps, std::string init,
      const QString& name, Id<Process::Port> id, QObject* parent);
  ~Enum();

  const std::vector<QString>& getValues() const { return values; }

  Enum(DataStream::Deserializer& vis, QObject* parent);
  Enum(JSONObject::Deserializer& vis, QObject* parent);
  Enum(DataStream::Deserializer&& vis, QObject* parent);
  Enum(JSONObject::Deserializer&& vis, QObject* parent);

  void setupExecution(ossia::inlet& inl) const noexcept override;
};

struct SCORE_LIB_PROCESS_EXPORT Button : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(Button)
  Button(const QString& name, Id<Process::Port> id, QObject* parent);
  ~Button();

  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT ImpulseButton : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(ImpulseButton)
  W_OBJECT(ImpulseButton)
public:
  ImpulseButton(const QString& name, Id<Process::Port> id, QObject* parent);
  ~ImpulseButton();

  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT HSVSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(HSVSlider)
  HSVSlider(
      ossia::vec4f init, const QString& name, Id<Process::Port> id, QObject* parent);
  ~HSVSlider();

  void setupExecution(ossia::inlet&) const noexcept override;
  auto getMin() const noexcept { return ossia::vec4f{0., 0., 0., 0.}; }
  auto getMax() const noexcept { return ossia::vec4f{1., 1., 1., 1.}; }
  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT XYSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(XYSlider)
  XYSlider(
      ossia::vec2f init, const QString& name, Id<Process::Port> id, QObject* parent);
  XYSlider(
      ossia::vec2f min, ossia::vec2f max, ossia::vec2f init, const QString& name,
      Id<Process::Port> id, QObject* parent);
  ~XYSlider();

  auto getMin() const noexcept { return domain().get().convert_min<ossia::vec2f>(); }
  auto getMax() const noexcept { return domain().get().convert_max<ossia::vec2f>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT XYZSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(XYZSlider)
  XYZSlider(
      ossia::vec3f init, const QString& name, Id<Process::Port> id, QObject* parent);
  XYZSlider(
      ossia::vec3f min, ossia::vec3f max, ossia::vec3f init, const QString& name,
      Id<Process::Port> id, QObject* parent);
  ~XYZSlider();

  auto getMin() const noexcept { return domain().get().convert_min<ossia::vec3f>(); }
  auto getMax() const noexcept { return domain().get().convert_max<ossia::vec3f>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT XYSpinboxes : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(XYSpinboxes)
  XYSpinboxes(
      ossia::vec2f init, const QString& name, Id<Process::Port> id, QObject* parent);
  XYSpinboxes(
      ossia::vec2f min, ossia::vec2f max, ossia::vec2f init, const QString& name,
      Id<Process::Port> id, QObject* parent);
  ~XYSpinboxes();

  auto getMin() const noexcept { return domain().get().convert_min<ossia::vec2f>(); }
  auto getMax() const noexcept { return domain().get().convert_max<ossia::vec2f>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT XYZSpinboxes : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(XYZSpinboxes)
  XYZSpinboxes(
      ossia::vec3f init, const QString& name, Id<Process::Port> id, QObject* parent);
  XYZSpinboxes(
      ossia::vec3f min, ossia::vec3f max, ossia::vec3f init, const QString& name,
      Id<Process::Port> id, QObject* parent);
  ~XYZSpinboxes();

  auto getMin() const noexcept { return domain().get().convert_min<ossia::vec3f>(); }
  auto getMax() const noexcept { return domain().get().convert_max<ossia::vec3f>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT MultiSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(MultiSlider)
  MultiSlider(
      ossia::value init, const QString& name, Id<Process::Port> id, QObject* parent);
  ~MultiSlider();

  ossia::value getMin() const noexcept;
  ossia::value getMax() const noexcept;
  void setupExecution(ossia::inlet& inl) const noexcept override;

  using Process::ControlInlet::ControlInlet;
};

// Outlets

struct SCORE_LIB_PROCESS_EXPORT Bargraph : public Process::ControlOutlet
{
  MODEL_METADATA_IMPL(Bargraph)
  Bargraph(
      float min, float max, float init, const QString& name, Id<Process::Port> id,
      QObject* parent);
  ~Bargraph();

  auto getMin() const noexcept { return domain().get().convert_min<float>(); }
  auto getMax() const noexcept { return domain().get().convert_max<float>(); }

  using Process::ControlOutlet::ControlOutlet;
};

} // namespace Process
