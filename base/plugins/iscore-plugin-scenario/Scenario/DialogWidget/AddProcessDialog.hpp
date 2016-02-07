#pragma once

#include <QWidget>



namespace Process
{
class ProcessList;
class ProcessFactory;
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
}
