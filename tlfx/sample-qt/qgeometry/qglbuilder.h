/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QGLBUILDER_H
#define QGLBUILDER_H

#include "qglnamespace.h"
#include "qglattributevalue.h"
#include "qgeometrydata.h"

#include <QSizeF>
#include <QVector>
#include <QList>
#include <QVector3D>
#include <QFont>
#include <QPainterPath>

#include <QtGui/private/qvectorpath_p.h>

QT_BEGIN_NAMESPACE

class QGLSection;
class QGLBuilderPrivate;

class QGLBuilder
{
public:
    QGLBuilder();
    virtual ~QGLBuilder();

    // section management
    void newSection(QGL::Smoothing sm = QGL::Smooth);

    // geometry building by primitive
    void addTriangles(const QGeometryData &triangle);
    void addQuads(const QGeometryData &quad);
    void addTriangleFan(const QGeometryData &fan);
    void addTriangleStrip(const QGeometryData &strip);
    void addTriangulatedFace(const QGeometryData &face);
    void addQuadStrip(const QGeometryData &strip);
    void addQuadsInterleaved(const QGeometryData &top,
                        const QGeometryData &bottom);
    inline void addPane(float size = 1.0f);
    inline void addPane(QSizeF size);

    QList<QGeometryData> optimized();

protected:
    // internal and test functions
    QGLSection *currentSection() const;
    QList<QGLSection*> sections() const;
    void setDefaultThreshold(int);

private:
    Q_DISABLE_COPY(QGLBuilder)
    void addSection(QGLSection *section);

    friend class QGLSection;

    QGLBuilderPrivate *dptr;
};

inline void QGLBuilder::addPane(float size)
{
    addPane(QSizeF(size, size));
}

inline void QGLBuilder::addPane(QSizeF size)
{
    QSizeF f = size / 2.0f;
    QVector2D a(-f.width(), -f.height());
    QVector2D b(f.width(), -f.height());
    QVector2D c(f.width(), f.height());
    QVector2D d(-f.width(), f.height());
    QVector2D ta(0.0f, 0.0f);
    QVector2D tb(1.0f, 0.0f);
    QVector2D tc(1.0f, 1.0f);
    QVector2D td(0.0f, 1.0f);
    QGeometryData quad;
    quad.appendVertex(a, b, c, d);
    quad.appendTexCoord(ta, tb, tc, td);
    addQuads(quad);
}

QGLBuilder& operator<<(QGLBuilder& builder, const QGL::Smoothing& smoothing);
QGLBuilder& operator<<(QGLBuilder& builder, const QGeometryData& triangles);


// Geometry builders:
// ------------------

QGeometryData qQuadPlane(QSizeF size = QSizeF(100.0f, 100.0f), int level = 3);
QGeometryData qCheckerQuadPlane(QSizeF size = QSizeF(100.0f, 100.0f), QPointF origin = QPointF(0,0), int level = 3, QColor color1 = Qt::darkGray, QColor color2 = Qt::lightGray);


namespace Qt {
	enum ExtrudeFlags {
		CapStart = 0x001,
		CapEnd = 0x002,
		CapAll = CapStart | CapEnd
	};
}

QGeometryData qExtrude(const qreal *polygon, int count, qreal extrude= 1, Qt::ExtrudeFlags flag = Qt::CapAll, uint hint = QVectorPath::PolygonHint | QVectorPath::OddEvenFill, const QTransform &matrix = QTransform());
QGeometryData qExtrude(const QVectorPath &path, qreal extrude= 1, Qt::ExtrudeFlags flag = Qt::CapAll, const QTransform &matrix = QTransform(), qreal lod = 1);
QGeometryData qExtrude(const QPainterPath &path, qreal extrude= 1, Qt::ExtrudeFlags flag = Qt::CapAll, const QTransform &matrix = QTransform(), qreal lod = 1);
QGeometryData qExtrude(const QGeometryData &path, qreal extrude= 1, Qt::ExtrudeFlags flag = Qt::CapAll, uint hint = QVectorPath::PolygonHint | QVectorPath::OddEvenFill, const QTransform &matrix = QTransform());


QGeometryData qtGeometryDataForPainterPath(const QPainterPath &_value, qreal lod = 1);
QGeometryData qtGeometryDataForVectorPath(const QVectorPath &_path, qreal lod = 1);



class QGLText
{
public:
	enum textCaps {
		CapStart = Qt::CapStart,
		CapEnd = Qt::CapEnd,
		CapAll = Qt::CapAll,
		CapLine = 0x00100
	};

	QGLText();
	QGLText(QString _text,QFont _font);

	QString text() const;
	void setText(QString _text);

	QFont font() const;
	void setFont(QFont _font);

	qreal extrude() const;
	void setExtrude(qreal _val);

	textCaps caps() const;
	void setCaps(textCaps _val);

	qreal stroke() const;
	void setStroke(qreal _val);

	qreal strokeExtrude() const;
	void setStrokeExtrude(qreal _val);

	Qt::Alignment Align() const;
	void setAlign(Qt::Alignment _val);

	QGeometryData buildText() const;

private:
	QString m_text;
	QFont m_font;
	qreal m_extrude;
	qreal m_strokeExtrude;
	qreal m_stroke;
	textCaps m_caps;
	Qt::Alignment m_align;
};

QGLBuilder& operator<<(QGLBuilder& _builder, const QGLText& _text);

QT_END_NAMESPACE

#endif // QGLBUILDER_H
