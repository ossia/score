#include <QWidget>

class QComboBox;

class ConstraintInspectorWidget;
class ConstraintModel;
class BoxWidget : public QWidget
{
		Q_OBJECT

	public:
		BoxWidget(ConstraintInspectorWidget* parent);

		void updateComboBox();
		void setModel(ConstraintModel*);

		const QString hiddenText{tr("Hide")};

	private:
		QComboBox* m_boxList;
		ConstraintModel* m_model;
};
