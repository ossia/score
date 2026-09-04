#pragma once
#include <score/tools/Uri.hpp>

#include <QByteArray>
#include <QString>

#include <score_lib_base_export.h>

#include <functional>
#include <vector>

namespace score
{
struct DocumentContext;

//! One entry of a listing.
struct DirEntry
{
  Uri uri;
  QString name;
  bool directory{};
  qint64 size{};
};

/**
 * @brief Where the files of a score actually are.
 *
 * A score refers to things -- sound files, shaders, the project folder -- that
 * live on a machine. Usually that is this machine, and reading one is a matter
 * of opening a path. But the machine running the score need not be the one
 * being typed at: a score playing on a headless box is edited from a laptop,
 * and score in a browser has no filesystem at all.
 *
 * So the question "give me the bytes of this" has more than one answer, and
 * code that wants them should ask rather than assume. That is all this is: the
 * asking, separated from the answering.
 *
 * Every call is asynchronous, including the ones a local implementation could
 * answer immediately. Not because a local read is slow, but because a remote
 * one cannot be made synchronous -- and a browser cannot even open a file
 * picker without returning first. An interface that let callers wait would be
 * an interface only the local implementation could satisfy.
 */
class SCORE_LIB_BASE_EXPORT Environment
{
public:
  template <typename T>
  using Callback = std::function<void(T)>;

  //! Reports why something could not be done.
  using Failure = QString;

  //! For a call whose success carries no value of its own.
  using Done = std::function<void()>;

  virtual ~Environment();

  //! Whether these files are reachable as paths by this process. False when
  //! they are on another machine, which is what decides whether code may take
  //! the shortcut of opening one directly.
  virtual bool isLocal() const noexcept = 0;

  //! Where this is on the local filesystem, or empty when it is not there.
  //! Only meaningful when isLocal().
  virtual QString resolve(const Uri& uri) const = 0;

  virtual void
  list(const Uri& uri, Callback<std::vector<DirEntry>> onListed,
       Callback<Failure> onFailed = {})
      = 0;

  virtual void
  read(const Uri& uri, Callback<QByteArray> onRead, Callback<Failure> onFailed = {})
      = 0;

  virtual void write(
      const Uri& uri, QByteArray data, Done onWritten, Callback<Failure> onFailed = {})
      = 0;
};

/**
 * @brief The files are here, on this machine.
 *
 * What score has always done, behind the interface. The callbacks are invoked
 * before the call returns; nothing is queued.
 */
class SCORE_LIB_BASE_EXPORT LocalEnvironment final : public Environment
{
public:
  explicit LocalEnvironment(const DocumentContext& ctx);
  ~LocalEnvironment() override;

  bool isLocal() const noexcept override { return true; }
  QString resolve(const Uri& uri) const override;

  void
  list(const Uri& uri, Callback<std::vector<DirEntry>> onListed,
       Callback<Failure> onFailed) override;
  void
  read(const Uri& uri, Callback<QByteArray> onRead, Callback<Failure> onFailed) override;
  void write(
      const Uri& uri, QByteArray data, Done onWritten,
      Callback<Failure> onFailed) override;

private:
  const DocumentContext& m_ctx;
};
}
