#include <core/application/MinimalApplication.hpp>

#include <QFile>
#include <QTextStream>
#include <JitCpp/AddonCompiler.hpp>


extern "C" Q_DECL_EXPORT
void blah()
{
    QTextStream stream(stdout);
    stream << "Hello from host function\n";
    stream.flush();
}

Q_DECL_EXPORT
void mangled_blah()
{
    QTextStream stream(stdout);
    stream << "Hello from mangled host function\n";
    stream.flush();
}
class Q_DECL_EXPORT foo {
public:
  foo();
  virtual ~foo();
};

foo::foo() {
  QTextStream stream(stdout);
  stream << "Hello foo\n";
  stream.flush();
}
foo::~foo() {
  QTextStream stream(stdout);
  stream << "Goodbye foo\n";
  stream.flush();
}
void on_startup()
{
  {
    auto compiler = Jit::makeCustomCompiler("benchmark_main");
    auto res = compiler(
        R"_(
      #include <iostream>

        class foo {
        public:
          foo();
          virtual ~foo();
        };

      extern "C" void blah();
      void mangled_blah();
      extern "C" [[dllexport]]
      void benchmark_main() {
        std::cout << "Hello world" << std::endl;
        blah();
        mangled_blah();
        foo f;
      }
    )_",
        {});
    res();
  }
/*
  {
    auto compiler = Jit::makeCustomCompiler("benchmark_main");
    QFile f{"/tmp/bench.cpp"};
    f.open(QIODevice::ReadOnly);
    auto src = f.readAll().toStdString();

    auto res = compiler(
        src,
        {"-I/home/jcelerier/travail/kfr/include",
         "-D_ENABLE_EXTENDED_ALIGNED_STORAGE"});
    res();
  }
*/
  qApp->exit(0);
}


extern "C"
Q_DECL_EXPORT
int bench_main(int argc, char** argv)
{
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");
  score::MinimalApplication app(argc, argv);

  QMetaObject::invokeMethod(&app, on_startup, Qt::QueuedConnection);
  return app.exec();
}
