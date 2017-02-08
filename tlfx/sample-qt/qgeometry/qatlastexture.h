/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QATLASTEXTURE_H
#define QATLASTEXTURE_H

#include <QtCore/QSize>
#include <QtCore/QRect>
#include <QtCore/QPoint>
#include <QtCore/QPointer>
#include <QtGui/QOpenGLTexture>

QT_BEGIN_NAMESPACE

namespace QGL
{

struct QAreaAllocatorNode;

class QAreaAllocator
{
public:
    QAreaAllocator(const QSize &size);
    ~QAreaAllocator();

    QRect allocate(const QSize &size);
    bool deallocate(const QRect &rect);
    bool isEmpty() const { return m_root == 0; }
    QSize size() const { return m_size; }
private:
    bool allocateInNode(const QSize &size, QPoint &result, const QRect &currentRect, QAreaAllocatorNode *node);
    bool deallocateInNode(const QPoint &pos, QAreaAllocatorNode *node);
    void mergeNodeWithNeighbors(QAreaAllocatorNode *node);

    QAreaAllocatorNode *m_root;
    QSize m_size;
};

} // namespace QGL

class QTexture;

class QTextureAtlas : public QObject
{
public:
    QTextureAtlas(const QSize &size);
    ~QTextureAtlas();
    void invalidate();

    int textureId() const;
    void bind(QOpenGLTexture::Filter filtering = QOpenGLTexture::Linear);
    void release();

    void upload(QTexture *texture);
    void uploadBgra(QTexture *texture);

    QTexture *create(const QImage &image);
    void remove(QTexture *t);

    QSize size() const { return m_size; }

    uint internalFormat() const { return m_internalFormat; }
    uint externalFormat() const { return m_externalFormat; }

private:
    QGL::QAreaAllocator m_allocator;
    unsigned int m_texture_id;
    QSize m_size;
    QList<QTexture *> m_pending_uploads;

    uint m_internalFormat;
    uint m_externalFormat;

    int m_atlas_transient_image_threshold;

    uint m_allocated : 1;
    uint m_use_bgra_fallback: 1;

    uint m_debug_overlay : 1;
};

class QTexture : public QObject
{
    Q_OBJECT
public:
    QTexture(QTextureAtlas *atlas, const QRect &textureRect, const QImage &image);
    ~QTexture();

    int textureId() const { return m_atlas->textureId(); }
    QSize textureSize() const { return atlasSubRectWithoutPadding().size(); }
    bool hasMipmaps() const { return false; }
    bool isAtlasTexture() const { return true; }

    QRectF normalizedTextureSubRect() const { return m_texture_coords_rect; }
    QRectF convertToNormalizedSourceRect(const QRectF &rect) const
    {
        QSize s = textureSize();
        QRectF r = normalizedTextureSubRect();

        qreal sx = r.width() / s.width();
        qreal sy = r.height() / s.height();

        return QRectF(r.x() + rect.x() * sx,
                    r.y() + rect.y() * sy,
                    rect.width() * sx,
                    rect.height() * sy);
    }

    QRect atlasSubRect() const { return m_allocated_rect; }
    QRect atlasSubRectWithoutPadding() const { return m_allocated_rect.adjusted(1, 1, -1, -1); }

    bool isTexture() const { return true; }

    QOpenGLTexture::Filter filtering() const { return m_filtering; }
    QOpenGLTexture::MipMapGeneration mipmapFiltering() const { return m_mipmaps; }
    QOpenGLTexture *removedFromAtlas() const;

    void releaseImage() { m_image = QImage(); }
    const QImage &image() const { return m_image; }

    void bind();
    void release() { m_atlas->release(); }

private:
    QRect m_allocated_rect;
    QRectF m_texture_coords_rect;

    QImage m_image;
    QOpenGLTexture::Filter m_filtering;
    QOpenGLTexture::MipMapGeneration m_mipmaps : 1;

    QPointer<QTextureAtlas> m_atlas;

    mutable QOpenGLTexture *m_nonatlas_texture;

    uint m_owns_texture : 1;
    uint m_mipmaps_generated : 1;
    uint m_retain_image: 1;
};

#define QGEOM_DEF_TEXTURE_ATLAS_SIZE QSize(512,512)

class QAtlasManager : public QObject
{
    Q_OBJECT

public:
    QAtlasManager(QSize defaultAtlasSize = QGEOM_DEF_TEXTURE_ATLAS_SIZE);
    ~QAtlasManager();

    void ensureTextureAtlasSize(QSize reqAtlasSize);

    QTexture *create(const QImage &image);

    quint32 atlasTextureId() const;
    QSize atlasTextureSize() const;

    void invalidate(QSize reqAtlasSize = QSize());

private:
    QTextureAtlas *m_atlas;

    QSize m_atlas_size;
    int m_atlas_size_limit;
};

QT_END_NAMESPACE

#endif
