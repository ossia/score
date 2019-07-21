#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <score/plugins/documentdelegate/DocumentDelegatePresenter.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/widgets/Layout.hpp>
#include <score/selection/Selectable.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/serialization/VariantSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/command/PropertyCommand.hpp>

#include <core/application/MinimalApplication.hpp>
#include <core/document/DocumentModel.hpp>
#include <score/plugins/PluginInstances.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <ossia/detail/apply.hpp>
#include <QDropEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <wobjectimpl.h>

#include <QCheckBox>
#include <QSpinBox>
#include <QInputDialog>
#include <QMessageBox>
#include <variant>
#include <verdigris>
#include <QFileDialog>

// TODO : implement remaining objects
// TODO : add ports
// TODO : add text and connect it
// TODO : add alternatives to inspector

namespace fxd
{
struct BackgroundWidget {
  static const constexpr QSizeF defaultSize{200., 200.};
  static const constexpr bool keepRatio{false};

  friend bool operator==(const BackgroundWidget& lhs, const BackgroundWidget& rhs) { return true; }
  friend bool operator!=(const BackgroundWidget& lhs, const BackgroundWidget& rhs) { return false; }
};
struct KnobWidget {
  static const constexpr QSizeF defaultSize{40., 40.};
  static const constexpr bool keepRatio{true};

  friend bool operator==(const KnobWidget& lhs, const KnobWidget& rhs) { return true; }
  friend bool operator!=(const KnobWidget& lhs, const KnobWidget& rhs) { return false; }
};
struct SliderWidget {
  static const constexpr QSizeF defaultSize{150., 15.};
  static const constexpr bool keepRatio{true};

  friend bool operator==(const SliderWidget& lhs, const SliderWidget& rhs) { return true; }
  friend bool operator!=(const SliderWidget& lhs, const SliderWidget& rhs) { return false; }
};
struct SpinboxWidget {
  static const constexpr QSizeF defaultSize{150., 15.};
  static const constexpr bool keepRatio{true};

  friend bool operator==(const SpinboxWidget& lhs, const SpinboxWidget& rhs) { return true; }
  friend bool operator!=(const SpinboxWidget& lhs, const SpinboxWidget& rhs) { return false; }
};
struct ComboWidget {
  static const constexpr QSizeF defaultSize{150., 15.};
  static const constexpr bool keepRatio{true};
  QStringList alternatives{"foo", "bar", "baz"};

  friend bool operator==(const ComboWidget& lhs, const ComboWidget& rhs) { return true; }
  friend bool operator!=(const ComboWidget& lhs, const ComboWidget& rhs) { return false; }
};
struct EnumWidget {
  static const constexpr QSizeF defaultSize{150., 50.};
  static const constexpr bool keepRatio{false};

  QStringList alternatives{"foo", "bar", "baz"};
  int rows{1};
  int columns{3};
  friend bool operator==(const EnumWidget& lhs, const EnumWidget& rhs) { return lhs.alternatives == rhs.alternatives && lhs.rows == rhs.rows && lhs.columns == rhs.columns; }
  friend bool operator!=(const EnumWidget& lhs, const EnumWidget& rhs) { return !(lhs == rhs); }
};
using WidgetImpl = eggs::variant<BackgroundWidget, KnobWidget, SliderWidget, SpinboxWidget, ComboWidget, EnumWidget>;
}
Q_DECLARE_METATYPE(fxd::WidgetImpl)
W_REGISTER_ARGTYPE(fxd::WidgetImpl)
JSON_METADATA(fxd::BackgroundWidget, "Background")
JSON_METADATA(fxd::KnobWidget, "Knob")
JSON_METADATA(fxd::SliderWidget, "Slider")
JSON_METADATA(fxd::SpinboxWidget, "SpinBox")
JSON_METADATA(fxd::EnumWidget, "Enum")
JSON_METADATA(fxd::ComboWidget, "Combo")
namespace fxd
{
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
  ~Widget() {}
  Widget(Id<Widget> c, QObject* parent) : IdentifiedObject{c, "Widget", parent}
  {
    selection.setParent(this);
    connect(this, &Widget::dataChanged,
            this, [this] (const WidgetImpl& t) {
      eggs::variants::apply([this] (const auto& t) {
        setSize(t.defaultSize);
      }, t);
    });
  }

  Widget(DataStream::Deserializer& vis, QObject* parent)
      : IdentifiedObject{vis, parent}
  {
    vis.writeTo(*this);
  }
  Widget(JSONObject::Deserializer& vis, QObject* parent)
      : IdentifiedObject{vis, parent}
  {
    vis.writeTo(*this);
  }
  Widget(DataStream::Deserializer&& vis, QObject* parent)
      : IdentifiedObject{vis, parent}
  {
    vis.writeTo(*this);
  }
  Widget(JSONObject::Deserializer&& vis, QObject* parent)
      : IdentifiedObject{vis, parent}
  {
    vis.writeTo(*this);
  }

  INLINE_PROPERTY_CREF(QString, name, {}, name, setName, nameChanged)
  INLINE_PROPERTY_VALUE(int, controlIndex, {}, controlIndex, setControlIndex, controlIndexChanged)
  INLINE_PROPERTY_VALUE(bool, enableText, {true}, enableText, setTextEnabled, textEnabledChanged)
  INLINE_PROPERTY_VALUE(QPointF, pos, {}, pos, setPos, posChanged)
  INLINE_PROPERTY_VALUE(QSizeF, size, {}, size, setSize, sizeChanged)
  INLINE_PROPERTY_VALUE(WidgetImpl, data, {}, data, setData, dataChanged)
  INLINE_PROPERTY_VALUE(QString, fxCode, {}, fxCode, setFxCode, fxCodeChanged)

  QString code(QString variable) const
  {
    struct
    {
      const Widget& self;
      QString& variable;

      QString code;
      void operator()(BackgroundWidget) noexcept
      {
        code += QString("    auto %1 = new score::BackgroundItem{&parent};\n")
              .arg(variable);
        code += QString("    c0_bg->setRect({%1, %2, %3, %4});\n")
              .arg(self.pos().x())
              .arg(self.pos().y())
              .arg(self.size().width())
              .arg(self.size().height())
            ;
      }
      void operator()(SliderWidget) noexcept
      {
        code += QString("    auto %1_item = %2("
                        "std::get<%3>(Metadata::controls), "
                        "%1, "
                        "parent, context, doc, portFactory);\n")
            .arg(variable)
            .arg(self.enableText() ? "makeControl" : "makeControlNoText")
            .arg(self.controlIndex());
        code += QString("    %1_item.root.setPos(%2, %3);\n")
            .arg(variable)
            .arg(self.pos().x())
            .arg(self.pos().y());
      }
      void operator()(KnobWidget) noexcept
      {
        operator()(SliderWidget{});
      }
      void operator()(SpinboxWidget) noexcept
      {
        operator()(SliderWidget{});
      }
      void operator()(ComboWidget) noexcept
      {
        operator()(SliderWidget{});
      }
      void operator()(EnumWidget) noexcept
      {

      }
      void operator()() noexcept
      {

      }
    } op{*this, variable};
    ossia::apply(op, data());
    return op.code;
  }

  bool keepRatio() const
  {
    return eggs::variants::apply([this] (const auto& t) {
      return t.keepRatio;
    }, m_data);
  }
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


  void loadCode(QString code)
  {
    QString controls_start = code.mid(code.indexOf("make_tuple(") + strlen("make_tuple(")).simplified();
    auto controls = controls_start.mid(0, controls_start.indexOf(";")).split(", Control::");
    int i = 0;
    for(QString& control : controls)
    {
      control.remove(QRegularExpression(".*::"));

      /**
         TODO :
         ::LineEdit(
         ::ChooserToggle{
       */


      auto widget = new Widget{getStrongId(widgets), this};
      widget->setControlIndex(i);
      widget->setName(QString("control_%1").arg(i));
      widget->setFxCode(control);
      if(control.contains("Knob"))
      {
        widget->setData(KnobWidget{});
      }
      else if(control.contains("Slider"))
      {
        widget->setData(SliderWidget{});
      }
      else if(control.contains("TempoChooser"))
      {
        widget->setData(SliderWidget{});
      }
      else if(control.contains("Spin"))
      {
        widget->setData(SpinboxWidget{});
      }
      else if(control.contains("MidiChannem"))
      {
        widget->setData(SpinboxWidget{});
      }
      else if(control.contains("TimeSigChooser"))
      {
        // TODO
      }
      else if(control.contains("WaveformChooser"))
      {
        widget->setData(EnumWidget{{"Sin","Triangle",  "Saw", "Square", "Sample & Hold", "Noise 1", "Noise 2", "Noise 3"}, 2, 4});
      }
      else if(control.contains("Chooser"))
      {
        widget->setData(ComboWidget{});
      }
      widgets.add(widget);
      i++;
    }
  }

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

auto& fxModel(const score::DocumentContext& ctx)
{
  return static_cast<DocumentModel&>(ctx.document.model().modelDelegate());
}

}
W_OBJECT_IMPL(fxd::Widget)

template <>
void DataStreamReader::read(const fxd::BackgroundWidget& w)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::BackgroundWidget& w)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::BackgroundWidget& w)
{
}

template <>
void JSONObjectWriter::write(fxd::BackgroundWidget& w)
{
}


template <>
void DataStreamReader::read(const fxd::SliderWidget& w)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::SliderWidget& w)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::SliderWidget& w)
{
}

template <>
void JSONObjectWriter::write(fxd::SliderWidget& w)
{
}


template <>
void DataStreamReader::read(const fxd::EnumWidget& w)
{
  m_stream << w.alternatives << w.columns << w.rows;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::EnumWidget& w)
{
  m_stream >> w.alternatives >> w.columns >> w.rows;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::EnumWidget& w)
{
  obj["Alternatives"] = toJsonArray(w.alternatives);
  obj["Columns"] = w.columns;
  obj["Rows"] = w.rows;
}

template <>
void JSONObjectWriter::write(fxd::EnumWidget& w)
{
  fromJsonArray(obj["Alternatives"].toArray(), w.alternatives);
  w.columns = obj["Columns"].toInt();
  w.rows = obj["Rows"].toInt();
}


template <>
void DataStreamReader::read(const fxd::KnobWidget& w)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::KnobWidget& w)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::KnobWidget& w)
{
}

template <>
void JSONObjectWriter::write(fxd::KnobWidget& w)
{
}

template <>
void DataStreamReader::read(const fxd::SpinboxWidget& w)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::SpinboxWidget& w)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::SpinboxWidget& w)
{
}

template <>
void JSONObjectWriter::write(fxd::SpinboxWidget& w)
{
}

template <>
void DataStreamReader::read(const fxd::ComboWidget& w)
{
  m_stream << w.alternatives;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::ComboWidget& w)
{
  m_stream >> w.alternatives;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::ComboWidget& w)
{
  obj["Alternatives"] = toJsonArray(w.alternatives);
}

template <>
void JSONObjectWriter::write(fxd::ComboWidget& w)
{
  fromJsonArray(obj["Alternatives"].toArray(), w.alternatives);
}


template <>
void DataStreamReader::read(const fxd::Widget& w)
{
  m_stream << w.m_name << w.m_pos << w.m_size << w.m_data;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::Widget& w)
{
  m_stream >> w.m_name >> w.m_pos >> w.m_size >> w.m_data;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::Widget& w)
{
  obj["Name"] = w.m_name;
  obj["Pos"] = toJsonValue(w.m_pos);
  obj["Size"] = toJsonValue(w.m_size);
  obj["Data"] = toJsonObject(w.m_data);
}

template <>
void JSONObjectWriter::write(fxd::Widget& w)
{
  w.m_name = obj["Name"].toString();
  w.m_pos = fromJsonValue<QPointF>(obj["Pos"]);
  w.m_size = fromJsonValue<QSizeF>(obj["Size"]);
  w.m_data = fromJsonObject<decltype(w.m_data)>(obj["Data"]);
}

template <>
void DataStreamReader::read(const fxd::DocumentModel& doc)
{
  const auto& widgets = doc.widgets;
  m_stream << (int32_t)widgets.size();

  for (const auto& widget : widgets)
  {
    readFrom(widget);
  }

  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::DocumentModel& doc)
{
  int32_t widget_count;
  m_stream >> widget_count;

  for (; widget_count-- > 0;)
  {
    auto wmodel = new fxd::Widget{*this, &doc};
    doc.widgets.add(wmodel);
  }
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::DocumentModel& doc)
{
  obj["Widgets"] = toJsonArray(doc.widgets);
}

template <>
void JSONObjectWriter::write(fxd::DocumentModel& doc)
{
  const auto& widgets = obj["Widgets"].toArray();
  for (const auto& json_vref : widgets)
  {
    auto wmodel = new fxd::Widget{
        JSONObject::Deserializer{json_vref.toObject()}, &doc};

    doc.widgets.add(wmodel);
  }
}


namespace fxd
{
class WidgetList : public QListWidget
{
public:
  WidgetList(QWidget* parent = nullptr) : QListWidget{parent}
  {
    setDragEnabled(true);
    setDragDropMode(DragOnly);
    setMaximumWidth(300);
    new QListWidgetItem{"Background", this};
    new QListWidgetItem{"Knob", this};
    new QListWidgetItem{"Slider", this};
    new QListWidgetItem{"SpinBox", this};
    new QListWidgetItem{"ComboBox", this};
    new QListWidgetItem{"Enum", this};
  }
};

const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"FxDesigner"};
  return key;
}
class AddWidget final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddWidget, "Add a widget")
public:
  AddWidget(const DocumentModel& model, Id<Widget> id, WidgetImpl type, QPointF pos)
      : m_document{model}, m_id{id}, m_type{type}, m_pos{pos}
  {
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    fxModel(ctx).widgets.remove(m_id);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    auto& doc = m_document.find(ctx);
    auto w = new Widget{m_id, &doc};
    w->setPos(m_pos);
    w->setData(m_type);
    doc.widgets.add(w);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override {}
  void deserializeImpl(DataStreamOutput& s) override {}

private:
  Path<DocumentModel> m_document;
  Id<Widget> m_id;
  WidgetImpl m_type{};
  QPointF m_pos{};
};

WidgetImpl implForIndex(int row)
{
  switch(row)
  {
    case 0:
      return BackgroundWidget{};
    case 1:
      return KnobWidget{};
    case 2:
      return SliderWidget{};
    case 3:
      return SpinboxWidget{};
    case 4:
      return ComboWidget{};
    case 5:
      return EnumWidget{};
    default:
      return {};
  }
}
class View : public QGraphicsView
{
public:
  View(QGraphicsScene* s, QWidget* p) : QGraphicsView{s, p}
  {
    setAcceptDrops(true);
  }
  void dragEnterEvent(QDragEnterEvent* event) override { event->accept(); }
  void dragMoveEvent(QDragMoveEvent* event) override { event->accept(); }
  void dragLeaveEvent(QDragLeaveEvent* event) override { event->accept(); }
  void dropEvent(QDropEvent* ev) override
  {
    auto data = ev->mimeData();

    QByteArray mime = data->data("application/x-qabstractitemmodeldatalist");
    QDataStream s{&mime, QIODevice::ReadOnly};

    while(!s.atEnd())
    {
      int row{};
      int col{};
      QMap<int, QVariant> roles;
      s >> row >> col >> roles;

      auto& fxmodel = fxModel(score::GUIAppContext().documents.currentDocument()->context());
      auto cmd = new AddWidget(
          fxmodel,
          getStrongId(fxmodel.widgets),
          implForIndex(row),
          mapToScene(ev->pos()));

      score::GUIAppContext()
          .documents.currentDocument()
          ->commandStack()
          .redoAndPush(cmd);
    }

    ev->accept();
  }
};
  class MoveHandle : public QObject, public QGraphicsItem
  {
    W_OBJECT(MoveHandle)
  public:
    MoveHandle()
    {
      setCursor(Qt::SizeAllCursor);
      setFlag(ItemIsMovable, true);
      setFlag(ItemIsSelectable, true);
      setFlag(ItemSendsScenePositionChanges, true);
    }
    void setRect(QRectF r)
    {
      prepareGeometryChange();
      m_rect = r;
      update();
    }

    QRectF boundingRect() const override
    {
      return m_rect;
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
      painter->setPen(Qt::red);
      painter->setBrush(Qt::transparent);
      painter->drawRect(m_rect);
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
      switch(change)
      {
        case GraphicsItemChange::ItemScenePositionHasChanged:
          positionChanged(value.toPointF());
          break;
        case GraphicsItemChange::ItemSelectedHasChanged:
          selectionChanged(value.toBool());
          break;
      }
      return QGraphicsItem::itemChange(change, value);
    }

    void positionChanged(QPointF p) W_SIGNAL(positionChanged, p)
    void selectionChanged(bool b) W_SIGNAL(selectionChanged, b)
    private:
      QRectF m_rect;
  };
  class ResizeHandle : public QObject, public QGraphicsItem
  {
    W_OBJECT(ResizeHandle)
  public:
    ResizeHandle()
    {
      setCursor(Qt::SizeFDiagCursor);
      setFlag(ItemIsSelectable, true);
      setFlag(ItemIsMovable, true);
      setFlag(ItemSendsScenePositionChanges, true);
    }
    QRectF boundingRect() const override
    {
      return {0., 0., 12., 12.};
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
      painter->setPen(Qt::white);
      //painter->drawLine(QPointF{11., 0.}, QPointF{12., 12.});
      //painter->drawLine(QPointF{0., 11.}, QPointF{12., 12.});
      painter->drawRect(boundingRect());
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
      switch(change)
      {
        case GraphicsItemChange::ItemScenePositionHasChanged:
          positionChanged(value.toPointF());
          break;
      }
      return QGraphicsItem::itemChange(change, value);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
    {
      QGraphicsItem::mouseReleaseEvent(ev);
      released();
    }

    void positionChanged(QPointF p) W_SIGNAL(positionChanged, p)
    void released() W_SIGNAL(released)
  };

class DocumentView final : public score::DocumentDelegateView, public Nano::Observer
{
  W_OBJECT(DocumentView)

public:
  DocumentView(const score::DocumentContext& ctx, QObject* parent)
      : score::DocumentDelegateView{parent}
      , m_widget{}
      , m_scene{}
      , m_view{&m_scene, nullptr}
  {
    auto& skin = score::Skin::instance();
    m_view.setBackgroundBrush(skin.Background2);
    m_view.setMinimumSize(400, 400);
    m_view.setSceneRect({-100., -100., 1000., 1000.});

    auto lay = new QHBoxLayout{&m_widget};

    lay->addWidget(&m_list);

    lay->addWidget(&m_view);

    for(auto& widget : fxModel(ctx).widgets)
      on_widgetAdded(widget);
    fxModel(ctx).widgets.mutable_added.connect<&DocumentView::on_widgetAdded>(*this);

    auto item = new QGraphicsRectItem;
    item->setRect(0, 0, 800, 400);
    item->setBrush(skin.Transparent1);
    item->setPen(QPen(skin.Base1.color(), 3));
    m_scene.addItem(item);
    m_view.centerOn(item);
  }

  void on_widgetAdded(Widget& w)
  {
    QGraphicsItem* item{};
    struct
    {
      Widget& w;
      QGraphicsItem*& item;
      DocumentView& self;
      void operator()(const BackgroundWidget&)
      {
        auto it = new score::BackgroundItem;
        it->setZValue(1);
        item = it;
        it->setRect(QRectF{QPointF{}, w.size()});
        self.createHandles(w, it);
      }

      void operator()(const KnobWidget&)
      {
        auto it = new score::QGraphicsKnob{nullptr};
        it->setZValue(10);
        item = it;
        it->setRect(QRectF{QPointF{}, w.size()});
        self.createHandles(w, it);
      }

      void operator()(const SliderWidget&)
      {
        auto it = new score::QGraphicsSlider{nullptr};
        it->setZValue(10);
        item = it;
        it->setRect(QRectF{QPointF{}, w.size()});
        self.createHandles(w, it);
      }
      void operator()(const SpinboxWidget&)
      {
        auto it = new score::QGraphicsIntSlider{nullptr};
        it->setZValue(10);
        item = it;
        it->setRect(QRectF{QPointF{}, w.size()});
        self.createHandles(w, it);
      }
      void operator()(const ComboWidget&)
      {
        auto it = new score::QGraphicsComboSlider{nullptr};
        it->setZValue(10);
        item = it;
        it->setRect(QRectF{QPointF{}, w.size()});
        self.createHandles(w, it);
      }

      void operator()(const EnumWidget& e)
      {
        auto it = new score::QGraphicsEnum{e.alternatives, nullptr};
        it->rows = e.rows;
        it->columns = e.columns;
        it->setZValue(10);
        item = it;
        it->setRect(QRectF{QPointF{}, w.size()});
        self.createHandles(w, it);
      }
    } vis{w, item, *this};

    eggs::variants::apply(vis, w.data());
    m_scene.addItem(item);
    item->setPos(w.pos());
  }

template<typename T>
void createHandles(Widget& w, T* item)
{
  auto mhandle = new MoveHandle;
  mhandle->setZValue(50);
  mhandle->setRect({QPointF{}, w.size()});
  auto rhandle = new ResizeHandle;
  rhandle->setZValue(100);

  // Move handle
  {
    m_scene.addItem(mhandle);
    mhandle->setPos(w.pos());
    connect(mhandle, &MoveHandle::selectionChanged,
            this, [=, &w] (bool b) {
      if(b)
      {
      score::GUIAppContext()
          .documents.currentDocument()
          ->selectionStack().pushNewSelection({&w});
      }

    });
    connect(mhandle, &MoveHandle::positionChanged,
            this, [=, &w] (QPointF pos) {
      if(w.resizing)
        return;
      w.moving = true;
      w.setPos(pos);
      item->setPos(pos);
      rhandle->setPos(w.pos() + QPointF{w.size().width(), w.size().height()});
      w.moving = false;
    });
  }

  // Resize handle
  {
    m_scene.addItem(rhandle);
    rhandle->setPos(w.pos() + QPointF{w.size().width(), w.size().height()});
    connect(rhandle, &ResizeHandle::released,
            this, [=, &w] {
      w.moving = true;
      rhandle->setPos(w.pos() + QPointF{w.size().width(), w.size().height()});
      w.moving = false;
    });
    connect(rhandle, &ResizeHandle::positionChanged,
            this, [=, &w] (QPointF pos) {
      if(w.moving)
        return;
      auto p = item->mapFromScene(pos);
      if(p.x() < 10 || p.y() < 10)
        return;

      w.resizing = true;
      if(w.keepRatio())
      {
        double curRatio = w.size().width() / w.size().height();
        p = {curRatio * p.y(), p.y()};
      }

      w.setSize({p.x(), p.y()});
      item->setRect({QPointF{}, p});
      mhandle->setRect({QPointF{}, p});
      w.resizing = false;
    });
  }
}
  ~DocumentView() override {}

  QWidget* getWidget() override { return &m_widget; }

  QWidget m_widget;
  WidgetList m_list;
  QGraphicsScene m_scene;
  View m_view;
  IdContainer<QGraphicsItem, Widget> m_widgets;
};

class DocumentPresenter final : public score::DocumentDelegatePresenter
{
  W_OBJECT(DocumentPresenter)
  friend class DisplayedElementsPresenter;

public:
  DocumentPresenter(
      const score::DocumentContext& ctx,
      score::DocumentPresenter* parent_presenter,
      const DocumentModel& delegate_model,
      DocumentView& delegate_view)
      : DocumentDelegatePresenter{parent_presenter,
                                  delegate_model,
                                  delegate_view}
  {
  }

  ~DocumentPresenter() override {}

  void setNewSelection(const Selection& old, const Selection& s) override
  {
    for (auto& obj : m_curSel)
    {
      if (obj && !s.contains(obj))
      {
        auto c = obj->findChild<Selectable*>();
        if (c)
        {
          c->set_impl(false);
        }
      }
    }

    for (auto& obj : s)
    {
      if (obj)
      {
        auto c = obj->findChild<Selectable*>();
        if (c)
        {
          c->set_impl(true);
        }
      }
    }
    m_curSel = s;
  }

private:
  Selection m_curSel;
};

class DocumentFactory final : public score::DocumentDelegateFactory
{
  SCORE_CONCRETE("bb7d624a-7e0d-41fa-b6ff-43f35b32c07d")

  score::DocumentDelegateView*
  makeView(const score::DocumentContext& ctx, QObject* parent) override
  {
    return new DocumentView{ctx, parent};
  }

  score::DocumentDelegatePresenter* makePresenter(
      const score::DocumentContext& ctx,
      score::DocumentPresenter* parent_presenter,
      const score::DocumentDelegateModel& model,
      score::DocumentDelegateView& view) override
  {
    return new DocumentPresenter{ctx,
                                 parent_presenter,
                                 static_cast<const DocumentModel&>(model),
                                 static_cast<DocumentView&>(view)};
  }

  void make(
      const score::DocumentContext& ctx,
      score::DocumentDelegateModel*& ptr,
      score::DocumentModel* parent) override
  {
    std::allocator<DocumentModel> alloc;
    auto res = alloc.allocate(1);
    ptr = res;
    alloc.construct(res, ctx, parent);
  }

  void load(
      const VisitorVariant& vis,
      const score::DocumentContext& ctx,
      score::DocumentDelegateModel*& ptr,
      score::DocumentModel* parent) override
  {
    std::allocator<DocumentModel> alloc;
    auto res = alloc.allocate(1);
    ptr = res;
    score::deserialize_dyn(vis, [&](auto&& deserializer) {
      alloc.construct(res, deserializer, ctx, parent);
      return ptr;
    });
  }
};

/// Code display
void showCode()
{
  auto doc = score::GUIAppContext().documents.currentDocument();
  if(!doc)
    return;

  auto& fx = fxModel(doc->context());

  QString code;
  int unnamed_count = 0;

  for(const Widget& widget : fx.widgets)
  {
    QString name = widget.name();
    if(name.isEmpty())
      name = QString("item%1").arg(unnamed_count++);

    code += widget.code(name) + "\n";
  }

  QInputDialog::getMultiLineText(nullptr, "code", "", code);
}

/// Inspectors

#define INSPECTOR_FACTORY(Factory, InspModel, InspObject, Uuid)               \
  class Factory final : public Inspector::InspectorWidgetFactory              \
  {                                                                           \
    SCORE_CONCRETE(Uuid)                                                      \
  public:                                                                     \
    Factory() = default;                                                      \
                                                                              \
    QWidget* make(                                                            \
        const InspectedObjects& sourceElements,                               \
        const score::DocumentContext& doc,                                    \
        QWidget* parent) const override                                       \
    {                                                                         \
      return new InspObject{                                                  \
          safe_cast<const InspModel&>(*sourceElements.first()), doc, parent}; \
    }                                                                         \
                                                                              \
    bool matches(const InspectedObjects& objects) const override              \
    {                                                                         \
      return dynamic_cast<const InspModel*>(objects.first());                 \
    }                                                                         \
  };

template <typename T, typename U>
struct WidgetFactory
{
  const U& object;
  const score::DocumentContext& ctx;
  Inspector::Layout& layout;
  QWidget* parent;

  QString prettyText(QString str)
  {
    SCORE_ASSERT(!str.isEmpty());
    str[0] = str[0].toUpper();
    for (int i = 1; i < str.size(); i++)
    {
      if (str[i].isUpper())
      {
        str.insert(i, ' ');
        i++;
      }
    }
    return QObject::tr(str.toUtf8().constData());
  }

  void setup()
  {
    if (auto widg = make((object.*(T::get()))()); widg != nullptr)
      layout.addRow(prettyText(T::name), (QWidget*)widg);
  }

  void setup(QString txt)
  {
    if (auto widg = make((object.*(T::get()))()); widg != nullptr)
      layout.addRow(txt, (QWidget*)widg);
  }

  template <typename X>
  auto make(X*) = delete;

  auto make(bool c)
  {
    using cmd =
        typename score::PropertyCommand_T<T>::template command<void>::type;
    auto cb = new QCheckBox{parent};
    cb->setCheckState(c ? Qt::Checked : Qt::Unchecked);

    QObject::connect(
        cb,
        &QCheckBox::stateChanged,
        parent,
        [& object = this->object, &ctx = this->ctx](int state) {
          bool cur = (object.*(T::get()))();
          if ((state == Qt::Checked && !cur)
              || (state == Qt::Unchecked && cur))
          {
            CommandDispatcher<> disp{ctx.commandStack};
            disp.submit(
                new cmd{object, state == Qt::Checked ? true : false});
          }
        });

    QObject::connect(&object, T::notify(), parent, [cb](bool cs) {
      if (cs != cb->checkState())
        cb->setCheckState(cs ? Qt::Checked : Qt::Unchecked);
    });
    return cb;
  }

  auto make(int c)
  {
    using cmd =
        typename score::PropertyCommand_T<T>::template command<void>::type;
    auto sb = new QSpinBox{parent};
    sb->setValue(c);
    sb->setRange(0, 100);

    QObject::connect(
        sb,
        SignalUtils::QSpinBox_valueChanged_int(),
        parent,
        [& object = this->object, &ctx = this->ctx](int state) {
          int cur = (object.*(T::get()))();
          if (state != cur)
          {
            CommandDispatcher<> disp{ctx.commandStack};
            disp.submit(new cmd{object, state});
          }
        });

    QObject::connect(&object, T::notify(), parent, [sb](int cs) {
      if (cs != sb->value())
        sb->setValue(cs);
    });
    return sb;
  }

  auto make(const QString& cur)
  {
    using cmd =
        typename score::PropertyCommand_T<T>::template command<void>::type;
    auto l = new QLineEdit{cur, parent};
    l->setSizePolicy(QSizePolicy::Policy{}, QSizePolicy::MinimumExpanding);
    QObject::connect(
        l,
        &QLineEdit::editingFinished,
        parent,
        [l, &object = this->object, &ctx = this->ctx]() {
          const auto& cur = (object.*(T::get()))();
          if (l->text() != cur)
          {
            CommandDispatcher<> disp{ctx.commandStack};
            disp.submit(new cmd{object, l->text()});
          }
        });

    QObject::connect(&object, T::notify(), parent, [l](const QString& txt) {
      if (txt != l->text())
        l->setText(txt);
    });
    return l;
  }
};

template <typename T, typename U>
auto setup_inspector(
    T,
    const U& sc,
    const score::DocumentContext& doc,
    Inspector::Layout& layout,
    QWidget* parent)
{
  WidgetFactory<T, U>{sc, doc, layout, parent}.setup();
}

template <typename T, typename U>
auto setup_inspector(
    T,
    QString txt,
    const U& sc,
    const score::DocumentContext& doc,
    Inspector::Layout& layout,
    QWidget* parent)
{
  WidgetFactory<T, U>{sc, doc, layout, parent}.setup(txt);
}

class WidgetInspector : public QWidget
{
public:
  WidgetInspector(
      const Widget& sc,
      const score::DocumentContext& doc,
      QWidget* parent)
      : QWidget{parent}
  {
    auto lay = new Inspector::Layout{this};
    lay->addWidget(new QLabel{sc.fxCode()});
    setup_inspector(Widget::p_name{}, sc, doc, *lay, this);
    setup_inspector(Widget::p_enableText{}, sc, doc, *lay, this);
    setup_inspector(Widget::p_controlIndex{}, sc, doc, *lay, this);
  }
};

INSPECTOR_FACTORY(
    WidgetInspectorFactory,
    fxd::Widget,
    fxd::WidgetInspector,
    "a703d205-3488-4f0e-817d-19fac52e2046")
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
    SetEnableText,
    Widget::p_enableText,
    "Set text")

SCORE_COMMAND_DECL_T(fxd::SetEnableText)

W_OBJECT_IMPL(fxd::MoveHandle)
W_OBJECT_IMPL(fxd::ResizeHandle)
W_OBJECT_IMPL(fxd::DocumentModel)
W_OBJECT_IMPL(fxd::DocumentPresenter)
W_OBJECT_IMPL(fxd::DocumentView)

class score_addon_fxdesigner final
    : public score::Plugin_QtInterface,
      public score::FactoryInterface_QtInterface,
      public score::CommandFactory_QtInterface,
      public score::ApplicationPlugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "7adcc7af-7eb7-4486-bc91-28f57171e41c")

public:
  score_addon_fxdesigner() {}
  ~score_addon_fxdesigner() override {}

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& key) const override
  {
    return instantiate_factories<
        score::ApplicationContext,
        FW<score::DocumentDelegateFactory, fxd::DocumentFactory>
        , FW<Inspector::InspectorWidgetFactory, fxd::WidgetInspectorFactory>
        >(ctx, key);
  }

  std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands()
  {
    std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
        fxd::CommandFactoryName(), CommandGeneratorMap{}};

    ossia::for_each_type<fxd::AddWidget>(
        score::commands::FactoryInserter{cmds.second});

    return cmds;
  }
};

int main(int argc, char** argv)
{
  score_addon_fxdesigner addon;
  score::staticPlugins().push_back(&addon);

  QCoreApplication::setOrganizationName("OSSIA");
  QCoreApplication::setOrganizationDomain("ossia.io");
  QCoreApplication::setApplicationName("FX Designer");

  QSettings s;
  s.setValue("PluginSettings/Whitelist",
             QStringList{"score_plugin_inspector"}
             );

  score::MinimalGUIApplication app{argc, argv};

  auto toolbar = app.view().findChildren<QToolBar*>(QString{}, Qt::FindDirectChildrenOnly).first();

  {
    auto act = new QAction(QObject::tr("Load code"));
    toolbar->addAction(act);
    QObject::connect(act, &QAction::triggered, [] {

      QFileDialog::getOpenFileContent("Header (*.hpp, *.h)", [] (const QString& name, const QByteArray& data) {

        fxd::DocumentModel& fxmodel = fxd::fxModel(score::GUIAppContext().documents.currentDocument()->context());
        fxmodel.loadCode(QString::fromUtf8(data));
      });
    });
  }

  {
    auto act = new QAction(QObject::tr("Print code"));
    toolbar->addAction(act);
    QObject::connect(act, &QAction::triggered, fxd::showCode);
  }

  QFile f(
      "/home/jcelerier/score/src/plugins/score-plugin-scenario/Scenario/"
      "resources/DefaultSkin.json");
  f.open(QIODevice::ReadOnly);
  auto skin = QJsonDocument::fromJson(f.readAll()).object();
  score::Skin::instance().load(skin);
  app.m_presenter->documentManager().newDocument(
      score::GUIAppContext(),
      Id<score::DocumentModel>{score::random_id_generator::getRandomId()},
      *app.m_presenter->applicationComponents()
           .interfaces<score::DocumentDelegateList>()
           .begin());

  app.exec();
}
