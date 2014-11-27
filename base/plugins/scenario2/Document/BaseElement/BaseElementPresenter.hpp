#pragma once
#include <QNamedObject>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>

class BaseElementPresenter : public iscore::DocumentDelegatePresenterInterface
{
	Q_OBJECT
	
	public:
		BaseElementPresenter(iscore::DocumentPresenter* parent_presenter,
							 iscore::DocumentDelegateModelInterface* model,
						     iscore::DocumentDelegateViewInterface* view);
		virtual ~BaseElementPresenter() = default;
		
	private:
	
};

