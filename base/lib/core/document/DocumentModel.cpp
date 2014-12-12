#include <core/document/DocumentModel.hpp>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
using namespace iscore;

void DocumentModel::setModelDelegate(DocumentDelegateModelInterface* m)
{
	if(m_model) m_model->deleteLater();
	m_model = m;
}
