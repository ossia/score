#pragma once
#include <QNamedObject>
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>

class BaseElementView : public iscore::DocumentDelegateViewInterface
{
	Q_OBJECT
	
	public:
	
		virtual ~BaseElementView() = default;
		
		virtual void setPresenter(iscore::DocumentDelegatePresenterInterface* presenter);
		virtual QWidget*getWidget();
		
	private:
		
};

