#include <UI/DynamicTreeWidgetItemDelegate.h>
#include <QApplication>

bool DynamicTreeWidgetItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                                const QStyleOptionViewItem &option, const QModelIndex &index)
{
  m_model = model;
  if (event->type() == QEvent::MouseMove) {
    if (index != m_lastUnderMouse) {
      if (m_lastUnderMouse.isValid()) {
        model->setData(m_lastUnderMouse, (int)Normal, Qt::UserRole + 1);
        emit needsUpdate(m_lastUnderMouse);
      }
      if (index.isValid() && index.column() == 0) {
        model->setData(index, (int)Hovered, Qt::UserRole + 1);
        emit needsUpdate(index);
        m_lastUnderMouse = index;
      } else {
        m_lastUnderMouse = QModelIndex();
      }
    }
  }
  if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
    if (index != m_lastUnderMouse) {
      if (m_lastUnderMouse.isValid()) {
        model->setData(m_lastUnderMouse, (int)Normal, Qt::UserRole + 1);
        emit needsUpdate(m_lastUnderMouse);
      }
      if (index.isValid() && index.column() == 0) {
        model->setData(index, (int)Pressed, Qt::UserRole + 1);
        emit needsUpdate(index);
        emit clicked(index);
        m_lastUnderMouse = index;
      } else {
        m_lastUnderMouse = QModelIndex();
      }
    } else {
      if (m_lastUnderMouse.isValid()) {
        model->setData(m_lastUnderMouse, (int)Pressed, Qt::UserRole + 1);
        emit needsUpdate(m_lastUnderMouse);
        emit clicked(m_lastUnderMouse);
      }
    }
  }
  if (event->type() == QEvent::MouseButtonRelease) {
    if (index != m_lastUnderMouse) {
      if (m_lastUnderMouse.isValid()) {
        model->setData(m_lastUnderMouse, (int)Normal, Qt::UserRole + 1);
        emit needsUpdate(m_lastUnderMouse);
      }
      if (index.isValid() && index.column() == 0) {
        model->setData(index, (int)Hovered, Qt::UserRole + 1);
        emit needsUpdate(index);
        m_lastUnderMouse = index;
      } else {
        m_lastUnderMouse = QModelIndex();
      }
    } else {
      if (m_lastUnderMouse.isValid()) {
        model->setData(m_lastUnderMouse, (int)Hovered, Qt::UserRole + 1);
        emit needsUpdate(m_lastUnderMouse);
      }
    }
  }
  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void DynamicTreeWidgetItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const
{
  if (index.column() != 0)
  {
    // All columns are normal except 0
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  auto button = index.data(Qt::UserRole + 0).toBool();

  if (button)
  {
    QStyleOptionButton opt;
    State s = (State)(index.data(Qt::UserRole + 1).toInt());
    if (s == Hovered)
      opt.state |= QStyle::State_MouseOver;
    if (s == Pressed)
      opt.state |= QStyle::State_Sunken;
    opt.state |= QStyle::State_Enabled;
    opt.rect = option.rect.adjusted(-1, -1, 1, 1);
    opt.text = trUtf8("Accetta");
    QApplication::style()->drawControl(QStyle::CE_PushButton, &opt, painter, 0);
  }
  else
  {
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
    int progress = index.model()->data(index, Qt::UserRole + 2/* Progressbar value */).toInt();
    progressBarOption.progress = progress < 0 ? 0 : progress;
    progressBarOption.text = QString::asprintf("%d%%", progressBarOption.progress);

    // Draw the progress bar onto the view
    QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, painter);
  }
}

void DynamicTreeWidgetItemDelegate::notifyMouseLeave()
{
  if (m_lastUnderMouse.isValid())
  {
    // Remove hover
    m_model->setData(m_lastUnderMouse, (int)Normal, Qt::UserRole + 1);
    emit needsUpdate(m_lastUnderMouse);
  }
}
