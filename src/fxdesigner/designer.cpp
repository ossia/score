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
#include <score/selection/Selectable.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/application/MinimalApplication.hpp>
#include <core/document/DocumentModel.hpp>
#include <score/plugins/PluginInstances.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QDropEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QListWidget>

#include <wobjectimpl.h>

#include <verdigris>
namespace fxd
{
enum WidgetType
{
  Background,
  Knob,
  Slider,
  Enum
};

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

  QString name() const { return m_name; }

  int widgetType() const { return m_widgetType; }

  QPointF pos() const { return m_pos; }

  QSizeF size() const { return m_size; }

  QVariant data() const { return m_data; }

  void setName(QString name)
  {
    if (m_name == name)
      return;

    m_name = name;
    nameChanged(m_name);
  }

  void setWidgetType(int widgetType)
  {
    if (m_widgetType == widgetType)
      return;

    m_widgetType = widgetType;
    widgetTypeChanged(m_widgetType);
  }

  void setPos(QPointF pos)
  {
    if (m_pos == pos)
      return;

    m_pos = pos;
    posChanged(m_pos);
  }

  void setSize(QSizeF size)
  {
    if (m_size == size)
      return;

    m_size = size;
    sizeChanged(m_size);
  }

  void setData(QVariant data)
  {
    if (m_data == data)
      return;

    m_data = data;
    dataChanged(m_data);
  }

  void nameChanged(QString name) W_SIGNAL(nameChanged, name)

  void widgetTypeChanged(int widgetType)
  W_SIGNAL(widgetTypeChanged, widgetType)

  void posChanged(QPointF pos) W_SIGNAL(posChanged, pos)

  void sizeChanged(QSizeF size) W_SIGNAL(sizeChanged, size)

  void dataChanged(QVariant data)
  W_SIGNAL(dataChanged, data)

  private
    : W_PROPERTY(QString, name READ name WRITE setName NOTIFY nameChanged)
    W_PROPERTY(
    int,
      widgetType READ widgetType WRITE setWidgetType NOTIFY
      widgetTypeChanged)
    W_PROPERTY(
    QPointF,
      pos READ pos WRITE setPos NOTIFY posChanged)
    W_PROPERTY(
    QSizeF,
      size READ size WRITE setSize NOTIFY sizeChanged)
    W_PROPERTY(
    QVariant,
      data READ data WRITE setData NOTIFY dataChanged)

    QString m_name{};
  int m_widgetType{};
  QPointF m_pos{};
  QSizeF m_size{};
  QVariant m_data{};
};

class FxModel;
inline FxModel* fxModel{};
class FxModel : public QObject
{
public:
  FxModel() { fxModel = this; }

  score::EntityMap<Widget> widgets;
};

}
W_OBJECT_IMPL(fxd::Widget)

template <>
void DataStreamReader::read(const fxd::Widget& w)
{
  m_stream << w.m_name << w.m_widgetType << w.m_pos << w.m_size << w.m_data;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::Widget& w)
{
  m_stream >> w.m_name >> w.m_widgetType >> w.m_pos >> w.m_size >> w.m_data;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::Widget& w)
{
  obj["Name"] = w.m_name;
  obj["Type"] = w.m_widgetType;
  obj["Pos"] = toJsonValue(w.m_pos);
  obj["Size"] = toJsonValue(w.m_size);
  // TODO  obj["Data"] = toJsonValue(w.m_data);
}

template <>
void JSONObjectWriter::write(fxd::Widget& w)
{
  w.m_name = obj["Name"].toString();
  w.m_widgetType = obj["Type"].toInt();
  w.m_pos = fromJsonValue<QPointF>(obj["Pos"]);
  w.m_size = fromJsonValue<QSizeF>(obj["Size"]);
  // TODO  w.m_data      = fromJsonValue<QVariant>(obj["Data"]);
}

#include <QMimeData>

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
    auto bg = new QListWidgetItem{
        "Background",
        this,
        int(QListWidgetItem::UserType + WidgetType::Background)};
    auto knob = new QListWidgetItem{
        "Knob", this, int(QListWidgetItem::UserType + WidgetType::Knob)};
    auto slider = new QListWidgetItem{
        "Slider", this, int(QListWidgetItem::UserType + WidgetType::Slider)};
    auto enumer = new QListWidgetItem{
        "Enum", this, int(QListWidgetItem::UserType + WidgetType::Enum)};
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
  AddWidget(const FxModel& model, Id<Widget> id, WidgetType type, QPointF pos)
      : m_id{id}, m_type{type}, m_pos{pos}
  {
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    fxModel->widgets.remove(m_id);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    auto w = new Widget{m_id, fxModel};
    w->setPos(m_pos);
    w->setSize(QSizeF{100., 100.});
    w->setWidgetType(m_type);
    fxModel->widgets.add(w);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override {}
  void deserializeImpl(DataStreamOutput& s) override {}

private:
  Id<Widget> m_id;
  WidgetType m_type{};
  QPointF m_pos{};
};

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

      auto cmd = new AddWidget(
          *fxModel,
          getStrongId(fxModel->widgets),
          (WidgetType)row,
          mapToScene(ev->pos()));

      score::GUIAppContext()
          .documents.currentDocument()
          ->commandStack()
          .redoAndPush(cmd);
    }

    ev->accept();
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

private:
  const score::DocumentContext& m_context;

  FxModel m_model;
};

  class MoveHandle : public QObject, public QGraphicsItem
  {
    W_OBJECT(MoveHandle)
  public:
    MoveHandle()
    {
      setCursor(Qt::SizeAllCursor);
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
      painter->drawLine(QPointF{0., 6.}, QPointF{12., 6.});
      painter->drawLine(QPointF{6., 0.}, QPointF{6., 12.});
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

    void positionChanged(QPointF p) W_SIGNAL(positionChanged, p)
  };
  class ResizeHandle : public QObject, public QGraphicsItem
  {
    W_OBJECT(ResizeHandle)
  public:
    ResizeHandle()
    {
      setCursor(Qt::SizeAllCursor);
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
      painter->drawLine(QPointF{11., 0.}, QPointF{12., 12.});
      painter->drawLine(QPointF{0., 11.}, QPointF{12., 12.});
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

    void positionChanged(QPointF p) W_SIGNAL(positionChanged, p)
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

    fxModel->widgets.mutable_added.connect<&DocumentView::on_widgetAdded>(
        *this);

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
    switch (w.widgetType())
    {
      case WidgetType::Background:
      {
        auto it = new score::BackgroundItem;
        it->setZValue(1);
        item = it;
        it->setRect(QRectF{QPointF{}, w.size()});
        createHandles(w, it);
        break;
      }
      case WidgetType::Knob:
      {
        auto it = new score::QGraphicsKnob{nullptr};
        it->setZValue(10);
        item = it;
        it->setRect(QRectF{QPointF{}, w.size()});
        createHandles(w, it);
        break;
      }
      case WidgetType::Slider:
      {
        auto it = new score::QGraphicsSlider{nullptr};
        it->setZValue(10);
        item = it;
        it->setRect(QRectF{QPointF{}, w.size()});
        createHandles(w, it);
        break;
      }
      case WidgetType::Enum:
      {
        auto it = new score::QGraphicsEnum{nullptr};
        it->setZValue(10);
        item = it;
        it->setRect(QRectF{QPointF{}, w.size()});
        createHandles(w, it);
        break;
      }
    }

    m_scene.addItem(item);
    item->setPos(w.pos());

  }

template<typename T>
void createHandles(Widget& w, T* item)
{
  auto mhandle = new MoveHandle;
  mhandle->setZValue(100);
  auto rhandle = new ResizeHandle;
  rhandle->setZValue(100);
  // Move handle
  {
    m_scene.addItem(mhandle);
    mhandle->setPos(w.pos() + QPointF{-3, -3});
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
    connect(rhandle, &ResizeHandle::positionChanged,
            this, [=, &w] (QPointF pos) {
      if(w.moving)
        return;
      w.resizing = true;
      auto p = item->mapFromScene(pos);
      w.setSize({p.x(), p.y()});
      item->setRect({QPointF{}, p});
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
}

W_OBJECT_IMPL(fxd::MoveHandle)
W_OBJECT_IMPL(fxd::ResizeHandle)
W_OBJECT_IMPL(fxd::DocumentModel)
W_OBJECT_IMPL(fxd::DocumentPresenter)
W_OBJECT_IMPL(fxd::DocumentView)
template <>
void DataStreamReader::read(const fxd::DocumentModel& obj)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::DocumentModel& doc)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::DocumentModel& doc)
{
}

template <>
void JSONObjectWriter::write(fxd::DocumentModel& doc)
{
}

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
        FW<score::DocumentDelegateFactory, fxd::DocumentFactory>>(ctx, key);
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
  score::MinimalGUIApplication app{argc, argv};

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
