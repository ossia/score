#include <core/application/Application.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/document/DocumentPresenter.hpp>

#include <iscore/plugins/panel/PanelPresenter.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>

#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>

#include <core/application/OpenDocumentsFile.hpp>

using namespace iscore;

DocumentContext::DocumentContext(Document& d):
    app{*safe_cast<iscore::Application*>(d.parent()->parent())},
    document{d},
    commandStack{d.commandStack()},
    selectionStack{d.selectionStack()},
    objectLocker{d.locker()}
{

}


Document::Document(
        const Id<DocumentModel>& id,
        DocumentDelegateFactoryInterface* factory,
        QWidget* parentview,
        QObject* parent) :
    NamedObject {"Document", parent},
    m_objectLocker{this},
    m_backupMgr{new DocumentBackupManager{*this}},
    m_context{*this}
{
    /// Construction of the document model

    // Note : we have to separate allocation
    // because the model delegates init might call IDocument::path()
    // which requires the pointer to m_model to be intialized.
    std::allocator<DocumentModel> allocator;
    m_model = allocator.allocate(1);
    allocator.construct(m_model, id, factory, this);
    m_view = new DocumentView{factory, *this, parentview};
    m_presenter = new DocumentPresenter{factory,
            *m_model,
            *m_view,
            this};

    init();

//    connect(m_model, &DocumentModel::fileNameChanged,
//            this, &Document::fileNameChanged);
}

void Document::init()
{
    con(m_selectionStack, &SelectionStack::currentSelectionChanged,
            this, [&] (const Selection& s)
            {
                for(auto& panel : m_model->panels())
                {
                    panel->setNewSelection(s);
                }
                m_model->setNewSelection(s);
            });

}

void Document::setBackupMgr(DocumentBackupManager* backupMgr)
{
    m_backupMgr = backupMgr;
}

QString Document::docFileName() const
{
    return model().docFileName();
}

void Document::setDocFileName(const QString &docFileName)
{
    m_model->setDocFileName(docFileName);
}

Document::~Document()
{
    // We need a custom destructor because
    // for the sake of simplicity, we want the presenter
    // to be deleted before the model.
    // (Else we would have to fine-grain the deletion of the selection stack).

    delete m_presenter;
    delete m_view;
}

const Id<DocumentModel>&Document::id() const
{
    return m_model->id();
}

void Document::setupNewPanel(PanelFactory* factory)
{
    m_model->addPanel(factory->makeModel(m_model));
}

void Document::bindPanelPresenter(PanelPresenter* pres)
{
    using namespace std;
    auto localmodel = std::find_if(begin(model().panels()),
                                   end(model().panels()),
                                   [&] (PanelModel* model)
    {
        return model->panelId() == pres->panelId();
    });
    ISCORE_ASSERT(localmodel != end(model().panels()));

    pres->setModel(*localmodel);
}
