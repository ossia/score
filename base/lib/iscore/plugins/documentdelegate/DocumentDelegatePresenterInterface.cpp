#include "DocumentDelegatePresenterInterface.hpp"
#include <core/document/DocumentPresenter.hpp>
using namespace iscore;


DocumentDelegatePresenterInterface::DocumentDelegatePresenterInterface(
        DocumentPresenter* parent_presenter,
        const QString& object_name,
        const DocumentDelegateModelInterface& model,
        DocumentDelegateViewInterface& view) :
    NamedObject {object_name, parent_presenter},
    m_model {model},
    m_view {view},
    m_parentPresenter {parent_presenter}
{

}

iscore::DocumentDelegatePresenterInterface::~DocumentDelegatePresenterInterface()
{

}
