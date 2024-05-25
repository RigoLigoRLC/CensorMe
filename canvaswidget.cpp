#include "canvaswidget.h"
#include <QEvent>
#include <QResizeEvent>
#include <vector>
#include <array>
#include <chrono>

CanvasWidget::CanvasWidget(QWidget *parent)
    : QWidget{parent}
{
    m_chunkSize = 15;
    m_previewShowMaskOnly = true;
    m_censorType = CensorType::CT_Pixelize;

    m_copyPainter.setRenderHint(QPainter::Antialiasing, false);
    m_copyPainter.setRenderHint(QPainter::SmoothPixmapTransform, false);
}

void CanvasWidget::setChunkSize(int chunkSize)
{
    m_chunkSize = chunkSize;

    recomputeCensoredImage();
}

void CanvasWidget::switchImage(QImage baseImage, QImage maskImage, CensorType type)
{
    m_baseImage = baseImage;
    m_censorType = type;
    m_maskImage = maskImage;
    this->setFixedSize(baseImage.size());
    m_censoredImage = QImage(baseImage.size(), QImage::Format_ARGB32);

    recomputeCensoredImage();
}

bool CanvasWidget::eventFilter(QObject *obj, QEvent *event)
{
    // This event filter is only ever installed onto scroll area
    switch (event->type()) {
    case QEvent::Resize: {
        auto e = (QResizeEvent*)event;
        redetermineWidgetSize((m_parentSize = e->size()));
        break;
    }
    default:
        break;
    }
    return false;
}

void CanvasWidget::paintEvent(QPaintEvent *pe)
{
    if (m_baseImage.isNull() || m_censoredImage.isNull()) {
        return;
    }

    QPainter p(this);
    p.drawImage(rect(), m_censoredImage.isNull() ? m_baseImage : m_censoredImage);
    p.setCompositionMode(QPainter::CompositionMode_Xor);
    p.drawText(20, 50, QString("Time taken: %1s").arg(m_censorComputationTime));
    p.end();
}

void CanvasWidget::wheelEvent(QWheelEvent *)
{

}

void CanvasWidget::redetermineWidgetSize(QSize containerSize)
{
    bool determineByHeight = (containerSize.width() / (double)containerSize.height()) >
                             (m_imageSize.width() / (double)m_imageSize.height());
    QSize newSize;
    if (determineByHeight) {
        newSize = {int((double)m_imageSize.width() / m_imageSize.height() * containerSize.height()),
                   containerSize.height()};
    } else {
        newSize = {containerSize.width(),
                   int((double)m_imageSize.height() / m_imageSize.width() * containerSize.width())};
    }
    setFixedSize(newSize);
}

void CanvasWidget::recomputeCensoredImage()
{
    switch (m_censorType) {

    case CT_Pixelize:
        censorMethodPixelize();
        break;
    case CT_GaussianBlur:
        break;
    case CT_White:
        break;
    }
}

void CanvasWidget::censorMethodPixelize()
{
    int chunkSize = m_chunkSize;
    QSize imgSmallSize((m_baseImage.width() + chunkSize - 1) / chunkSize,
                       (m_baseImage.height() + chunkSize - 1) / chunkSize);
    QImage imgSmall(imgSmallSize, QImage::Format_ARGB32);

    auto hrcBegin = std::chrono::high_resolution_clock::now();

    // Construct the mean buckets for a line of resulting pixelized image
    std::vector<std::array<uint64_t, 3>> meanBucket;
    meanBucket.resize(imgSmall.width());
    for (auto &&i : meanBucket) i.fill(0); // Zero all

    std::vector<size_t> chunkWidths(imgSmall.width(), chunkSize);
    if (m_baseImage.width() % chunkSize) chunkWidths.back() = m_baseImage.width() % chunkSize;

    size_t nextRowToDump = 1;
    for (size_t y = 0; y < m_baseImage.height(); y++) {
        // One line was previously finished
        if (nextRowToDump == (y / chunkSize)) {
            for (size_t i = 0; i < meanBucket.size(); i++) {
                auto px = imgSmall.bits() + ((nextRowToDump - 1) * imgSmall.width() + i) * 4;
                size_t meanChunkPixelCount = chunkWidths[i] * chunkSize;

                px[3] = 0xff; // Alpha
                px[2] = uint8_t(meanBucket[i][0] / meanChunkPixelCount); // Red
                px[1] = uint8_t(meanBucket[i][1] / meanChunkPixelCount); // Green
                px[0] = uint8_t(meanBucket[i][2] / meanChunkPixelCount); // Blue

                meanBucket[i].fill(0);
            }
            nextRowToDump++;
        }

        for (size_t x = 0; x < m_baseImage.width(); x++) {
            auto color = m_baseImage.pixel(int(x), int(y));
            meanBucket[x / chunkSize][0] += qRed(color);
            meanBucket[x / chunkSize][1] += qGreen(color);
            meanBucket[x / chunkSize][2] += qBlue(color);
        }
    }

    // Last line of pixelated
    for (size_t i = 0; i < meanBucket.size(); i++) {
        auto px = imgSmall.bits() + ((nextRowToDump - 1) * imgSmall.width() + i) * 4;
        size_t meanChunkPixelCount = chunkWidths[i] * (m_baseImage.height() % chunkSize ?
                                                       m_baseImage.height() % chunkSize :
                                                       nextRowToDump * chunkSize);

        px[3] = 0xff; // Alpha
        px[2] = uint8_t(meanBucket[i][0] / meanChunkPixelCount); // Red
        px[1] = uint8_t(meanBucket[i][1] / meanChunkPixelCount); // Green
        px[0] = uint8_t(meanBucket[i][2] / meanChunkPixelCount); // Blue

        meanBucket[i].fill(0);
    }


    m_copyPainter.begin(&m_censoredImage);
    m_copyPainter.drawImage(QRect(QPoint{0, 0},
                                  QPoint{imgSmall.width() * chunkSize,
                                         imgSmall.height() * chunkSize}
                                  ),
                            imgSmall);
    m_copyPainter.end();

    auto hrcEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> timeTaken = hrcEnd - hrcBegin;
    m_censorComputationTime = timeTaken.count();
    m_imageSize = m_baseImage.size();

    redetermineWidgetSize(m_parentSize);
    update();
}


