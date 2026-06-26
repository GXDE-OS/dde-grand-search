// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "highlightlabel.h"
#include "highlightutils.h"

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QEvent>

HighlightLabel::HighlightLabel(QWidget *parent, Qt::WindowFlags f)
    : QLabel(parent, f)
{
    setTextFormat(Qt::PlainText);
    m_textOption.setWrapMode(QTextOption::WrapAnywhere);
}

void HighlightLabel::setPlainText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    relayout();
}

void HighlightLabel::setKeywords(const QStringList &keywords)
{
    if (m_keywords == keywords)
        return;
    m_keywords = keywords;
    relayout();
}

void HighlightLabel::setElideMode(ElideMode mode)
{
    if (m_elideMode == mode)
        return;
    m_elideMode = mode;
    relayout();
}

void HighlightLabel::setMaxLines(int lines)
{
    if (m_maxLines == lines)
        return;
    m_maxLines = lines;
    relayout();
}

HighlightLabel::ElideMode HighlightLabel::elideMode() const
{
    return m_elideMode;
}

int HighlightLabel::maxLines() const
{
    return m_maxLines;
}

bool HighlightLabel::isElided() const
{
    return m_elided;
}

QSize HighlightLabel::sizeHint() const
{
    if (m_text.isEmpty())
        return QSize(0, 0);
    return m_layout.boundingRect().size().toSize();
}

QSize HighlightLabel::minimumSizeHint() const
{
    return sizeHint();
}

void HighlightLabel::relayout()
{
    doLayout();
    update();
}

void HighlightLabel::doLayout()
{
    m_textOption.setAlignment(alignment());
    m_layout.clearLayout();
    m_elided = false;

    if (m_text.isEmpty()) {
        m_layout.setText("");
        updateGeometry();
        return;
    }

    int layoutWidth = contentsRect().width();
    if (layoutWidth <= 0) {
        m_layout.setText("");
        updateGeometry();
        return;
    }

    ElideInfo info = computeElidedText(layoutWidth);
    m_elided = info.elided;
    buildFinalLayout(info.displayText, info.origPosMapping, layoutWidth);
}

HighlightLabel::ElideInfo HighlightLabel::computeElidedText(int layoutWidth) const
{
    ElideInfo info;
    info.displayText = m_text;

    if (m_maxLines <= 0)
        return info;

    QTextLayout probeLayout(m_text, font());
    probeLayout.setTextOption(m_textOption);

    probeLayout.beginLayout();
    for (int i = 0; i < m_maxLines; ++i) {
        QTextLine line = probeLayout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(layoutWidth);

        if (i == m_maxLines - 1) {
            int consumed = line.textStart() + line.textLength();
            if (consumed < m_text.length()) {
                info.elided = true;
                QFontMetrics fm(font());
                QString remaining = m_text.mid(line.textStart());

                HighlightUtils::ElideResult elideResult;
                if (m_elideMode == ElideMode::Smart) {
                    ElideInfo smartInfo = computeSmartElidedText(remaining, layoutWidth, fm);
                    elideResult.text = smartInfo.displayText;
                    elideResult.origPositions = smartInfo.origPosMapping;
                    info.elided = smartInfo.elided;
                } else {
                    auto qtMode = static_cast<Qt::TextElideMode>(m_elideMode);
                    Qt::TextElideMode lastLineElideMode =
                            (m_maxLines > 1 && qtMode == Qt::ElideMiddle) ? Qt::ElideLeft : qtMode;
                    elideResult = HighlightUtils::elideWithTracking(remaining, lastLineElideMode, layoutWidth, fm);
                }
                info.displayText = m_text.left(line.textStart()) + elideResult.text;

                info.origPosMapping.resize(info.displayText.length());
                int headLen = line.textStart();
                for (int j = 0; j < headLen; ++j)
                    info.origPosMapping[j] = j;
                for (int j = 0; j < elideResult.origPositions.size(); ++j) {
                    int origPos = elideResult.origPositions[j];
                    info.origPosMapping[headLen + j] = (origPos == -1) ? -1 : (line.textStart() + origPos);
                }
            }
        }
    }
    probeLayout.endLayout();

    return info;
}

void HighlightLabel::buildFinalLayout(const QString &displayText, const QVector<int> &origPosMapping, int layoutWidth)
{
    QVector<QTextLayout::FormatRange> formats = buildFormatRanges(displayText, origPosMapping);

    m_layout.clearLayout();
    m_layout.setText(displayText);
    m_layout.setFont(font());
    m_layout.setTextOption(m_textOption);
    m_layout.setFormats(formats);

    m_layout.beginLayout();
    int lineIndex = 0;
    qreal height = 0;
    QFontMetrics fm(font());
    qreal lineHeight = fm.height();

    while (true) {
        QTextLine line = m_layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(layoutWidth);
        line.setPosition(QPointF(0, height));
        height += lineHeight;
        ++lineIndex;
        if (m_maxLines > 0 && lineIndex >= m_maxLines)
            break;
    }
    m_layout.endLayout();

    updateGeometry();
}

HighlightLabel::ElideInfo HighlightLabel::computeSmartElidedText(
        const QString &text, int maxWidth, const QFontMetrics &fm) const
{
    ElideInfo info;
    info.displayText = text;

    if (text.isEmpty() || m_keywords.isEmpty())
        return info;

    if (HighlightUtils::elideWithTracking(text, Qt::ElideRight, maxWidth, fm).text == text)
        return info;

    auto matchRanges = HighlightUtils::findMatchRanges(text, m_keywords);
    if (matchRanges.isEmpty()) {
        HighlightUtils::ElideResult fallback = HighlightUtils::elideWithTracking(text, Qt::ElideRight, maxWidth, fm);
        info.displayText = fallback.text;
        info.origPosMapping = fallback.origPositions;
        info.elided = true;
        return info;
    }

    HighlightUtils::ElideResult windowResult = HighlightUtils::smartElideWithTracking(text, maxWidth, fm, matchRanges);
    if (windowResult.text.isEmpty()) {
        HighlightUtils::ElideResult fallback = HighlightUtils::elideWithTracking(text, Qt::ElideRight, maxWidth, fm);
        info.displayText = fallback.text;
        info.origPosMapping = fallback.origPositions;
        info.elided = true;
        return info;
    }

    info.displayText = windowResult.text;
    info.origPosMapping = windowResult.origPositions;
    info.elided = true;
    return info;
}

QVector<QTextLayout::FormatRange> HighlightLabel::buildFormatRanges(
        const QString &displayText, const QVector<int> &origPosMapping) const
{
    QTextCharFormat fmt = HighlightUtils::defaultHighlightFormat(font(), palette());
    return HighlightUtils::buildFormatRanges(displayText, origPosMapping, m_text, m_keywords, fmt);
}

void HighlightLabel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (m_text.isEmpty())
        return;

    QPainter painter(this);

    QRectF layoutRect = m_layout.boundingRect();
    Qt::Alignment align = alignment();

    qreal y = 0;
    Qt::Alignment vAlign = align & Qt::AlignVertical_Mask;
    if (vAlign & Qt::AlignBottom)
        y = height() - layoutRect.height();
    else if (vAlign & Qt::AlignVCenter)
        y = (height() - layoutRect.height()) / 2.0;

    m_layout.draw(&painter, QPointF(0, y));
}

void HighlightLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    relayout();
}

void HighlightLabel::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange
        || event->type() == QEvent::PaletteChange) {
        relayout();
    }
    QLabel::changeEvent(event);
}
