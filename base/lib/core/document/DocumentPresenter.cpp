#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>

#include "DocumentPresenter.hpp"
#include <iscore/tools/NamedObject.hpp>

class QObject;


using namespace iscore;

DocumentPresenter::DocumentPresenter(DocumentDelegateFactoryInterface* fact,
                                     const DocumentModel& m,
                                     DocumentView& v,
                                     QObject* parent) :
    NamedObject {"DocumentPresenter", parent},
            m_view{v},
            m_model{m},
            m_presenter{fact->makePresenter(this,
                                            m_model.modelDelegate(),
                                            m_view.viewDelegate())}
{
}

