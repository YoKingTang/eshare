#include <UI/MainWindow.h>
#include "ui_mainwindow.h"
#include <QTreeWidget>
#include <QItemDelegate>
#include <QGroupBox>
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

#include <iostream> // DEBUG

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

    //m_defaultDownloadPath = QDir::currentPath();

    connect(ui->pushButton, SIGNAL(clicked(bool)), this, SLOT(SIMULATE_SEND(bool)));

    initialize_peers();
    initialize_server();
}

MainWindow::~MainWindow()
{
  delete ui;
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

  // Process every peer found
  size_t index = 0;
  for(auto& tuple : m_peers)
  {
    QString ip, hostname; int service_port, transfer_port;
    std::tie(ip, service_port, transfer_port, hostname) = tuple; // Unpack peer data

    QTreeWidgetItem *item = new QTreeWidgetItem(m_peersView);
    item->setText(1, hostname);
    item->setTextAlignment(1, Qt::AlignLeft);
    item->setIcon(0, QIcon(":res/red_light.png"));

    ++index;
  }
}

void MainWindow::initialize_server()
{
  if (!m_service_server.listen(QHostAddress::LocalHost, m_service_port)) {
    QMessageBox::critical(this, tr("Server"),
                          tr("Errore durante l'inizializzazione del service server: '%1'\n\nL'applicazione sara' chiusa.")
                          .arg(m_service_server.errorString()));
    exit(1); // Cannot recover
  }

  connect(&m_service_server, SIGNAL(newConnection()), this, SLOT(process_new_connection()));

  qDebug() << "[initialize_server] Server now listening on port " << m_service_port;
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

void MainWindow::add_new_external_transfer_request(TransferRequest req)
{
  m_external_transfer_requests.append(req);
  // TODO: update UI
}

void MainWindow::add_new_my_transfer_request(TransferRequest req)
{
  m_my_transfer_requests.append(req);
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

  auto read_transfer_request = [](QTcpSocket *socket, TransferRequest& fill) {
    QDataStream read_stream(socket);
    read_stream.setVersion(QDataStream::Qt_5_7);
    read_stream.startTransaction();
    read_stream >> fill.m_unique_id >> fill.m_file_path >> fill.m_size >>
                   fill.m_sender_address >> fill.m_sender_transfer_port;
    if (!read_stream.commitTransaction())
    {
      read_stream.rollbackTransaction();
      return false; // Wait for more data
    }
    return true;
  };

  if (state == "not_processed")
  {
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

    // TODO: command ping

    if (command == "FILE")
    {
      socket->setProperty("state", "sending_transfer_request");
      TransferRequest temp;
      if(!read_transfer_request(socket, temp))
        return; // Wait for more data
      add_new_external_transfer_request(temp);
      socket->disconnectFromHost();
    }
  }
  else if (state == "sending_transfer_request")
  {
    TransferRequest temp;
    if(!read_transfer_request(socket, temp))
      return; // Wait for more data
    add_new_external_transfer_request(temp);
    socket->disconnectFromHost();
  }
}

void MainWindow::server_socket_error(QAbstractSocket::SocketError err) // SLOT
{
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
  qDebug() << "[server_socket_error] Error: " << err;
  socket->abort();
}

void MainWindow::service_socket_connected() // SLOT
{
  TransferRequest req = TransferRequest::generate_unique();
  req.m_file_path = "C:/Users/Alex/Desktop/car.mpg";
  req.m_size = 6277450;
  req.m_sender_address = get_local_address();
  req.m_sender_transfer_port = m_transfer_port;
  QByteArray block;
  QDataStream out(&block, QIODevice::WriteOnly);
  out.setVersion(QDataStream::Qt_5_7);
  out << QString("FILE") << req.m_unique_id << req.m_file_path << req.m_size
      << req.m_sender_address << req.m_sender_transfer_port;
  m_service_socket.write(block);
}

void MainWindow::service_socket_read_ready() // SLOT
{

}

void MainWindow::service_socket_error(QAbstractSocket::SocketError err) // SLOT
{
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
  qDebug() << "[server_socket_error] Error: " << err;
  socket->abort();
}

void MainWindow::SIMULATE_SEND(bool) // SLOT DEBUG
{
  // GRAB FIRST ONE
  auto first = m_peers.front();

  QString ip, hostname; int service_port, transfer_port;
  std::tie(ip, service_port, transfer_port, hostname) = first;

  connect(&m_service_socket, SIGNAL(connected()), this, SLOT(service_socket_connected()));
  connect(&m_service_socket, SIGNAL(readyRead()), this, SLOT(service_socket_read_ready()));
  connect(&m_service_socket, SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(service_socket_error(QAbstractSocket::SocketError)), Qt::DirectConnection);

  m_service_socket.abort();
  m_service_socket.connectToHost(ip, service_port);
}
