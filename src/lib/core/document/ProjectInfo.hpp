#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsFactory.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsPresenter.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsView.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QImage>
#include <QSize>
#include <QString>

#include <score_lib_base_export.h>

#include <optional>
#include <verdigris>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QCheckBox;

namespace score
{
class FormWidget;
}

/**
 * Project-wide information stored inside every .score file:
 * name, author, description, web page, dates and a thumbnail.
 *
 * It is a regular project settings plug-in, so it shows up as the "Project"
 * page of the Project Settings dialog and travels with the document like the
 * other project settings. The thumbnail is a PNG stored as base64 in the JSON.
 * By default it is refreshed from a capture of the document view every time
 * the document is saved; the user can instead pick a fixed image.
 *
 * ProjectInfo::peek() reads this information back from a .score file on disk
 * without loading the document, for the start screen and similar browsers.
 */
namespace score::ProjectInfo
{
class Model;
}

UUID_METADATA(
    , score::DocumentPluginFactory, score::ProjectInfo::Model,
    "aac1402f-6851-4a1a-ab50-5f213acd1262")

namespace score::ProjectInfo
{
//! Same uuid as above, as text, for the on-disk lookup in peek().
inline constexpr const char* uuid_string = "aac1402f-6851-4a1a-ab50-5f213acd1262";

class SCORE_LIB_BASE_EXPORT Model final : public score::ProjectSettingsModel
{
  W_OBJECT(Model)
  SCORE_SERIALIZE_FRIENDS
  MODEL_METADATA_IMPL(Model)

  QString m_Name;
  QString m_Author;
  QString m_Description;
  QString m_Url; // web page of the project: documentation, tutorial...
  QDateTime m_Created;
  QDateTime m_LastSaved;
  QByteArray m_Thumbnail; // PNG data, empty if none
  bool m_AutomaticThumbnail{true};

public:
  Model(const score::DocumentContext&, QObject* parent);
  ~Model() override;

  template <typename Impl>
  Model(const score::DocumentContext& ctx, Impl& vis, QObject* parent)
      : score::ProjectSettingsModel{ctx, vis, parent}
  {
    vis.writeTo(*this);
  }

  //! Size of the stored thumbnail, 16:10 like the document view.
  static constexpr QSize thumbnailSize{480, 300};

  //! Scales and crops an image to thumbnailSize and encodes it as PNG.
  static QByteArray encodeThumbnail(const QImage& img);

  QImage thumbnailImage() const;

  SCORE_SETTINGS_PARAMETER_HPP(SCORE_LIB_BASE_EXPORT, QString, Name)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_LIB_BASE_EXPORT, QString, Author)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_LIB_BASE_EXPORT, QString, Description)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_LIB_BASE_EXPORT, QString, Url)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_LIB_BASE_EXPORT, QDateTime, Created)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_LIB_BASE_EXPORT, QDateTime, LastSaved)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_LIB_BASE_EXPORT, QByteArray, Thumbnail)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_LIB_BASE_EXPORT, bool, AutomaticThumbnail)
};

SCORE_SETTINGS_PARAMETER(Model, Name)
SCORE_SETTINGS_PARAMETER(Model, Author)
SCORE_SETTINGS_PARAMETER(Model, Description)
SCORE_SETTINGS_PARAMETER(Model, Url)
SCORE_SETTINGS_PARAMETER(Model, Thumbnail)
SCORE_SETTINGS_PARAMETER(Model, AutomaticThumbnail)

class SCORE_LIB_BASE_EXPORT View final : public score::ProjectSettingsView
{
  W_OBJECT(View)
public:
  View();

  void setName(QString);
  void NameChanged(QString arg) W_SIGNAL(NameChanged, arg)
  void setAuthor(QString);
  void AuthorChanged(QString arg) W_SIGNAL(AuthorChanged, arg)
  void setDescription(QString);
  void DescriptionChanged(QString arg) W_SIGNAL(DescriptionChanged, arg)
  void setUrl(QString);
  void UrlChanged(QString arg) W_SIGNAL(UrlChanged, arg)
  void setThumbnail(QByteArray);
  void ThumbnailChanged(QByteArray arg) W_SIGNAL(ThumbnailChanged, arg)

  void setCreated(QDateTime);
  void setLastSaved(QDateTime);

  SETTINGS_UI_TOGGLE_HPP(AutomaticThumbnail)

private:
  QWidget* getWidget() override;
  void chooseThumbnail();

  score::FormWidget* m_widg{};
  QLineEdit* m_name{};
  QLineEdit* m_author{};
  QPlainTextEdit* m_description{};
  QLineEdit* m_url{};
  QLabel* m_created{};
  QLabel* m_lastSaved{};
  QLabel* m_thumbnailPreview{};
  QByteArray m_thumbnail;
};

class SCORE_LIB_BASE_EXPORT Presenter final : public score::ProjectSettingsPresenter
{
public:
  using model_type = Model;
  using view_type = View;
  Presenter(Model&, View&, QObject* parent);

private:
  QString settingsName() override;
  QIcon settingsIcon() override;
};

SCORE_DECLARE_PROJECTSETTINGS_FACTORY(
    Factory, Model, Presenter, View, "aac1402f-6851-4a1a-ab50-5f213acd1262")

/**
 * @brief What peek() extracts from a .score file.
 */
struct SCORE_LIB_BASE_EXPORT Info
{
  QString name;
  QString author;
  QString description;
  QString url;
  QDateTime created;
  QDateTime lastSaved;
  QImage thumbnail;
};

/**
 * @brief Reads the project information stored in a .score file.
 *
 * Does not load the document. Returns std::nullopt if the file cannot be
 * parsed or predates this information. Safe to call from a worker thread.
 */
SCORE_LIB_BASE_EXPORT
std::optional<Info> peek(const QString& path);
}
