#pragma once
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/panel/PanelDelegate.hpp>
#include <iscore/plugins/panel/PanelDelegateFactory.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/widgets/MarginLess.hpp>

#include <QAbstractItemModel>
#include <QFile>
#include <QTreeView>
#include <QVBoxLayout>
#include <QHeaderView>

class QToolButton;
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
    void setSelected(QList<const IdentifiedObjectAbstract*> sel);

    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

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

    QList<const QObject*> m_root;
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
      setAnimated(true);
      setAlternatingRowColors(true);
      setMidLineWidth(40);
      setUniformRowHeights(true);
      setWordWrap(false);
      setMouseTracking(true);

      con(model, &ObjectItemModel::changed,
          this, &QTreeView::expandAll);
    }

    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override
    {
      if(selected.size() > 0 && !updatingSelection)
      {
        iscore::SelectionDispatcher d{m_ctx.selectionStack};
        auto obj = (IdentifiedObjectAbstract*) selected.at(0).indexes().at(0).internalPointer();
        d.setAndCommit(Selection{obj});
      }
    }

    ObjectItemModel model;

    bool updatingSelection{false};
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

class SelectionStackWidget final : public QWidget
{
public:
  SelectionStackWidget(iscore::SelectionStack& s, QWidget* parent);

private:
  QToolButton* m_prev{};
  QToolButton* m_next{};
  iscore::SelectionStack& m_stack;
};

class ObjectPanelDelegate final : public iscore::PanelDelegate
{
public:
  ObjectPanelDelegate(const iscore::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;
  const iscore::PanelStatus& defaultPanelStatus() const override;

  void on_modelChanged(
      iscore::MaybeDocument oldm, iscore::MaybeDocument newm) override;
  void setNewSelection(const Selection& sel) override;

  SizePolicyWidget* m_widget{};
  QVBoxLayout* m_lay{};
  SelectionStackWidget* m_stack{};
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
