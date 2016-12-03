#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>

#include "DocumentPresenter.hpp"

class QObject;

namespace iscore
{
DocumentPresenter::DocumentPresenter(
    DocumentDelegateFactory& fact,
    const DocumentModel& m,
    DocumentView& v,
    QObject* parent)
    : QObject{parent}
    , m_view{v}
    , m_model{m}
    , m_presenter{fact.makePresenter(
          this, m_model.modelDelegate(), m_view.viewDelegate())}
{
}
}
