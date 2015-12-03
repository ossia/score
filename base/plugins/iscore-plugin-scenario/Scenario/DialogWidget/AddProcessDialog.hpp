#pragma once

#include <QWidget>

#include <Process/ProcessFactoryKey.hpp>

class ProcessList;

class AddProcessDialog final : public QWidget
{
        Q_OBJECT

    public:
        AddProcessDialog(
                const ProcessList& plist,
                QWidget* parent = 0);

    public slots:
        void launchWindow();

    signals:
        void okPressed(const ProcessFactoryKey&);

    private:
        const ProcessList& m_factoryList;

};
