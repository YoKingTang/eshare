#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "pickreceiver.h"
#include "dynamictreewidgetitem.h"
#include <QTreeWidget>
#include <QItemDelegate>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QLinearGradient>
#include <QPainter>
#include <QDir>
#include <QDragMoveEvent>
#include <QMimeData>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <functional>

#include <iostream> // DEBUG


//QString addr, hostname; int port;
//std::tie(addr, port, hostname) = transfer.getPeer();
//item->setText(1, hostname); // Receiver
//QString localFile = transfer.getClientSourceFilePath();
//if (localFile.isEmpty())
//  localFile = "[N/A]"; // Should never happen
//item->setText(2, localFile); // Source file (local file that we're sending)

//void socketFailure();
//void timeout();
//void transferDenied();
//void fileLocked();
//void fileSentPercentage(int);

//void receivingComplete();
//void fileReceivedPercentage(int);

//QString addr, hostname; int port;
//std::tie(addr, port, hostname) = transfer.getPeer();
//item->setText(1, hostname); // Sender
//QString destinationFile = transfer.getServerDestinationFilePath();
//if (destinationFile.isEmpty())
//  destinationFile = "[N/A]";
//item->setText(2, destinationFile); // Destination (local file written)

//item->setFlags(item->flags() & ~Qt::ItemIsEditable);
//item->setTextAlignment(1, Qt::AlignHCenter);

// TransfersView is the tree widget which lists all of the ongoing file transfers
class TransfersView : public QTreeWidget
{

public:
    TransfersView(QWidget*) {}
};

// TransfersViewDelegate is used to draw the progress bars
class TransfersViewDelegate : public QItemDelegate
{

public:
    inline TransfersViewDelegate(MainWindow *mainWindow, bool sentView = true) :
      m_sentView(sentView),
      m_mainWindow(mainWindow),
      QItemDelegate(mainWindow) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index ) const Q_DECL_OVERRIDE
    {
        if (index.column() != 0) {
            QItemDelegate::paint(painter, option, index);
            return;
        }

        // Set up a QStyleOptionProgressBar to precisely mimic the
        // environment of a progress bar
        QStyleOptionProgressBar progressBarOption;
        progressBarOption.state = QStyle::State_Enabled;
        progressBarOption.direction = QApplication::layoutDirection();
        progressBarOption.rect = option.rect;
        progressBarOption.fontMetrics = QApplication::fontMetrics();
        progressBarOption.minimum = 0;
        progressBarOption.maximum = 100;
        progressBarOption.textAlignment = Qt::AlignCenter;
        progressBarOption.textVisible = true;

        // Set the progress and text values of the style option
        //int progress = m_mainWindow->getPercentageForViewItem (m_sentView, index.row());
        int progress = index.model()->data(index, Qt::UserRole /* Progressbar value */).toInt();
        progressBarOption.progress = progress < 0 ? 0 : progress;
        progressBarOption.text = QString::asprintf("%d%%", progressBarOption.progress);

        // Draw the progress bar onto the view
        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, painter);
    }
private:
    bool m_sentView = true;
    MainWindow *m_mainWindow = nullptr;
};

const char MainWindow::PING[] = "PING?";
const char MainWindow::PONG[] = "PONG!";

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setWindowTitle("eKAshare");
    this->setAcceptDrops(true);

    QHBoxLayout *hbox = new QHBoxLayout; // Main layout

    {
      QWidget *leftWidgets = new QWidget(this);
      leftWidgets->setContentsMargins(-9, -9, -9, -9); // Remove widget border (usually 11px)
      QSizePolicy spLeft(QSizePolicy::Preferred, QSizePolicy::Preferred);
      spLeft.setHorizontalStretch(3);
      leftWidgets->setSizePolicy(spLeft);

      QVBoxLayout *vbox = new QVBoxLayout;
      leftWidgets->setLayout(vbox);

      QGroupBox *sentGB = new QGroupBox("Files inviati", this);
      {
          m_sentView = new TransfersView(this);

          QStringList headers;
          headers << tr("Progresso") << tr("Destinatario") << tr("File");

          // Main transfers list
          m_sentView->setItemDelegate(new TransfersViewDelegate(this));
          m_sentView->setHeaderLabels(headers);
          m_sentView->setSelectionBehavior(QAbstractItemView::SelectRows);
          m_sentView->setAlternatingRowColors(true);
          m_sentView->setRootIsDecorated(false);

          QVBoxLayout *vbox = new QVBoxLayout;
          vbox->addWidget(m_sentView);

          sentGB->setLayout(vbox);
      }

      QGroupBox *receivedGB = new QGroupBox("Files ricevuti", this);
      {
          m_receivedView = new TransfersView(this);

          QStringList headers;
          headers << tr("Progresso") << tr("Mittente") << tr("Destinazione");

          // Main transfers list
          m_receivedView->setItemDelegate(new TransfersViewDelegate(this));
          m_receivedView->setHeaderLabels(headers);
          m_receivedView->setSelectionBehavior(QAbstractItemView::SelectRows);
          m_receivedView->setAlternatingRowColors(true);
          m_receivedView->setRootIsDecorated(false);

          QVBoxLayout *vbox = new QVBoxLayout;
          vbox->addWidget(m_receivedView);

          receivedGB->setLayout(vbox);
      }

      vbox->addWidget(sentGB);
      vbox->addWidget(receivedGB);

      hbox->addWidget(leftWidgets);

      QGroupBox *onlineGB = new QGroupBox("Peers", this);
      QSizePolicy spRight(QSizePolicy::Preferred, QSizePolicy::Preferred);
      spRight.setHorizontalStretch(1);
      onlineGB->setSizePolicy(spRight);
      {
        m_peersView = new QTreeWidget(this);

        QStringList headers;
        headers << tr("") << tr("");

        //m_peersView->setItemDelegate(new PeersViewDelegate(this));
        m_peersView->setHeaderLabels(headers);
        m_peersView->resizeColumnToContents(0);
        m_peersView->header()->close();
        m_peersView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_peersView->setAlternatingRowColors(true);
        m_peersView->setRootIsDecorated(false);

        QVBoxLayout *vbox = new QVBoxLayout;
        vbox->addWidget(m_peersView);

        onlineGB->setLayout(vbox);
      }

      hbox->addWidget(onlineGB);
    }

    ui->centralWidget->setLayout(hbox);

    // Continue initialization

    m_defaultDownloadPath = QDir::currentPath();

    initializePeers();
    initializeServer();
}

MainWindow::~MainWindow() {
  if (m_peersPingTimer)
    m_peersPingTimer->stop();
  delete ui;
}

void MainWindow::initializePeers() {
  // Load peers and other settings from configuration files
  readPeersListAndSettings();

  // Process every peer found
  size_t index = 0;
  for(auto& tuple : m_peers) {
    QString ip, hostname; int port;
    std::tie(ip, port, hostname) = tuple; // Unpack peer data

    QTreeWidgetItem *item = new QTreeWidgetItem(m_peersView);
    item->setText(1, hostname);
    item->setTextAlignment(1, Qt::AlignLeft);
    item->setIcon(0, QIcon(":res/red_light.png"));

    // Create client socket and link the signals
    m_peersClientPingSockets.emplace_back(std::make_unique<QTcpSocket>());
    connect(m_peersClientPingSockets.back().get(), SIGNAL(connected()), this, SLOT(socketConnected()));
    connect(m_peersClientPingSockets.back().get(), SIGNAL(readyRead()), this, SLOT(updateClientProgress()));
    connect(m_peersClientPingSockets.back().get(), SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(socketError(QAbstractSocket::SocketError)));

    m_peersClientPingSockets.back()->setProperty("peer_index", index);
    m_peersClientPingSockets.back()->setProperty("command", PingClientSocketState::DONE);

    // Add this peer to the list of authorized and registered ones
    m_registeredPeers.insert(ip, index);

    // Mark it offline
    m_isPeerOnline.emplace_back(false);

    // Generate autocompletion list from peers' data
    m_peersCompletionList.append(hostname);

    ++index;
  }


  // Ping all peers asynchronously at regular intervals while this window is on
  m_peersPingTimer = std::make_unique<QTimer>(this);
  connect(m_peersPingTimer.get(), SIGNAL(timeout()), this, SLOT(pingAllPeers()));
  m_peersPingTimer->start(5000);
}

void MainWindow::initializeServer() {
  if (!m_tcpServer.listen(QHostAddress::LocalHost, m_localPort)) {
    QMessageBox::critical(this, tr("Server"),
                          tr("Errore durante l'inizializzazione del server: '%1'\n\nL'applicazione sara' chiusa.")
                          .arg(m_tcpServer.errorString()));
    exit(-1); // Cannot recover
  }

  connect(&m_tcpServer, SIGNAL(newConnection()), this, SLOT(acceptConnection()));

  qDebug() << "[initializeServer] Server now listening on port " << m_localPort;
}

namespace {
  inline std::string& ltrim(std::string &s) {
      s.erase(s.begin(), std::find_if(s.begin(), s.end(),
              std::not1(std::ptr_fun<int, int>(isspace))));
      return s;
  }

  // trim from end
  inline std::string& rtrim(std::string &s) {
      s.erase(std::find_if(s.rbegin(), s.rend(),
              std::not1(std::ptr_fun<int, int>(isspace))).base(), s.end());
      return s;
  }

  // trim from both ends
  inline std::string& trim(std::string &s) {
      return ltrim(rtrim(s));
  }
}

void MainWindow::readPeersListAndSettings() {
  using namespace std;
  ifstream file("peers.cfg");
  if (!file) {
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText("File di configurazione 'peers.cfg' mancante nella directory\n\n '" + QDir::currentPath() + "'\n\n"
                   "L'applicazione sara' in grado di ricevere files ma non di inviarne.");
    msgBox.exec();
    return;
  }

  unsigned char utf8BOM[3] = {0xef, 0xbb, 0xbf};
  unsigned char foundBOM[3];
  file >> foundBOM;
  if (memcmp(utf8BOM, utf8BOM, sizeof(utf8BOM)) != 0)
    file.seekg(ios::beg); // Not a UTF8, treat it as ASCII

  QStringList list;
  string line;
  while(getline(file, line)) {
    if(line.empty())
      continue;
    size_t i = 0;
    while(isspace(line[i])) ++i;
    if (line[i] == '#')
      continue;

    stringstream ss(line);
    string property;
    ss >> property;
    ss >> string(); // Eat '='

    if (property.compare("localport") == 0) {
      ss >> m_localPort;
    } else if (property.compare("peer") == 0) {
      string addr;
      int port = 66; // Default
      ss >> addr;
      auto s = addr.find('/');
      if (s != addr.npos) {
        port = stoi(addr.substr(s + 1));
        addr.resize(s);
      }
      string hostname;
      getline(ss, hostname); // All the rest
      hostname = trim(hostname);

      m_peers.emplace_back(QString::fromStdString(addr), port, QString::fromStdString(hostname));
    } else if (property.compare("default_download_path") == 0) {
      string downloadPath;
      getline(ss, downloadPath); // All the rest
      downloadPath = trim(downloadPath);

      auto path = QString::fromStdString(downloadPath);

      if (QDir(path).exists() == false) {
        QMessageBox::warning(this, "Path non valida", "Il percorso per il download specificato nel file di configurazione\n'"
                             + path + "'\n non esiste.\nVerra' utilizzato il percorso di default\n'"
                             + m_defaultDownloadPath + "'");
      } else
        m_defaultDownloadPath = path;
    }
  }
  file.close();
}

void MainWindow::pingAllPeers() { // SLOT

  // qDebug() << "[TIMER ELAPSED - pingAllPeers] Trying to ping all peers";

  size_t index = 0;
  for(auto& tuple : m_peers) {
    // Try to connect to all peers for a ping

    QString ip, hostname; int port;
    std::tie(ip, port, hostname) = tuple; // Unpack peer data

    QTcpSocket& socket = *m_peersClientPingSockets[index];

    // Give other time to slow pings
    auto state = *static_cast<PingClientSocketState*>(socket.property("command").data());
    if (state == PingClientSocketState::DONE) {
      socket.setProperty("command", PingClientSocketState::CONTACTED_HOST_FOR_PING);
      socket.connectToHost(ip, port);
    }

    ++index;
  }
}

void MainWindow::socketConnected() // SLOT
{
  // Get the sender socket on this endpoint
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());

  // Get the operation the socket requested and the endpoint it communicated with
  auto state = *static_cast<PingClientSocketState*>(socket->property("command").data());
  const int peerIndex = socket->property("peer_index").toInt();

  switch(state) {
    case CONTACTED_HOST_FOR_PING: {
      socket->setProperty("command", PingClientSocketState::WAITING_FOR_PONG);
      socket->write(PING, sizeof(PING)); // N.b. TCP/IP MTU over ethernet packet size ~ 1500 bytes
    } break;
    default: {
      // Unhandled state - error. Reset connection
      qDebug() << "Unhandled state for a ping socket - resetting socket";
      socket->abort();
      socket->setProperty("command", PingClientSocketState::DONE);
    }
  }
}

void MainWindow::updateClientProgress() // SLOT
{
  // Get the sender socket on this endpoint
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());

  // Get the operation the socket requested and the endpoint it communicated with
  auto state = *static_cast<PingClientSocketState*>(socket->property("command").data());
  const int peerIndex = socket->property("peer_index").toInt();
  auto& peer = m_peers[peerIndex];


  switch(state) {
    case WAITING_FOR_PONG: {

      QByteArray data = socket->readAll();
      QString command = QString(data);

      if(command.compare(PONG) == 0) {

        // Ping finished
        socket->setProperty("command", PingClientSocketState::DONE);
        socket->disconnectFromHost();
        auto item = m_peersView->topLevelItem(peerIndex);
        item->setIcon(0, QIcon(":res/green_light.png"));
        m_isPeerOnline[peerIndex] = true;
        //qDebug() << "[updateClientProgress] Peer " << std::get<2>(peer) << " is alive 'n kicking";

      } else {

        // Ping failed
        socket->setProperty("command", PingClientSocketState::DONE);
        socket->disconnectFromHost();
        auto item = m_peersView->topLevelItem(peerIndex);
        item->setIcon(0, QIcon(":res/red_light.png"));
        m_isPeerOnline[peerIndex] = false;
        qDebug() << "[updateClientProgress] Peer " << std::get<2>(peer) << " failed its pong response";

      }
    } break;
    default: {
      // Unrecognized state - reset socket
      qDebug() << "Unrecognized state for ping socket - resetting socket";
      socket->abort();
      socket->setProperty("command", PingClientSocketState::DONE);
    }
  }
}

void MainWindow::socketError(QAbstractSocket::SocketError) // SLOT
{
  // Get the sender socket on this endpoint
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());

  // Get the operation the socket requested and the endpoint it communicated with
  //auto state = *static_cast<PingClientSocketState*>(socket->property("command").data());
  //const int peerIndex = socket->property("peer_index").toInt();
  //auto& peer = m_peers[peerIndex];

  int peerIndex = socket->property("peer_index").toInt();
  auto item = m_peersView->topLevelItem(peerIndex);
  item->setIcon(0, QIcon(":res/red_light.png"));
  m_isPeerOnline[peerIndex] = false;
  socket->abort();
  socket->setProperty("command", PingClientSocketState::DONE);
  //qDebug() << "[socketError] Peer " << std::get<2>(peer) << " is NOT alive";


}

void MainWindow::acceptConnection() // SLOT
{
  if (m_tcpServer.hasPendingConnections() == false)
    return;

  auto newSocketConnection = m_tcpServer.nextPendingConnection();
  if (m_tcpServerConnections.find(newSocketConnection) != m_tcpServerConnections.end())
    return; // This connection is already being processed

  newSocketConnection->setProperty("command", ListeningServerSocketState::IDLE);
  m_tcpServerConnections.insert(newSocketConnection);

  QString addr = newSocketConnection->peerAddress().toString();
  //int port = newSocketConnection->peerPort();

  // Check that this is a registered peer, otherwise refuse the connection
  {
    auto it = m_registeredPeers.find(addr);
    if (it == m_registeredPeers.end()) {
      // Not registered/authorized
      newSocketConnection->abort();
      m_tcpServerConnections.remove(newSocketConnection);
      return;
    }
    newSocketConnection->setProperty("peer_index", it.value());
  }

  // Process the connection
  connect(newSocketConnection, SIGNAL(readyRead()),
          this, SLOT(updateServerProgress()));
  connect(newSocketConnection, SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(serverSocketError(QAbstractSocket::SocketError)));

//  serverStatusLabel->setText(tr("Accepted connection"));

  //qDebug() << "[SERVER] Accepted connection from " << addr << "/" << port;
}

void MainWindow::updateServerProgress() // SLOT
{
  // Get the sender socket on this endpoint
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
  // Get our client socket state
  auto state = *static_cast<ListeningServerSocketState*>(socket->property("command").data());

  // int bytesReceived = (int)socket->bytesAvailable();
  QByteArray data = socket->readAll();

  // Interpret client request
  switch(state) {
    case IDLE: {
      // Interpret this bulk data as a command
      QString command = QString(data);

      if(command.compare(PING) == 0) {

        // Answer with "PONG!" and close 
        socket->write(PONG, sizeof(PONG));
        socket->disconnectFromHost();
        connect(socket, &QAbstractSocket::disconnected,
                socket, &QObject::deleteLater); // Mark for deletion as soon as disconnected
        m_tcpServerConnections.remove(socket);

      } else if (command.compare(PeerFileTransfer::REQUEST_SEND_PERMISSION) == 0) {

        qDebug() << "[SERVER] Received REQUEST_SEND_PERMISSION, acknowledging";

        // Permission to start transfer -> ask the user or do as instructed
        //DEBUG - ALWAYS TRUE
        const bool permission = true;

        if(permission) {

          // Start transfer
          qDebug() << "[SERVER] Transmission Acknowledged - Start transfer";

          // Disconnect all signals from this socket, the PeerFileTransfer class will
          // handle them anyway from this point onward with another socket.
          socket->disconnect();

          size_t peerIndex = socket->property("peer_index").toULongLong();
          m_transferThreads.emplace_back(std::make_unique<PeerThreadTransfer>(SERVER, this, peerIndex, m_peers[peerIndex],
                                                                              m_defaultDownloadPath,
                                                                              socket->socketDescriptor())); // Server version
          // Delete the original socket when the thread is done (security measure to prevent socket descriptor thrashing)
          connect(m_transferThreads.back().get(), SIGNAL(finished()), socket, SLOT(deleteLater()));

          addTransferToAppropriateView(*m_transferThreads.back());

          m_transferThreads.back()->start(); // Start the transfer

        } else {
          qDebug() << "[SERVER] Denying transfer request - sending NACK_SEND_PERMISSION";
          socket->write(PeerFileTransfer::NACK_SEND_PERMISSION, strlen(PeerFileTransfer::NACK_SEND_PERMISSION) + 1);
          socket->disconnectFromHost();
          connect(socket, &QAbstractSocket::disconnected,
                  socket, &QObject::deleteLater); // Mark for deletion as soon as disconnected
          m_tcpServerConnections.remove(socket);
        }

      }
    }
  }


}

void MainWindow::serverSocketError(QAbstractSocket::SocketError) // SLOT
{
  // Get the sender socket on this endpoint
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
  qDebug() << "[ERROR - serverSocketError] Unexpected error while communicating with client";
  socket->abort();
  m_tcpServerConnections.remove(socket);
  connect(socket, &QAbstractSocket::disconnected,
          socket, &QObject::deleteLater); // Mark for deletion as soon as disconnected
}

QString MainWindow::getDownloadPath() const {
  return m_defaultDownloadPath;
}

//bool MainWindow::isPeerAlive(QString ip, int port) {
//  m_tcpClient.connectToHost(ip, port);
//  m_tcpClient.setProperty()
//  return true;
//}

//// Returns the job at a given row
//const TransferClient *MainWindow::jobForRow(int row) const
//{
//    // Return the client at the given row.
//    return jobs.at(row).client;
//}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
  // Accept any file
  QUrl url(event->mimeData()->text());
  if (url.isValid() && url.scheme().toLower() == "file")
    event->accept();
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event) {
  // Accept any file
  QUrl url(event->mimeData()->text());
  if (url.isValid() && url.scheme().toLower() == "file")
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event) {
  // Accept any drop if they exist

  const QMimeData* mimeData = event->mimeData();

  // check for our needed mime type, here a file or a list of files
  if (mimeData->hasUrls()) {
    QStringList pathList;
    QList<QUrl> urlList = mimeData->urls();

    // Get local paths and check for existence
    for (int i = 0; i < urlList.size(); ++i) {
      QString filepath = urlList.at(i).toLocalFile();
      if (!QFile::exists(filepath)) {
        QMessageBox::warning(this, "File non trovato", "File inaccessibile o inesistente:\n\n'"
                             + filepath + "'\n\nOperazione annullata.");
        return;
      }
      pathList.append(filepath);
    }

    // Bulk ready, get the receiver and start the transfer
    processSendFiles(pathList);
  }
}

void MainWindow::processSendFiles(const QStringList files) {
  // Prompt for a receiver with a word list based on all host names
  PickReceiver dialog(m_peersCompletionList, this);
  auto dialogResult = dialog.exec();
  if (dialogResult != QDialog::DialogCode::Accepted)
    return;

  int index = dialog.getSelectedItem();

  if (index == -1 || index >= m_peers.size()) {
    QMessageBox::warning(this, "Selezione non valida", "Peer non valido.");
    return;
  }

  if (m_isPeerOnline[index] == false) {
    QMessageBox::warning(this, "Peer offline", "Impossibile iniziare un trasferimento con un peer offline.\n"
                         "Controllare la connessione, i cavi di rete e che le porte su eventuali firewall siano aperte.");
    return;
  }

  // >> Initiate transfer with peer <<

  for(auto& file : files) {
    m_transferThreads.emplace_back(std::make_unique<PeerThreadTransfer>(CLIENT, this, index, m_peers[index],
                                                                        file, 0 /* socket descriptor not needed */)); // Client version

    addTransferToAppropriateView(*m_transferThreads.back());

    m_transferThreads.back()->start(); // Start the transfer
  }
}

void MainWindow::addTransferToAppropriateView(PeerThreadTransfer& transferThread) {
  // Add the transfer to the appropriate view (incoming or outgoing)
  if (transferThread.m_type == SERVER) {

    // This endpoint is receiving a file

    DynamicTreeWidgetItem *item = new DynamicTreeWidgetItem(m_receivedView);

    // item->setText(0, "0"); // Progress styled by delegate
    QString addr, hostname; int port;
    std::tie(addr, port, hostname) = transferThread.m_peer;
    item->setText(1, hostname); // Sender
    QString destinationFile = "[N/A]";
    item->setText(2, destinationFile); // Destination (local file written)

    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setTextAlignment(1, Qt::AlignHCenter);

    // Link update signals and slots
    connect(&transferThread, SIGNAL(filePercentage(int)), item, SLOT(filePercentage(int)));
    connect(&transferThread, SIGNAL(destinationAvailable(QString)), item, SLOT(destinationAvailable(QString)));

  } else {

    // This endpoint is sending a file

    DynamicTreeWidgetItem *item = new DynamicTreeWidgetItem(m_sentView);

    // item->setText(0, "0"); // Progress styled by delegate
    QString addr, hostname; int port;
    std::tie(addr, port, hostname) = transferThread.m_peer;
    item->setText(1, hostname); // Receiver
    QString localFile = transferThread.m_fileOrDownloadPath;
    item->setText(2, localFile); // Source file (local file that we're sending)

    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setTextAlignment(1, Qt::AlignHCenter);

    // Link update signals and slots
    connect(&transferThread, SIGNAL(filePercentage(int)), item, SLOT(filePercentage(int)));
  }
}
