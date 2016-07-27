#include <UI/MainWindow.h>
#include <ui_mainwindow.h>
#include <UI/PickReceiver.h>
#include <UI/DynamicTreeWidgetItem.h>
#include <UI/DynamicTreeWidgetItemDelegate.h>
#include <Receiver/TransferStarter.h>
#include <QNetworkConfigurationManager>
#include <QNetworkSession>
#include <QPushButton>
#include <QSettings>
#include <QTreeWidget>
#include <QGroupBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QDir>
#include <QDragMoveEvent>
#include <QMimeData>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <functional>
#include <QDataStream>
#include <QNetworkInterface>
#include <QFileInfo>
#include <functional>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setWindowTitle("Pannello di controllo");
    this->setWindowIcon(QIcon(":/Res/ekashare_icon.png"));
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
          m_sentView = new TransferTreeView(this);

          QStringList headers;
          headers << tr("Progresso") << tr("Destinatario") << tr("File");

          // Main outgoing transfers list
          m_sentView->setHeaderLabels(headers);
          m_sentView->setSelectionBehavior(QAbstractItemView::SelectRows);
          m_sentView->setAlternatingRowColors(true);
          m_sentView->setRootIsDecorated(false);

          QPushButton *clear_button = new QPushButton(this);
          clear_button->setText("Cancella Inviati");
          connect(clear_button, SIGNAL(clicked(bool)), SLOT(clear_sent(bool)));

          QVBoxLayout *vbox = new QVBoxLayout;
          vbox->addWidget(m_sentView);
          vbox->addWidget(clear_button);

          sentGB->setLayout(vbox);
      }

      QGroupBox *receivedGB = new QGroupBox("Files ricevuti", this);
      {
          m_receivedView = new TransferTreeView(this);

          QStringList headers;
          headers << tr("Progresso") << tr("Mittente") << tr("Destinazione");

          // Main incoming transfers list
          m_receivedView->setHeaderLabels(headers);
          m_receivedView->setSelectionBehavior(QAbstractItemView::SelectRows);
          m_receivedView->setAlternatingRowColors(true);
          m_receivedView->setRootIsDecorated(false);
          connect(m_receivedView, SIGNAL(click(QModelIndex)), this, SLOT(listview_transfer_accepted(QModelIndex)));

          QPushButton *clear_button = new QPushButton(this);
          clear_button->setText("Cancella Ricevuti");
          connect(clear_button, SIGNAL(clicked(bool)), SLOT(clear_received(bool)));

          QVBoxLayout *vbox = new QVBoxLayout;
          vbox->addWidget(m_receivedView);
          vbox->addWidget(clear_button);

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

        m_peersView->setHeaderLabels(headers);
        m_peersView->resizeColumnToContents(0);
        m_peersView->header()->close();
        m_peersView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_peersView->setAlternatingRowColors(true);
        m_peersView->setRootIsDecorated(false);

        QVBoxLayout *vbox = new QVBoxLayout;
        vbox->addWidget(m_peersView);

        onlineGB->setLayout(vbox);

        {
          QPixmap eka(":/Res/ekashare_icon.png");
          QLabel *ekaImg = new QLabel(this);
          ekaImg->setPixmap(eka);
          ekaImg->setAlignment(Qt::AlignCenter);

          vbox->addWidget(ekaImg);
        }
      }

      hbox->addWidget(onlineGB);
    }

    ui->centralWidget->setLayout(hbox);

    initialize_tray_icon();
    m_tray_icon->show();

    // Continue with non-UI initialization

    m_default_download_path = QDir::currentPath();

    initialize_peers();

    // Network initialization
    QNetworkConfigurationManager manager;
    if (manager.capabilities() & QNetworkConfigurationManager::NetworkSessionRequired) {
        // Get saved network configuration
        QSettings settings(QSettings::UserScope, QLatin1String("eshare"));
        settings.beginGroup(QLatin1String("QtNetwork"));
        const QString id = settings.value(QLatin1String("DefaultNetworkConfiguration")).toString();
        settings.endGroup();

        // If the saved network configuration is not currently discovered use the system default
        QNetworkConfiguration config = manager.configurationFromIdentifier(id);
        if ((config.state() & QNetworkConfiguration::Discovered) != QNetworkConfiguration::Discovered) {
            config = manager.defaultConfiguration();
        }

        m_network_session = new QNetworkSession(config, this);
        connect(m_network_session, &QNetworkSession::opened, this, &MainWindow::network_session_opened);
        connect(m_network_session, static_cast<void(QNetworkSession::*)(QNetworkSession::SessionError)>(&QNetworkSession::error),
              [&](QNetworkSession::SessionError){
          QMessageBox::critical(this, tr("Errore di rete"),
                                tr("Impossibile inizializzare il gestore di interfacce di rete: '%1'")
                                .arg(m_network_session->errorString()));
          exit(1); // Cannot recover
        });

        m_network_session->open();
    }
    else
        network_session_opened();
}

MainWindow::~MainWindow()
{
  if (m_transfer_listener->isRunning())
    m_transfer_listener->terminate();
  if (m_peers_ping_timer)
    m_peers_ping_timer->stop();
  delete ui;
}

void MainWindow::network_session_opened() // SLOT
{
  // Save the used configuration
  if (m_network_session) {
      QNetworkConfiguration config = m_network_session->configuration();
      QString id;
      if (config.type() == QNetworkConfiguration::UserChoice)
          id = m_network_session->sessionProperty(QLatin1String("UserChoiceConfiguration")).toString();
      else
          id = config.identifier();

      QSettings settings(QSettings::UserScope, QLatin1String("eshare"));
      settings.beginGroup(QLatin1String("QtNetwork"));
      settings.setValue(QLatin1String("DefaultNetworkConfiguration"), id);
      settings.endGroup();
  }

  // Perform last initialization steps
  initialize_servers();
  initialize_peers_ping();
}

void MainWindow::clear_sent(bool) // SLOT
{
  bool pending_uncompleted = false;

  {
    QMutexLocker lock(&m_my_transfer_requests_mutex);
    for(int i = 0; i < m_my_transfer_requests.size(); ++i)
    {
      int value = m_sentView->topLevelItem(i)->data(2, Qt::UserRole + 2).toInt();
      if (value != 100)
      {
        pending_uncompleted = true;
        break;
      }
    }
    if (!pending_uncompleted)
    {
      // No pending sent requests, just clear
      m_sentView->clear();
      m_sentView->resetDelegate();
      m_my_transfer_requests.clear();
      return;
    }
  }
  auto reply = QMessageBox::question(this, "Conferma di cancellazione", "Ci sono trasferimenti inviati non ancora"
                                     "completati. Si vuole cancellare lo stesso tutti i trasferimenti inviati?\n"
                                     "(I destinatari non saranno in grado di completare un trasferimento annullato)",
                                     QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes)
  {
    QMutexLocker lock(&m_my_transfer_requests_mutex);
    m_sentView->clear();
    m_sentView->resetDelegate();
    m_my_transfer_requests.clear();
  }
}

void MainWindow::clear_received(bool) // SLOT
{
  bool pending_uncompleted = false;

  {
    for(int i = 0; i < m_external_transfer_requests.size(); ++i)
    {
      int value = m_receivedView->topLevelItem(i)->data(2, Qt::UserRole + 2).toInt();
      if (value != 100)
      {
        pending_uncompleted = true;
        break;
      }
    }
    if (!pending_uncompleted)
    {
      // No pending sent requests, just clear
      m_receivedView->clear();
      m_receivedView->resetDelegate();
      m_external_transfer_requests.clear();
      return;
    }
  }
  auto reply = QMessageBox::question(this, "Conferma di cancellazione", "Ci sono trasferimenti ricevuti non ancora"
                                     "completati. Si vuole cancellare lo stesso tutti i trasferimenti ricevuti?\n"
                                     "(I mittenti non saranno in grado di completare un trasferimento annullato)",
                                     QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes)
  {
    m_receivedView->clear();
    m_receivedView->resetDelegate();
    m_external_transfer_requests.clear();
  }
}

void MainWindow::initialize_tray_icon()
{
  m_open_action = new QAction(tr("&Apri Pannello di Controllo"), this);
  connect(m_open_action, &QAction::triggered, this, &QWidget::showNormal);

  m_quit_action = new QAction(tr("&Esci"), this);
  connect(m_quit_action, &QAction::triggered, qApp, &QCoreApplication::quit);

  m_tray_icon_menu = new QMenu(this);
  m_tray_icon_menu->addAction(m_open_action);
  m_tray_icon_menu->addSeparator();
  m_tray_icon_menu->addAction(m_quit_action);

  m_tray_icon = new QSystemTrayIcon(this);
  m_tray_icon->setContextMenu(m_tray_icon_menu);
  m_tray_icon->setIcon(QIcon(":/Res/ekashare_icon.png"));
  m_tray_icon->setToolTip("eKAshare");

  // Handle double click messages
  connect(m_tray_icon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
          this, SLOT(tray_icon_activated(QSystemTrayIcon::ActivationReason)));

  connect(m_tray_icon, &QSystemTrayIcon::messageClicked,
          this, [&](){
                      this->showNormal();
                    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  event->ignore();
  this->hide();
}

void MainWindow::tray_icon_activated(QSystemTrayIcon::ActivationReason reason) // SLOT
{
  if (reason == QSystemTrayIcon::DoubleClick)
    this->showNormal();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  // Accept any file
  QUrl url(event->mimeData()->text());
  if (url.isValid() && url.scheme().toLower() == "file")
    event->accept();
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
  // Accept any file
  QUrl url(event->mimeData()->text());
  if (url.isValid() && url.scheme().toLower() == "file")
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
  // Accept any drop if they exist
  const QMimeData* mime_data = event->mimeData();

  // Check the mime type, allowed types: a file or a list of files
  if (mime_data->hasUrls()) {
    QStringList path_list;
    QList<QUrl> url_list = mime_data->urls();

    // Get local paths and check for existence
    for (int i = 0; i < url_list.size(); ++i) {
      QString filepath = url_list.at(i).toLocalFile();
      if (!QFile::exists(filepath)) {
        QMessageBox::warning(this, "File non trovato", "File inaccessibile o inesistente:\n\n'"
                             + filepath + "'\n\nOperazione annullata.");
        return;
      }
      path_list.append(filepath);
    }

    // path_list now contains the bulk of files to process

    // Prompt for a receiver with a word list based on all host names
    PickReceiver dialog(m_peers_completion_list, this);
    auto dialogResult = dialog.exec();
    if (dialogResult != QDialog::DialogCode::Accepted)
      return;

    int index = dialog.getSelectedItem();

    if (index == -1 || index >= m_peers.size()) {
      QMessageBox::warning(this, "Selezione non valida", "Peer non valido.");
      return;
    }

    if (m_peer_online[index] == false) {
      QMessageBox::warning(this, "Peer offline", "Impossibile iniziare un trasferimento con un peer offline.\n"
                           "Controllare la connessione, i cavi di rete e che le porte su eventuali firewall siano aperte.");
      return;
    }

    // >> Send requests to peer <<

    QTcpSocket *temporary_service_socket = new QTcpSocket(this);
    m_my_pending_requests_to_send.clear();

    for(auto& file : path_list)
    {
      TransferRequest req = TransferRequest::generate_unique();
      req.m_file_path = file;
      req.m_size = QFile(file).size();
      req.m_sender_address = get_local_address();
      req.m_sender_transfer_port = m_transfer_port;

      m_my_pending_requests_to_send.append(req);
    }

    auto peer = m_peers[index];

    QString ip, hostname; int service_port, transfer_port;
    std::tie(ip, service_port, transfer_port, hostname) = peer;

    connect(temporary_service_socket, SIGNAL(connected()), this, SLOT(temporary_service_socket_connected()));
    connect(temporary_service_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(temporary_service_socket_error(QAbstractSocket::SocketError)));
    connect(temporary_service_socket, &QAbstractSocket::disconnected,
            temporary_service_socket, &QObject::deleteLater);

    temporary_service_socket->connectToHost(ip, service_port);
  }
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

void MainWindow::read_peers_from_file()
{
  using namespace std;
  ifstream file("peers.cfg");
  if (!file)
  {
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
  while(getline(file, line))
  {
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

    if (property.compare("local_service_port") == 0)
    {
      ss >> m_service_port;
    }
    else if (property.compare("local_transfer_port") == 0)
    {
      ss >> m_transfer_port;
    }
    else if (property.compare("peer") == 0)
    {
      string addr;
      int service_port = 66; // Default
      int transfer_port = 67; // Default
      ss >> addr;
      auto s1 = addr.find('/');
      auto s2 = addr.find('/', s1 + 1);
      if (s1 != addr.npos && s2 != addr.npos)
      {
        service_port = stoi(addr.substr(s1 + 1, s2));
        transfer_port = stoi(addr.substr(s2 + 1));
        addr.resize(s1);
      }
      string hostname;
      getline(ss, hostname); // All the rest
      hostname = trim(hostname);

      m_peers.emplace_back(QString::fromStdString(addr), service_port, transfer_port, QString::fromStdString(hostname));
    }
    else if (property.compare("default_download_path") == 0)
    {
      string download_path;
      getline(ss, download_path); // All the rest
      download_path = trim(download_path);

      auto path = QString::fromStdString(download_path);

      if (QDir(path).exists() == false)
      {
        QMessageBox::warning(this, "Path non valida", "Il percorso per il download specificato nel file di configurazione\n'"
                             + path + "'\n non esiste.\nVerra' utilizzato il percorso di default\n'"
                             + m_default_download_path + "'");
      }
      else
        m_default_download_path = path;
    }
  }
  file.close();
}

void MainWindow::initialize_peers()
{
  // Load peers and other settings from configuration files
  read_peers_from_file();

  m_peer_online.resize(m_peers.size(), false);

  // Process every peer found
  size_t index = 0;
  for(auto& tuple : m_peers)
  {
    QString ip, hostname; int service_port, transfer_port;
    std::tie(ip, service_port, transfer_port, hostname) = tuple; // Unpack peer data

    QTreeWidgetItem *item = new QTreeWidgetItem(m_peersView);
    item->setText(1, hostname);
    item->setTextAlignment(1, Qt::AlignLeft);
    item->setIcon(0, QIcon(":Res/red_light.png"));

    // Generate autocompletion list from peers' data
    m_peers_completion_list.append(hostname);

    ++index;
  }
}

void MainWindow::initialize_servers()
{
  if (!m_service_server.listen(QHostAddress::Any, m_service_port))
  {
    QMessageBox::critical(this, tr("Server"),
                          tr("Errore durante l'inizializzazione del service server: '%1'\n\nL'applicazione sara' chiusa.")
                          .arg(m_service_server.errorString()));
    exit(1); // Cannot recover
  }

  connect(&m_service_server, SIGNAL(newConnection()), this, SLOT(process_new_connection()));

  qDebug() << "[initialize_server] Service server now listening on port " << m_service_port;

  m_transfer_listener = std::make_unique<TransferListener>(this, std::bind(&MainWindow::my_transfer_retriever, this,
                                                                           std::placeholders::_1, std::placeholders::_2));

  m_transfer_listener->set_transfer_port(m_transfer_port);
  m_transfer_listener->moveToThread(m_transfer_listener.get());
  m_transfer_listener->start();

  qDebug() << "[initialize_server] Transfer server now listening on port " << m_transfer_port;
}

void MainWindow::initialize_peers_ping()
{
  // Ping all peers asynchronously at regular intervals while this window is on
  m_peers_ping_timer = std::make_unique<QTimer>(this);
  connect(m_peers_ping_timer.get(), SIGNAL(timeout()), this, SLOT(ping_peers()));
  m_peers_ping_timer->start(10000);
}

QString MainWindow::get_local_address()
{
  foreach (const QHostAddress &address, QNetworkInterface::allAddresses()) {
    if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress(QHostAddress::LocalHost))
      return address.toString();
  }
  qDebug() << "[get_local_address] Could not determine local address";
  return QString("127.0.0.1");
}

void MainWindow::add_new_external_transfer_requests(QVector<TransferRequest> reqs)
{
  qint64 total_size = 0;

  for(auto& request : reqs)
  {
    m_external_transfer_requests.append(request);

    DynamicTreeWidgetItem *item = new DynamicTreeWidgetItem(m_receivedView);

    item->setData(0, Qt::UserRole + 0, true); // Start with accept button
    item->setData(0, Qt::UserRole + 2, 0); // Progressbar 0%

    // item->setText(0, "0"); // Progress styled by delegate
    item->setText(1, get_peer_name_from_address(request.m_sender_address)); // Sender
    item->setText(2, form_local_destination_file(request)); // Destination (local file written)

    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setTextAlignment(1, Qt::AlignHCenter);

    total_size += request.m_size;
  }

  QString info_text;
  QTextStream stream(&info_text);
  stream << "Ricevuta richiesta di trasferimento per ";
  if (reqs.size() > 1)
    stream << reqs.size() << " files (";
  else
    stream << " 1 file (";

  stream << Chunker::format_size_human(total_size);
  stream << ") da \n\t" << get_peer_name_from_address(reqs.first().m_sender_address) << " @ "
         << reqs.first().m_sender_address << "\n";

  stream.flush();
  m_tray_icon->showMessage("eKAshare", info_text, QSystemTrayIcon::Information, 2000);
}

void MainWindow::add_new_my_transfer_requests(QVector<TransferRequest> reqs)
{
  QMutexLocker lock(&m_my_transfer_requests_mutex);
  for (auto& req : reqs)
  {
    m_my_transfer_requests.append(req);

    DynamicTreeWidgetItem *item = new DynamicTreeWidgetItem(m_sentView);

    item->setData(0, Qt::UserRole + 0, false); // Start with progress bar - no control on sent
    item->setData(0, Qt::UserRole + 2, 0); // Progressbar 0%

    // item->setText(0, "0"); // Progress styled by delegate
    item->setText(1, get_peer_name_from_address(req.m_sender_address)); // Sender
    QString destinationFile = req.m_file_path;
    item->setText(2, destinationFile); // Destination (remote file written)

    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setTextAlignment(1, Qt::AlignHCenter);
  }
}

bool MainWindow::my_transfer_retriever(TransferRequest& req, DynamicTreeWidgetItem *& item_ptr)
{
  QMutexLocker lock(&m_my_transfer_requests_mutex);
  int index = 0;
  for(auto& el : m_my_transfer_requests)
  {
    if (el.m_unique_id == req.m_unique_id)
    {
      req = el;
      item_ptr = (DynamicTreeWidgetItem*)m_sentView->topLevelItem(index);
      return true;
    }
    ++index;
  }
  return false;
}

QString MainWindow::form_local_destination_file(TransferRequest& req) const
{
  // Create a local file destination from an external request
  QFileInfo finfo(QFile(req.m_file_path).fileName());
  QString fname(finfo.fileName());
  return m_default_download_path + QDir::separator() + fname;
}

QString MainWindow::get_peer_name_from_address(QString address) const
{
  for(auto& peer : m_peers)
  {
    QString ip, hostname; int service_port, transfer_port;
    std::tie(ip, service_port, transfer_port, hostname) = peer;
    if (ip == address)
      return hostname;
  }
  return QString("[Unknown]");
}

void MainWindow::process_new_connection() // SLOT
{
  if (m_service_server.hasPendingConnections() == false)
      return;

  auto new_connection_socket = m_service_server.nextPendingConnection();
  new_connection_socket->setProperty("state", "not_processed");

  // Process the connection
  connect(new_connection_socket, SIGNAL(readyRead()), this, SLOT(server_ready_read()));
  connect(new_connection_socket, SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(server_socket_error(QAbstractSocket::SocketError)));
  connect(new_connection_socket, &QAbstractSocket::disconnected,
          new_connection_socket, &QObject::deleteLater);
}

void MainWindow::server_ready_read() // SLOT
{
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
  auto state = socket->property("state").toString();

  auto read_transfer_requests = [](QTcpSocket *socket, QVector<TransferRequest>& fill, int files_n) {
    QDataStream read_stream(socket);
    read_stream.setVersion(QDataStream::Qt_5_7);
    read_stream.startTransaction();
    for(int i = 0; i < files_n; ++i)
    {
      TransferRequest req;
      read_stream >> req.m_unique_id >> req.m_file_path >> req.m_size >>
                     req.m_sender_address >> req.m_sender_transfer_port;
      fill.append(req);
    }
    if (!read_stream.commitTransaction())
    {
      read_stream.rollbackTransaction();
      fill.clear();
      return false; // Wait for more data
    }
    return true;
  };

  if (state == "not_processed")
  {
    QString command;
    qint64 n_files;
    QDataStream read_stream(socket);
    read_stream.setVersion(QDataStream::Qt_5_7);
    read_stream.startTransaction();
    read_stream >> command >> n_files;
    if (!read_stream.commitTransaction())
    {
      read_stream.rollbackTransaction();
      return; // Wait for more data
    }

    if (command == "PING?")
    {
      socket->setProperty("state", "pong_sent");

      QByteArray block;
      QDataStream out(&block, QIODevice::WriteOnly);
      out.setVersion(QDataStream::Qt_5_7);
      out << QString("PONG!");

      socket->write(block);
      socket->flush();
      socket->disconnectFromHost();
    }
    else if (command == "FILES")
    {
      if (n_files <= 0 || n_files > 500)
      {
        qDebug() << "[server_ready_read] Dropping invalid n_files request: " << n_files;
        socket->abort();
        return;
      }
      socket->setProperty("state", "sending_transfer_requests");
      socket->setProperty("files_n", (int)n_files);

      QVector<TransferRequest> temp;
      if(!read_transfer_requests(socket, temp, n_files))
        return; // Wait for more data

      socket->setProperty("state", "processed");

      add_new_external_transfer_requests(temp);

      socket->flush();
      socket->disconnectFromHost();
    }
  }
  else if (state == "sending_transfer_requests")
  {
    int n_files = socket->property("files_n").toInt();

    QVector<TransferRequest> temp;
    if(!read_transfer_requests(socket, temp, n_files))
      return; // Wait for more data

    socket->setProperty("state", "processed");

    add_new_external_transfer_requests(temp);

    socket->flush();
    socket->disconnectFromHost();
  }
}

void MainWindow::server_socket_error(QAbstractSocket::SocketError err) // SLOT
{
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
  qDebug() << "[server_socket_error] Error: " << err;
  socket->abort();
}

void MainWindow::temporary_service_socket_connected() // SLOT
{
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());

  // Send pending transfer requests

  QByteArray block;
  QDataStream out(&block, QIODevice::WriteOnly);
  out.setVersion(QDataStream::Qt_5_7);

  out << QString("FILES") << qint64(m_my_pending_requests_to_send.size());

  for(auto& req : m_my_pending_requests_to_send)
  {
    out << req.m_unique_id << req.m_file_path << req.m_size
        << req.m_sender_address << req.m_sender_transfer_port;

  }

  socket->write(block);

  add_new_my_transfer_requests(m_my_pending_requests_to_send);

  socket->flush();
  socket->disconnectFromHost();
}

void MainWindow::temporary_service_socket_error(QAbstractSocket::SocketError err) // SLOT
{
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
  m_my_pending_requests_to_send.clear(); // No local pending requests can be sent
  qDebug() << "[temporary_service_socket_error] Failed sending latest TransferRequest vector: " << err;
  socket->abort();
}

void MainWindow::service_socket_error(QAbstractSocket::SocketError err) // SLOT
{
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
  qDebug() << "[server_socket_error] Error: " << err;
  socket->abort();
}

void MainWindow::listview_transfer_accepted(QModelIndex index) // SLOT
{
  qDebug() << "[listview_transfer_accepted] Accepting transfer - " << index.row();

  auto req = m_external_transfer_requests[index.row()];

  TransferStarter *ts = new TransferStarter(req, form_local_destination_file(req));
  connect(ts, SIGNAL(finished()), ts, SLOT(deleteLater()));

  DynamicTreeWidgetItem *item = (DynamicTreeWidgetItem*)m_receivedView->topLevelItem(index.row());
  connect(ts, SIGNAL(update_percentage(int)), item, SLOT(update_percentage(int)));

  ts->start();
}

void MainWindow::ping_peers() // SLOT
{
  size_t index = 0;
  for(auto& tuple : m_peers)
  {
    QString ip, hostname; int service_port, transfer_port;
    std::tie(ip, service_port, transfer_port, hostname) = tuple;

    QTcpSocket *ping_socket = new QTcpSocket(this);
    ping_socket->setProperty("state", "awaiting_ping_connection");
    ping_socket->setProperty("peer_id", index);

    connect(ping_socket, SIGNAL(connected()), this, SLOT(ping_socket_connected()));
    connect(ping_socket, SIGNAL(readyRead()), this, SLOT(ping_socket_ready_read()));
    connect(ping_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(ping_failed(QAbstractSocket::SocketError)));
    connect(ping_socket, &QAbstractSocket::disconnected,
            ping_socket, &QObject::deleteLater);

    ping_socket->connectToHost(ip, service_port);

    ++index;
  }
}

void MainWindow::ping_failed(QAbstractSocket::SocketError err) // SLOT
{
  (void)err;
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
  auto peer_id = socket->property("peer_id").toInt();

  m_peer_online[peer_id] = false;

  auto item = m_peersView->topLevelItem(peer_id);
  item->setIcon(0, QIcon(":Res/red_light.png"));

  socket->abort();
}

void MainWindow::ping_socket_connected() // SLOT
{
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());

  QByteArray block;
  QDataStream out(&block, QIODevice::WriteOnly);
  out.setVersion(QDataStream::Qt_5_7);
  out << QString("PING?") << qint64(0) /* Placeholder for 0 files */;

  socket->write(block);
  socket->flush();
}

void MainWindow::ping_socket_ready_read() // SLOT
{
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
  auto peer_id = socket->property("peer_id").toInt();

  QString command;
  QDataStream read_stream(socket);
  read_stream.setVersion(QDataStream::Qt_5_7);
  read_stream.startTransaction();
  read_stream >> command;
  if (!read_stream.commitTransaction())
  {
    read_stream.rollbackTransaction();
    return; // Wait for more data
  }

  if (command == "PONG!")
  {
    m_peer_online[peer_id] = true;

    auto item = m_peersView->topLevelItem(peer_id);
    item->setIcon(0, QIcon(":Res/green_light.png"));
  }
  else
  {
    m_peer_online[peer_id] = false;

    auto item = m_peersView->topLevelItem(peer_id);
    item->setIcon(0, QIcon(":Res/red_light.png"));

    qDebug() << "[ping_socket_ready_read] Wrong ping response received";
  }
  socket->disconnectFromHost();
}
