#pragma once

#include <QWidget>

#include <Process/ProcessFactoryKey.hpp>

class DynamicProcessList;

class AddProcessDialog final : public QWidget
{
        Q_OBJECT

    public:
        AddProcessDialog(
                const DynamicProcessList& plist,
                QWidget* parent = 0);

    public slots:
        void launchWindow();

    signals:
        void okPressed(const ProcessFactoryKey&);

    private:
        const DynamicProcessList& m_factoryList;

};
