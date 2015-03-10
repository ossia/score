#include "FocusDispatcher.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <Document/BaseElement/BaseElementModel.hpp>

FocusDispatcher::FocusDispatcher(iscore::Document& doc):
    m_baseElementModel{iscore::IDocument::modelDelegate<BaseElementModel>(doc)}
{

}

void FocusDispatcher::focus(ProcessSharedModelInterface* obj)
{
    m_baseElementModel.setFocusedProcess(obj);
}
