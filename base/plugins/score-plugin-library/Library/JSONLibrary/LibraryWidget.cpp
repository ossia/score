// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LibraryWidget.hpp"
#include <Library/JSONLibrary/FileSystemModel.hpp>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QScrollArea>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SearchLineEdit.hpp>
#include <QSortFilterProxyModel>
#include <wobjectimpl.h>
#include <Library/LibrarySettings.hpp>

W_OBJECT_IMPL(Library::ProcessTreeView)
namespace Library
{
  class RecursiveFilterProxy final
      : public QSortFilterProxyModel
  {
  public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

  private:
    bool filterAcceptsRow(
          int srcRow,
          const QModelIndex& srcParent) const override
    {
      if (filterAcceptsRowItself(srcRow, srcParent))
      {
        return true;
      }

      // Accept if any of the parents is accepted on its own
      for (QModelIndex parent = srcParent; parent.isValid();
           parent = parent.parent())
        if (filterAcceptsRowItself(parent.row(), parent.parent()))
        {
          return true;
        }

      // Accept if any of the children is accepted on its own
      return hasAcceptedChildren(srcRow, srcParent);
    }

    bool filterAcceptsRowItself(
          int srcRow,
          const QModelIndex& srcParent) const
    {
      QModelIndex index = sourceModel()->index(srcRow, 0, srcParent);
      return sourceModel()->data(index).toString().contains(filterRegExp());
    }

    bool hasAcceptedChildren(
          int srcRow,
          const QModelIndex& srcParent) const
    {
      QModelIndex index = sourceModel()->index(srcRow, 0, srcParent);

      if (!index.isValid())
        return false;

      SCORE_ASSERT(index.model());
      const int childCount = index.model()->rowCount(index);

      if (childCount == 0)
        return false;

      for (int i = 0; i < childCount; ++i)
      {
        if (filterAcceptsRowItself(i, index))
          return true;

        if (hasAcceptedChildren(i, index))
          return true;
      }

      return false;
    }
  };

  struct ItemModelFilterLineEdit final
      : public score::SearchLineEdit
  {
  public:
    ItemModelFilterLineEdit(QSortFilterProxyModel& proxy, QTreeView& tv, QWidget* p):
      score::SearchLineEdit{p}
    , m_proxy{proxy}
    , m_view{tv}
    {
      connect(this, &QLineEdit::textEdited,
              this, [=] { search(); });

      setStyleSheet(R"_(
QScrollArea
{
    border: 1px solid #3A3939;
    border-radius: 2px;
    padding: 0;
    background-color: #12171A;
}
QScrollArea QLabel
{
    background-color: #12171A;
}
)_");
    }

    void search() override
    {
      if(text() != m_proxy.filterRegExp().pattern())
      {
        m_proxy.setFilterRegExp(QRegExp(text(), Qt::CaseInsensitive, QRegExp::FixedString));

        if(text().isEmpty())
          m_view.collapseAll();
        else
          m_view.expandAll();
      }
    }

    QSortFilterProxyModel& m_proxy;
    QTreeView& m_view;
  };

void ProcessTreeView::selectionChanged(const QItemSelection& sel, const QItemSelection& desel)
{
  if (sel.size() > 0)
  {
    auto idx = sel.indexes().front();
    //auto model =
  }
  else if(desel.size() > 0)
  {
    selected({});
  }
}


class InfoWidget final
    : public QScrollArea
{
public:
  InfoWidget(QWidget* parent)
  {
    setMinimumHeight(200);
    setMaximumHeight(200);
    auto lay = new QFormLayout{this};
    lay->addWidget(&m_name);
    lay->addRow(tr("Author"), &m_author);
    lay->addRow(tr("Tags"), &m_tags);
  }

  void setData(const ProcessData& d)
  {
    m_name.setText(d.name);
    if(auto f = score::GUIAppContext().interfaces<Process::ProcessFactoryList>().get(d.key))
    {
      m_tags.setText(f->tags().join(", "));
    }
    else
    {
      m_tags.clear();
    }
  }

  QLabel m_name;
  QLabel m_author;
  QLabel m_tags;
};

static void setup_treeview(QTreeView& tv)
{
  tv.setHeaderHidden(true);
  tv.setDragEnabled(true);
  tv.setDropIndicatorShown(true);
  tv.setAlternatingRowColors(true);
  tv.setSelectionMode(QAbstractItemView::SingleSelection);
  for (int i = 1; i < tv.model()->columnCount(); ++i)
    tv.hideColumn(i);
}

ProcessWidget::ProcessWidget(QAbstractItemModel& model, QWidget* parent)
  : QWidget{parent}
{
  auto lay = new score::MarginLess<QVBoxLayout>;

  this->setLayout(lay);

  auto m_proxy = new RecursiveFilterProxy{this};
  m_proxy->setSourceModel(&model);
  m_proxy->setFilterKeyColumn(0);
  lay->addWidget(new ItemModelFilterLineEdit{*m_proxy, m_tv, this});
  lay->addWidget(&m_tv);
  m_tv.setModel(m_proxy);
  setup_treeview(m_tv);

  auto widg = new InfoWidget{this};
  lay->addWidget(widg);
}

ProcessWidget::~ProcessWidget()
{

}


SystemLibraryWidget::SystemLibraryWidget(const score::GUIApplicationContext& ctx, QWidget* parent)
  : QWidget{parent}
  , m_model{new FileSystemModel{ctx, this}}
  , m_proxy{new RecursiveFilterProxy{this}}
{;
  auto lay = new score::MarginLess<QVBoxLayout>;

  this->setLayout(lay);

  m_proxy->setSourceModel(m_model);
  m_proxy->setFilterKeyColumn(0);
  lay->addWidget(new ItemModelFilterLineEdit{*m_proxy, m_tv, this});
  lay->addWidget(&m_tv);
  m_tv.setModel(m_proxy);
  setup_treeview(m_tv);
  m_tv.setAcceptDrops(true);

  auto widg = new InfoWidget{this};
  lay->addWidget(widg);

  setRoot(ctx.settings<Library::Settings::Model>().getPath());

}

SystemLibraryWidget::~SystemLibraryWidget()
{

}

void SystemLibraryWidget::setRoot(QString path)
{
  auto idx = m_model->setRootPath(path);
  m_tv.setRootIndex(m_proxy->mapFromSource(idx));
  for (int i = 1; i < m_model->columnCount(); ++i)
    m_tv.hideColumn(i);
}

ProjectLibraryWidget::ProjectLibraryWidget(const score::GUIApplicationContext& ctx, QWidget* parent)
  : QWidget{parent}
  , m_model{new FileSystemModel{ctx, this}}
  , m_proxy{new RecursiveFilterProxy{this}}
{
  auto lay = new score::MarginLess<QVBoxLayout>;

  this->setLayout(lay);

  m_proxy->setSourceModel(m_model);
  m_proxy->setFilterKeyColumn(0);
  lay->addWidget(new ItemModelFilterLineEdit{*m_proxy, m_tv, this});
  lay->addWidget(&m_tv);
  m_tv.setModel(m_proxy);
  setup_treeview(m_tv);
  m_tv.setAcceptDrops(true);

  auto widg = new InfoWidget{this};
  lay->addWidget(widg);
}

ProjectLibraryWidget::~ProjectLibraryWidget()
{

}

void ProjectLibraryWidget::setRoot(QString path)
{
  if(!path.isEmpty())
  {
    auto idx = m_model->setRootPath(path);

    m_tv.setModel(m_proxy);
    m_tv.setRootIndex(m_proxy->mapFromSource(idx));
    for (int i = 1; i < m_model->columnCount(); ++i)
      m_tv.hideColumn(i);
  }
  else
  {
    m_tv.setModel(nullptr);
  }
}

}
