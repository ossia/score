#pragma once
#include <QWidget>

class QPushButton;
class QCheckBox;

// TODO refactor with SelectableButton
class EventShortCut: public QWidget
{
        Q_OBJECT
    public:

        EventShortCut(QString eventId, QWidget* parent = 0);

        bool isChecked();
        QString eventName();

    signals:
        void eventSelected();

    private:
        QCheckBox* m_box;
        QPushButton* m_eventBtn;

};
