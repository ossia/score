#include <core/document/DocumentPresenter.hpp>

#include "DocumentDelegatePresenter.hpp"

namespace iscore
{
DocumentDelegatePresenter::DocumentDelegatePresenter(
    DocumentPresenter* parent_presenter,
    const DocumentDelegateModel& model,
    DocumentDelegateView& view)
    : QObject{parent_presenter}
    , m_model{model}
    , m_view{view}
    , m_parentPresenter{parent_presenter}
{
}

iscore::DocumentDelegatePresenter::
    ~DocumentDelegatePresenter()
    = default;
}
