#pragma once
#include <QDialog>
#include <QString>

class IpWidget;
class QSpinBox;
class QWidget;

class IpDialog : public QDialog
{
    public:
        explicit IpDialog(QWidget* parent);

        int port() const;
        const QString& ip() const;

    private:
        void accepted();
        void rejected();

        QSpinBox* m_portBox{};
        IpWidget* m_ipBox{};

        int m_port{};
        QString m_ip;
};
