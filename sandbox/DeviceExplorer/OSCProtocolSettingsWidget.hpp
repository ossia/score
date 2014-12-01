#pragma once

class QLineEdit;
class QSpinBox;

#include "ProtocolSettingsWidget.hpp"

class OSCProtocolSettingsWidget : public ProtocolSettingsWidget
{
 Q_OBJECT

public:
  OSCProtocolSettingsWidget(QWidget *parent = nullptr);

  virtual QList<QString> getSettings() const override;

  virtual void setSettings(const QList<QString> &settings) override;

protected slots:
  void openFileDialog();

protected:
  void buildGUI();

  void setDefaults();

protected:
  QLineEdit *m_deviceNameEdit;
  QSpinBox *m_portOutputSBox;
  QSpinBox *m_portInputSBox;
  QLineEdit *m_localHostEdit;
  QLineEdit *m_namespaceFilePathEdit;
};

