#include "peerfiletransfer.h"
#include "mainwindow.h"
#include <QDataStream>

const char PeerFileTransfer::REQUEST_SEND_PERMISSION[] = "SEND?";
const char PeerFileTransfer::ACK_SEND_PERMISSION[] = "ACK!";
const char PeerFileTransfer::NACK_SEND_PERMISSION[] = "NOPE!";

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
  m_socket = std::make_unique<QTcpSocket>();
  m_type = CLIENT;
  connect(m_socket.get(), SIGNAL(connected()), this, SLOT(clientSocketConnected()), Qt::DirectConnection);
  connect(m_socket.get(), SIGNAL(readyRead()), this, SLOT(clientReadReady()), Qt::DirectConnection);
  connect(m_socket.get(), SIGNAL(bytesWritten(qint64)), this, SLOT(updateClientProgress(qint64)), Qt::DirectConnection);
  connect(m_socket.get(), SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(error(QAbstractSocket::SocketError)), Qt::DirectConnection);
  connect(m_socket.get(), SIGNAL(disconnected()), this, SLOT(onDisconnected()), Qt::DirectConnection);
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
  m_socket = std::make_unique<QTcpSocket>();
  if (!m_socket->setSocketDescriptor(socketDescriptor)) { // To operate in a different thread, we create another socket
                                                          // and assign the socket descriptor from the native socket
    qDebug() << "[SERVER ERROR] COULD NOT ASSIGN SOCKET DESCRIPTOR";
  }
  m_type = SERVER;
  connect(m_socket.get(), SIGNAL(readyRead()),
          this, SLOT(updateServerProgress()), Qt::DirectConnection);
  connect(m_socket.get(), SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(error(QAbstractSocket::SocketError)), Qt::DirectConnection);
  connect(m_socket.get(), SIGNAL(disconnected()), this, SLOT(onDisconnected()), Qt::DirectConnection);
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
    m_socket->abort(); // Reset, just in case
    m_socket->connectToHost(addr, port);

    qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] Trying to connect, auth timer started";

  } else { // SERVER

    m_bytesToRead = 0; // Unknown file dimension for now

    // Signal to start the transfer
    CHECK_SOCKET_IO_SUCCESS(m_socket->write(PeerFileTransfer::ACK_SEND_PERMISSION, strlen(PeerFileTransfer::ACK_SEND_PERMISSION) + 1));
    m_socket->flush();

    m_serverState = AUTHORIZATION_SENT;

    qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] Sent ACK_SEND_PERMISSION, now waiting for header size and header data";
  }
}

void PeerFileTransfer::clientSocketConnected() // SLOT
{
  // Ask for authorization to send
  m_socket->write(REQUEST_SEND_PERMISSION);
  // Authorization timer is already running
  qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] Sent REQUEST_SEND_PERMISSION, waiting for permission";
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
      qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] TRANSFER DENIED";
      m_parentThread->exit(1); // Exit the thread event loop

    } else if (command.compare(ACK_SEND_PERMISSION) == 0) {

      // Transfer accepted!
      m_clientState = SENDING_HEADER_SIZE_AND_HEADER;
      m_authorizationTimer->stop();

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
      m_serializedHeaderBytes = serializeHeader(m_fileHeader);
      m_serializedHeaderSize = qint64(m_serializedHeaderBytes.size());

      QByteArray block;
      QDataStream out(&block, QIODevice::WriteOnly);
      out.setVersion(QDataStream::Qt_5_7);
      out << qint64(0) /* Placeholder for this stream size */;
      out << m_fileHeader.filePath << m_fileHeader.fileSize;
      out.device()->seek(0);
      out << (qint64)(block.size() - sizeof(qint64));


      // Calculate data transfers for the header
      m_bytesToWrite = block.size();
      m_bytesWritten = 0;

      // Send header size and header
      CHECK_SOCKET_IO_SUCCESS(m_socket->write(block));
      m_socket->flush();

      qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] Sending serialized header size and header";
    }

  }

//  QByteArray data = m_socket->readAll();
//  QString command = QString(data);
//  qDebug()  << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] Unhandled server data received: {" << command << "}";
}

void PeerFileTransfer::updateClientProgress(qint64 bytesWritten) // SLOT
{
  m_bytesWritten += bytesWritten;

  // Main Client DFA
  switch(m_clientState) {
    case SENDING_HEADER_SIZE_AND_HEADER: {

      qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] Successfully written " << bytesWritten << " out of " << m_bytesToWrite << " to the server";

      // Check that the server has received the header size and header
      if (m_bytesWritten < m_bytesToWrite) {

        // Resend missing header data (this shouldn't happen often)

        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_7);
        out << qint64(0) /* Placeholder for this stream size */;
        out << m_fileHeader.filePath << m_fileHeader.fileSize;
        out.device()->seek(0);
        out << (qint64)(block.size() - sizeof(qint64));
        block.right(m_bytesToWrite - m_bytesWritten);

        CHECK_SOCKET_IO_SUCCESS(m_socket->write(block));
        m_socket->flush();

        qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] Header size and data wasn't entirely received, resending missing parts";

      } else {

        // Header size and header sent successfully, now send out the rest
        m_clientState = SENDING_CHUNKS;

        m_bytesToWrite = m_chunker->getFileSize();
        m_bytesWritten = 0;
        qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] Header size and header successfully sent to the server! Starting chunking..";

        // Send first chunk
        CHECK_SOCKET_IO_SUCCESS(m_socket->write(m_chunker->readNextFileChunk()));

      }
    } break;
    case SENDING_CHUNKS: {

      // Stop the process if the peer we're sending data to went offline
      if (m_mainWindow->isPeerActive(m_peerIndex) == false) {
        m_socket->abort();
        qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT ERROR] Server suddently disconnected! Transfer corrupted/incompleted";
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
        qDebug() << "{" << TRANSFER_NUMBER << "} " << "[CLIENT] '" << m_file << "' >> TRANSFER SUCCESSFUL!!! <<";
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
  qDebug() << "{" << TRANSFER_NUMBER << "} " << ">> FAILED TRANSFER FOR FILE [" << m_file << "]!!!";
  m_socket->abort();
  emit socketFailure();
  m_parentThread->exit(1); // Exit the thread event loop
  return;
}

void PeerFileTransfer::onDisconnected() // SLOT
{
  if (m_socket->isOpen())
    m_socket->close();
  m_parentThread->quit();
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

    //m_fileHeader = deserializeHeader(m_serializedHeaderBytes);
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
    qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] File chunker set to '" << destinationFilePath << "'";
  };

  if(!m_socket->isValid()) {
    qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] WARNING - invalid socket!";
  }

  qint64 bytesReceived = 0;
  while (true) { // Necessary since readyRead signals aren't reentrant or queued

    bytesReceived = m_socket->bytesAvailable();
    if (bytesReceived == 0)
      return;

    // Main Server DFA
    switch(m_serverState) {
      case AUTHORIZATION_SENT:
      case RECEIVING_HEADER_SIZE_AND_HEADER: {

        m_serverState = RECEIVING_HEADER_SIZE_AND_HEADER;

        if (bytesReceived < sizeof(qint64)) { // Is the data for the header size available?
          qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] Received less than " << sizeof(qint64) << " bytes to determine header size, can't proceed";
          continue; // Can't do much except waiting for more data
        }

        QDataStream clientReadStream(m_socket.get());
        clientReadStream >> m_serializedHeaderSize;

        m_serverState = RECEIVING_HEADER;

        // Go to next step

      } break;
      case RECEIVING_HEADER: {

        qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] Receiving serialized header data";

        if (bytesReceived < m_serializedHeaderSize) { // Is the data for the entire header data available?
          qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] Received " << bytesReceived << " out of " << m_serializedHeaderSize << " bytes of header data, can't proceed";
          continue; // Can't do much except waiting for more data
        }

        QDataStream clientReadStream(m_socket.get());
        clientReadStream >> m_fileHeader.filePath >> m_fileHeader.fileSize;

        setupChunking();

      } break;
      case RECEIVING_CHUNKS: {

        // Core routine - receiving big chunks of data

        if (bytesReceived == 0)
          continue;

        QByteArray chunk = m_socket->readAll();        

        m_completionPercentage = (static_cast<double>(m_bytesRead + chunk.size()) / static_cast<double>(m_bytesToRead)) * 100;
        emit fileReceivedPercentage(m_completionPercentage);

        qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] Reading chunk of size " << chunk.size() << " [" << m_completionPercentage << "%]";

        // Check if we're done
        if (m_bytesRead + chunk.size() == m_bytesToRead) {

          // DONE! SUCCESS
          m_chunker->writeNextFileChunk(chunk);
          m_chunker->close();

          qDebug() << "{" << TRANSFER_NUMBER << "} " << "[SERVER] >> FILE SUCCESSFULLY RECEIVED! <<";
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
  }
}
