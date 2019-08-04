#pragma once
#include "widget.hpp"
#include "handles.hpp"

#include <Process/Dataflow/PortItem.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/PropertyCommand.hpp>
#include <QListWidget>
#include <QMimeData>
#include <QDropEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <score/graphics/TextItem.hpp>

namespace Dataflow
{
class PortItem;
}
namespace fxd {

class WidgetList : public QListWidget
{
public:
  WidgetList(QWidget* parent = nullptr);

  static
  WidgetImpl implForIndex(int row);
};

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

class View : public QGraphicsView
{
public:
  View(QGraphicsScene* s, QWidget* p);
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* ev) override;
};

class DocumentView final
    : public score::DocumentDelegateView
    , public Nano::Observer
{
public:
  struct WidgetUI
  {
    WidgetUI() = delete;
    WidgetUI(const WidgetUI&) = default;
    WidgetUI(WidgetUI&&) = default;
    WidgetUI& operator=(const WidgetUI&) = delete;
    WidgetUI& operator=(WidgetUI&&) = delete;
    Widget& widget;
    QGraphicsItem* control{};
    Dataflow::PortItem* port{};
    MoveHandle* portHandle{};
  };

  DocumentView(const score::DocumentContext& ctx, QObject* parent);
  void on_widgetAdded(Widget& w);

  void createHandles(Widget& w, score::SimpleTextItem* item);
  void createHandles(Widget& w, Dataflow::PortItem* item);

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
          context.selectionStack.pushNewSelection({&w});
        }
      });

      connect(mhandle, &MoveHandle::positionChanged,
              this, [=, &w] (QPointF pos) {
        if(w.resizing)
          return;
        w.moving = true;
        auto curpos = w.pos();

        w.setPos(pos);
        item->setPos(pos);
        auto& ui = this->m_widgets.at(w.id());
        if(ui.port)
        {
          auto portDelta = ui.portHandle->pos() - curpos;
          ui.port->setPos(pos + portDelta);
          ui.portHandle->setPos(ui.port->pos());
          w.setPortPos(ui.port->pos());
        }
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

  ~DocumentView() override;
  QWidget* getWidget() override;

  void on_widgetRemoved(const Widget& w);
  const score::DocumentContext& context;
private:
  std::unordered_map<Id<Widget>, WidgetUI> m_widgets;
  QWidget m_widget;
  WidgetList m_list;
  QGraphicsScene m_scene;
  View m_view;
};


}
