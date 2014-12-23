#pragma once
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>

class BaseElementPresenter;
class TemporalConstraintViewModel;
class ConstraintModel;

/**
 * @brief The BaseElementModel class
 *
 * L'élément de base pour i-score est un constraintle,
 * qui ne contient qu'un seul Content. (le faire hériter pour cela ? )
 */

class BaseElementModel : public iscore::DocumentDelegateModelInterface
{
	Q_OBJECT

	public:
		BaseElementModel(QObject* parent);
		virtual ~BaseElementModel() = default;

		TemporalConstraintViewModel* constraintViewModel()
		{
			return m_viewModel;
		}

		virtual void reset() override;

	signals:
		void sigReset();

	private:
		ConstraintModel* m_baseConstraint{};
		TemporalConstraintViewModel* m_viewModel{};
};

