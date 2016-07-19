#include "peerfiletransfer.h"
#include <QDataStream>
#include <QFileInfo>
#include <QDir>

const char PeerFileTransfer::REQUEST_SEND_PERMISSION[] = "SEND?";
const char PeerFileTransfer::ACK_SEND_PERMISSION[] = "ACK!";
const char PeerFileTransfer::NACK_SEND_PERMISSION[] = "NOPE!";

PeerFileTransfer::PeerFileTransfer(std::tuple<QString /* Ip */, int /* port */, QString /* hostname */>
                                   peer, QString file) : // Client
  m_file(file),
  m_peer(peer)
{
  m_socket = std::make_unique<QTcpSocket>();
  m_type = CLIENT;
  connect(m_socket.get(), SIGNAL(connected()), this, SLOT(clientSocketConnected()));
  connect(m_socket.get(), SIGNAL(readyRead()), this, SLOT(clientReadReady()));
  connect(m_socket.get(), SIGNAL(bytesWritten(qint64)), this, SLOT(updateClientProgress(qint64)));
  connect(m_socket.get(), SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(socketError(QAbstractSocket::SocketError)));
}

PeerFileTransfer::PeerFileTransfer(std::tuple<QString /* Ip */, int /* port */, QString /* hostname */>
                                   peer, QTcpSocket *socket, QString downloadPath) : // Server
  m_peer(peer),
  m_downloadPath(downloadPath)
{
  //socket->state()
  m_socket.reset(socket);
  m_type = SERVER;
  connect(m_socket.get(), SIGNAL(readyRead()),
          this, SLOT(updateServerProgress()));
  connect(m_socket.get(), SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(error(QAbstractSocket::SocketError)));
}

namespace {
  QByteArray serializeHeader(const FileHeader& fileHeader) {
      QByteArray byteArray;

      QDataStream stream(&byteArray, QIODevice::WriteOnly);
      stream.setVersion(QDataStream::Qt_5_7);

      stream << fileHeader.filePath
             << fileHeader.fileSize;

      return byteArray;
  }

  FileHeader deserializeHeader(const QByteArray& byteArray) {
      QDataStream stream(byteArray);
      stream.setVersion(QDataStream::Qt_5_7);

      FileHeader result;

      stream >> result.filePath
             >> result.fileSize;

      return result;
  }
}

void PeerFileTransfer::start() {

  QString addr, hostname; int port;
  std::tie(addr, port, hostname) = m_peer;

  if (m_type == CLIENT) {

    m_clientState = UNAUTHORIZED;

    // Connect to endpoint and ask for authorization
    m_socket->connectToHost(addr, port);

    // Start authorization timer. If authorization cannot be completed in this time,
    // just drop the entire transfer
    m_authorizationTimer = std::make_unique<QTimer>(this);
    connect(m_authorizationTimer.get(), SIGNAL(timeout()), this, SLOT(authTimeout()));
    m_authorizationTimer->setSingleShot(true);
    m_authorizationTimer->start(50000); // 50 seconds ~ human intervention

  } else { // SERVER

    m_bytesToRead = 0; // Unknown file dimension for now

    // Signal to start the transfer
    m_socket->write(PeerFileTransfer::ACK_SEND_PERMISSION, strlen(PeerFileTransfer::ACK_SEND_PERMISSION) + 1);

    m_serverState = AUTHORIZATION_SENT;

  }
}

void PeerFileTransfer::clientSocketConnected() // SLOT
{
  // Ask for authorization to send
  m_socket->write(REQUEST_SEND_PERMISSION);
  // Authorization timer is already running
}

void PeerFileTransfer::clientReadReady() // SLOT
{
  if(m_clientState == UNAUTHORIZED) {
    // Ready to read the response

    QByteArray data = m_socket->readAll();
    QString command = QString(data);

    if(command.compare(NACK_SEND_PERMISSION) == 0) {

      // Transfer denied - abort
      m_socket->close();
      emit transferDenied();
    } else if (command.compare(ACK_SEND_PERMISSION) == 0) {

      // Transfer accepted!
      m_clientState = SENDING_HEADER_SIZE_AND_HEADER;

      // Create chunker for this file
      m_chunker = std::make_unique<FileChunker>(m_file);

      // Create file header
      m_fileHeader = {
        m_file,
        m_chunker->getFileSize()
      };
      QByteArray serializedHeader = serializeHeader(m_fileHeader);

      // Send a stream of header-size + header
      QDataStream stream(m_serializedHeaderSizeAndHeader);
      stream << qint64(serializedHeader.size());
      stream.writeBytes(serializedHeader, serializedHeader.size());

      // Calculate data transfers for the header
      m_bytesToWrite = m_serializedHeaderSizeAndHeader.size();
      m_bytesWritten = 0;

      // Start sending out header size and header
      m_socket->write(m_serializedHeaderSizeAndHeader);
    }

  }
}

void PeerFileTransfer::updateClientProgress(qint64 bytesWritten) // SLOT
{
  m_bytesWritten += bytesWritten;

  switch(m_clientState) {
    case SENDING_HEADER_SIZE_AND_HEADER: {

      // Check that the server has received the header size and header
      if (m_bytesWritten < m_bytesToWrite) {
        // Resend the rest of the header
        QByteArray remainingData = m_serializedHeaderSizeAndHeader.right(m_serializedHeaderSizeAndHeader.size() - m_bytesWritten);
        m_socket->write(remainingData);
      } else {
        // Header size and header sent successfully, now send out the rest
        m_clientState = SENDING_CHUNKS;

        m_bytesToWrite = m_chunker->getFileSize();
        m_bytesWritten = 0;

        // Send first chunk
        m_socket->write(m_chunker->readNextFileChunk());
      }
    } break;
    case SENDING_CHUNKS: {

      // Check if we transferred everything
      if (m_bytesWritten == m_bytesToWrite) {
        // SUCCESS
        m_clientState = SENDING_FINISHED;
        m_socket->close();
        qDebug() << "[" + m_file + "] >> TRANSFER SUCCESSFUL!!! <<";
        return;
      }

      if (bytesWritten < m_chunker->chunkSize()) {
        // We need to resend part of last chunk
        m_chunker->movePointerBack(m_chunker->chunkSize() - bytesWritten);
      }

      m_socket->write(m_chunker->readNextFileChunk());

    } break;
  };
}

void PeerFileTransfer::error(QAbstractSocket::SocketError) // SLOT
{
  // Get the operation the socket requested
  //auto state = *static_cast<ClientSocketState*>(m_socket->property("command").data());

  qDebug() << ">> FAILED TRANSFER FOR FILE [" + m_file + "]!!! <<";
  m_socket->abort();
  emit failure();
}

void PeerFileTransfer::authTimeout() // SLOT
{
  m_socket->abort();
  emit timeout();
}

void PeerFileTransfer::updateServerProgress() // SLOT
{
  //qint64 bytesReceived = m_socket->bytesAvailable();

  auto setupChunking = [&]() { // Executed when both header size and serialized header are ready
    QDataStream stream(m_serializedHeaderSizeAndHeader);
    QByteArray serializedHeader;
    {
      char* raw;
      uint len = static_cast<uint>(m_serializedHeaderSize);
      stream.readBytes(raw, len);
      serializedHeader.fromRawData(raw, m_serializedHeaderSize);
      delete raw;
    }

    m_fileHeader = deserializeHeader(serializedHeader);
    m_bytesToRead = m_fileHeader.fileSize;
    m_bytesRead = 0;

    // Get filename and create file to start writing data
    QFileInfo fileInfo(QFile(m_fileHeader.filePath).fileName());
    QString filename(fileInfo.fileName());
    m_chunker = std::make_unique<FileChunker>(m_downloadPath + QDir::separator() + filename);

    m_serverState = RECEIVING_CHUNKS;
  };

  // Main DFA
  switch(m_serverState) {
    case AUTHORIZATION_SENT:
    case RECEIVING_HEADER_SIZE_AND_HEADER: {
      m_serverState = RECEIVING_HEADER_SIZE_AND_HEADER;

      m_serializedHeaderSizeAndHeader.append(m_socket->readAll());

      if (m_serializedHeaderSizeAndHeader.size() > sizeof(qint64)) {
        // Get the serialized header size
        QDataStream stream(m_serializedHeaderSizeAndHeader);
        stream >> m_serializedHeaderSize;

        m_serverState = RECEIVING_HEADER;

        if (m_serializedHeaderSizeAndHeader.size() == sizeof(qint64) + m_serializedHeaderSize) {
          // We're lucky, both header size and serialized header have arrived
          setupChunking();
        }
        // No luck, next step we'll be receiving other serialized header data
      }
    } break;
    case RECEIVING_HEADER: {

      m_serializedHeaderSizeAndHeader.append(m_socket->readAll());

      if (m_serializedHeaderSizeAndHeader.size() == sizeof(qint64) + m_serializedHeaderSize) {
        // Done, both header size and serialized header have arrived
        setupChunking();
      }
    } break;
    case RECEIVING_CHUNKS: {

      // Core routine - receiving big chunks of data

      QByteArray chunk = m_socket->readAll();

      // Check if we're done
      if (m_bytesRead + chunk.size() == m_bytesToRead) {
        // DONE! SUCCESS

        m_chunker->writeNextFileChunk(chunk);
        m_chunker->close();

        qDebug() << "[serverTransfer] >> FILE SUCCESSFULLY RECEIVED! <<";
        m_serverState = RECEIVING_FINISHED;
        m_socket->close();
        emit receivingComplete();
        return;
      }

      // We still have stuff to read. At this point it doesn't matter if we did read a complete chunk or
      // less than a chunk

      m_bytesRead += chunk.size();
      m_chunker->writeNextFileChunk(chunk);

    } break;
  };
}
