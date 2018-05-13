// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentDelegatePresenter.hpp"

#include <core/document/DocumentPresenter.hpp>
#include <score/selection/Selection.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::DocumentDelegatePresenter)
namespace score
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

score::DocumentDelegatePresenter::~DocumentDelegatePresenter() = default;
}
