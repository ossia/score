// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/DocumentDelegatePresenter.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::DocumentPresenter)
namespace score
{
DocumentPresenter::DocumentPresenter(
    const score::DocumentContext& ctx,
    DocumentDelegateFactory& fact,
    const DocumentModel& m,
    DocumentView& v,
    QObject* parent)
    : QObject{parent}
    , m_view{v}
    , m_model{m}
    , m_presenter{fact.makePresenter(
          ctx, this, m_model.modelDelegate(), m_view.viewDelegate())}
{
}

void DocumentPresenter::setNewSelection(const Selection& s)
{
  m_presenter->setNewSelection(s);
}
}
