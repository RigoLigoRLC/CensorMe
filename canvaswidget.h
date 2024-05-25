#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include "defs.h"
#include <QWidget>
#include <QPainter>

class CanvasWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget *parent = nullptr);

    void setCensorType(CensorType type);
    CensorType getCensorType();

    void setChunkSize(int chunkSize);
    void setBrushSize(int diameterPx);

    void switchImage(QImage baseImage, QImage maskImage, CensorType type);
    QPixmap getCurrentMaskImage();

protected:
    virtual bool eventFilter(QObject *obj, QEvent *event) override;
    virtual void paintEvent(QPaintEvent*) override;
    virtual void wheelEvent(QWheelEvent*) override;
    virtual void mouseMoveEvent(QMouseEvent*) override;
    virtual void mousePressEvent(QMouseEvent*) override;
    virtual void mouseReleaseEvent(QMouseEvent*) override;
    virtual void enterEvent(QEvent*) override;
    virtual void leaveEvent(QEvent*) override;

private:
    void processMouseDrag();

    void redetermineWidgetSize(QSize containerSize);

    void recomputeCensoredImage();
    void censorMethodPixelize();

private:
    QSize m_parentSize;

    QSize m_imageSize;
    QImage m_baseImage;
    QImage m_maskImage;
    QImage m_censoredImage;
    QPainter m_copyPainter, m_drawCensorPainter;
    double m_censorComputationTime;

    QPoint m_mouseHoverPos, m_mouseLastHoverPos;
    bool m_brushShown;
    int m_brushSize; // Diameter
    enum {
        None,
        DrawCensor,
        EraseCensor,
        DragCanvas,
    } m_mouseActionType;

    CensorType m_censorType;
    int m_chunkSize; // for pixelize
    bool m_previewShowMaskOnly;
    bool m_previewShowFullCensored;

signals:
};

#endif // CANVASWIDGET_H
