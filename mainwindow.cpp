#include "mainwindow.h"
#include "ui_mainwindow.h"
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

    if (property.compare("local_ping_port") == 0)
    {
      ss >> m_ping_port;
    }
    else if (property.compare("local_transfer_port") == 0)
    {
      ss >> m_transfer_port;
    }
    else if (property.compare("peer") == 0)
    {
      string addr;
      int ping_port = 66; // Default
      int transfer_port = 67; // Default
      ss >> addr;
      auto s1 = addr.find('/');
      auto s2 = addr.find('/', s1 + 1);
      if (s1 != addr.npos && s2 != addr.npos)
      {
        ping_port = stoi(addr.substr(s1 + 1, s2));
        transfer_port = stoi(addr.substr(s2 + 1));
        addr.resize(s1);
      }
      string hostname;
      getline(ss, hostname); // All the rest
      hostname = trim(hostname);

      m_peers.emplace_back(QString::fromStdString(addr), ping_port, transfer_port, QString::fromStdString(hostname));
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
    QString ip, hostname; int ping_port, transfer_port;
    std::tie(ip, ping_port, transfer_port, hostname) = tuple; // Unpack peer data

    QTreeWidgetItem *item = new QTreeWidgetItem(m_peersView);
    item->setText(1, hostname);
    item->setTextAlignment(1, Qt::AlignLeft);
    item->setIcon(0, QIcon(":res/red_light.png"));

    ++index;
  }
}

void MainWindow::initialize_server()
{

}
