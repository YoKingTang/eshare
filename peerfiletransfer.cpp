#include "peerfiletransfer.h"
#include "mainwindow.h"
#include <QDataStream>

const char PeerFileTransfer::REQUEST_SEND_PERMISSION[] = "SEND?";
const char PeerFileTransfer::ACK_SEND_PERMISSION[] = "ACK!";
const char PeerFileTransfer::NACK_SEND_PERMISSION[] = "NOPE!";

#define CHECK_SOCKET_IO_SUCCESS(socketCall) do { \
                                              auto ret = socketCall; \
                                              if (ret == -1) { \
                                                qDebug() << "SOCKET ERROR at line " << __LINE__; \
                                                m_socket->abort(); \
                                                emit socketFailure(); \
                                                m_parentThread->exit(1); \
                                              } \
                                            } while(0)

 // CLIENT
PeerFileTransfer::PeerFileTransfer(QThread *parent, MainWindow *mainWindow, size_t peerIndex,
                                   std::tuple<QString /* Ip */, int /* port */, QString /* hostname */>
                                   peer, QString file) :
  m_parentThread(parent),
  m_mainWindow(mainWindow),
  m_peerIndex(peerIndex),
  m_file(file),
  m_peer(peer)
{
  m_socket = std::make_unique<QTcpSocket>();
  m_type = CLIENT;
  connect(m_socket.get(), SIGNAL(connected()), this, SLOT(clientSocketConnected()));
  connect(m_socket.get(), SIGNAL(readyRead()), this, SLOT(clientReadReady()));
  connect(m_socket.get(), SIGNAL(bytesWritten(qint64)), this, SLOT(updateClientProgress(qint64)));
  connect(m_socket.get(), SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(error(QAbstractSocket::SocketError)));
}

// SERVER
PeerFileTransfer::PeerFileTransfer(QThread *parent, MainWindow *mainWindow, size_t peerIndex,
                                   std::tuple<QString /* Ip */, int /* port */, QString /* hostname */>
                                   peer, qintptr socketDescriptor, QString downloadPath) :
  m_parentThread(parent),
  m_mainWindow(mainWindow),
  m_peerIndex(peerIndex),
  m_peer(peer),
  m_downloadPath(downloadPath)
{
  m_socket = std::make_unique<QTcpSocket>();
  m_socket->setSocketDescriptor(socketDescriptor); // To operate in a different thread, we create another socket
                                                   // and assign the socket descriptor from the native socket
  m_type = SERVER;
  connect(m_socket.get(), SIGNAL(readyRead()),
          this, SLOT(updateServerProgress()));
  connect(m_socket.get(), SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(error(QAbstractSocket::SocketError)));
}

PeerFileTransfer::~PeerFileTransfer() {
  if (m_socket->isOpen())
    m_socket->close();
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

void PeerFileTransfer::execute() {

  QString addr, hostname; int port;
  std::tie(addr, port, hostname) = m_peer;

  if (m_type == CLIENT) {

    m_clientState = UNAUTHORIZED;

    // Start authorization timer. If authorization cannot be completed in this time,
    // just drop the entire transfer
    m_authorizationTimer = std::make_unique<QTimer>(this);
    connect(m_authorizationTimer.get(), SIGNAL(timeout()), this, SLOT(authTimeout()));
    m_authorizationTimer->setSingleShot(true);
    m_authorizationTimer->start(50000); // 50 seconds ~ human intervention

    // Connect to endpoint and ask for authorization
    m_socket->connectToHost(addr, port);

    qDebug() << "[CLIENT] Trying to connect, auth timer started";

  } else { // SERVER

    m_bytesToRead = 0; // Unknown file dimension for now

    // Signal to start the transfer
    CHECK_SOCKET_IO_SUCCESS(m_socket->write(PeerFileTransfer::ACK_SEND_PERMISSION, strlen(PeerFileTransfer::ACK_SEND_PERMISSION) + 1));

    m_serverState = AUTHORIZATION_SENT;

    qDebug() << "[SERVER] Sent ACK_SEND_PERMISSION, now waiting for header size and header data";

  }
}

void PeerFileTransfer::clientSocketConnected() // SLOT
{
  // Ask for authorization to send
  m_socket->write(REQUEST_SEND_PERMISSION);
  // Authorization timer is already running
  qDebug() << "[CLIENT] Sent REQUEST_SEND_PERMISSION, waiting for permission";
}

void PeerFileTransfer::clientReadReady() // SLOT
{
  if(m_clientState == UNAUTHORIZED) {
    // Ready to read the response

    QByteArray data = m_socket->readAll();
    QString command = QString(data);

    if(command.compare(NACK_SEND_PERMISSION) == 0) {

      // Transfer denied - abort
      m_authorizationTimer->stop();
      m_socket->close();
      emit transferDenied();
      qDebug() << "[CLIENT] TRANSFER DENIED";
      m_parentThread->exit(1); // Exit the thread event loop

    } else if (command.compare(ACK_SEND_PERMISSION) == 0) {

      // Transfer accepted!
      m_clientState = SENDING_HEADER_SIZE_AND_HEADER;
      m_authorizationTimer->stop();

      // Create chunker for this file
      m_chunker = std::make_unique<FileChunker>(m_file);
      if (!m_chunker->open(READONLY)) {
        // Could not open the file for reading - abort all the transfer!
        qDebug() << "[CLIENT] ABORTING - COULD NOT OPEN FILE!";
        m_socket->abort();
        emit fileLocked();
        m_parentThread->exit(1); // Exit the thread event loop
      }

      // Create file header
      m_fileHeader = {
        m_file,
        m_chunker->getFileSize()
      };
      m_serializedHeaderBytes = serializeHeader(m_fileHeader);
      m_serializedHeaderSize = qint64(m_serializedHeaderBytes.size());

      // Send the header-size then start sending out the header
      CHECK_SOCKET_IO_SUCCESS(m_socket->write((const char*)&m_serializedHeaderSize, sizeof(qint64)));

      // Calculate data transfers for the header
      m_bytesToWrite = m_serializedHeaderSize + sizeof(qint64);
      m_bytesWritten = 0;

      // Start sending out header size and header
      CHECK_SOCKET_IO_SUCCESS(m_socket->write(m_serializedHeaderBytes));
      qDebug() << "[CLIENT] Sending out serialized header size and header";
    }

  }
}

void PeerFileTransfer::updateClientProgress(qint64 bytesWritten) // SLOT
{
  m_bytesWritten += bytesWritten;

  // Main Client DFA
  switch(m_clientState) {
    case SENDING_HEADER_SIZE_AND_HEADER: {

      if (m_bytesWritten < sizeof(qint64)) {
        qDebug() << "[CLIENT] Could not send header size - connection issues";
        m_socket->abort();
        emit socketFailure();
        m_parentThread->exit(1); // Exit the thread event loop
        return;
      }

      qDebug() << "[CLIENT] Successfully written " << bytesWritten << " out of " << m_bytesToWrite << " to the server";

      // Check that the server has received the header size and header
      if (m_bytesWritten < m_bytesToWrite) {
        // Resend the rest of the header
        QByteArray remainingData = m_serializedHeaderBytes.right(m_serializedHeaderSize - static_cast<int>(m_bytesWritten - sizeof(qint64)));
        CHECK_SOCKET_IO_SUCCESS(m_socket->write(remainingData));
      } else {
        // Header size and header sent successfully, now send out the rest
        m_clientState = SENDING_CHUNKS;

        m_bytesToWrite = m_chunker->getFileSize();
        m_bytesWritten = 0;
        qDebug() << "[CLIENT] Header size and header successfully sent to the server! Starting chunking..";

        // Send first chunk
        CHECK_SOCKET_IO_SUCCESS(m_socket->write(m_chunker->readNextFileChunk()));
      }
    } break;
    case SENDING_CHUNKS: {

      // Stop the process if the peer we're sending data to went offline
      if (m_mainWindow->isPeerActive(m_peerIndex) == false) {
        m_socket->abort();
        qDebug() << "[CLIENT ERROR] Server suddently disconnected! Transfer corrupted/incompleted";
        emit serverPeerWentOffline();
        m_parentThread->exit(1); // Exit the thread event loop
      }

      m_completionPercentage = (static_cast<double>(m_bytesWritten) / static_cast<double>(m_bytesToWrite)) * 100;
      emit fileSentPercentage(m_completionPercentage);

      // Check if we transferred everything
      if (m_bytesWritten == m_bytesToWrite) {
        // SUCCESS
        m_clientState = SENDING_FINISHED;
        m_socket->disconnectFromHost();
        qDebug() << "[" + m_file + "] >> TRANSFER SUCCESSFUL!!! <<";
        m_parentThread->exit(0); // Exit the thread event loop
        return;
      }

      if (bytesWritten < m_chunker->chunkSize()) {
        // We need to resend part of last chunk
        m_chunker->movePointerBack(m_chunker->chunkSize() - bytesWritten);
      }

      CHECK_SOCKET_IO_SUCCESS(m_socket->write(m_chunker->readNextFileChunk()));

    } break;
  };
}

void PeerFileTransfer::error(QAbstractSocket::SocketError) // SLOT
{
  // Get the operation the socket requested
  //auto state = *static_cast<ClientSocketState*>(m_socket->property("command").data());

  //auto test = err;
  qDebug() << ">> FAILED TRANSFER FOR FILE [" + m_file + "]!!!";
  m_socket->abort();
  emit socketFailure();
  m_parentThread->exit(1); // Exit the thread event loop
  return;
}

void PeerFileTransfer::authTimeout() // SLOT
{
  m_socket->abort();
  emit timeout();
  m_parentThread->exit(1); // Exit the thread event loop
  return;
}

void PeerFileTransfer::updateServerProgress() // SLOT
{
  auto setupChunking = [&]() { // Executed when both header size and serialized header are ready
    QDataStream stream(m_serializedHeaderBytes /* Read only access */);
    stream.setVersion(QDataStream::Qt_5_7);

    m_fileHeader = deserializeHeader(m_serializedHeaderBytes);
    m_bytesToRead = m_fileHeader.fileSize;
    m_bytesRead = 0;
    qDebug() << "[SERVER] Header size and header successfully received! Now Chunking";

    m_serverState = RECEIVING_CHUNKS;

    // We have a destination filepath to write data
    QString destinationFilePath = getServerDestinationFilePath();
    emit destinationAvailable(destinationFilePath);

    // Get filename and create file to start writing data
    m_chunker = std::make_unique<FileChunker>(destinationFilePath);
    if (!m_chunker->open(READWRITE)) {
      // Could not open the file for writing - abort all the transfer!
      qDebug() << "[SERVER] ABORTING - COULD NOT OPEN FILE!";
      m_socket->abort();
      emit fileLocked();
      m_parentThread->exit(1); // Exit the thread event loop
      return;
    }
  };

  if(!m_socket->isValid()) {
    qDebug() << "[SERVER] WARNING - invalid socket!";
  }

  qint64 bytesReceived = m_socket->bytesAvailable();
  while (bytesReceived > 0) { // Necessary since readyRead signals aren't reentrant or queued

    // Main Server DFA
    switch(m_serverState) {
      case AUTHORIZATION_SENT:
      case RECEIVING_HEADER_SIZE_AND_HEADER: {

        m_serverState = RECEIVING_HEADER_SIZE_AND_HEADER;

        CHECK_SOCKET_IO_SUCCESS(m_socket->read((char*)&m_serializedHeaderSize, sizeof(qint64)));

        m_serverState = RECEIVING_HEADER;

        m_serializedHeaderBytes = m_socket->read(m_serializedHeaderSize);

        if (m_serializedHeaderBytes.size() == m_serializedHeaderSize) {
          // We're lucky, both header size and serialized header have arrived
          setupChunking();
        }

        // No luck, next step we'll be receiving other serialized header data

      } break;
      case RECEIVING_HEADER: {

        qDebug() << "[SERVER] Still receiving serialized header data";
        m_serializedHeaderBytes.append(m_socket->read(m_serializedHeaderSize - m_serializedHeaderBytes.size()));

        if (m_serializedHeaderBytes.size() == m_serializedHeaderSize) {
          // Done, both header size and serialized header have arrived
          setupChunking();
        }
      } break;
      case RECEIVING_CHUNKS: {

        // Core routine - receiving big chunks of data

        QByteArray chunk = m_socket->readAll();

        m_completionPercentage = (static_cast<double>(m_bytesRead + chunk.size()) / static_cast<double>(m_bytesToRead)) * 100;
        emit fileReceivedPercentage(m_completionPercentage);

        // Check if we're done
        if (m_bytesRead + chunk.size() == m_bytesToRead) {

          // DONE! SUCCESS
          m_chunker->writeNextFileChunk(chunk);
          m_chunker->close();

          qDebug() << "[serverTransfer] >> FILE SUCCESSFULLY RECEIVED! <<";
          m_serverState = RECEIVING_FINISHED;
          m_socket->disconnectFromHost();
          emit receivingComplete();
          m_parentThread->exit(0); // Exit the thread event loop
          return;
        }

        // We still have stuff to read. At this point it doesn't matter if we did read a complete chunk or
        // less than a chunk

        m_bytesRead += chunk.size();
        m_chunker->writeNextFileChunk(chunk);

      } break;
    };

    bytesReceived = m_socket->bytesAvailable();
  }
}
