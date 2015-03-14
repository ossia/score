#pragma once
#include <QWidget>
struct Message;
class QLineEdit;
class MessageWidget : public QWidget
{
        Q_OBJECT
    public:
        MessageWidget(const Message& m, QWidget* parent);

    signals:
        // TODO needs removeMessageFromState
        void removeMe();

    private:
        QLineEdit* m_lineEdit{};

};
