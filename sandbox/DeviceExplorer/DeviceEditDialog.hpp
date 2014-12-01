#pragma once

class QComboBox;
class QGridLayout;
class ProtocolSettingsWidget;


#include <QDialog>
#include <QList>
#include <QString>


class DeviceEditDialog : public QDialog
{
  Q_OBJECT

public:

  DeviceEditDialog(QWidget *parent);
  ~DeviceEditDialog();

  //TODO: use QVariant ???
  /*
    first element is protocol name, second element (if present) is node name.
  */
  QList<QString> getSettings() const;

  void setSettings(QList<QString> &settings);

protected slots:

  void updateProtocolWidget();

protected:

  void buildGUI();

  void initAvailableProtocols();

protected:

  QComboBox *m_protocolCBox;
  ProtocolSettingsWidget *m_protocolWidget;
  QGridLayout *m_gLayout;
  QList<QList<QString> > m_previousSettings;
  int m_index;
};

