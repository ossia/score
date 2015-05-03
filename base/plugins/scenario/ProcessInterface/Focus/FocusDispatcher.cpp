#include "FocusDispatcher.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>

FocusDispatcher::FocusDispatcher(iscore::Document& doc):
    m_baseElementModel{iscore::IDocument::modelDelegate_generic(doc)}
{
    connect(this, SIGNAL(focus(const ProcessViewModel*)),
            &m_baseElementModel, SLOT(setFocusedViewModel(const ProcessViewModel*)), Qt::QueuedConnection);
}
