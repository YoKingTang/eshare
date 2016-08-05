#include <UI/MainWindow.h>
#include <QApplication>
#include <QCommandLineParser>
#include <QtGlobal>
#include <fstream>

static std::ofstream debug_file;

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
  if (!debug_file.is_open())
    debug_file.open("ekashare_debug.log");

  QByteArray localMsg = msg.toLocal8Bit();
  switch (type) {
  case QtDebugMsg:
      debug_file << "[Debug] " << localMsg.constData() << "(" << context.file << ":" << context.line << ", " << context.function << ")\n";
      break;
  case QtInfoMsg:
      debug_file << "[Info] " << localMsg.constData() << "(" << context.file << ":" << context.line << ", " << context.function << ")\n";
      break;
  case QtWarningMsg:
      debug_file << "[Warning] " << localMsg.constData() << "(" << context.file << ":" << context.line << ", " << context.function << ")\n";
      break;
  case QtCriticalMsg:
      debug_file << "[CRITICAL] " << localMsg.constData() << "(" << context.file << ":" << context.line << ", " << context.function << ")\n";
      break;
  case QtFatalMsg:
      debug_file << "[FATAL] " << localMsg.constData() << "(" << context.file << ":" << context.line << ", " << context.function << ")\n";
      abort();
  }
  debug_file.flush();
}

int main(int argc, char *argv[])
{    
    QApplication a(argc, argv);
    QGuiApplication::setApplicationDisplayName("eKAshare");
    QCoreApplication::setApplicationName("eshare");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("File transfer utility - copyright EKA srl");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {"debug",
            QCoreApplication::translate("main", "Stampa tutti i messaggi di debug su un file di log (ekashare_debug.log)")},
        {"background",
            QCoreApplication::translate("main", "Avvia l'applicazione in background")}
    });

    parser.process(a);


    if (parser.isSet("debug"))
      qInstallMessageHandler(myMessageOutput); // install: set the callback

    MainWindow w;
    if (!parser.isSet("background"))
      w.show();

    auto ret_code = a.exec();

    if(debug_file.is_open())
      debug_file.close();

    return ret_code;
}
