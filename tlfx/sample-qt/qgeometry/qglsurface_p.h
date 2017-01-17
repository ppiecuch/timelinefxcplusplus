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

#ifndef QGLSURFACE_P_H
#define QGLSURFACE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qglsurface.h"

#include <QPainter>

QT_BEGIN_NAMESPACE

class QGLMaskedSurfacePrivate;

class QGLPainterSurface : public QGLAbstractSurface
{
public:
    static const int QGLPAINTER_SURFACE_ID = 503;

    explicit QGLPainterSurface(QPainter *painter)
        : QGLAbstractSurface(QGLPAINTER_SURFACE_ID)
        , m_painter(painter)
    {
    }
    ~QGLPainterSurface() {}

    bool activate(QGLAbstractSurface *prevSurface);
    void deactivate(QGLAbstractSurface *nextSurface);
    QRect viewportGL() const;
    bool isValid() const;

private:
    QPainter *m_painter;

    Q_DISABLE_COPY(QGLPainterSurface)
};

class QGLDrawBufferSurface : public QGLAbstractSurface
{
public:
    QGLDrawBufferSurface(QGLAbstractSurface *surface, GLenum buffer)
        : QGLAbstractSurface(500)
        , m_surface(surface), m_buffer(buffer) {}
    ~QGLDrawBufferSurface() {}

    bool activate(QGLAbstractSurface *prevSurface);
    void deactivate(QGLAbstractSurface *nextSurface);
    QRect viewportGL() const;
    bool isValid() const;

private:
    QGLAbstractSurface *m_surface;
    GLenum m_buffer;

    Q_DISABLE_COPY(QGLDrawBufferSurface)
};

class QGLMaskedSurface : public QGLAbstractSurface
{
public:
    enum BufferMaskBit
    {
        RedMask     = 0x0001,
        GreenMask   = 0x0002,
        BlueMask    = 0x0004,
        AlphaMask   = 0x0008
    };
    Q_DECLARE_FLAGS(BufferMask, BufferMaskBit)

    QGLMaskedSurface();
    QGLMaskedSurface
        (QGLAbstractSurface *surface, QGLMaskedSurface::BufferMask mask);
    ~QGLMaskedSurface();

    QGLAbstractSurface *surface() const;
    void setSurface(QGLAbstractSurface *surface);

    QGLMaskedSurface::BufferMask mask() const;
    void setMask(QGLMaskedSurface::BufferMask mask);

    bool activate(QGLAbstractSurface *prevSurface = 0);
    void deactivate(QGLAbstractSurface *nextSurface = 0);
    QRect viewportGL() const;
    bool isValid() const;

private:
    QScopedPointer<QGLMaskedSurfacePrivate> d_ptr;

    Q_DECLARE_PRIVATE(QGLMaskedSurface)
    Q_DISABLE_COPY(QGLMaskedSurface)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QGLMaskedSurface::BufferMask)

QT_END_NAMESPACE

#endif // QGLSURFACE_P_H
