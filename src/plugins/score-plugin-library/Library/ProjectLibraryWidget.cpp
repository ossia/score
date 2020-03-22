#include "ProjectLibraryWidget.hpp"
#include <Library/FileSystemModel.hpp>
#include <Library/ItemModelFilterLineEdit.hpp>
#include <Library/RecursiveFilterProxy.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/LibraryWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/widgets/MarginLess.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QVBoxLayout>

namespace Library
{

ProjectLibraryWidget::ProjectLibraryWidget(
    const score::GUIApplicationContext& ctx,
    QWidget* parent)
    : QWidget{parent}
    , m_model{new FileSystemModel{ctx, this}}
    , m_proxy{new RecursiveFilterProxy{this}}
{
  auto lay = new score::MarginLess<QVBoxLayout>;
  setStatusTip(QObject::tr("This panel shows the project library.\n"
                           "It lists the files in the folder where the score is saved."));

  this->setLayout(lay);

  m_proxy->setSourceModel(m_model);
  m_proxy->setFilterKeyColumn(0);
  lay->addWidget(new ItemModelFilterLineEdit{*m_proxy, m_tv, this});
  lay->addWidget(&m_tv);
  m_tv.setModel(m_proxy);
  setup_treeview(m_tv);
  connect(&m_tv, &QTreeView::doubleClicked, this, [&](const QModelIndex& idx) {
    auto doc = ctx.docManager.currentDocument();
    if (!doc)
      return;

    auto path = m_model->filePath(m_proxy->mapToSource(idx));
    for (auto lib : libraryInterface(path))
    {
      if (lib->onDoubleClick(path, doc->context()))
        return;
    }
  });
  m_tv.setAcceptDrops(true);
}

ProjectLibraryWidget::~ProjectLibraryWidget() {}

void ProjectLibraryWidget::setRoot(QString path)
{
  if (!path.isEmpty())
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
