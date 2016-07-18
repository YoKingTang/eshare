#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTreeWidget>
#include <QItemDelegate>

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

    {
        m_sentView = new TransfersView(this);

        QStringList headers;
        headers << tr("Progresso") << tr("Destinatario") << tr("File");

        // Main torrent list
        m_sentView->setItemDelegate(new TransfersViewDelegate(this));
        m_sentView->setHeaderLabels(headers);
        m_sentView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_sentView->setAlternatingRowColors(true);
        m_sentView->setRootIsDecorated(false);

        QVBoxLayout *vbox = new QVBoxLayout;
        vbox->addWidget(m_sentView);

        ui->groupBox_sentFiles->setLayout(vbox);
    }

    {
        m_receivedView = new TransfersView(this);

        QStringList headers;
        headers << tr("Progresso") << tr("Mittente") << tr("File");

        // Main torrent list
        m_receivedView->setItemDelegate(new TransfersViewDelegate(this));
        m_receivedView->setHeaderLabels(headers);
        m_receivedView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_receivedView->setAlternatingRowColors(true);
        m_receivedView->setRootIsDecorated(false);

        QVBoxLayout *vbox = new QVBoxLayout;
        vbox->addWidget(m_receivedView);

        ui->groupBox_receivedFiles->setLayout(vbox);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
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
