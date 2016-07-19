#include "pickreceiver.h"
#include "ui_pickreceiver.h"
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QItemSelectionModel>
#include <QItemDelegate>
#include <QPainter>

// A more advanced autocompleter
//class AdvancedAutoCompleter : public QCompleter {
//public:
//  AdvancedAutoCompleter(QObject *parent = nullptr) :
//    QCompleter(parent), m_filterProxyModel(this) {}

//  void setModel(QAbstractItemModel *model) {
//    m_sourceModel = model;
//    QCompleter::setModel(&m_filterProxyModel);
//  }

//  class InnerProxyModel : public QSortFilterProxyModel {
//  public:
//    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) {
//      index0 = this->sourceModel()->index(source_row, 0, source_parent);
//      QString(this->sourceModel()->data(index0)).toLower()
//      return m_string in ;
//    }
//  };

//  QStringList splitPath(const QString& path) {
//    m_string = path;
//    {
//      // Update model
//      if (!m_modifiedModel)
//        m_filterProxyModel.setSourceModel(m_sourceModel);

//      // Perform a deep-split
//      auto pattern = QRegExp(m_string, Qt::CaseInsensitive, QRegExp::FixedString);
//      m_filterProxyModel.setFilterRegExp(pattern);
//    }

//    if (m_filterProxyModel.rowCount() == 0) {
//      m_modifiedModel = false;
//      m_filterProxyModel.setSourceModel(new QStringListModel(QStringList(path)));
//      return QStringList(path);
//    }

//    return QStringList();
//  }
//private:
//  QString m_string;
//  QAbstractItemModel *m_sourceModel = nullptr;
//  QSortFilterProxyModel m_filterProxyModel;
//};



// This delegate for the items in the QCompletion popup model helps drawing and selecting
// the first item of the searchbox
class TreeViewItemDelegate : public QItemDelegate
{

public:
    inline TreeViewItemDelegate(QAbstractItemModel *comboBoxModel,
                                QItemSelectionModel *selModel,
                                QMainWindow *mainWindow) :
      m_comboBoxModel(comboBoxModel),
      m_selModel(selModel),
      m_mainWindow(mainWindow),
      QItemDelegate(mainWindow) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index ) const Q_DECL_OVERRIDE
    {
        if (index.row() == 0) {

          auto realIndex = index.data(Qt::UserRole).toInt();
          //qDebug() << "Selected one is " << realIndex;

          // Also select the first item (and clear the previous one)
          m_selModel->setCurrentIndex(index, QItemSelectionModel::Current);
        }




        //m_mainWindow->isPeerActive()

        //auto item = m_peersView->topLevelItem(peerIndex);
        //item->setIcon(0, QIcon(":res/red_light.png"));

        QItemDelegate::paint(painter, option, index);

//        // Draw the progress bar onto the view
//        QBrush color(QColor(255, 0, 0));
//        painter->fillRect(0, 0, 10, 10, color );
    }
private:
    QItemSelectionModel *m_selModel = nullptr;
    QMainWindow *m_mainWindow = nullptr;
    QAbstractItemModel *m_comboBoxModel = nullptr;
};

PickReceiver::PickReceiver(QStringList completionWords, QMainWindow *parent) :
  QDialog((QWidget*)parent),
  ui(new Ui::PickReceiver)
{
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

  m_completer = new QCompleter(completionWords, this);

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

  treeView->setItemDelegate(new TreeViewItemDelegate(ui->comboBox_Receiver->model(),
                                                     treeView->selectionModel(), parent));


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
  setResult(DialogCode::Accepted);
  this->close();
}

void PickReceiver::on_pushButton_Cancel_clicked() {
  setResult(DialogCode::Rejected);
  this->close();
}
