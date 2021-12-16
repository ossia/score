// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/document/DocumentContext.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/Bind.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentBackupManager.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>

#include <QObject>
#include <QWidget>

#include <wobjectimpl.h>

#include <vector>

W_OBJECT_IMPL(score::Document)
class Selection;
namespace score
{
DocumentContext::DocumentContext(Document& d)
    : app{score::GUIAppContext()}
    , document{d}
    , commandStack{d.m_commandStack}
    , selectionStack{d.selectionStack()}
    , objectLocker{d.locker()}
    , focus{d.focusManager()}
    , coarseUpdateTimer{d.m_documentCoarseUpdateTimer}
    , execTimer{d.m_execTimer}
    , dispatcher{d.m_disp}
{
}

const std::vector<DocumentPlugin*>& DocumentContext::pluginModels() const
{
  return document.model().pluginModels();
}

Document::Document(
    const QString& name,
    const Id<DocumentModel>& id,
    DocumentDelegateFactory& factory,
    QWidget* parentview,
    QObject* parent)
    : QObject{parent}
    , m_metadata{name}
    , m_commandStack{*this}
    , m_objectLocker{this}
    , m_backupMgr{nullptr}
    , m_context{*this}
    , m_virgin{true}
{
  /// Construction of the document model

  // Note : we have to separate allocation
  // because the model delegates init might call IDocument::path()
  // which requires the pointer to m_model to be intialized.
  std::allocator<DocumentModel> allocator;
  m_model = allocator.allocate(1);
  new (m_model) DocumentModel(id, m_context, factory, this);

  // TODO don't build them / destroy them if !application.gui.
  if (parentview)
  {
    m_view = new DocumentView{factory, *this, parentview};
    m_presenter
        = new DocumentPresenter{m_context, factory, *m_model, *m_view, this};
  }

  init();

  //    connect(m_model, &DocumentModel::fileNameChanged,
  //            this, &Document::fileNameChanged);
}

void Document::init()
{
  con(m_selectionStack,
      &SelectionStack::currentSelectionChanged,
      this,
      [&](const Selection& old, const Selection& s) {
        Selection oldfiltered = old;
        oldfiltered.removeAll(nullptr);
        Selection filtered = s;
        filtered.removeAll(nullptr);
        for (auto& panel : m_context.app.panels())
        {
          panel.setNewSelection(filtered);
        }
        m_presenter->setNewSelection(oldfiltered, filtered);
      });

  updateTimers();

  m_documentCoarseUpdateTimer.start();
}

void Document::updateTimers()
{
  int rate = m_context.app.applicationSettings.uiEventRate;
  rate = std::max(1, rate);

  m_documentCoarseUpdateTimer.setInterval(rate * 2);
  m_execTimer.setInterval(rate);
}

void Document::ready()
{
  if(m_view)
    m_view->viewDelegate().ready();

  if(m_initialData)
  {
    // when we load / restore from a crash, we reuse the original data in order to
    // restore from the exact same state again in case of another crash

    m_backupMgr = new DocumentBackupManager{*std::move(m_initialData), *this};
    m_initialData.reset();

    // when we restore, we call updateBackupData later, when *all* documents are restored,
    // in order to make sure that we don't loose data
  }
  else
  {
    m_backupMgr = new DocumentBackupManager{saveAsByteArray(), *this};
    m_backupMgr->updateBackupData();
  }
}

Document::~Document()
{
  // We need a custom destructor because
  // for the sake of simplicity, we want the presenter
  // to be deleted before the model.
  // (Else we would have to fine-grain the deletion of the selection stack).

  m_commandStack.blockSignals(true);
  m_selectionStack.blockSignals(true);
  m_focus.blockSignals(true);

  delete m_presenter;
  delete m_view;
  delete m_model;
}

const Id<DocumentModel>& Document::id() const noexcept
{
  return m_model->id();
}
}
