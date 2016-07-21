#include "peerfiletransfer.h"
#include "mainwindow.h"
#include <QDataStream>

int PeerFileTransfer::NEW_TRANSFER_NUMBER = 0;

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
  TRANSFER_NUMBER = NEW_TRANSFER_NUMBER++;
  qDebug() << "{" << TRANSFER_NUMBER << "} " << " Starting client transfer for file " << file;
  m_socket = std::make_unique<QTcpSocket>(this);
  m_type = CLIENT;
  qRegisterMetaType<QAbstractSocket::SocketError>();
  Qt::ConnectionType ctype = static_cast<Qt::ConnectionType>(Qt::UniqueConnection | Qt::QueuedConnection);
  connect(m_socket.get(), SIGNAL(connected()), this, SLOT(clientSocketConnected()), ctype);
  connect(m_socket.get(), SIGNAL(readyRead()), this, SLOT(clientReadReady()), ctype);
  connect(m_socket.get(), SIGNAL(bytesWritten(qint64)), this, SLOT(updateClientProgress(qint64)), ctype);
  connect(m_socket.get(), SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(error(QAbstractSocket::SocketError)), ctype);
  connect(m_socket.get(), SIGNAL(disconnected()), this, SLOT(onDisconnected()), ctype);
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
  TRANSFER_NUMBER = NEW_TRANSFER_NUMBER++;
  qDebug() << "{" << TRANSFER_NUMBER << "} " << " Starting server transfer";
  m_socket = std::make_unique<QTcpSocket>(this);
  if (!m_socket->setSocketDescriptor(socketDescriptor)) { // To operate in a different thread, we create another socket
                                                          // and assign the socket descriptor from the native socket
    qDebug() << "[SERVER ERROR] COULD NOT ASSIGN SOCKET DESCRIPTOR";
  }
  m_socket->disconnect();
  m_type = SERVER;
  qRegisterMetaType<QAbstractSocket::SocketError>();
  Qt::ConnectionType ctype = static_cast<Qt::ConnectionType>(Qt::UniqueConnection | Qt::QueuedConnection);
  connect(m_socket.get(), SIGNAL(readyRead()),
          this, SLOT(updateServerProgress()), ctype);
  connect(m_socket.get(), SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(error(QAbstractSocket::SocketError)), ctype);
  connect(m_socket.get(), SIGNAL(disconnected()), this, SLOT(onDisconnected()), ctype);
}

PeerFileTransfer::~PeerFileTransfer() {
  if (m_socket->isOpen())
    m_socket->disconnectFromHost();
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
    m_socket->abort(); // Reset, just in case
    m_socket->connectToHost(addr, port);

    qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] Trying to connect, auth timer started";

  } else { // SERVER

    m_bytesToRead = 0; // Unknown file or header dimension for now

    m_serverState = AUTHORIZATION_SENT;

    if(!m_socket->isValid() || m_socket->state() != QAbstractSocket::ConnectedState) {
      qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] WARNING - invalid socket!";
    }

    QString command = QString(PeerFileTransfer::ACK_SEND_PERMISSION);
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_7);
    out << command;
    CHECK_SOCKET_IO_SUCCESS(m_socket->write(block));

    qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] Sent ACK_SEND_PERMISSION, now waiting for header size and header data";
  }
}

void PeerFileTransfer::clientSocketConnected() // SLOT
{
  // Ask for authorization to send
  m_clientState = UNAUTHORIZED;
  m_socket->write(REQUEST_SEND_PERMISSION);
  m_socket->flush();
  // Authorization timer is already running
  qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] Sent REQUEST_SEND_PERMISSION, waiting for permission";
  //m_socket->waitForBytesWritten();
}

void PeerFileTransfer::clientReadReady() // SLOT
{

  QString command;
  QDataStream clientReadStream(m_socket.get());
  clientReadStream.startTransaction();
  clientReadStream >> command;
  if (!clientReadStream.commitTransaction())
    return; // Wait for more data

  if(m_clientState == UNAUTHORIZED) {
    // Ready to read the response

    if(command.compare(NACK_SEND_PERMISSION) == 0) {

      // Transfer denied - abort
      m_authorizationTimer->stop();
      m_socket->disconnectFromHost();
      emit transferDenied();
      qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] TRANSFER DENIED";
      m_parentThread->exit(1); // Exit the thread event loop

    } else if (command.compare(ACK_SEND_PERMISSION) == 0) {

      // Transfer accepted!
      m_clientState = SENDING_HEADER_SIZE_AND_HEADER;
      m_authorizationTimer->stop();

      m_lastChunkAcknowledged = false; // Will wait for a confirmation
      sendData();

      qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] Sending serialized header size and header";
    }

  } else {
    // Confirmation of receipt

    if(command.compare(ACK_CHUNK) != 0) {
      // Something is wrong - abort
      m_socket->disconnectFromHost();
      emit socketFailure();
      qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] SERVER FAILED TO ACK";
      m_parentThread->exit(1); // Exit the thread event loop
    }

    m_lastChunkAcknowledged = true;

    switch(m_clientState) {
      case SENDING_HEADER_SIZE_AND_HEADER: {
        m_clientState = SENDING_CHUNKS;
        m_bytesToWrite = m_chunker->getFileSize();
        m_bytesWritten = 0;
        sendData();
      } break;
      case SENDING_CHUNKS: {

        if (m_allChunksSent) {
          // SUCCESS
          m_clientState = SENDING_FINISHED;

          m_socket->disconnectFromHost();

          qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] '" << m_file << "' >> TRANSFER SUCCESSFUL!!! <<";
          m_parentThread->exit(0); // Exit the thread event loop
          return;
        }

        sendData(); // Continue sending

      } break;
    };

  }

}

void PeerFileTransfer::sendData() {

  // Main Client DFA
  switch(m_clientState) {
    case SENDING_HEADER_SIZE_AND_HEADER: {

      // Create chunker for this file
      m_chunker = std::make_unique<FileChunker>(m_file);
      if (!m_chunker->open(READONLY)) {
        // Could not open the file for reading - abort all the transfer!
        qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] ABORTING - COULD NOT OPEN FILE!";
        m_socket->abort();
        emit fileLocked();
        m_parentThread->exit(1); // Exit the thread event loop
      }

      // Create file header
      m_fileHeader = {
        m_file,
        m_chunker->getFileSize()
      };

      QByteArray block;
      QDataStream out(&block, QIODevice::WriteOnly);
      out.setVersion(QDataStream::Qt_5_7);
      out << m_fileHeader.filePath << m_fileHeader.fileSize;

      // Calculate data transfers for the header
      m_bytesToWrite = block.size();
      m_bytesWritten = 0;

      // Send header size and header
      CHECK_SOCKET_IO_SUCCESS(m_socket->write(block));

    } break;
    case SENDING_CHUNKS: {

      // Stop the process if the peer we're sending data to went offline
      if (m_mainWindow->isPeerActive(m_peerIndex) == false) {
        m_socket->abort();
        qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT ERROR] Server suddently disconnected! Transfer corrupted/incompleted";
        emit serverPeerWentOffline();
        m_parentThread->exit(1); // Exit the thread event loop
      }

      CHECK_SOCKET_IO_SUCCESS(m_socket->write(m_chunker->readNextFileChunk()));
      qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] Just sent a chunk";

      m_lastChunkAcknowledged = false;

      if (m_chunker->reachedEOF())
        m_allChunksSent = true;

    } break;
  };
}

void PeerFileTransfer::updateClientProgress(qint64 bytesWritten) // SLOT
{
  m_bytesWritten += bytesWritten;

  if (m_clientState >= SENDING_CHUNKS) { // Only fire up events for file transmission, not header transmission
    m_completionPercentage = (static_cast<double>(m_bytesWritten) / static_cast<double>(m_bytesToWrite)) * 100;
    emit fileSentPercentage(m_completionPercentage);
  }
}

void PeerFileTransfer::error(QAbstractSocket::SocketError err) // SLOT
{
  // Get the operation the socket requested
  //auto state = *static_cast<ClientSocketState*>(m_socket->property("command").data());

  qDebug() << "{" << TRANSFER_NUMBER << "} " << ">> FAILED TRANSFER FOR FILE [" << m_file << "]" << err << "!!!";
  m_socket->abort();
  emit socketFailure();
  m_parentThread->exit(1); // Exit the thread event loop
  return;
}

void PeerFileTransfer::onDisconnected() // SLOT
{
  qDebug() << "{" << TRANSFER_NUMBER << "} onDisconnected()";
  if (m_socket->isOpen())
    m_socket->disconnectFromHost();
  m_parentThread->quit();
}

void PeerFileTransfer::authTimeout() // SLOT
{
  qDebug() << "{" << TRANSFER_NUMBER << "} authTimeout()";
  m_socket->abort();
  emit timeout();
  m_parentThread->exit(1); // Exit the thread event loop
  return;
}

void PeerFileTransfer::updateServerProgress() // SLOT
{
  if(!m_socket->isValid()) {
    qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] WARNING - invalid socket!";
  }

  while(m_socket->bytesAvailable() > 0) {

    // Main Server DFA
    switch(m_serverState) {
      case AUTHORIZATION_SENT:
      case RECEIVING_HEADER_SIZE_AND_HEADER: {

        m_serverState = RECEIVING_HEADER_SIZE_AND_HEADER;

        QDataStream clientReadStream(m_socket.get());
        clientReadStream.startTransaction();
        clientReadStream >> m_fileHeader.filePath >> m_fileHeader.fileSize;
        if (!clientReadStream.commitTransaction())
          return; // Wait for more data

        { // Setup chunking
          m_bytesToRead = m_fileHeader.fileSize;
          m_bytesRead = 0;
          qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] Header size and header successfully received! Data is:\n" <<
            "     { Size: " << m_fileHeader.fileSize << " }\n" <<
            "     { File: " << m_fileHeader.filePath << " }\n" <<
            " [SERVER] Now receiving chunks";

          m_serverState = RECEIVING_CHUNKS;

          // We have a destination filepath to write data
          QString destinationFilePath = getServerDestinationFilePath();
          emit destinationAvailable(destinationFilePath);

          // Get filename and create file to start writing data
          m_chunker = std::make_unique<FileChunker>(destinationFilePath);
          if (!m_chunker->open(READWRITE)) {
            // Could not open the file for writing - abort all the transfer!
            qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] ABORTING - COULD NOT OPEN FILE!";
            m_socket->abort();
            emit fileLocked();
            m_parentThread->exit(1); // Exit the thread event loop
            return;
          }
          // qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] File chunker set to '" << destinationFilePath << "'";
        }

        // Header completely received - send out ACK
        QString command = QString(PeerFileTransfer::ACK_CHUNK);
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_7);
        out << command;
        CHECK_SOCKET_IO_SUCCESS(m_socket->write(block));
        qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] Acknowledged header";

      } break;
      case RECEIVING_CHUNKS: {

        // Core routine - receiving big chunks of data

        // Calculate how much data we're expecting to receive (an entire chunk? Less than a chunk?)
        qint64 nextChunkSize = 0;
        if (m_bytesToRead < m_chunker->chunkSize())
          nextChunkSize = m_bytesToRead;
        else {
          qint64 remainingData = m_bytesToRead - m_bytesRead;
          nextChunkSize = std::min(remainingData, m_chunker->chunkSize());
        }

        if (m_socket->bytesAvailable() < nextChunkSize)
          return; // Wait for more data

        // Read a chunk (or less than one for the last one)
        QByteArray chunk = m_socket->read(nextChunkSize);

        m_bytesRead += chunk.size();
        m_chunker->writeNextFileChunk(chunk); // Write it down

        m_completionPercentage = (static_cast<double>(m_bytesRead) / static_cast<double>(m_bytesToRead)) * 100;
        emit fileReceivedPercentage(m_completionPercentage);

        qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] Reading chunk of size " << chunk.size() << " [" << m_completionPercentage << "%]";

        // Acknowledge this chunk
        QString command = QString(PeerFileTransfer::ACK_CHUNK);
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_7);
        out << command;
        CHECK_SOCKET_IO_SUCCESS(m_socket->write(block));
        qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] Acknowledged header";

        // Check if we're done with this chunk
        if (m_bytesRead  == m_bytesToRead) {

          // DONE! SUCCESS
          m_chunker->close();

          qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] >> FILE SUCCESSFULLY RECEIVED! <<";
          m_serverState = RECEIVING_FINISHED;
          m_socket->disconnectFromHost();
          emit receivingComplete();
          m_parentThread->exit(0); // Exit the thread event loop
          return;
        }

      } break;
    };
  }
}
