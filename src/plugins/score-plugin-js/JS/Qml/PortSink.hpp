#pragma once
#include <QPointer>
#include <QQmlProperty>
#include <QQmlPropertyValueSource>

#include <verdigris>

namespace Process
{
class Outlet;
class ProcessModel;
}

namespace JS
{

struct PortSink
    : public QObject
    , public QQmlPropertyValueSource
{
  W_OBJECT(PortSink)
  W_INTERFACE(QQmlPropertyValueSource)
  QML_ELEMENT

public:
  explicit PortSink(QObject* parent = nullptr);
  ~PortSink();

  void setTarget(const QQmlProperty& prop) override;

  INLINE_PROPERTY_CREF(QString, process, = "", process, setProcess, processChanged)
  INLINE_PROPERTY_CREF(QVariant, port, = "", port, setPort, portChanged)

private:
  Process::ProcessModel* processInstance() const noexcept;
  void rebuild();
  QQmlProperty m_targetProperty;
  QPointer<Process::Outlet> m_outlet{};
};

}
