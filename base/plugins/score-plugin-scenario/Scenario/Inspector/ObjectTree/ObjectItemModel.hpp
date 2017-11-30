#pragma once
#include <score/document/DocumentContext.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/widgets/MarginLess.hpp>

#include <QAbstractItemModel>
#include <QFile>
#include <QTreeView>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPushButton>
#include <QLabel>

class QToolButton;
class QGraphicsSceneMouseEvent;
namespace Scenario
{
// TimeSync / event / state / state processes
// or
// Interval / processes
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

    bool isAlive(QObject* obj) const;

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
    mutable QMap<const QObject*, QPointer<const QObject>> m_aliveMap;

    std::vector<QMetaObject::Connection> m_itemCon;
};

class ObjectWidget final: public QTreeView
{
  public:
    ObjectWidget(const score::DocumentContext& ctx, QWidget* par);
    ObjectItemModel model;

    bool updatingSelection{false};

  private:
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

    void contextMenuEvent(QContextMenuEvent* ev) override;
    const score::DocumentContext& m_ctx;
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

class NeightborSelector
{
public:
  NeightborSelector(score::SelectionStack& s, ObjectWidget* objects);
  // does the current selection have a neightbor in that direction ?
  bool hasRight() const;
  bool hasLeft() const;
  bool hasUp() const;
  bool hasDown() const;

  // Select neighbor
  void selectRight();
  void selectLeft();
  void selectUp();
  void selectDown();

private:
  score::SelectionStack& m_stack;
  ObjectWidget* m_objects{};
  score::SelectionDispatcher m_selectionDispatcher;
};

class SearchWidget final : public QWidget
{
public:
  SearchWidget(const score::GUIApplicationContext& ctx);

  void toggle() { this->isHidden() ? this->show() : this->hide();}
  void search();

  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent *event) override;
  void on_findAddresses(QStringList strlst);
  void search_for(QString address);

private:

  QLineEdit* m_lineEdit{};
  QPushButton* m_btn;
  const score::GUIApplicationContext& m_ctx;
};

class SelectionStackWidget final : public QWidget
{
public:
  SelectionStackWidget(score::SelectionStack& s, QWidget* parent, ObjectWidget* objects);

private:
  QToolButton* m_prev{};
  QToolButton* m_next{};
  QLabel* m_label{};
  QToolButton* m_left{};
  QToolButton* m_right{};
  QToolButton* m_up{};
  QToolButton* m_down{};
  score::SelectionStack& m_stack;
  NeightborSelector m_selector;
};

class ObjectPanelDelegate final : public score::PanelDelegate
{
public:
  ObjectPanelDelegate(const score::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;
  const score::PanelStatus& defaultPanelStatus() const override;

  void on_modelChanged(
      score::MaybeDocument oldm, score::MaybeDocument newm) override;
  void setNewSelection(const Selection& sel) override;

  SizePolicyWidget* m_widget{};
  QVBoxLayout* m_lay{};
  SelectionStackWidget* m_stack{};
  ObjectWidget* m_objects{};
  SearchWidget* m_searchWidget{};
};

class ObjectPanelDelegateFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("aea973e2-84aa-4b8a-b0f0-b6ce39b6f15a")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override
  {
    return std::make_unique<ObjectPanelDelegate>(ctx);
  }
};
}
