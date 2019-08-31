#include "view.hpp"

#include <Process/Dataflow/Port.hpp>
#include <core/document/DocumentModel.hpp>
#include <wobjectimpl.h>
#include <QMimeData>
#include <QVBoxLayout>
W_OBJECT_IMPL(fxd::DocumentView)
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
  , context{ctx, m_focus}
  , m_widget{}
  , m_scene{}
  , m_view{&m_scene, nullptr}
{
  auto& skin = score::Skin::instance();
  m_view.setBackgroundBrush(skin.DarkGray);
  m_view.setMinimumSize(400, 400);
  m_view.setSceneRect({-100., -100., 1000., 1000.});

  auto lay = new QHBoxLayout{&m_widget};

  lay->addWidget(&m_list);
  lay->addWidget(&m_view);

  for(auto& widget : fxModel(ctx).widgets)
    on_widgetAdded(widget);
  fxModel(ctx).widgets.mutable_added.connect<&DocumentView::on_widgetAdded>(*this);
  fxModel(ctx).widgets.removing.connect<&DocumentView::on_widgetRemoved>(*this);

  m_rect.setBrush(skin.Transparent1);
  m_rect.setPen(QPen(skin.Base1.color(), 3));

  auto& model = ctx.model<fxd::DocumentModel>();
  con(model, &DocumentModel::rectSizeChanged,
      this, [this] (const QSizeF& sz) { m_rect.setRect(0, 0, sz.width(), sz.height()); });
  const QSizeF& sz = model.rectSize();
  m_rect.setRect(0, 0, sz.width(), sz.height());

  m_scene.addItem(&m_rect);
  m_view.centerOn(&m_rect);
}

void DocumentView::on_widgetAdded(Widget& w)
{
  auto [it, ok] = this->m_widgets.insert({w.id(), WidgetUI{w}});
  WidgetUI& ui = it->second;

  ui.control = w.makeItem(*this);
  m_scene.addItem(ui.control);

  if(eggs::variants::apply([] (auto t) { return t.hasPort; }, w.data()))
  {
    auto& ui = it->second;
    ui.port = new Dataflow::PortItem{global_control_port, context, nullptr};
    ui.port->setZValue(40);
    ui.port->setPos(ui.widget.portPos());
    createHandles(w, ui.port);
    m_scene.addItem(ui.port);
  }
}

void DocumentView::createHandles(Widget& w, score::SimpleTextItem* item)
{
  auto mhandle = new MoveHandle;
  mhandle->setZValue(50);
  mhandle->setPos(w.portPos());
  mhandle->setRect({QPointF{}, item->boundingRect().size()});
  con(w, &Widget::dataChanged, mhandle, [mhandle, item] {
    mhandle->setRect({QPointF{}, item->boundingRect().size()});
  });
  con(w, &Widget::posChanged, mhandle, [mhandle, item] (QPointF pos) {
    item->setPos(pos);
    mhandle->setPos(pos);
  });

  // Move handle
  {
    m_scene.addItem(mhandle);
    mhandle->setPos(w.pos());
    connect(mhandle, &MoveHandle::selectionChanged,
            this, [=, &w] (bool b) { handleItemSelection(w, b); });

    connect(mhandle, &MoveHandle::positionChanged,
            this, [=, &w] (QPointF pos) {
      if(w.resizing)
        return;
      this->context.dispatcher.submit<SetPos>(w, pos);
    });
    connect(mhandle, &MoveHandle::released,
            this, [=] { this->context.dispatcher.commit(); });
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
            this, [=, &w] (bool b) { handleItemSelection(w, b); });

    connect(mhandle, &MoveHandle::positionChanged,
            this, [=, &w] (QPointF pos) {
      if(w.moving)
        return;

     this->context.dispatcher.submit<SetPortPos>(w, pos);
    });
    connect(mhandle, &MoveHandle::released,
            this, [=] {
      this->context.dispatcher.commit();
    });
  }
}

void DocumentView::handleItemSelection(Widget& w, bool b)
{
  if(b)
  {
    context.selectionStack.pushNewSelection({&w});
  }
  else
  {
    context.selectionStack.deselectObjects({&w});
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
