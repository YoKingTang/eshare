#ifndef PEERFILETRANSFER_H
#define PEERFILETRANSFER_H

#include "filechunker.h"
#include <QTcpSocket>
#include <QThread>
#include <QFileInfo>
#include <QDir>
#include <QString>
#include <QTimer>
#include <memory>
#include <tuple>

enum TransferType { CLIENT /* This endpoint is sending a file */,
                    SERVER /* This endpoint is receiving a file */};

enum ClientSocketState {
  UNAUTHORIZED,
  SENDING_HEADER_SIZE_AND_HEADER,
  SENDING_CHUNKS,
  SENDING_FINISHED
};

enum ServerSocketState {
  AUTHORIZATION_SENT,
  RECEIVING_HEADER_SIZE_AND_HEADER,
  RECEIVING_HEADER,
  RECEIVING_CHUNKS,
  RECEIVING_FINISHED
};

struct FileHeader {
  QString filePath;
  qint64 fileSize;
};

class MainWindow;
class PeerThreadTransfer;

class PeerFileTransfer : public QObject
{
  Q_OBJECT

public:
  PeerFileTransfer(QThread *parent, MainWindow *mainWindow, size_t peerIndex, std::tuple<QString, int, QString> peer,
                   QString file); // Client
  PeerFileTransfer(QThread *parent, MainWindow *mainWindow, size_t peerIndex, std::tuple<QString, int, QString> peer,
                   qintptr socketDescriptor, QString downloadPath); // Server
  ~PeerFileTransfer();

  void execute(); // Start the transfer (i.e. transmitting/receiving the file)

  // Negotiations to transfer files
  static const char REQUEST_SEND_PERMISSION[];
  static const char ACK_SEND_PERMISSION[];
  static const char NACK_SEND_PERMISSION[];

private:
  friend class PeerThreadTransfer;

  QThread *m_parentThread = nullptr;
  MainWindow *m_mainWindow = nullptr;
  TransferType m_type;

  std::unique_ptr<QTimer> m_authorizationTimer;

  size_t m_peerIndex = -1;
  std::tuple<QString, int, QString> m_peer;
  QString m_file; // If this is a client transfer
  std::unique_ptr<QTcpSocket> m_socket;

  std::unique_ptr<FileChunker> m_chunker;
  QByteArray m_serializedHeaderBytes;

  // Client
  ClientSocketState m_clientState = UNAUTHORIZED;
  qint64 m_serializedHeaderSize = 0;
  FileHeader m_fileHeader;
  qint64 m_bytesToWrite = 0;
  qint64 m_bytesWritten = 0;

  // Server
  ServerSocketState m_serverState = AUTHORIZATION_SENT;
  qint64 m_bytesToRead = 0;
  qint64 m_bytesRead = 0;
  QString m_downloadPath; // Folder where to save files

  // Global to both
  int m_completionPercentage = 0;

  static int NEW_TRANSFER_NUMBER; // DEBUG
  int TRANSFER_NUMBER; // DEBUG

private slots:
  // Client only
  void clientSocketConnected();
  void clientReadReady();
  void updateClientProgress(qint64 bytesWritten);  
  void authTimeout();

  // Server only
  void updateServerProgress();

  // Both
  void error(QAbstractSocket::SocketError);
  void onDisconnected();

signals:
  void socketFailure();
  void timeout();
  void transferDenied();
  void fileLocked();
  void fileSentPercentage(int);
  void serverPeerWentOffline();

  void receivingComplete();
  void fileReceivedPercentage(int);
  // Destination filename has been received from header and downloadpath
  void destinationAvailable(QString destination);  

public:
  // Getters and status queries

  inline TransferType getType() const {
    return m_type;
  }
  inline int getCompletionPercentage() const { // [0;100]
    return m_completionPercentage;
  }
  QString getProgressAsString() const { // e.g. "1.2 GB / 12 GB"
    // 1.2 GB / 12 GB
    // ^^^^^^   ^^^^^
    // size     totalSize

    QString size = "0 KB";
    QString totalSize = "[N/A]";

    if (m_chunker) {
      totalSize = FileChunker::formatSizeHuman(m_chunker->getFileSize());
    }

    if (m_type == CLIENT && m_clientState >= SENDING_HEADER_SIZE_AND_HEADER) {
      // Chunker is active at this point in read-only mode
      size = FileChunker::formatSizeHuman (m_bytesWritten);
    } else if (m_type == SERVER && m_serverState >= RECEIVING_CHUNKS) {
      // Chunker is active at this point in read/write mode
      size = FileChunker::formatSizeHuman (m_bytesRead);
    }

    return size + " / " + totalSize;
  }

  inline std::tuple<QString, int, QString> getPeer() const {
    return m_peer;
  }

  QString getServerDestinationFilePath() const {
    if (m_type == SERVER && m_serverState >= RECEIVING_CHUNKS) {
      QFileInfo fileInfo(QFile(m_fileHeader.filePath).fileName());
      QString filename(fileInfo.fileName());
      return m_downloadPath + QDir::separator() + filename;
    }
    return QString();
  }

  QString getClientSourceFilePath() const {
    if (m_type == CLIENT) {
      return m_file;
    }
    return QString();
  }
};

#endif // PEERFILETRANSFER_H
