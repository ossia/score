#include "FocusDispatcher.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <Document/BaseElement/BaseElementModel.hpp>

FocusDispatcher::FocusDispatcher(iscore::Document& doc):
    m_baseElementModel{iscore::IDocument::modelDelegate<BaseElementModel>(doc)}
{
    connect(this, SIGNAL(focus(ProcessViewModelInterface*)),
            &m_baseElementModel, SLOT(setFocusedViewModel(ProcessViewModelInterface*)));
}
