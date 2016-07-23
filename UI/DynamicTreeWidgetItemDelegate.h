#ifndef DYNAMICTREEWIDGETITEMDELEGATE_H
#define DYNAMICTREEWIDGETITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QObject>
#include <QEvent>

enum State {Normal = 0, Pressed, Hovered};

// TransfersViewDelegate is used to draw the progress bars and other goodies on the list view items
class DynamicTreeWidgetItemDelegate : public QStyledItemDelegate
{

  // Qt::UserRole + 0 -> (bool)Button?
  // Qt::UserRole + 1 -> (int - State)Button style
  // Qt::UserRole + 2 -> (int)Progressbar value
  Q_OBJECT
public:
    DynamicTreeWidgetItemDelegate(QObject *parent = Q_NULLPTR) :
      QStyledItemDelegate(parent) {}

    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                      const QStyleOptionViewItem &option, const QModelIndex &index);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index ) const Q_DECL_OVERRIDE;

    void notifyMouseLeave();
private:
    QModelIndex m_lastUnderMouse;
    QAbstractItemModel *m_model;

signals:
    void needsUpdate(const QModelIndex&);
    void clicked(const QModelIndex);
};

#endif // DYNAMICTREEWIDGETITEMDELEGATE_H
