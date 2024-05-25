#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->scrollArea->installEventFilter(ui->widCanvas); // Canvas needs to know resizes happened there
     // Force trigger once
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actOpenOneImg_triggered()
{
    auto file = QFileDialog::getOpenFileName(this,
                                             tr("Open one single image"),
                                             "",
                                             "JPEG/PNG Image (*.jpg;*.jpeg;*.png)");

    if (file.isEmpty()) {
        return;
    }

    QImage img(file);
    if (img.isNull()) {
        QMessageBox::critical(this, tr("Failed to load image"), tr("Probably is not supported format"));
        return;
    }

    ui->widCanvas->switchImage(img, QImage(), CensorType::CT_Pixelize);
}


void MainWindow::on_sliderChunkSize_sliderMoved(int position)
{
    ui->widCanvas->setChunkSize(position);
    ui->lblChunkSize->setText(QString("%1px").arg(position));
}


void MainWindow::on_sliderBrushSize_sliderMoved(int position)
{
    ui->widCanvas->setBrushSize(position);
}

