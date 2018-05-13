#pragma once
#include <QWidget>
#include <score_lib_base_export.h>

class QComboBox;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
namespace score
{
class SettingsDelegateModel;
template <class Model>
class SettingsDelegatePresenter;

template <class Model>
class SettingsDelegateView : public QObject
{
public:
  using Presenter = SettingsDelegatePresenter<Model>;
  using QObject::QObject;
  ~SettingsDelegateView() = default;
  virtual void setPresenter(Presenter* presenter)
  {
    m_presenter = presenter;
  }

  Presenter* getPresenter()
  {
    return m_presenter;
  }

  virtual QWidget* getWidget()
      = 0; // QML? ownership transfer ? ? ? what about "this" case ?

protected:
  Presenter* m_presenter{};
};
using GlobalSettingsView = SettingsDelegateView<SettingsDelegateModel>;
}

#define SETTINGS_UI_COMBOBOX_HPP(Control) \
public:                                   \
  void set##Control(QString);             \
public:                                   \
  void Control##Changed(QString arg)      \
       W_SIGNAL(Control##Changed, arg);   \
                                          \
private:                                  \
  QComboBox* m_##Control{};

#define SETTINGS_UI_NUM_COMBOBOX_HPP(Control) \
public:                                       \
  void set##Control(int);                     \
  void Control##Changed(int arg)              \
       W_SIGNAL(Control##Changed, arg);       \
                                              \
private:                                      \
  QComboBox* m_##Control{};

#define SETTINGS_UI_TOGGLE_HPP(Control) \
public:                                 \
  void set##Control(bool);              \
  void Control##Changed(bool arg)       \
       W_SIGNAL(Control##Changed, arg); \
                                        \
private:                                \
  QCheckBox* m_##Control{};

#define SETTINGS_UI_SPINBOX_HPP(Control) \
public:                                  \
  void set##Control(int);                \
  void Control##Changed(int arg)         \
       W_SIGNAL(Control##Changed, arg);  \
                                         \
private:                                 \
  QSpinBox* m_##Control{};

#define SETTINGS_UI_DOUBLE_SPINBOX_HPP(Control) \
public:                                         \
  void set##Control(double);                    \
  void Control##Changed(double arg)             \
       W_SIGNAL(Control##Changed, arg);         \
                                                \
private:                                        \
  QDoubleSpinBox* m_##Control{};

#define SETTINGS_UI_COMBOBOX_SETUP(Text, Control, Values)                  \
  m_##Control = new QComboBox{m_widg};                                     \
  m_##Control->addItems(Values);                                           \
  lay->addRow(tr(Text), m_##Control);                                      \
  connect(                                                                 \
      m_##Control, SignalUtils::QComboBox_currentIndexChanged_int(), this, \
      [this](int i) { Control##Changed(m_##Control->itemText(i)); });

#define SETTINGS_UI_NUM_COMBOBOX_SETUP(Text, Control, Values)              \
  m_##Control = new QComboBox{m_widg};                                     \
  for (auto v : Values)                                                    \
    m_##Control->addItem(QString::number(v));                              \
  lay->addRow(tr(Text), m_##Control);                                      \
  connect(                                                                 \
      m_##Control, SignalUtils::QComboBox_currentIndexChanged_int(), this, \
      [this](int i) { Control##Changed(m_##Control->itemText(i).toInt()); });

#define SETTINGS_UI_SPINBOX_SETUP(Text, Control)                   \
  m_##Control = new QSpinBox{m_widg};                              \
  lay->addRow(tr(Text), m_##Control);                              \
  connect(                                                         \
      m_##Control, SignalUtils::QSpinBox_valueChanged_int(), this, \
      &View::Control##Changed);

#define SETTINGS_UI_DOUBLE_SPINBOX_SETUP(Text, Control)                     \
  m_##Control = new QDoubleSpinBox{m_widg};                                 \
  lay->addRow(tr(Text), m_##Control);                                       \
  connect(                                                                  \
      m_##Control, SignalUtils::QDoubleSpinBox_valueChanged_double(), this, \
      &View::Control##Changed);

#define SETTINGS_UI_TOGGLE_SETUP(Text, Control) \
  m_##Control = new QCheckBox{m_widg};          \
  lay->addRow(tr(Text), m_##Control);           \
  connect(m_##Control, &QCheckBox::toggled, this, &View::Control##Changed);

#define SETTINGS_UI_COMBOBOX_IMPL(Control)                     \
  void View::set##Control(QString val)                         \
  {                                                            \
    int idx = m_##Control->findData(QVariant::fromValue(val)); \
    if (idx != -1 && idx != m_##Control->currentIndex())       \
      m_##Control->setCurrentIndex(idx);                       \
    else                                                       \
    {                                                          \
      idx = m_##Control->findText(val);                        \
      if (idx != -1 && idx != m_##Control->currentIndex())     \
        m_##Control->setCurrentIndex(idx);                     \
    }                                                          \
  }

#define SETTINGS_UI_NUM_COMBOBOX_IMPL(Control)                 \
  void View::set##Control(int val)                             \
  {                                                            \
    int idx = m_##Control->findData(QVariant::fromValue(val)); \
    if (idx != -1 && idx != m_##Control->currentIndex())       \
      m_##Control->setCurrentIndex(idx);                       \
    else                                                       \
    {                                                          \
      idx = m_##Control->findText(QString::number(val));       \
      if (idx != -1 && idx != m_##Control->currentIndex())     \
        m_##Control->setCurrentIndex(idx);                     \
    }                                                          \
  }

#define SETTINGS_UI_SPINBOX_IMPL(Control) \
  void View::set##Control(int val)        \
  {                                       \
    int cur = m_##Control->value();       \
    if (cur != val)                       \
      m_##Control->setValue(val);         \
  }

#define SETTINGS_UI_DOUBLE_SPINBOX_IMPL(Control) \
  void View::set##Control(double val)            \
  {                                              \
    int cur = m_##Control->value();              \
    if (cur != val)                              \
      m_##Control->setValue(val);                \
  }

#define SETTINGS_UI_TOGGLE_IMPL(Control) \
  void View::set##Control(bool val)      \
  {                                      \
    bool cur = m_##Control->isChecked(); \
    if (cur != val)                      \
      m_##Control->setChecked(val);      \
  }
