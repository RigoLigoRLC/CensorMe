#include "defs.h"
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDirIterator>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->scrollArea->installEventFilter(ui->widCanvas); // Canvas needs to know resizes happened there

    m_previewModeGroup.addButton(ui->radPreviewOrig, PM_Original);
    m_previewModeGroup.addButton(ui->radPreviewFullyCensored, PM_FullyCensored);
    m_previewModeGroup.addButton(ui->radPreviewMask, PM_MaskOnly);
    m_previewModeGroup.addButton(ui->radPreviewMaskOnImg, PM_MaskOnImage);
    m_previewModeGroup.addButton(ui->radPreviewFinal, PM_FinalPreview);
    connect(&m_previewModeGroup, &QButtonGroup::idClicked, this, &MainWindow::previewModeButtonGroupIdClicked);
    ui->widCanvas->setPreviewMode(m_previewModeGroup.checkedId());

    ui->lstFileList->setModel(&m_fsModel);

    setOperatingInFolderMode(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::previewModeButtonGroupIdClicked(int id)
{
    ui->widCanvas->setPreviewMode(id);
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
    setOperatingInFolderMode(false);

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
    ui->lblChunkSize->setText(tr("%1px").arg(position));
}


void MainWindow::on_sliderBrushSize_sliderMoved(int position)
{
    ui->widCanvas->setBrushSize(position);
    ui->lblBrushSize->setText(tr("%1px").arg(position));
}


void MainWindow::on_actOpenFolder_triggered()
{
    auto folder = QFileDialog::getExistingDirectory(this,
                                                    tr("Open a folder of many images"),
                                                    "");
    if (folder.isEmpty()) {
        return;
    }

    setOperatingInFolderMode(true);

    m_fsModel.setFilter(QDir::Filter::Files | QDir::NoDotAndDotDot);
    m_fsModel.setNameFilters({"*.jpg", "*.jpeg", "*.png"});
    m_fsModel.setNameFilterDisables(false);
    m_fsModel.setRootPath(folder);
    ui->lstFileList->setRootIndex(m_fsModel.index(folder));

//    QDirIterator dit(folder, {"*.jpg", "*.jpeg", "*.png"}, QDir::Filter::Files | QDir::NoDotAndDotDot);
//    QStringList fileList;
//    while (dit.hasNext()) {
//        auto file = dit.next();
//        qDebug() << file;
//        fileList << file;
//    }
}

void MainWindow::setOperatingInFolderMode(bool isInFolderMode)
{
    ui->lstFileList->setEnabled(isInFolderMode);
    ui->lstFileList->setVisible(isInFolderMode ? ui->btnToggleFileList->isChecked() : false);
    ui->btnToggleFileList->setEnabled(isInFolderMode);
    ui->btnNextImg->setEnabled(isInFolderMode);
    ui->btnPrevImg->setEnabled(isInFolderMode);
}


void MainWindow::on_btnToggleFileList_toggled(bool checked)
{
    ui->lstFileList->setVisible(checked);
}

