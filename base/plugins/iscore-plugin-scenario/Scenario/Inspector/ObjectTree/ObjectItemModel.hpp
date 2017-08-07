#pragma once
#include <QAbstractItemModel>
#include <QFile>
#include <QTreeView>
#include <QVBoxLayout>

#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/panel/PanelDelegate.hpp>
#include <iscore/plugins/panel/PanelDelegateFactory.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/widgets/MarginLess.hpp>
namespace Scenario
{
// Timenode / event / state / state processes
// or
// Constraint / processes
class ObjectItemModel final
    : public QAbstractItemModel
    , public Nano::Observer
{
    Q_OBJECT
  public:
    ObjectItemModel(QObject* parent);
    void setSelected(const QObject* sel);

    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;

    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;

    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

  signals:
    void changed();

  private:
    void setupConnections();
    void cleanConnections();

    template<typename... Args>
    void recompute(Args&&...)
    {
      cleanConnections();

      beginResetModel();
      endResetModel();

      setupConnections();

      changed();
    }

    const QObject* m_root{};
    QMetaObject::Connection m_con;

    std::vector<QMetaObject::Connection> m_itemCon;

};

class ObjectWidget final: public QTreeView
{
  public:
    ObjectWidget(const iscore::DocumentContext& ctx, QWidget* par)
      : QTreeView{par}
      , model{this}
      , m_ctx{ctx}
    {
      setModel(&model);
      setHeaderHidden(true);
      setAnimated(true);
      setAlternatingRowColors(true);
      setMidLineWidth(40);
      setUniformRowHeights(true);
      setWordWrap(false);

      con(model, &ObjectItemModel::changed,
          this, &QTreeView::expandAll);
    }

    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override
    {
      if(selected.size() > 0)
      {
        iscore::SelectionDispatcher d{m_ctx.selectionStack};
        auto obj = (IdentifiedObjectAbstract*) selected.at(0).indexes().at(0).internalPointer();
        d.setAndCommit(Selection{obj});
      }
    }
    ObjectItemModel model;

  private:
    const iscore::DocumentContext& m_ctx;
};

class SizePolicyWidget final : public QWidget
{
  public:
    using QWidget::QWidget;

    QSize sizeHint() const override
    {
      return m_sizePolicy;
    }
    void setSizeHint(QSize s) { m_sizePolicy = s; }
  private:
    QSize m_sizePolicy;
};

class ObjectPanelDelegate final : public iscore::PanelDelegate
{
public:
  ObjectPanelDelegate(const iscore::GUIApplicationContext& ctx)
    : iscore::PanelDelegate{ctx}
    , m_widget{new SizePolicyWidget}
    , m_lay{new iscore::MarginLess<QVBoxLayout>{m_widget}}
{
    m_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_widget->setMinimumHeight(100);
    m_widget->setSizeHint({200, 100});
}

private:
  QWidget* widget() override
  {
    return m_widget;
  }

  const iscore::PanelStatus& defaultPanelStatus() const override
  {
    static const iscore::PanelStatus status{true, Qt::RightDockWidgetArea, 8,
                                            QObject::tr("Objects"),
                                            QObject::tr("Ctrl+Shift+O")};

    return status;
  }


  void on_modelChanged(
      iscore::MaybeDocument oldm, iscore::MaybeDocument newm) override
  {
    using namespace iscore;
    delete m_objects;
    m_objects = nullptr;
    if (newm)
    {
      m_objects = new ObjectWidget{*newm, m_widget};

      m_objects->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
      m_lay->addWidget(m_objects);

      setNewSelection(newm->selectionStack.currentSelection());
    }
  }

  void setNewSelection(const Selection& sel) override
  {
    if (m_objects)
    {
      if(sel.empty())
      {
        m_objects->model.setSelected(nullptr);
      }
      else
      {
        m_objects->model.setSelected(sel.begin()->data());
      }
      m_objects->expandAll();
    }
  }

  SizePolicyWidget* m_widget{};
  QVBoxLayout* m_lay{};
  ObjectWidget* m_objects{};
};

class ObjectPanelDelegateFactory final : public iscore::PanelDelegateFactory
{
  ISCORE_CONCRETE("aea973e2-84aa-4b8a-b0f0-b6ce39b6f15a")

  std::unique_ptr<iscore::PanelDelegate>
  make(const iscore::GUIApplicationContext& ctx) override
  {
    return std::make_unique<ObjectPanelDelegate>(ctx);
  }
};
}
