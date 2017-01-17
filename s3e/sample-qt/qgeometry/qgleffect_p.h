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

#ifndef QGLEFFECT_P_H
#define QGLEFFECT_P_H

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

#include "qglpainter.h"
#include <QtCore/QScopedPointer.h>

QT_BEGIN_NAMESPACE

#if defined(QT_OPENGL_ES_2)
#define QGL_SHADERS_ONLY 1
#endif

class QGLFlatColorEffectPrivate;
class QGLPerVertexColorEffectPrivate;
class QGLFlatTextureEffectPrivate;
class QGLFlatDecalTextureEffectPrivate;
class QGLLitMaterialEffectPrivate;
class QGLShaderProgramEffectPrivate;

class QOpenGLShaderProgram;

class QGLFlatColorEffect : public QGLAbstractEffect
{
public:
    QGLFlatColorEffect();
    virtual ~QGLFlatColorEffect();

    bool supportsPicking() const;
    void setActive(QGLPainter *painter, bool flag);
    void update(QGLPainter *painter, QGLPainter::Updates updates);

private:
    QScopedPointer<QGLFlatColorEffectPrivate> d_ptr;

    Q_DECLARE_PRIVATE(QGLFlatColorEffect)
    Q_DISABLE_COPY(QGLFlatColorEffect)
};

class QGLPerVertexColorEffect : public QGLAbstractEffect
{
public:
    QGLPerVertexColorEffect();
    virtual ~QGLPerVertexColorEffect();

    void setActive(QGLPainter *painter, bool flag);
    void update(QGLPainter *painter, QGLPainter::Updates updates);

private:
    QScopedPointer<QGLPerVertexColorEffectPrivate> d_ptr;

    Q_DECLARE_PRIVATE(QGLPerVertexColorEffect)
    Q_DISABLE_COPY(QGLPerVertexColorEffect)
};

class QGLFlatTextureEffect : public QGLAbstractEffect
{
public:
    QGLFlatTextureEffect();
    virtual ~QGLFlatTextureEffect();

    void setActive(QGLPainter *painter, bool flag);
    void update(QGLPainter *painter, QGLPainter::Updates updates);

private:
    QScopedPointer<QGLFlatTextureEffectPrivate> d_ptr;

    Q_DECLARE_PRIVATE(QGLFlatTextureEffect)
    Q_DISABLE_COPY(QGLFlatTextureEffect)
};

class QGLFlatDecalTextureEffect : public QGLAbstractEffect
{
public:
    QGLFlatDecalTextureEffect();
    virtual ~QGLFlatDecalTextureEffect();

    void setActive(QGLPainter *painter, bool flag);
    void update(QGLPainter *painter, QGLPainter::Updates updates);

private:
    QScopedPointer<QGLFlatDecalTextureEffectPrivate> d_ptr;

    Q_DECLARE_PRIVATE(QGLFlatDecalTextureEffect)
    Q_DISABLE_COPY(QGLFlatDecalTextureEffect)
};

class QGLLitMaterialEffect : public QGLAbstractEffect
{
public:
    QGLLitMaterialEffect();
    virtual ~QGLLitMaterialEffect();

    void setActive(QGLPainter *painter, bool flag);
    void update(QGLPainter *painter, QGLPainter::Updates updates);

protected:
    QGLLitMaterialEffect
        (GLenum mode, const char *vshader, const char *fshader,
         const QString& programName);

private:
    QScopedPointer<QGLLitMaterialEffectPrivate> d_ptr;

    Q_DECLARE_PRIVATE(QGLLitMaterialEffect)
    Q_DISABLE_COPY(QGLLitMaterialEffect)
};


class QGLLitTextureEffect : public QGLLitMaterialEffect
{
public:
    virtual ~QGLLitTextureEffect();

protected:
    QGLLitTextureEffect(GLenum mode, const char *vshader, const char *fshader,
                        const QString& programName);
};

class QGLLitDecalTextureEffect : public QGLLitTextureEffect
{
public:
    QGLLitDecalTextureEffect();
    virtual ~QGLLitDecalTextureEffect();
};

class QGLLitModulateTextureEffect : public QGLLitTextureEffect
{
public:
    QGLLitModulateTextureEffect();
    virtual ~QGLLitModulateTextureEffect();
};

class QGLShaderProgramEffect : public QGLAbstractEffect
{
public:
    QGLShaderProgramEffect();
    virtual ~QGLShaderProgramEffect();

    void setActive(QGLPainter *painter, bool flag);
    void update(QGLPainter *painter, QGLPainter::Updates updates);

    QByteArray vertexShader() const;
    void setVertexShader(const QByteArray &source);
    void setVertexShaderFromFile(const QString &fileName);

    QByteArray geometryShader() const;
    void setGeometryShader(const QByteArray &source);
    void setGeometryShaderFromFile(const QString &fileName);

    QByteArray fragmentShader() const;
    void setFragmentShader(const QByteArray &source);
    void setFragmentShaderFromFile(const QString &fileName);

    int maximumLights() const;
    void setMaximumLights(int value);

    QOpenGLShaderProgram *program() const;

protected:
    virtual bool beforeLink();
    virtual void afterLink();

private:
    QScopedPointer<QGLShaderProgramEffectPrivate> d_ptr;

    Q_DISABLE_COPY(QGLShaderProgramEffect)
    Q_DECLARE_PRIVATE(QGLShaderProgramEffect)
};

QT_END_NAMESPACE

#endif // QGLEFFECT_P_H
