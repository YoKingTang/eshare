#include <UI/TransferTreeView.h>
#include <QEvent>
#include <QKeyEvent>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#define STRICT_TYPED_ITEMIDS
#include <Shlobj.h>
#endif

TransferTreeView::TransferTreeView(bool receiving_view, QWidget *parent) :
  m_receiving_view(receiving_view),
  QTreeWidget(parent)
{
  m_delegate = new DynamicTreeWidgetItemDelegate(this);
  connect(m_delegate, SIGNAL(needsUpdate(const QModelIndex&)), this, SLOT(update(const QModelIndex&)));
  connect(m_delegate, SIGNAL(clicked(const QModelIndex)), this, SLOT(clicked(const QModelIndex)));
  connect(this, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(double_clicked(QModelIndex)));
  this->setItemDelegate(m_delegate);
  this->installEventFilter(this);
}

bool TransferTreeView::TransferTreeView::eventFilter(QObject *obj, QEvent *event)
{
  if (obj != this->viewport())
    return QWidget::eventFilter(obj, event);
  switch (event->type()) {
    case QEvent::Leave:
      m_delegate->notifyMouseLeave();
      break;
    case QEvent::MouseMove:
      QModelIndex index = this->indexAt(static_cast<QMouseEvent*>(event)->pos());
      if (!index.isValid())
        m_delegate->notifyMouseLeave();
      break;
  }
  return QWidget::eventFilter(obj, event);
}

void TransferTreeView::clicked(const QModelIndex item) // SLOT
{
  // Set style to progressbar
  this->model()->setData(item, false, Qt::UserRole + 0);
  emit click(item); // Forward on
}

void TransferTreeView::double_clicked(const QModelIndex item) // SLOT
{
#ifdef _WIN32
  auto file_path = this->topLevelItem(item.row())->data(2, Qt::DisplayRole).toString();
  auto progress = this->topLevelItem(item.row())->data(0, Qt::UserRole + 2).toInt();

  QFileInfo finfo(file_path);
  if (m_receiving_view && progress < 100)
  {
    // Just show the directory
    ShellExecuteA(NULL, "open", finfo.dir().absolutePath().toStdString().c_str(),
                  NULL, NULL, SW_SHOWDEFAULT);
  }
  else
  {
    QString full_path = finfo.absoluteDir().absolutePath();

    std::wstring path = full_path.toStdWString();
    std::wstring file = file_path.toStdWString();

    std::replace(path.begin(), path.end(), '/', '\\');
    std::replace(file.begin(), file.end(), '/', '\\');

    PIDLIST_ABSOLUTE dir = ILCreateFromPath(path.c_str());
    PIDLIST_ABSOLUTE item1 = ILCreateFromPath(file.c_str());
    const PIDLIST_ABSOLUTE selection[] = {item1};
    UINT count = sizeof(selection) / sizeof(PIDLIST_ABSOLUTE);

    SHOpenFolderAndSelectItems(dir, count, (PCUITEMID_CHILD_ARRAY)selection, 0);

    ILFree(dir);
    ILFree(item1);
  }
#endif
}

// FIXME: this can probably be done in a better way (i.e. reset the delegate/model association)
void TransferTreeView::resetDelegate()
{
  if (m_delegate)
    delete m_delegate;
  m_delegate = new DynamicTreeWidgetItemDelegate(this);
  connect(m_delegate, SIGNAL(needsUpdate(const QModelIndex&)), this, SLOT(update(const QModelIndex&)));
  connect(m_delegate, SIGNAL(clicked(const QModelIndex)), this, SLOT(clicked(const QModelIndex)));
  this->setItemDelegate(m_delegate);
}
