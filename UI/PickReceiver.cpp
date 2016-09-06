#include <UI/PickReceiver.h>
#include <UI/MainWindow.h>
#include "ui_pickreceiver.h"
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QItemSelectionModel>
#include <QItemDelegate>
#include <QPainter>
#include <QPixmap>

/*
 * The code below is a QCompletion delegate which searches through a proxy model the index into
 * the original model corresponding to the selected index. It is an enhanced version of the
 * following code for delegates
class MyCompleter: public QCompleter {
    Q_OBJECT
public:
    MyCompleter(QObject *p = 0): QCompleter(p) {
        connect(this, SIGNAL(activated(QModelIndex)), SLOT(generateIndexSignal(QModelIndex)));
    }
    MyCompleter(QAbstractItemModel *model, QObject *p = 0): QCompleter(model, p) {
        connect(this, SIGNAL(activated(QModelIndex)), SLOT(generateIndexSignal(QModelIndex)));
    }
    MyCompleter(const QStringList& strings, QObject *p = 0): QCompleter(strings, p) {
        connect(this, SIGNAL(activated(QModelIndex)), SLOT(generateIndexSignal(QModelIndex)));
    }
signals:
    void selectedSourceRow(int index);
private slots:
    void generateIndexSignal(const QModelIndex& index)
    {
        QAbstractItemModel * const baseModel = model();
        auto lol = completionRole();
        auto lol1 = index.data();
        QModelIndexList indexList = baseModel->match(
            baseModel->index(0, completionColumn(), QModelIndex()),
            completionRole(),
            index.data(),
            1,
            Qt::MatchExactly);
        if (!indexList.isEmpty()) {
            emit selectedSourceRow(indexList.at(0).row());
        }
    }
};
*/

// This delegate for the items in the QCompletion popup model helps drawing and selecting
// the first item of the searchbox
class TreeViewItemDelegate : public QItemDelegate
{

public:
    inline TreeViewItemDelegate(QAbstractProxyModel *proxyModel, // The proxy model used by QCompleter
                                QItemSelectionModel *selModel,
                                MainWindow *mainWindow) :
      m_proxyModel(proxyModel),
      m_selModel(selModel),
      m_mainWindow(mainWindow),
      QItemDelegate(mainWindow) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index ) const Q_DECL_OVERRIDE
    {
        if (index.row() == 0) {

          // Select the first item of the proxy model (i.e. TreeView) and clear the previous one
          m_selModel->setCurrentIndex(index, QItemSelectionModel::Current);

          auto ind = m_proxyModel->mapToSource(index); // Index into the QCompleter model

          //qDebug() << "model mapped to source through QAbstractProxyModel: " << ind.model();

          // Get the index in the original combo box for the newly selected item
          const QAbstractItemModel * const baseModel = ind.model();
          QModelIndexList indexList = baseModel->match(
              baseModel->index(0, 0 /* */, QModelIndex()),
              Qt::EditRole,
              ind.data(),
              1,
              Qt::MatchExactly);

          // Draw an online icon for the candidate one
          if (!indexList.isEmpty()) {
            bool active = m_mainWindow->is_peer_active(indexList.at(0).row());
            QRect itemRect = this->rect(option, index, Qt::EditRole);
            if (active) {
              painter->drawPixmap(itemRect.x() + itemRect.width(), itemRect.y(), 15, 15,
                                  QIcon(":Res/green_light.png").pixmap(QSize(15, 15)));
            } else {
              painter->drawPixmap(itemRect.x() + itemRect.width(), itemRect.y(), 15, 15,
                                  QIcon(":Res/red_light.png").pixmap(QSize(15, 15)));
            }
          }

        }

        QItemDelegate::paint(painter, option, index); // Draw the rest
    }
private:
    QItemSelectionModel *m_selModel = nullptr;
    MainWindow *m_mainWindow = nullptr;
    QAbstractProxyModel *m_proxyModel = nullptr;
};

PickReceiver::PickReceiver(QStringList completionWords, MainWindow *parent) :
  QDialog((QWidget*)parent),
  ui(new Ui::PickReceiver)
{
  this->setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Dialog);
  auto flags = this->windowFlags();
  flags = flags & ~Qt::WindowContextHelpButtonHint;
  this->setWindowFlags(flags);

  ui->setupUi(this);

  int peerIndex = 0;
  for(auto& word : completionWords) {
    // Store the peerIndex in the Qt::UserRole for this item
    ui->comboBox_Receiver->addItem(word, peerIndex);
    ++peerIndex;
  }

  //qDebug() << "ui->comboBox_Receiver model: " << ui->comboBox_Receiver->model();

  m_completer = new QCompleter(completionWords, this);

  //qDebug() << "m_completer model: " << m_completer->model();

  // Set partial matches active
  m_completer->setCaseSensitivity(Qt::CaseInsensitive);
  m_completer->setFilterMode(Qt::MatchContains);
  m_completer->setCompletionMode(QCompleter::PopupCompletion);
  //m_completer->popup()->setSelectionMode(QAbstractItemView::SingleSelection); // Unnecessary

  QTreeView *treeView = new QTreeView(this);
  m_completer->setPopup(treeView);
  treeView->setRootIsDecorated(false);
  treeView->header()->hide();
  treeView->header()->setStretchLastSection(false);
  treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);

  //qDebug() << "treeView model: " << treeView->model();

  // Get the proxy model used in the completer
  auto proxyModel = m_completer->findChild<QAbstractProxyModel*>();
  // Set up a delegate to autoselect the first item and to show additional online graphics
  treeView->setItemDelegate(new TreeViewItemDelegate(proxyModel, treeView->selectionModel(), parent));

  ui->comboBox_Receiver->setEditable(true);
  ui->comboBox_Receiver->setCompleter(m_completer);

  ui->comboBox_Receiver->setFocus();
}

PickReceiver::~PickReceiver() {
  delete ui;
}

int PickReceiver::getSelectedItem() const {
  return ui->comboBox_Receiver->currentIndex();
}

void PickReceiver::on_pushButton_OK_clicked() {
  this->accept();
}

void PickReceiver::on_pushButton_Cancel_clicked() {
  this->reject();
}
