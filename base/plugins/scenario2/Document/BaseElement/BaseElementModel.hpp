#pragma once
#include <QNamedObject>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>

class BaseElementPresenter;
class IntervalModel;

/**
 * @brief The BaseElementModel class
 *
 * L'élément de base pour i-score est un intervalle,
 * qui ne contient qu'un seul Content. (le faire hériter pour cela ? )
 */

class BaseElementModel : public iscore::DocumentDelegateModelInterface
{
	Q_OBJECT

	public:
		BaseElementModel(QObject* parent);
		virtual ~BaseElementModel() = default;

		IntervalModel* intervalModel()
		{
			return m_baseInterval;
		}

	private:
		IntervalModel* m_baseInterval{};
};

