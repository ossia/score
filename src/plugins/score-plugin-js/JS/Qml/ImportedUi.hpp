#pragma once

#include <QMetaProperty>
#include <QPointer>
#include <QQuickItem>
#include <QSizeF>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <vector>

class QQmlComponent;
class QQmlEngine;

namespace JS
{
// Hosts trusted generated QML in an independent engine on this item's thread.
class ImportedUi : public QQuickItem
{
  Q_OBJECT
  Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
  Q_PROPERTY(
      QStringList importPaths READ importPaths WRITE setImportPaths NOTIFY
          importPathsChanged)
  Q_PROPERTY(QVariantList mapping READ mapping WRITE setMapping NOTIFY mappingChanged)
  Q_PROPERTY(QVariantMap values READ values WRITE setValues NOTIFY valuesChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
  Q_PROPERTY(QSizeF designSize READ designSize NOTIFY designSizeChanged)
  Q_PROPERTY(QVariantList objects READ objects NOTIFY objectsChanged)

public:
  explicit ImportedUi(QQuickItem* parent = nullptr);
  ~ImportedUi() override;

  const QUrl& source() const noexcept { return m_source; }
  const QStringList& importPaths() const noexcept { return m_importPaths; }
  const QVariantList& mapping() const noexcept { return m_mapping; }
  const QVariantMap& values() const noexcept { return m_values; }
  const QString& status() const noexcept { return m_status; }
  const QString& errorString() const noexcept { return m_error; }
  QSizeF designSize() const noexcept { return m_designSize; }
  const QVariantList& objects() const noexcept { return m_objects; }

  void setSource(const QUrl& source);
  void setImportPaths(const QStringList& paths);
  // Unsupported JavaScript values report an error without replacing accepted data.
  void setMapping(const QVariantList& mapping);
  Q_INVOKABLE void reload();
  void setValues(const QVariantMap& values);

Q_SIGNALS:
  void sourceChanged();
  void importPathsChanged();
  void mappingChanged();
  void valuesChanged();
  void statusChanged();
  void errorStringChanged();
  void designSizeChanged();
  void objectsChanged();
  void event(const QString& name, const QVariant& value);

protected:
  void componentComplete() override;
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private Q_SLOTS:
  void mappedSignal();

private:
  struct Binding
  {
    QString name;
    QPointer<QObject> target;
    QMetaProperty property;
    int signalIndex{-1};
    bool input{};
    bool output{};
  };

  void scheduleUpdate(bool reload);
  void reloadNow();
  void finishLoading();
  void clearLoaded();
  void clearBindings();
  void configureMappings();
  QString applyValues();
  void fit();
  void setState(const QString& status, const QString& error = {});

  QUrl m_source;
  QStringList m_importPaths;
  QVariantList m_mapping;
  QVariantMap m_values;
  QString m_status{QStringLiteral("empty")};
  QString m_error;
  QSizeF m_designSize;
  QVariantList m_objects;
  std::unique_ptr<QQmlEngine> m_engine;
  std::unique_ptr<QQmlComponent> m_component;
  QQuickItem* m_viewport{};
  QPointer<QQuickItem> m_root;
  std::vector<Binding> m_bindings;
  std::vector<QMetaObject::Connection> m_connections;
  bool m_complete{};
  bool m_updatePending{};
  bool m_reloadPending{};
  bool m_mappingValid{};
  bool m_applyingValues{};
  bool m_dispatchingOutputs{};
  bool m_valuesPending{};
  bool m_destroying{};
};
}
