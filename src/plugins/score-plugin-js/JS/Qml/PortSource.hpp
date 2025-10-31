#pragma once
#include <QPointer>
#include <QQmlProperty>
#include <QQmlPropertyValueSource>

#include <verdigris>

namespace Process
{
class ControlInlet;
class ProcessModel;
}

namespace JS
{

struct PortSource
    : public QObject
    , public QQmlPropertyValueSource
{
  W_OBJECT(PortSource)
  W_INTERFACE(QQmlPropertyValueSource)
  QML_ELEMENT

public:
  explicit PortSource(QObject* parent = nullptr);
  ~PortSource();

  void setTarget(const QQmlProperty& prop) override;
  void on_newUIValue();
  W_SLOT(on_newUIValue);

  INLINE_PROPERTY_CREF(QString, process, = "", process, setProcess, processChanged)
  INLINE_PROPERTY_CREF(QVariant, port, = "", port, setPort, portChanged)

private:
  Process::ProcessModel* processInstance() const noexcept;
  void rebuild();
  QQmlProperty m_targetProperty;
  QPointer<Process::ControlInlet> m_inlet{};
  bool m_writingValue{};
};

}
