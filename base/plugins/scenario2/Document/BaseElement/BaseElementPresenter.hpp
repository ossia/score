#pragma once
#include <QNamedObject>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>

class BaseElementPresenter : public iscore::DocumentDelegatePresenterInterface
{
	Q_OBJECT
	
	public:
	
		virtual ~BaseElementPresenter() = default;
		
	private:
	
};

