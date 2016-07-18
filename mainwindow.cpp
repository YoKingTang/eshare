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
#include <fstream>
#include <string>
#include <sstream>

// PeersView is the tree widget which lists all the reachable peers
class PeersView : public QTreeWidget
{
public:
  PeersView(QWidget*) {};
};

// PeersView is used to draw the online checks
class PeersViewDelegate : public QItemDelegate
{

public:
    inline PeersViewDelegate(MainWindow *mainWindow) : QItemDelegate(mainWindow) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index ) const Q_DECL_OVERRIDE
    {
        if (index.column() != 0) {
            QItemDelegate::paint(painter, option, index);
            return;
        }
        painter->setRenderHint(QPainter::Antialiasing);

        QLinearGradient redGradient(0, 0, 10, 0);
        redGradient.setColorAt(0, QColor(100, 0, 0));
        redGradient.setColorAt(1, QColor(255, 0, 0));
        painter->setBrush(QBrush(redGradient));
        QRect rect = option.rect;
        //painter->drawEllipse();
    }
};


// TransfersView is the tree widget which lists all of the ongoing file transfers
class TransfersView : public QTreeWidget
{

public:
    TransfersView(QWidget*);

//signals:
//    void fileDropped(const QString &fileName);

//protected:
//    void dragMoveEvent(QDragMoveEvent *event) Q_DECL_OVERRIDE;
//    void dropEvent(QDropEvent *event) Q_DECL_OVERRIDE;
};

// TransfersViewDelegate is used to draw the progress bars
class TransfersViewDelegate : public QItemDelegate
{

public:
    inline TransfersViewDelegate(MainWindow *mainWindow) : QItemDelegate(mainWindow) {}

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
        int progress = 23; //qobject_cast<MainWindow *>(parent())->jobForRow(index.row())->progress();
        progressBarOption.progress = progress < 0 ? 0 : progress;
        progressBarOption.text = QString::asprintf("%d%%", progressBarOption.progress);

        // Draw the progress bar onto the view
        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, painter);
    }
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QHBoxLayout *hbox = new QHBoxLayout; // Main layout

    {
      QWidget *leftWidgets = new QWidget;
      QSizePolicy spLeft(QSizePolicy::Preferred, QSizePolicy::Preferred);
      spLeft.setHorizontalStretch(3);
      leftWidgets->setSizePolicy(spLeft);

      QVBoxLayout *vbox = new QVBoxLayout;
      leftWidgets->setLayout(vbox);


      QGroupBox *sentGB = new QGroupBox("Files inviati");
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

      QGroupBox *receivedGB = new QGroupBox("Files ricevuti");
      {
          m_receivedView = new TransfersView(this);

          QStringList headers;
          headers << tr("Progresso") << tr("Mittente") << tr("File");

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

      QGroupBox *onlineGB = new QGroupBox("Peers");
      QSizePolicy spRight(QSizePolicy::Preferred, QSizePolicy::Preferred);
      spRight.setHorizontalStretch(1);
      onlineGB->setSizePolicy(spRight);
      {
        m_peersView = new PeersView(this);

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

    initializePeers();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initializePeers() {
  auto peers = readPeersList();

  for(int i = 0; i < peers.size(); i += 2) {
    QString addr = peers[i];
    QString hostname = peers[i + 1];

    QTreeWidgetItem *item = new QTreeWidgetItem(m_peersView);
    item->setIcon(0, QIcon(":res/red_light.png"));
    item->setText(1, hostname);
    item->setTextAlignment(1, Qt::AlignLeft);
  }
}

QStringList MainWindow::readPeersList() {
  using namespace std;
  ifstream file("peers.cfg");
  if (!file) {
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText("File di configurazione 'peers.cfg' mancante nella directory\n\n '" + QDir::currentPath() + "'\n\n"
                   "L'applicazione sara' in grado di ricevere files ma non di inviarne.");
    msgBox.exec();
    return QStringList();
  }
  QStringList list;
  string line;
  while(getline(file, line)) {
    if(line.empty())
      continue;
    size_t i = 0;
    while(isspace(line[i])) ++i;
    if (line[i] == '#')
      continue;
    string addr, hostname;
    stringstream ss(line);
    ss >> addr;
    getline(ss, hostname); // All the rest
    list.append(QString::fromStdString(addr));
    list.append(QString::fromStdString(hostname));
  }
  file.close();
  return list;
}

//// Returns the job at a given row
//const TransferClient *MainWindow::jobForRow(int row) const
//{
//    // Return the client at the given row.
//    return jobs.at(row).client;
//}

TransfersView::TransfersView(QWidget*) {
    //setAcceptDrops(true);
}
