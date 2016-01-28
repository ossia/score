#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <qnamespace.h>

#include "FocusDispatcher.hpp"

FocusDispatcher::FocusDispatcher(iscore::Document& doc):
    m_baseElementModel{iscore::IDocument::modelDelegate_generic(doc)}
{
    connect(this, SIGNAL(focus(QPointer<Process::LayerPresenter>)),
            &m_baseElementModel, SIGNAL(setFocusedPresenter(QPointer<Process::LayerPresenter>)),
            Qt::QueuedConnection);
}
