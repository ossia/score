#include <QWidget>
#pragma once

class AddProcessDialog : public QWidget
{
        Q_OBJECT

    public:
        AddProcessDialog(QWidget* parent = 0);
    public slots:
        void launchWindow();

    signals:
        void okPressed(QString);
};
