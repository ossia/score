#pragma once
#include <QObject>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class ISCORE_PLUGIN_SCENARIO_EXPORT ModelConsistency final : public QObject
{
        Q_OBJECT

        Q_PROPERTY(bool valid READ isValid WRITE setValid NOTIFY validChanged)
        Q_PROPERTY(bool warning READ warning WRITE setWarning NOTIFY warningChanged)

        bool m_valid{true};
        bool m_warning{false};

    public:
        explicit ModelConsistency(QObject *parent);
        ModelConsistency(const ModelConsistency& other);
        ModelConsistency& operator=(const ModelConsistency& other);

        bool isValid() const;
        bool warning() const;

        void setValid(bool arg);
        void setWarning(bool warning);

    signals:
        void validChanged(bool arg);
        void warningChanged(bool warning);

};
}
