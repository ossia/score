#include <core/document/DocumentModel.hpp>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
using namespace iscore;

void DocumentModel::setModel(DocumentDelegateModelInterface* m)
{
	m_model = m;
	m_model->setParent(this);
}
