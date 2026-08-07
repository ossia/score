#pragma once
#include <score/document/DocumentContext.hpp>
#include <score/tools/Environment.hpp>
#include <score/tools/Uri.hpp>

#include <QAbstractItemModel>
#include <QTreeView>

#include <score_plugin_library_export.h>

#include <functional>
#include <memory>
#include <vector>

namespace Library
{
/**
 * @brief The files of a score that lives on another machine.
 *
 * QFileSystemModel cannot do this, and not by accident: it is built on paths
 * this process can stat, and a listing that has to cross a socket is neither
 * synchronous nor local. That is the same reason score::Environment is
 * asynchronous, so this is the model that fits it.
 *
 * Listings are fetched when a folder is first expanded and kept afterwards.
 * Nothing is polled: the other machine's disk is not being watched, so a folder
 * shows what it held when it was opened. Collapsing and expanding again asks
 * afresh, which is the cheapest honest refresh available.
 */
class SCORE_PLUGIN_LIBRARY_EXPORT RemoteFileSystemModel final
    : public QAbstractItemModel
{
public:
  //! How to reach the environment, not the environment itself: a session
  //! replaces it after the panel has been told the document exists.
  using EnvironmentSource = std::function<score::Environment*()>;

  RemoteFileSystemModel(EnvironmentSource env, QObject* parent);
  ~RemoteFileSystemModel() override;

  //! Show this folder, and forget anything shown before.
  void setRoot(const score::Uri& uri);

  //! What a row names, for whoever wants to open or drop it.
  score::Uri uriAt(const QModelIndex& index) const;
  bool isDirectory(const QModelIndex& index) const;

  QModelIndex
  index(int row, int column, const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& index) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QVariant
  headerData(int section, Qt::Orientation orientation, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  bool hasChildren(const QModelIndex& parent) const override;
  bool canFetchMore(const QModelIndex& parent) const override;
  void fetchMore(const QModelIndex& parent) override;

  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QModelIndexList& indexes) const override;

private:
  struct Entry
  {
    score::Uri uri;
    QString name;
    bool directory{};
    qint64 size{};

    Entry* parent{};
    std::vector<std::unique_ptr<Entry>> children;

    //! Asked for, so that a folder that is genuinely empty is not asked about
    //! again every time the view repaints.
    bool requested{};
    bool listed{};
  };

  Entry* entryOf(const QModelIndex& index) const;
  QModelIndex indexOf(Entry& e) const;

  EnvironmentSource m_env;
  std::unique_ptr<Entry> m_root;
};
}

namespace Library
{
/**
 * @brief Browsing the library of the machine a score runs on.
 *
 * Deliberately not SystemLibraryWidget with a different model: that one is
 * built around QFileSystemModel's filePath() and a proxy pinned to a root
 * index, neither of which a remote listing has. Dragging out of it works,
 * which is what the library is for.
 */
class SCORE_PLUGIN_LIBRARY_EXPORT RemoteLibraryWidget final : public QTreeView
{
public:
  RemoteLibraryWidget(QWidget* parent);
  ~RemoteLibraryWidget() override;

  //! Show `root` as seen through `env`. Replaces whatever was shown.
  void browse(RemoteFileSystemModel::EnvironmentSource env, const score::Uri& root);

  //! Nothing to show -- no document, or one whose files are on this machine.
  void clear();

private:
  RemoteFileSystemModel* m_model{};
};
}
