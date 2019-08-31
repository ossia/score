#pragma once
#include "widget.hpp"
#include "handles.hpp"

#include <Process/Dataflow/PortItem.hpp>
#include <Process/ProcessContext.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/tools/Bind.hpp>
#include <QListWidget>
#include <QDropEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <score/graphics/TextItem.hpp>
#include <Process/Focus/FocusDispatcher.hpp>

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
    w->setData(m_type);
    w->setDefaultPortPos();
    w->setPos(m_pos);
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
  W_OBJECT(DocumentView)
public:
  struct WidgetUI
  {
#if (__cplusplus <= 201703L) && !defined(_MSC_VER)
    WidgetUI() = delete;
    WidgetUI(const WidgetUI&) = default;
    WidgetUI(WidgetUI&&) = default;
    WidgetUI& operator=(const WidgetUI&) = delete;
    WidgetUI& operator=(WidgetUI&&) = delete;
#endif

    Widget& widget;
    QGraphicsItem* control{};
    Dataflow::PortItem* port{};
    MoveHandle* portHandle{};
  };

  DocumentView(const score::DocumentContext& ctx, QObject* parent);
  void on_widgetAdded(Widget& w);

  void createHandles(Widget& w, score::SimpleTextItem* item);
  void createHandles(Widget& w, Dataflow::PortItem* item);

  void handleItemSelection(Widget& w, bool b);

  template<typename T>
  void createHandles(Widget& w, T* item)
  {
    auto mhandle = new MoveHandle;
    mhandle->setZValue(50);
    mhandle->setRect({QPointF{}, w.size()});
    auto rhandle = new ResizeHandle;
    rhandle->setZValue(100);

    con(w, &Widget::portPosChanged,
        this, [this, &w] (QPointF portPos) {
      auto& ui = this->m_widgets.at(w.id());

      if(ui.port)
      {
        ui.port->setPos(portPos);
        if(ui.portHandle)
          ui.portHandle->setPos(portPos);
      }
    });

    con(w, &Widget::posChanged, mhandle, [mhandle, rhandle, item, &w] (QPointF pos) {
      item->setPos(pos);
      mhandle->setPos(pos);
      rhandle->setPos(w.pos() + QPointF{w.size().width(), w.size().height()});
    });
    con(w, &Widget::sizeChanged, mhandle, [mhandle, item] (QSizeF p) {
      item->setRect({QPointF{}, p});
      mhandle->setRect({QPointF{}, p});
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
    }

    connect(mhandle, &MoveHandle::released, this, [=] {
      this->context.dispatcher.commit();
    });

    // Resize handle
    {
      m_scene.addItem(rhandle);
      rhandle->setPos(w.pos() + QPointF{w.size().width(), w.size().height()});
      connect(rhandle, &ResizeHandle::positionChanged,
              this, [=, &w] (QPointF pos) {
        if(w.moving)
          return;
        auto p = item->mapFromScene(pos);
        if(p.x() < 10 || p.y() < 10)
          return;

        if(w.keepRatio())
        {
          double curRatio = w.size().width() / w.size().height();
          p = {curRatio * p.y(), p.y()};
        }

        this->context.dispatcher.submit<SetSize>(w, QSizeF{p.x(), p.y()});
      });

      connect(rhandle, &ResizeHandle::released,
              this, [=, &w] {
        this->context.dispatcher.commit();
        w.moving = true;
        rhandle->setPos(w.pos() + QPointF{w.size().width(), w.size().height()});
        w.moving = false;
      });
    }
  }

  ~DocumentView() override;
  QWidget* getWidget() override;

  QSizeF backgroundRectSize() { return m_rect.boundingRect().size(); }
  void backgroundRectSize(QSizeF sz) { if(sz != backgroundRectSize()) m_rect.setRect({0, 0, sz.width(), sz.height()}); }

  void on_widgetRemoved(const Widget& w);
private:
  FocusDispatcher m_focus;
public:
  Process::ProcessPresenterContext context;
private:
  std::unordered_map<Id<Widget>, WidgetUI> m_widgets;
  QWidget m_widget;
  WidgetList m_list;
  QGraphicsScene m_scene;
  View m_view;
  QGraphicsRectItem m_rect;
};


}
