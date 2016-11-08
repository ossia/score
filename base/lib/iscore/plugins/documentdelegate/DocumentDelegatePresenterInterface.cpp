#include <core/document/DocumentPresenter.hpp>

#include "DocumentDelegatePresenterInterface.hpp"


namespace iscore
{
DocumentDelegatePresenterInterface::DocumentDelegatePresenterInterface(
        DocumentPresenter* parent_presenter,
        const DocumentDelegateModelInterface& model,
        DocumentDelegateViewInterface& view) :
    QObject {parent_presenter},
    m_model {model},
    m_view {view},
    m_parentPresenter {parent_presenter}
{

}

iscore::DocumentDelegatePresenterInterface::~DocumentDelegatePresenterInterface() = default;

}
