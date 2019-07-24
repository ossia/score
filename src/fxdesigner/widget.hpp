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
#include <ossia/detail/apply.hpp>
#include <variant>
#include <verdigris>
#include <QGraphicsItem>
#include <QRegularExpression>

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
  static const constexpr bool keepRatio{true};
  static const constexpr bool hasPort{true};

  friend bool operator==(const KnobWidget& lhs, const KnobWidget& rhs) { return true; }
  friend bool operator!=(const KnobWidget& lhs, const KnobWidget& rhs) { return false; }
};
struct SliderWidget {
  static const constexpr QSizeF defaultSize{50., 30.};
  static const constexpr bool keepRatio{true};
  static const constexpr bool hasPort{true};

  friend bool operator==(const SliderWidget& lhs, const SliderWidget& rhs) { return true; }
  friend bool operator!=(const SliderWidget& lhs, const SliderWidget& rhs) { return false; }
};
struct SpinboxWidget {
  static const constexpr QSizeF defaultSize{50., 30.};
  static const constexpr bool keepRatio{true};
  static const constexpr bool hasPort{true};

  friend bool operator==(const SpinboxWidget& lhs, const SpinboxWidget& rhs) { return true; }
  friend bool operator!=(const SpinboxWidget& lhs, const SpinboxWidget& rhs) { return false; }
};
struct ComboWidget {
  static const constexpr QSizeF defaultSize{50., 30.};
  static const constexpr bool keepRatio{true};
  static const constexpr bool hasPort{true};

  QStringList alternatives{"foo", "bar", "baz"};

  friend bool operator==(const ComboWidget& lhs, const ComboWidget& rhs) { return lhs.alternatives == rhs.alternatives; }
  friend bool operator!=(const ComboWidget& lhs, const ComboWidget& rhs) { return !(lhs == rhs); }
};
struct EnumWidget {
  static const constexpr QSizeF defaultSize{150., 50.};
  static const constexpr bool keepRatio{false};
  static const constexpr bool hasPort{true};

  QStringList alternatives{"foo", "bar", "baz"};
  int rows{1};
  int columns{3};

  friend bool operator==(const EnumWidget& lhs, const EnumWidget& rhs) { return lhs.alternatives == rhs.alternatives && lhs.rows == rhs.rows && lhs.columns == rhs.columns; }
  friend bool operator!=(const EnumWidget& lhs, const EnumWidget& rhs) { return !(lhs == rhs); }
};
using WidgetImpl = eggs::variant<
  BackgroundWidget
, TextWidget
, KnobWidget
, SliderWidget
, SpinboxWidget
, ComboWidget
, EnumWidget
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

  INLINE_PROPERTY_CREF(QString, name, {}, name, setName, nameChanged)
  INLINE_PROPERTY_VALUE(int, controlIndex, {}, controlIndex, setControlIndex, controlIndexChanged)
  INLINE_PROPERTY_VALUE(QPointF, pos, {}, pos, setPos, posChanged)
  INLINE_PROPERTY_VALUE(QPointF, portPos, {}, portPos, setPortPos, portPosChanged)
  INLINE_PROPERTY_VALUE(QSizeF, size, {}, size, setSize, sizeChanged)
  INLINE_PROPERTY_VALUE(WidgetImpl, data, {}, data, setData, dataChanged)
  INLINE_PROPERTY_VALUE(QString, fxCode, {}, fxCode, setFxCode, fxCodeChanged)
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

  score::EntityMap<Widget> widgets;
private:
  const score::DocumentContext& m_context;
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

}

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
