#pragma once
#include <QWidget>
#include <Process/ProcessFactory.hpp>

class AddProcessDialog final : public QWidget
{
        Q_OBJECT

    public:
        AddProcessDialog(QWidget* parent = 0);
    public slots:
        void launchWindow();

    signals:
        void okPressed(const ProcessFactoryKey&);
};
