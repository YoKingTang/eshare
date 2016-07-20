#ifndef PEERFILETRANSFER_H
#define PEERFILETRANSFER_H

#include "filechunker.h"
#include <QTcpSocket>
#include <QDataStream>
#include <QString>
#include <QBuffer>
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

class PeerFileTransfer : public QObject
{
  Q_OBJECT

public:
  PeerFileTransfer(std::tuple<QString, int, QString> peer, QString file); // Client
  PeerFileTransfer(std::tuple<QString, int, QString> peer, QTcpSocket *socket, QString downloadPath); // Server
  ~PeerFileTransfer();

  void start(); // Start transferring/receiving the file

  // Negotiations to transfer files
  static const char REQUEST_SEND_PERMISSION[];
  static const char ACK_SEND_PERMISSION[];
  static const char NACK_SEND_PERMISSION[];

private:
  TransferType m_type;

  bool m_authorized = false;
  std::unique_ptr<QTimer> m_authorizationTimer;

  std::tuple<QString, int, QString> m_peer;
  QString m_file; // If this is a client transfer
  std::unique_ptr<QTcpSocket> m_socket;

  QBuffer m_buffer;
  QDataStream m_stream;
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
  void setupDestinationFilePath();

private slots:
  void clientSocketConnected();
  void clientReadReady();
  void updateClientProgress(qint64 bytesWritten);
  void error(QAbstractSocket::SocketError);
  void authTimeout();

  void updateServerProgress();

signals:
  void socketFailure();
  void timeout();
  void transferDenied();
  void fileLocked();

  void receivingComplete();
};

#endif // PEERFILETRANSFER_H
