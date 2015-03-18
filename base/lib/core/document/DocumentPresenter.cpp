#include "DocumentPresenter.hpp"

#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <iscore/tools/utilsCPP11.hpp>
#include <core/document/DocumentView.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/panel/PanelModelInterface.hpp>


using namespace iscore;

DocumentPresenter::DocumentPresenter(DocumentDelegateFactoryInterface* fact,
                                     DocumentModel* m,
                                     DocumentView* v,
                                     QObject* parent) :
    NamedObject {"DocumentPresenter", parent},
            m_view{v},
            m_model{m},
            m_presenter{fact->makePresenter(this,
                                            m_model->modelDelegate(),
                                            m_view->viewDelegate())}
{
}

