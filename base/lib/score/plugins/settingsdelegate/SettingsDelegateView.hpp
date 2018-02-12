#pragma once
#include <QWidget>
#include <score_lib_base_export.h>

namespace score
{
class SettingsDelegatePresenter;

class SCORE_LIB_BASE_EXPORT SettingsDelegateView : public QObject
{
public:
  using QObject::QObject;
  virtual ~SettingsDelegateView();
  virtual void setPresenter(SettingsDelegatePresenter* presenter)
  {
    m_presenter = presenter;
  }

  SettingsDelegatePresenter* getPresenter()
  {
    return m_presenter;
  }

  virtual QWidget* getWidget()
      = 0; // QML? ownership transfer ? ? ? what about "this" case ?

protected:
  SettingsDelegatePresenter* m_presenter{};
};
}


#define SETTINGS_UI_COMBOBOX_HPP(Control)      \
  public: void set ## Control(QString);        \
  Q_SIGNALS: void Control ## Changed(QString); \
  private: QComboBox* m_ ## Control{};

#define SETTINGS_UI_TOGGLE_HPP(Control)        \
  public: void set ## Control(bool);           \
  Q_SIGNALS: void Control ## Changed(bool);    \
  private: QCheckBox* m_ ## Control{};

#define SETTINGS_UI_SPINBOX_HPP(Control)        \
  public: void set ## Control(int);            \
  Q_SIGNALS: void Control ## Changed(int);     \
  private: QSpinBox* m_ ## Control{};





#define SETTINGS_UI_COMBOBOX_SETUP(Text, Control, Values) \
  m_ ## Control = new QComboBox{m_widg}; \
  m_ ## Control->addItems(Values); \
  lay->addRow(tr(Text), m_ ## Control); \
  connect(m_ ## Control, SignalUtils::QComboBox_currentIndexChanged_int(), this, \
  [this] (int i) { Control ## Changed( m_ ## Control->itemText(i) ); } );

#define SETTINGS_UI_SPINBOX_SETUP(Text, Control) \
  m_ ## Control = new QSpinBox{m_widg}; \
  lay->addRow(tr(Text), m_ ## Control); \
  connect(m_ ## Control, SignalUtils::QSpinBox_valueChanged_int(), \
          this,  &View::Control ## Changed);

#define SETTINGS_UI_TOGGLE_SETUP(Text, Control) \
  m_ ## Control = new QCheckBox{m_widg}; \
  lay->addRow(tr(Text), m_ ## Control); \
  connect(m_ ## Control, &QCheckBox::toggled, \
          this, &View::Control ## Changed);


#define SETTINGS_UI_COMBOBOX_IMPL(Control)                       \
  void View::set ## Control(QString val) {                       \
    int idx = m_ ## Control->findData(QVariant::fromValue(val)); \
    if(idx != -1 && idx != m_ ## Control->currentIndex())        \
       m_ ## Control->setCurrentIndex(idx);                      \
  else { idx = m_ ## Control->findText(val);                     \
         if (idx != -1 && idx != m_ ## Control->currentIndex())  \
            m_ ## Control->setCurrentIndex(idx);                 \
  }                                                              \
}

#define SETTINGS_UI_SPINBOX_IMPL(Control)                        \
  void View::set ## Control(int val) {                           \
  int cur = m_ ## Control->value();                              \
  if(cur != val)                                                 \
    m_ ## Control->setValue(val);                                \
}


#define SETTINGS_UI_TOGGLE_IMPL(Control)                        \
  void View::set ## Control(bool val) {                         \
  bool cur = m_ ## Control->isChecked();                        \
  if(cur != val)                                                \
    m_ ## Control->setChecked(val);                             \
}

