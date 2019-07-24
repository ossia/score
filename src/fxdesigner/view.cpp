#include "view.hpp"

#include <Process/Dataflow/Port.hpp>

namespace fxd
{

View::View(QGraphicsScene* s, QWidget* p) : QGraphicsView{s, p}
{
  setAcceptDrops(true);
}

void View::dragEnterEvent(QDragEnterEvent* event) { event->accept(); }

void View::dragMoveEvent(QDragMoveEvent* event) { event->accept(); }

void View::dragLeaveEvent(QDragLeaveEvent* event) { event->accept(); }

void View::dropEvent(QDropEvent* ev)
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

    auto doc = score::GUIAppContext().documents.currentDocument();
    auto& fxmodel = fxModel(doc->context());
    auto cmd = new AddWidget(
          fxmodel,
          getStrongId(fxmodel.widgets),
          WidgetList::implForIndex(row),
          mapToScene(ev->pos()));

    doc->commandStack()
        .redoAndPush(cmd);
  }

  ev->accept();
}




DocumentView::DocumentView(const score::DocumentContext& ctx, QObject* parent)
  : score::DocumentDelegateView{parent}
  , m_context{ctx}
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
  fxModel(ctx).widgets.removing.connect<&DocumentView::on_widgetRemoved>(*this);

  auto item = new QGraphicsRectItem;
  item->setRect(0, 0, 800, 400);
  item->setBrush(skin.Transparent1);
  item->setPen(QPen(skin.Base1.color(), 3));
  m_scene.addItem(item);
  m_view.centerOn(item);
}

void DocumentView::on_widgetAdded(Widget& w)
{
  WidgetUI ui{w};

  ui.control = w.makeItem(*this);
  m_scene.addItem(ui.control);
  ui.control->setPos(w.pos());

  auto [it, ok] = this->m_widgets.insert({w.id(), ui});
  if(eggs::variants::apply([] (auto t) { return t.hasPort; }, w.data()))
  {
    auto& ui = it->second;
    static Process::ControlInlet p{Id<Process::Port>{}, nullptr};
    ui.port = new Dataflow::PortItem{p, m_context, nullptr};
    ui.port->setZValue(40);
    ui.port->setPos(ui.control->pos() + QPointF{ui.control->boundingRect().width() / 2., ui.control->boundingRect().height() + 10});
    w.setPortPos(ui.port->pos());
    createHandles(w, ui.port);
    m_scene.addItem(ui.port);
  }
}

void DocumentView::createHandles(Widget& w, score::SimpleTextItem* item)
{
  auto mhandle = new MoveHandle;
  mhandle->setZValue(50);
  mhandle->setRect({QPointF{}, w.size()});

  // Move handle
  {
    m_scene.addItem(mhandle);
    mhandle->setPos(w.pos());
    connect(mhandle, &MoveHandle::selectionChanged,
            this, [=, &w] (bool b) {
      if(b)
      {
        m_context.selectionStack.pushNewSelection({&w});
      }
    });

    connect(mhandle, &MoveHandle::positionChanged,
            this, [=, &w] (QPointF pos) {
      if(w.resizing)
        return;
      w.moving = true;
      w.setPos(pos);
      item->setPos(pos);
      w.moving = false;
    });
  }
}

void DocumentView::createHandles(Widget& w, Dataflow::PortItem* item)
{
  auto& ui = m_widgets.at(w.id());
  auto mhandle = new MoveHandle;
  ui.portHandle = mhandle;
  mhandle->setZValue(50);
  mhandle->setRect(((QGraphicsItem*)item)->boundingRect());

  // Move handle
  {
    m_scene.addItem(mhandle);
    mhandle->setPos(item->pos());
    connect(mhandle, &MoveHandle::selectionChanged,
            this, [=, &w] (bool b) {
      if(b)
      {
        m_context.selectionStack.pushNewSelection({&w});
      }
    });

    connect(mhandle, &MoveHandle::positionChanged,
            this, [=, &w] (QPointF pos) {
      item->setPos(pos);
    });
  }
}
void DocumentView::on_widgetRemoved(const Widget& w)
{
}

DocumentView::~DocumentView() {}

QWidget* DocumentView::getWidget() { return &m_widget; }





WidgetList::WidgetList(QWidget* parent) : QListWidget{parent}
{
  setDragEnabled(true);
  setDragDropMode(DragOnly);
  setMaximumWidth(300);
  new QListWidgetItem{"Background", this};
  new QListWidgetItem{"Text", this};
  new QListWidgetItem{"Knob", this};
  new QListWidgetItem{"Slider", this};
  new QListWidgetItem{"SpinBox", this};
  new QListWidgetItem{"ComboBox", this};
  new QListWidgetItem{"Enum", this};
}

WidgetImpl WidgetList::implForIndex(int row)
{
  switch(row)
  {
  case 0:
    return BackgroundWidget{};
  case 1:
    return TextWidget{};
  case 2:
    return KnobWidget{};
    case 3:
      return SliderWidget{};
    case 4:
      return SpinboxWidget{};
    case 5:
      return ComboWidget{};
    case 6:
      return EnumWidget{};
    default:
      return {};
  }
}

}
