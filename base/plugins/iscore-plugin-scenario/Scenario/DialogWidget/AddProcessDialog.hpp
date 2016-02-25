#pragma once
#include <QWidget>
#include <iscore/plugins/customfactory/UuidKey.hpp>

namespace Process
{
class ProcessList;
class StateProcessList;
class ProcessFactory;
class StateProcessFactory;
}
namespace Scenario
{

class AddProcessDialog final : public QWidget
{
        Q_OBJECT

    public:
        AddProcessDialog(
                const Process::ProcessList& plist,
                QWidget* parent = 0);

        void launchWindow();

    signals:
        void okPressed(const UuidKey<Process::ProcessFactory>&);

    private:
        const Process::ProcessList& m_factoryList;
};

class AddStateProcessDialog final : public QWidget
{
        Q_OBJECT

    public:
        AddStateProcessDialog(
                const Process::StateProcessList& plist,
                QWidget* parent = 0);

        void launchWindow();

    signals:
        void okPressed(const UuidKey<Process::StateProcessFactory>&);

    private:
        const Process::StateProcessList& m_factoryList;
};
}
