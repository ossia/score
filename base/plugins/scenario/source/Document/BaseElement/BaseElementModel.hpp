#pragma once
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <tools/ObjectPath.hpp>

class BaseElementPresenter;
class FullViewConstraintViewModel;
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
        BaseElementModel (QByteArray data, QObject* parent);
        BaseElementModel (QObject* parent);
        virtual ~BaseElementModel() = default;

        ConstraintModel* constraintModel() const
        {
            return m_baseConstraint;
        }

        virtual QByteArray save() override;
        virtual QJsonObject toJson() override;


    private:
        ConstraintModel* m_baseConstraint {};
};

