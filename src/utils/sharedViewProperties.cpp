#include "sharedViewProperties.h"
#include <QtMath>

SharedViewProperties::SharedViewProperties(QObject* parent) :
    QObject(parent) {
}

qreal SharedViewProperties::zoom() const {
    return m_zoom;
}
qreal SharedViewProperties::centerX() const {
    return m_centerX;
}
qreal SharedViewProperties::centerY() const {
    return m_centerY;
}
bool SharedViewProperties::isZoomed() const {
    return m_zoom > 1.0;
}

void SharedViewProperties::setZoom(qreal zoom) {
    qreal newZoom = qBound(1.0, zoom, m_maxZoom);
    if (qFuzzyCompare(m_zoom, newZoom))
        return;
    m_zoom = newZoom;
    emit viewChanged();
}

void SharedViewProperties::setCenterX(qreal centerX) {
    if (qFuzzyCompare(m_centerX, centerX))
        return;
    m_centerX = centerX;
    emit viewChanged();
}

void SharedViewProperties::setCenterY(qreal centerY) {
    if (qFuzzyCompare(m_centerY, centerY))
        return;
    m_centerY = centerY;
    emit viewChanged();
}

void SharedViewProperties::reset() {
    m_zoom = 1.0;
    m_centerX = 0.5;
    m_centerY = 0.5;
    emit viewChanged();
}

void SharedViewProperties::applyPan(qreal dx, qreal dy) {
    if (!isZoomed())
        return;
    m_centerX += dx / m_zoom;
    m_centerY += dy / m_zoom;
    emit viewChanged();
}

void SharedViewProperties::applyZoom(qreal factor, qreal videoX, qreal videoY) {
    qreal newZoom = qBound(1.0, m_zoom * factor, m_maxZoom);
    if (qFuzzyCompare(newZoom, m_zoom))
        return;

    qreal shiftCoef = 1.0 / m_zoom - 1.0 / newZoom;
    m_centerX += (videoX - 0.5) * shiftCoef;
    m_centerY += (videoY - 0.5) * shiftCoef;
    m_zoom = newZoom;

    if (m_zoom <= 1.001) {
        reset();
    }

    emit viewChanged();
}

void SharedViewProperties::zoomToSelection(
    const QRectF& selection, const QRectF& videoRect, qreal currentZoom, qreal currentCenterX, qreal currentCenterY) {
    QRectF currentViewRect(
        currentCenterX - 0.5 / currentZoom, currentCenterY - 0.5 / currentZoom, 1.0 / currentZoom, 1.0 / currentZoom);

    QRectF selectionInView;
    selectionInView.setX((selection.x() - videoRect.x()) / videoRect.width());
    selectionInView.setY((selection.y() - videoRect.y()) / videoRect.height());
    selectionInView.setWidth(selection.width() / videoRect.width());
    selectionInView.setHeight(selection.height() / videoRect.height());

    QRectF mappedSelection;
    mappedSelection.setX(currentViewRect.x() + selectionInView.x() * currentViewRect.width());
    mappedSelection.setY(currentViewRect.y() + selectionInView.y() * currentViewRect.height());
    mappedSelection.setWidth(selectionInView.width() * currentViewRect.width());
    mappedSelection.setHeight(selectionInView.height() * currentViewRect.height());

    QRectF clampedRect = mappedSelection.intersected(currentViewRect);
    if (clampedRect.width() <= 1e-6 || clampedRect.height() <= 1e-6)
        return;

    m_zoom = qBound(1.0, m_zoom * (currentViewRect.width() / clampedRect.width()), m_maxZoom);
    m_centerX = clampedRect.center().x();
    m_centerY = clampedRect.center().y();

    emit viewChanged();
}