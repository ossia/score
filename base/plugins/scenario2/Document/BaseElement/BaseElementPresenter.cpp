#include "BaseElementPresenter.hpp"
using namespace iscore;

BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter, 
										   DocumentDelegateModelInterface* model, 
										   DocumentDelegateViewInterface* view):
	DocumentDelegatePresenterInterface(parent_presenter, model, view)
{
	
}
