#include <core/document/DocumentModel.hpp>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <interface/panel/PanelModelInterface.hpp>
#include <exception>


using namespace iscore;

DocumentModel::DocumentModel(QObject* parent) :
    NamedObject {"DocumentModel", parent}
{

}

void DocumentModel::setModelDelegate(DocumentDelegateModelInterface* m)
{
    // TODO do this at construction.
    if(m_model)
    {
        delete m_model;
    }

    m_model = m;
}

PanelModelInterface* DocumentModel::panel(QString name) const
{
    using namespace std;
    auto it = find_if(begin(m_panelModels),
                      end(m_panelModels),
                      [&](PanelModelInterface * pm)
    {
        return pm->objectName() == name;
    });

    return it != end(m_panelModels) ? *it : nullptr;

}

void DocumentModel::setNewSelection(const Selection& s)
{
    m_model->setNewSelection(s);
    // TODO are the panels opt-in or opt-out ?
    // Inspector is opt-in.
}
