#pragma once

#include <QWidget>

#include <Process/ProcessFactoryKey.hpp>

namespace Process
{
class ProcessList;
}

class AddProcessDialog final : public QWidget
{
        Q_OBJECT

    public:
        AddProcessDialog(
                const Process::ProcessList& plist,
                QWidget* parent = 0);

        void launchWindow();

    signals:
        void okPressed(const ProcessFactoryKey&);

    private:
        const Process::ProcessList& m_factoryList;

};
