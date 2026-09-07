#include "ImportedUi.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJSValue>
#include <QMetaMethod>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlIncubator>
#include <QScopedValueRollback>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QVarLengthArray>

#include <cmath>

#include <algorithm>
#include <optional>
#include <utility>

namespace JS
{
namespace
{
// Never retain an engine-bound QJSValue in the public value/event contract.
std::optional<QVariant> plainValue(const QVariant& value)
{
  if(value.metaType() == QMetaType::fromType<QJSValue>())
  {
    auto converted = value.value<QJSValue>().toVariant();
    if(converted.metaType() == QMetaType::fromType<QJSValue>())
      return std::nullopt;
    return plainValue(converted);
  }
  if(value.metaType() == QMetaType::fromType<QVariantMap>())
  {
    auto map = value.toMap();
    for(auto it = map.begin(); it != map.end(); ++it)
    {
      auto converted = plainValue(it.value());
      if(!converted)
        return std::nullopt;
      it.value() = std::move(*converted);
    }
    return map;
  }
  if(value.metaType() == QMetaType::fromType<QVariantList>())
  {
    auto list = value.toList();
    for(auto& entry : list)
    {
      auto converted = plainValue(entry);
      if(!converted)
        return std::nullopt;
      entry = std::move(*converted);
    }
    return list;
  }
  return value;
}

QString qmlErrors(const QList<QQmlError>& errors)
{
  QStringList lines;
  for(const auto& error : errors)
    lines.push_back(error.toString());
  return lines.join(QLatin1Char('\n'));
}

// Cancellation here precedes componentComplete, including Window visibility and
// Component.onCompleted. QQmlIncubator::clear supports recursive cancellation.
class ItemIncubator final : public QQmlIncubator
{
public:
  ItemIncubator()
      : QQmlIncubator{Synchronous}
  {
  }
  QString rejection;
  QPointer<QObject> rejectedObject;

protected:
  void setInitialState(QObject* object) override
  {
    if(qobject_cast<QQuickItem*>(object))
      return;
    rejection
        = QStringLiteral(
              "The entry root is %1, not a Qt Quick Item. Select the exported content "
              "form (.ui.qml or Item-based .qml), not the Window/ApplicationWindow "
              "bootstrap.")
              .arg(QString::fromLatin1(object->metaObject()->className()));
    rejectedObject = object;
    clear();
  }
};

void collectObjects(QObject* object, QSet<QObject*>& visited, QList<QObject*>& objects)
{
  if(!object || visited.contains(object))
    return;
  visited.insert(object);
  objects.push_back(object);
  for(auto child : object->children())
    collectObjects(child, visited, objects);
  if(auto item = qobject_cast<QQuickItem*>(object))
    for(auto child : item->childItems())
      collectObjects(child, visited, objects);
}

QVariantMap describeObject(const QString& name, const QString& kind, QObject* object)
{
  QVariantList properties;
  QStringList signals;
  const auto meta = object->metaObject();
  for(int i = 0; i < meta->propertyCount(); ++i)
  {
    const auto property = meta->property(i);
    properties.push_back(
        QVariantMap{
            {QStringLiteral("name"), QString::fromLatin1(property.name())},
            {QStringLiteral("type"), QString::fromLatin1(property.typeName())},
            {QStringLiteral("readable"), property.isReadable()},
            {QStringLiteral("writable"), property.isWritable()},
            {QStringLiteral("notifySignal"),
             QString::fromLatin1(property.notifySignal().methodSignature())}});
  }
  for(int i = 0; i < meta->methodCount(); ++i)
  {
    const auto method = meta->method(i);
    if(method.methodType() == QMetaMethod::Signal
       && method.access() == QMetaMethod::Public)
      signals.push_back(QString::fromLatin1(method.methodSignature()));
  }
  signals.removeDuplicates();
  return {
      {QStringLiteral("name"), name},
      {QStringLiteral("kind"), kind},
      {QStringLiteral("className"), QString::fromLatin1(meta->className())},
      {QStringLiteral("properties"), properties},
      {QStringLiteral("signals"), signals}};
}
}

ImportedUi::ImportedUi(QQuickItem* parent)
    : QQuickItem{parent}
    , m_viewport{new QQuickItem{this}}
{
  m_viewport->setTransformOrigin(QQuickItem::TopLeft);
}

ImportedUi::~ImportedUi()
{
  m_destroying = true;
  clearLoaded();
}

void ImportedUi::setSource(const QUrl& source)
{
  if(m_source == source)
    return;
  m_source = source;
  scheduleUpdate(true);
  Q_EMIT sourceChanged();
}

void ImportedUi::setImportPaths(const QStringList& paths)
{
  if(m_importPaths == paths)
    return;
  m_importPaths = paths;
  scheduleUpdate(true);
  Q_EMIT importPathsChanged();
}

void ImportedUi::setMapping(const QVariantList& mapping)
{
  auto converted = plainValue(mapping);
  if(!converted)
  {
    setState(
        QStringLiteral("error"),
        QStringLiteral("Mapping contains an unsupported JavaScript value."));
    return;
  }
  auto normalized = converted->toList();
  if(m_mapping == normalized)
  {
    if(m_status == QStringLiteral("error"))
      scheduleUpdate(false);
    return;
  }
  m_mapping = std::move(normalized);
  scheduleUpdate(false);
  Q_EMIT mappingChanged();
}

void ImportedUi::setValues(const QVariantMap& values)
{
  auto converted = plainValue(values);
  if(!converted)
  {
    setState(
        QStringLiteral("error"),
        QStringLiteral("Input values contain an unsupported JavaScript value."));
    return;
  }
  auto normalized = converted->toMap();
  const bool changed = m_values != normalized;
  if(changed)
    m_values = std::move(normalized);
  if(m_mappingValid && !m_updatePending && !m_applyingValues
     && (changed || m_status == QStringLiteral("error")))
  {
    if(m_dispatchingOutputs)
      m_valuesPending = true;
    else
    {
      const auto error = applyValues();
      setState(
          error.isEmpty() ? QStringLiteral("ready") : QStringLiteral("error"), error);
    }
  }
  if(changed)
    Q_EMIT valuesChanged();
}

void ImportedUi::componentComplete()
{
  QQuickItem::componentComplete();
  m_complete = true;
  scheduleUpdate(true);
}

void ImportedUi::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  fit();
}

void ImportedUi::scheduleUpdate(bool reload)
{
  if(m_destroying)
    return;
  m_reloadPending |= reload;
  if(!m_complete || m_updatePending)
    return;
  m_updatePending = true;
  // Coalesce QML property assignments, and never destroy a component from inside
  // its own callback or an imported control's signal emission.
  QTimer::singleShot(0, this, [this] {
    m_updatePending = false;
    if(std::exchange(m_reloadPending, false))
      reloadNow();
    else if(m_root)
      configureMappings();
  });
}

void ImportedUi::setState(const QString& status, const QString& error)
{
  const bool changed = m_status != status;
  const bool errorChanged = m_error != error;
  m_status = status;
  m_error = error;
  if(errorChanged)
    Q_EMIT errorStringChanged();
  if(changed)
    Q_EMIT statusChanged();
}

void ImportedUi::clearBindings()
{
  m_mappingValid = false;
  m_valuesPending = false;
  for(const auto& connection : m_connections)
    QObject::disconnect(connection);
  m_connections.clear();
  m_bindings.clear();
}

void ImportedUi::clearLoaded()
{
  Q_ASSERT(QThread::currentThread() == thread());
  clearBindings();
  if(m_component)
    QObject::disconnect(m_component.get(), nullptr, this, nullptr);
  // Item deletion belongs to its owning thread. Qt Quick retires scene graph
  // resources itself; neither this engine nor its objects move between threads.
  if(m_root)
  {
    auto root = m_root.data();
    m_root = nullptr;
    root->setParentItem(nullptr);
    delete root;
  }
  m_component.reset();
  m_engine.reset();
  const bool hadSize = m_designSize != QSizeF{};
  const bool hadObjects = !m_objects.isEmpty();
  m_designSize = {};
  m_objects.clear();
  if(!m_destroying)
  {
    if(hadSize)
      Q_EMIT designSizeChanged();
    if(hadObjects)
      Q_EMIT objectsChanged();
  }
}

void ImportedUi::reload()
{
  scheduleUpdate(true);
}

void ImportedUi::reloadNow()
{
  clearLoaded();
  if(m_source.isEmpty())
  {
    setState(QStringLiteral("empty"));
    return;
  }
  auto url = m_source;
  if(url.isRelative())
  {
    if(auto context = qmlContext(this))
      url = context->resolvedUrl(url);
  }
  if(!url.isValid() || url.isRelative())
  {
    setState(
        QStringLiteral("error"),
        QStringLiteral("The QML entry must resolve to an absolute URL: %1")
            .arg(m_source.toString()));
    return;
  }
  for(const auto& path : m_importPaths)
  {
    if(!QDir::isAbsolutePath(path) || !QFileInfo{path}.isDir())
    {
      setState(
          QStringLiteral("error"),
          QStringLiteral("Import path must be an existing absolute directory: %1")
              .arg(path));
      return;
    }
  }
  setState(QStringLiteral("loading"));
  m_engine = std::make_unique<QQmlEngine>();
  for(auto it = m_importPaths.crbegin(); it != m_importPaths.crend(); ++it)
    m_engine->addImportPath(*it);
  m_component = std::make_unique<QQmlComponent>(m_engine.get());
  m_component->loadUrl(url, QQmlComponent::Asynchronous);
  if(m_component->isLoading())
  {
    QObject::connect(
        m_component.get(), &QQmlComponent::statusChanged, this,
        [this](QQmlComponent::Status status) {
      if(status != QQmlComponent::Loading)
        finishLoading();
    });
  }
  else
    finishLoading();
}

void ImportedUi::finishLoading()
{
  if(!m_component || m_reloadPending)
    return;
  QObject::disconnect(m_component.get(), nullptr, this, nullptr);
  if(m_component->isError())
  {
    setState(QStringLiteral("error"), qmlErrors(m_component->errors()));
    return;
  }
  if(!m_component->isReady())
    return;
  ItemIncubator incubator;
  m_component->create(incubator, m_engine->rootContext());
  if(!incubator.rejection.isEmpty())
  {
    // clear() may defer deletion; dispose of a rejected object before its engine.
    delete incubator.rejectedObject.data();
    setState(QStringLiteral("error"), incubator.rejection);
    return;
  }
  if(!incubator.isReady())
  {
    auto error = qmlErrors(incubator.errors());
    if(error.isEmpty())
      error = QStringLiteral("QML entry creation did not produce an Item.");
    setState(QStringLiteral("error"), error);
    return;
  }
  m_root = qobject_cast<QQuickItem*>(incubator.object());
  QQmlEngine::setObjectOwnership(m_root, QQmlEngine::CppOwnership);
  m_root->setParent(m_viewport);
  const qreal w = m_root->width() > 0. ? m_root->width() : m_root->implicitWidth();
  const qreal h = m_root->height() > 0. ? m_root->height() : m_root->implicitHeight();
  if(!std::isfinite(w) || !std::isfinite(h) || w <= 0. || h <= 0.)
  {
    setState(
        QStringLiteral("error"),
        QStringLiteral(
            "The content Item needs positive finite width/height or "
            "implicitWidth/implicitHeight to define its native design size."));
    return;
  }
  m_designSize = QSizeF{w, h};
  m_viewport->setSize(m_designSize);
  if(m_root->width() <= 0.)
    m_root->setWidth(w);
  if(m_root->height() <= 0.)
    m_root->setHeight(h);
  m_root->setParentItem(m_viewport);
  fit();
  Q_EMIT designSizeChanged();
  configureMappings();
}

void ImportedUi::fit()
{
  if(m_designSize.width() <= 0. || m_designSize.height() <= 0.)
    return;
  const qreal scale = std::max(
      qreal{0.},
      std::min(width() / m_designSize.width(), height() / m_designSize.height()));
  m_viewport->setScale(scale);
  m_viewport->setPosition(
      QPointF{
          (width() - m_designSize.width() * scale) / 2.,
          (height() - m_designSize.height() * scale) / 2.});
}

void ImportedUi::configureMappings()
{
  clearBindings();
  if(!m_root || m_designSize.isEmpty())
    return;
  QMap<QString, QObject*> aliases;
  QMap<QString, QList<QObject*>> named;
  QList<QObject*> descendants;
  QSet<QObject*> visited;
  collectObjects(m_root, visited, descendants);
  for(auto object : std::as_const(descendants))
    if(!object->objectName().isEmpty())
      named[object->objectName()].push_back(object);

  const auto rootMeta = m_root->metaObject();
  // Public object-valued QML properties include aliases declared by base forms.
  // Do not expose inherited implementation pointers such as Item.parent.
  for(int i = QQuickItem::staticMetaObject.propertyCount();
      i < rootMeta->propertyCount(); ++i)
  {
    const auto property = rootMeta->property(i);
    if(property.isReadable())
    {
      const auto value = property.read(m_root);
      if(auto object = value.value<QObject*>())
        aliases.insert(QString::fromLatin1(property.name()), object);
    }
  }
  QVariantList discovered;
  discovered.push_back(
      describeObject(QStringLiteral("."), QStringLiteral("root"), m_root));
  for(auto it = aliases.cbegin(); it != aliases.cend(); ++it)
    discovered.push_back(describeObject(it.key(), QStringLiteral("alias"), it.value()));
  for(auto it = named.cbegin(); it != named.cend(); ++it)
    if(it.value().size() == 1 && !aliases.contains(it.key())
       && it.key() != QStringLiteral("."))
      discovered.push_back(
          describeObject(it.key(), QStringLiteral("objectName"), it.value().front()));
  if(m_objects != discovered)
  {
    m_objects = std::move(discovered);
    Q_EMIT objectsChanged();
  }

  QSet<QString> names;
  const auto fail
      = [this](qsizetype index, const QString& name, const QString& message) {
    clearBindings();
    setState(
        QStringLiteral("error"),
        QStringLiteral("Mapping %1 (%2): %3").arg(index + 1).arg(name, message));
  };
  for(qsizetype index = 0; index < m_mapping.size(); ++index)
  {
    const auto& entry = m_mapping[index];
    if(entry.metaType() != QMetaType::fromType<QVariantMap>())
    {
      fail(
          index, {},
          QStringLiteral("expected an object with name, object and direction fields."));
      return;
    }
    const auto map = entry.toMap();
    const auto name = map.value(QStringLiteral("name")).toString();
    const auto targetName = map.value(QStringLiteral("object")).toString();
    const auto direction = map.value(QStringLiteral("direction")).toString();
    const auto propertyName = map.value(QStringLiteral("property")).toString();
    const auto signalName = map.value(QStringLiteral("signal")).toString();
    if(name.isEmpty() || names.contains(name))
    {
      fail(index, name, QStringLiteral("name must be nonempty and unique."));
      return;
    }
    names.insert(name);
    const bool input
        = direction == QStringLiteral("input") || direction == QStringLiteral("both");
    const bool output = direction == QStringLiteral("output")
                        || direction == QStringLiteral("both")
                        || direction == QStringLiteral("event");
    if(!input && !output)
    {
      fail(
          index, name,
          QStringLiteral("direction must be input, output, both or event."));
      return;
    }
    QObject* target{};
    if(targetName == QStringLiteral("."))
      target = m_root;
    else if(aliases.contains(targetName))
      target = aliases.value(targetName);
    else if(named.value(targetName).size() == 1)
      target = named.value(targetName).front();
    else
    {
      fail(
          index, name,
          named.value(targetName).size() > 1
              ? QStringLiteral(
                    "objectName '%1' is ambiguous; expose a public root object alias.")
                    .arg(targetName)
              : QStringLiteral(
                    "target '%1' was not found. Use '.' for the root, a public root "
                    "object alias, or a unique objectName; a QML id alone is private.")
                    .arg(targetName));
      return;
    }
    if(target->thread() != thread())
    {
      fail(index, name, QStringLiteral("target belongs to another thread."));
      return;
    }
    Binding binding;
    binding.name = name;
    binding.target = target;
    binding.input = input;
    binding.output = output;
    const auto meta = target->metaObject();
    if(!propertyName.isEmpty())
      binding.property
          = meta->property(meta->indexOfProperty(propertyName.toUtf8().constData()));
    if((direction != QStringLiteral("event") || !propertyName.isEmpty())
       && !binding.property.isValid())
    {
      fail(
          index, name,
          QStringLiteral("property '%1' does not exist on '%2'.")
              .arg(propertyName, targetName));
      return;
    }
    if(binding.property.isValid()
       && ((input && !binding.property.isWritable())
           || (output && !binding.property.isReadable())))
    {
      fail(
          index, name,
          QStringLiteral("property '%1' must be %2.")
              .arg(
                  propertyName, input && output ? QStringLiteral("readable and writable")
                                : input         ? QStringLiteral("writable")
                                                : QStringLiteral("readable")));
      return;
    }
    if(output)
    {
      QMetaMethod signal;
      const auto encoded = signalName.toUtf8();
      if(encoded.contains('('))
        signal = meta->method(meta->indexOfSignal(
            QMetaObject::normalizedSignature(encoded.constData()).constData()));
      else
      {
        for(int i = 0; i < meta->methodCount(); ++i)
        {
          const auto candidate = meta->method(i);
          if(candidate.methodType() == QMetaMethod::Signal
             && candidate.name() == encoded)
          {
            if(signal.isValid()
               && signal.methodSignature() != candidate.methodSignature())
            {
              fail(
                  index, name,
                  QStringLiteral(
                      "signal '%1' is overloaded; specify its complete signature.")
                      .arg(signalName));
              return;
            }
            signal = candidate;
          }
        }
      }
      if(!signal.isValid() || signal.methodType() != QMetaMethod::Signal
         || signal.access() != QMetaMethod::Public)
      {
        fail(
            index, name,
            QStringLiteral("public signal '%1' does not exist on '%2'.")
                .arg(signalName, targetName));
        return;
      }
      binding.signalIndex = signal.methodIndex();
      const auto slot
          = metaObject()->method(metaObject()->indexOfSlot("mappedSignal()"));
      // One connection per sender/signal; the slot dispatches all matching names.
      const auto connection = QObject::connect(
          target, signal, this, slot,
          static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
      if(connection)
        m_connections.push_back(connection);
      else if(
          std::none_of(
              m_bindings.cbegin(), m_bindings.cend(), [&](const Binding& existing) {
        return existing.target == target && existing.signalIndex == binding.signalIndex;
      }))
      {
        fail(
            index, name,
            QStringLiteral("could not connect signal '%1'.").arg(signalName));
        return;
      }
    }
    m_bindings.push_back(std::move(binding));
  }
  // Report the loss of dynamic delegates/aliases instead of silently using a
  // stale object. Reconfiguration resolves all targets again on the next turn.
  QSet<QObject*> watched;
  for(const auto& binding : m_bindings)
  {
    if(watched.contains(binding.target))
      continue;
    watched.insert(binding.target);
    m_connections.push_back(
        QObject::connect(binding.target, &QObject::destroyed, this, [this] {
      scheduleUpdate(false);
    }));
  }
  m_mappingValid = true;
  const auto error = applyValues();
  setState(error.isEmpty() ? QStringLiteral("ready") : QStringLiteral("error"), error);
}

QString ImportedUi::applyValues()
{
  QScopedValueRollback guard{m_applyingValues, true};
  for(const auto& binding : m_bindings)
  {
    if(!binding.input)
      continue;
    const auto it = m_values.constFind(binding.name);
    if(it == m_values.cend())
      continue;
    if(!binding.target)
      return QStringLiteral(
                 "Input '%1': the mapped object was destroyed; expose a stable control "
                 "alias.")
          .arg(binding.name);
    if(!binding.property.write(binding.target, it.value()))
      return QStringLiteral(
                 "Input '%1': cannot write value of type %2 to property '%3' (%4).")
          .arg(
              binding.name,
              QString::fromLatin1(
                  it.value().typeName() ? it.value().typeName() : "null"),
              QString::fromLatin1(binding.property.name()),
              QString::fromLatin1(binding.property.typeName()));
  }
  return {};
}

void ImportedUi::mappedSignal()
{
  if(m_applyingValues || !m_mappingValid || m_updatePending || m_destroying)
    return;
  const auto object = sender();
  const auto index = senderSignalIndex();
  // Snapshot before calling consumers: their feedback may write other mappings.
  QVarLengthArray<std::pair<QString, QVariant>, 4> outputs;
  for(const auto& binding : m_bindings)
  {
    if(binding.output && binding.target == object && binding.signalIndex == index)
    {
      auto value = binding.property.isValid() ? plainValue(binding.property.read(object))
                                              : std::optional<QVariant>{QVariant{true}};
      if(!value)
      {
        setState(
            QStringLiteral("error"),
            QStringLiteral("Output '%1' contains an unsupported JavaScript value.")
                .arg(binding.name));
        return;
      }
      outputs.emplace_back(binding.name, std::move(*value));
    }
  }
  const QPointer<ImportedUi> alive{this};
  const bool wasDispatching = std::exchange(m_dispatchingOutputs, true);
  for(const auto& [name, value] : outputs)
  {
    Q_EMIT event(name, value);
    if(!alive)
      return;
    if(m_updatePending)
      break;
  }
  m_dispatchingOutputs = wasDispatching;
  if(!m_dispatchingOutputs && std::exchange(m_valuesPending, false) && m_mappingValid
     && !m_updatePending)
  {
    const auto error = applyValues();
    setState(error.isEmpty() ? QStringLiteral("ready") : QStringLiteral("error"), error);
  }
}
}
