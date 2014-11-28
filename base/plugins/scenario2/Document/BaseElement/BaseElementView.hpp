#pragma once
#include <QNamedObject>
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>
class QGraphicsScene;
class QGraphicsView;
class BaseElementPresenter;
class BaseElementView : public iscore::DocumentDelegateViewInterface
{
	Q_OBJECT

	public:
		BaseElementView(QObject* parent);
		virtual ~BaseElementView() = default;

		virtual void setPresenter(iscore::DocumentDelegatePresenterInterface* presenter);
		virtual QWidget*getWidget();

	private:
		QGraphicsScene* m_scene{};
		QGraphicsView* m_view{};

		BaseElementPresenter* m_presenter{};
};

