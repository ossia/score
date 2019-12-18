#pragma once
#include <score/tools/IdentifierGeneration.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <score/selection/Selectable.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/serialization/VariantSerialization.hpp>
#include <score/document/DocumentContext.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortType.hpp>
#include <ossia/detail/apply.hpp>
#include <variant>
#include <verdigris>
#include <QJsonDocument>
#include <QGraphicsItem>

namespace fxd {
struct BackgroundWidget {
  static const constexpr QSizeF defaultSize{200., 200.};
  static const constexpr bool keepRatio{false};
  static const constexpr bool hasPort{false};

  friend bool operator==(const BackgroundWidget& lhs, const BackgroundWidget& rhs) { return true; }
  friend bool operator!=(const BackgroundWidget& lhs, const BackgroundWidget& rhs) { return false; }
};
struct TextWidget {
  static const constexpr QSizeF defaultSize{50., 12.};
  static const constexpr bool keepRatio{false};
  static const constexpr bool hasPort{false};

  QString text{"Text"};

  friend bool operator==(const TextWidget& lhs, const TextWidget& rhs) { return lhs.text == rhs.text; }
  friend bool operator!=(const TextWidget& lhs, const TextWidget& rhs) { return lhs.text != rhs.text; }
};
struct KnobWidget {
  static const constexpr QSizeF defaultSize{40., 40.};
  static const constexpr QPointF defaultPortPos{0.5 * defaultSize.width() - 0.5 * 13, defaultSize.height() + 10.};
  static const constexpr bool keepRatio{true};
  static const constexpr bool hasPort{true};

  friend bool operator==(const KnobWidget& lhs, const KnobWidget& rhs) { return true; }
  friend bool operator!=(const KnobWidget& lhs, const KnobWidget& rhs) { return false; }
};
struct SliderWidget {
  static const constexpr QSizeF defaultSize{50., 30.};
  static const constexpr QPointF defaultPortPos{0.5 * defaultSize.width() - 0.5 * 13, defaultSize.height() + 10.};
  static const constexpr bool keepRatio{true};
  static const constexpr bool hasPort{true};

  friend bool operator==(const SliderWidget& lhs, const SliderWidget& rhs) { return true; }
  friend bool operator!=(const SliderWidget& lhs, const SliderWidget& rhs) { return false; }
};
struct SpinboxWidget {
  static const constexpr QSizeF defaultSize{50., 30.};
  static const constexpr QPointF defaultPortPos{0.5 * defaultSize.width() - 0.5 * 13, defaultSize.height() + 10.};
  static const constexpr bool keepRatio{true};
  static const constexpr bool hasPort{true};

  friend bool operator==(const SpinboxWidget& lhs, const SpinboxWidget& rhs) { return true; }
  friend bool operator!=(const SpinboxWidget& lhs, const SpinboxWidget& rhs) { return false; }
};
struct ComboWidget {
  static const constexpr QSizeF defaultSize{50., 30.};
  static const constexpr QPointF defaultPortPos{0.5 * defaultSize.width() - 0.5 * 13, defaultSize.height() + 10.};
  static const constexpr bool keepRatio{true};
  static const constexpr bool hasPort{true};

  QStringList alternatives{"foo", "bar", "baz"};

  friend bool operator==(const ComboWidget& lhs, const ComboWidget& rhs) { return lhs.alternatives == rhs.alternatives; }
  friend bool operator!=(const ComboWidget& lhs, const ComboWidget& rhs) { return !(lhs == rhs); }
};
struct EnumWidget {
  static const constexpr QSizeF defaultSize{150., 50.};
  static const constexpr QPointF defaultPortPos{0.5 * defaultSize.width() - 0.5 * 13, defaultSize.height() + 10.};
  static const constexpr bool keepRatio{false};
  static const constexpr bool hasPort{true};

  QStringList alternatives{"foo", "bar", "baz"};
  int rows{1};
  int columns{3};

  friend bool operator==(const EnumWidget& lhs, const EnumWidget& rhs) { return lhs.alternatives == rhs.alternatives && lhs.rows == rhs.rows && lhs.columns == rhs.columns; }
  friend bool operator!=(const EnumWidget& lhs, const EnumWidget& rhs) { return !(lhs == rhs); }
};
struct EmptyWidget {
  static const constexpr QSizeF defaultSize{10., 10.};
  static const constexpr bool keepRatio{true};
  static const constexpr bool hasPort{false};

  friend bool operator==(const EmptyWidget& lhs, const EmptyWidget& rhs) { return true; }
  friend bool operator!=(const EmptyWidget& lhs, const EmptyWidget& rhs) { return false; }
};
using WidgetImpl = eggs::variant<
  BackgroundWidget
, TextWidget
, KnobWidget
, SliderWidget
, SpinboxWidget
, ComboWidget
, EnumWidget
, EmptyWidget
>;
}
Q_DECLARE_METATYPE(fxd::WidgetImpl)
W_REGISTER_ARGTYPE(fxd::WidgetImpl)
JSON_METADATA(fxd::BackgroundWidget, "Background")
JSON_METADATA(fxd::TextWidget, "Text")
JSON_METADATA(fxd::KnobWidget, "Knob")
JSON_METADATA(fxd::SliderWidget, "Slider")
JSON_METADATA(fxd::SpinboxWidget, "SpinBox")
JSON_METADATA(fxd::EnumWidget, "Enum")
JSON_METADATA(fxd::ComboWidget, "Combo")
JSON_METADATA(fxd::EmptyWidget, "Empty")

namespace fxd
{
class DocumentView;
class Widget : public IdentifiedObject<Widget>
{
  W_OBJECT(Widget)
  SCORE_SERIALIZE_FRIENDS
public:
  Selectable selection;
  bool moving{};
  bool resizing{};

  Widget() = delete;
  Widget(const Widget&) = delete;
  ~Widget();
  Widget(Id<Widget> c, QObject* parent);

  Widget(DataStream::Deserializer& vis, QObject* parent);
  Widget(JSONObject::Deserializer& vis, QObject* parent);
  Widget(DataStream::Deserializer&& vis, QObject* parent);
  Widget(JSONObject::Deserializer&& vis, QObject* parent);

  QString code(QString variable) const;
  QGraphicsItem* makeItem(DocumentView& view);

  bool keepRatio() const;
  void setDefaultPortPos() noexcept;

private:
  QPointF m_pos{};
public:
  QPointF pos() const noexcept { return m_pos; }
  void setPos(QPointF pos)
  {
    if(moving)
      return;
    if (pos != m_pos)
    {
      moving = true;
      auto curpos = this->m_pos;

      m_pos = pos;

      posChanged(pos);

      auto portDelta = this->m_portPos - curpos;
      setPortPos(pos + portDelta);
      moving = false;
    }
  } W_SLOT(setPos)
  void posChanged(QPointF v) W_SIGNAL(posChanged, v)
  PROPERTY(QPointF, pos READ pos WRITE setPos NOTIFY posChanged)

private:
  QSizeF m_size{};
public:
  QSizeF size() const noexcept { return m_size; }
  void setSize(QSizeF size)
  {
    if(resizing)
      return;
    if (size != m_size)
    {
      resizing = true;
      m_size = size;
      sizeChanged(size);
      resizing = false;
    }
  } W_SLOT(setSize)
  void sizeChanged(QSizeF v) W_SIGNAL(sizeChanged, v)
  PROPERTY(QSizeF, size READ size WRITE setSize NOTIFY sizeChanged)

  INLINE_PROPERTY_CREF(QString, name, {}, name, setName, nameChanged)
  INLINE_PROPERTY_VALUE(int, controlIndex, {}, controlIndex, setControlIndex, controlIndexChanged)
  INLINE_PROPERTY_VALUE(QPointF, portPos, {}, portPos, setPortPos, portPosChanged)
  INLINE_PROPERTY_VALUE(WidgetImpl, data, {}, data, setData, dataChanged)
  INLINE_PROPERTY_VALUE(QString, fxCode, {}, fxCode, setFxCode, fxCodeChanged)
  INLINE_PROPERTY_VALUE(Process::PortType, portType, {}, portType, setPortType, portTypeChanged)

};


class DocumentModel final : public score::DocumentDelegateModel,
                            public Nano::Observer
{
  W_OBJECT(DocumentModel)
  SCORE_SERIALIZE_FRIENDS
public:
  DocumentModel(const score::DocumentContext& ctx, QObject* parent)
      : score::DocumentDelegateModel{Id<score::DocumentDelegateModel>(
                                         score::id_generator::getFirstId()),
                                     "FXDocument",
                                     parent}
      , m_context{ctx}
  {
  }


  void loadCode(QString code);

  template <typename Impl>
  DocumentModel(Impl& vis, const score::DocumentContext& ctx, QObject* parent)
      : score::DocumentDelegateModel{vis, parent}, m_context{ctx}
  {
    vis.writeTo(*this);
  }

  ~DocumentModel() override {}

  void serialize(const VisitorVariant& vis) const override
  {
    serialize_dyn(vis, *this);
  }

  QSizeF rectSize() const noexcept { return m_rectSize; }
  void setRectSize(QSizeF sz) { if(m_rectSize != sz) { m_rectSize = sz; rectSizeChanged(sz); } }
  void rectSizeChanged(QSizeF sz) W_SIGNAL(rectSizeChanged, sz)
  PROPERTY(QSizeF, rectSize READ rectSize WRITE setRectSize NOTIFY rectSizeChanged)

  score::EntityMap<Widget> widgets;
  void loadCode(QJsonDocument code);
  private:
  const score::DocumentContext& m_context;
  QSizeF m_rectSize{600, 300};
};

inline auto& fxModel(const score::DocumentContext& ctx)
{
  return static_cast<DocumentModel&>(ctx.document.model().modelDelegate());
}

inline const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"FxDesigner"};
  return key;
}


inline Process::ValueInlet global_message_port{Id<Process::Port>{1}, nullptr};
inline Process::MidiInlet global_midi_port{Id<Process::Port>{2}, nullptr};
inline Process::AudioInlet global_audio_port{Id<Process::Port>{3}, nullptr};
inline Process::ControlInlet global_control_port{Id<Process::Port>{4}, nullptr};
}
W_REGISTER_ARGTYPE(Process::PortType)

PROPERTY_COMMAND_T(
    fxd,
    SetName,
    Widget::p_name,
    "Set name")

SCORE_COMMAND_DECL_T(fxd::SetName)

PROPERTY_COMMAND_T(
    fxd,
    SetIndex,
    Widget::p_controlIndex,
    "Set index")

SCORE_COMMAND_DECL_T(fxd::SetIndex)

PROPERTY_COMMAND_T(
    fxd,
    SetData,
    Widget::p_data,
    "Set data")

SCORE_COMMAND_DECL_T(fxd::SetData)

PROPERTY_COMMAND_T(
    fxd,
    SetPos,
    Widget::p_pos,
    "Set pos")

SCORE_COMMAND_DECL_T(fxd::SetPos)

PROPERTY_COMMAND_T(
    fxd,
    SetSize,
    Widget::p_size,
    "Set size")

SCORE_COMMAND_DECL_T(fxd::SetSize)

PROPERTY_COMMAND_T(
    fxd,
    SetPortPos,
    Widget::p_portPos,
    "Set port pos")

SCORE_COMMAND_DECL_T(fxd::SetPortPos)

PROPERTY_COMMAND_T(
    fxd,
    SetRectSize,
    DocumentModel::p_rectSize,
    "Set size")

SCORE_COMMAND_DECL_T(fxd::SetRectSize)
