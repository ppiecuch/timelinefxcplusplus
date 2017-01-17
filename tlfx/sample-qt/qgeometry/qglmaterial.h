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

#ifndef QGLMATERIAL_H
#define QGLMATERIAL_H

#include "qglattributeset.h"

#include <QtCore/qobject.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qurl.h>

#include <QtGui/qcolor.h>

QT_BEGIN_NAMESPACE

class QGLTexture2D;
class QGLTextureCube;
class QGLPainter;
class QGLPainter;
class QGLMaterial;
class QGLTwoSidedMaterial;

class QGLMaterialPrivate;
class QGLColorMaterialPrivate;
class QGLTwoSidedMaterialPrivate;
class QGLMaterialCollectionPrivate;

class QGLAbstractMaterial : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(QGLAbstractMaterial)
public:
    explicit QGLAbstractMaterial(QObject *parent = 0);
    ~QGLAbstractMaterial();

    virtual QGLMaterial *front() const;
    virtual QGLMaterial *back() const;

    virtual bool isTransparent() const = 0;

    virtual void bind(QGLPainter *painter) = 0;
    virtual void release(QGLPainter *painter, QGLAbstractMaterial *next) = 0;
    virtual void prepareToDraw(QGLPainter *painter, const QGLAttributeSet &attributes);

Q_SIGNALS:
    void materialChanged();
};

class QGLMaterial : public QGLAbstractMaterial
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QGLMaterial)
    Q_DISABLE_COPY(QGLMaterial)
    Q_ENUMS(TextureCombineMode)
    Q_PROPERTY(QColor ambientColor READ ambientColor WRITE setAmbientColor NOTIFY ambientColorChanged)
    Q_PROPERTY(QColor diffuseColor READ diffuseColor WRITE setDiffuseColor NOTIFY diffuseColorChanged)
    Q_PROPERTY(QColor specularColor READ specularColor WRITE setSpecularColor NOTIFY specularColorChanged)
    Q_PROPERTY(QColor emittedLight READ emittedLight WRITE setEmittedLight NOTIFY emittedLightChanged)
    Q_PROPERTY(float shininess READ shininess WRITE setShininess NOTIFY shininessChanged)
    Q_PROPERTY(QGLTexture2D *texture READ texture WRITE setTexture NOTIFY texturesChanged)
    Q_PROPERTY(QGLMaterial::TextureCombineMode textureCombineMode READ textureCombineMode WRITE setTextureCombineMode NOTIFY texturesChanged)
    Q_PROPERTY(QUrl textureUrl READ textureUrl WRITE setTextureUrl NOTIFY texturesChanged)
public:
    explicit QGLMaterial(QObject *parent = 0);
    ~QGLMaterial();

    QColor ambientColor() const;
    void setAmbientColor(const QColor& value);

    QColor diffuseColor() const;
    void setDiffuseColor(const QColor& value);

    QColor specularColor() const;
    void setSpecularColor(const QColor& value);

    QColor emittedLight() const;
    void setEmittedLight(const QColor& value);

    void setColor(const QColor& value);

    float shininess() const;
    void setShininess(float value);

    QGLTexture2D *texture(int layer = 0) const;
    void setTexture(QGLTexture2D *value, int layer = 0);

    QUrl textureUrl(int layer = 0) const;
    void setTextureUrl(const QUrl &url, int layer = 0);

    enum TextureCombineMode
    {
        Modulate,
        Decal,
        Replace
    };

    QGLMaterial::TextureCombineMode textureCombineMode(int layer = 0) const;
    void setTextureCombineMode(QGLMaterial::TextureCombineMode mode, int layer = 0);

    int textureLayerCount() const;

    QGLMaterial *front() const;
    bool isTransparent() const;
    void bind(QGLPainter *painter);
    void release(QGLPainter *painter, QGLAbstractMaterial *next);
    void prepareToDraw(QGLPainter *painter, const QGLAttributeSet &attributes);

Q_SIGNALS:
    void ambientColorChanged();
    void diffuseColorChanged();
    void specularColorChanged();
    void emittedLightChanged();
    void shininessChanged();
    void texturesChanged();

private:
    friend class QGLMaterialCollection;
    friend class QGLTwoSidedMaterial;

    void bindTextures(QGLPainter *painter);
    void bindEffect(QGLPainter *painter, const QGLAttributeSet &attributes, bool twoSided);

    QScopedPointer<QGLMaterialPrivate> d_ptr;
};

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QGLMaterial &material);
#endif

class QGLColorMaterial : public QGLAbstractMaterial
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QGLColorMaterial)
    Q_DISABLE_COPY(QGLColorMaterial)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
public:
    explicit QGLColorMaterial(QObject *parent = 0);
    ~QGLColorMaterial();

    QColor color() const;
    void setColor(const QColor &color);

    bool isTransparent() const;
    void bind(QGLPainter *painter);
    void release(QGLPainter *painter, QGLAbstractMaterial *next);
    void prepareToDraw(QGLPainter *painter, const QGLAttributeSet &attributes);

Q_SIGNALS:
    void colorChanged();

private:
    QScopedPointer<QGLColorMaterialPrivate> d_ptr;
};

class QGLTwoSidedMaterial : public QGLAbstractMaterial
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QGLTwoSidedMaterial)
    Q_DISABLE_COPY(QGLTwoSidedMaterial)
    Q_PROPERTY(QGLMaterial *front READ front WRITE setFront NOTIFY frontChanged)
    Q_PROPERTY(QGLMaterial *back READ back WRITE setBack NOTIFY backChanged)
public:
    explicit QGLTwoSidedMaterial(QObject *parent = 0);
    ~QGLTwoSidedMaterial();

    QGLMaterial *front() const;
    void setFront(QGLMaterial *material);

    QGLMaterial *back() const;
    void setBack(QGLMaterial *material);

    bool isTransparent() const;
    void bind(QGLPainter *painter);
    void release(QGLPainter *painter, QGLAbstractMaterial *next);
    void prepareToDraw(QGLPainter *painter, const QGLAttributeSet &attributes);

Q_SIGNALS:
    void frontChanged();
    void backChanged();

private:
    QScopedPointer<QGLTwoSidedMaterialPrivate> d_ptr;
};


class QGLMaterialCollection : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QGLMaterialCollection)
    Q_DISABLE_COPY(QGLMaterialCollection)
public:
    QGLMaterialCollection(QObject *parent = 0);
    virtual ~QGLMaterialCollection();

    QGLMaterial *material(int index) const;
    QGLMaterial *material(const QString &name) const;

    bool contains(QGLMaterial *material) const;
    bool contains(const QString &name) const;

    int indexOf(QGLMaterial *material) const;
    int indexOf(const QString &name) const;

    QString materialName(int index) const;

    bool isMaterialUsed(int index) const;
    void markMaterialAsUsed(int index);
    void removeUnusedMaterials();

    int addMaterial(QGLMaterial *material);
    void removeMaterial(QGLMaterial *material);
    QGLMaterial *removeMaterial(int index);

    bool isEmpty() const;
    int size() const;

private Q_SLOTS:
    void materialDeleted();

private:
    QScopedPointer<QGLMaterialCollectionPrivate> d_ptr;
};

QT_END_NAMESPACE

#endif // QGLMATERIAL_H
