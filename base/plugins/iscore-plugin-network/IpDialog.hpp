#pragma once
#include <QDialog>
class QSpinBox;
class IpWidget;
class IpDialog : public QDialog
{
    public:
        IpDialog(QWidget* parent);

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
