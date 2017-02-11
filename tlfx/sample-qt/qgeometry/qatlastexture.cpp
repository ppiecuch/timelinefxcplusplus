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

#include <QtCore/QVarLengthArray>
#include <QtCore/QElapsedTimer>
#include <QtCore/QtMath>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLTexture>
#include <QtGui/QPainter>
#include <QtGui/QWindow>
#include <QtGui/qpa/qplatformnativeinterface.h>

#include "qglnamespace.h"
#include "qatlastexture.h"

QT_BEGIN_NAMESPACE

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

static int qt_envInt(const char *name, int defaultValue)
{
    if (Q_LIKELY(!qEnvironmentVariableIsSet(name)))
        return defaultValue;
    bool ok = false;
    int value = qgetenv(name).toInt(&ok);
    return ok ? value : defaultValue;
}

static QElapsedTimer qsg_renderer_timer;

namespace QGL
{

enum SplitType
{
    VerticalSplit,
    HorizontalSplit
};

static const int maxMargin = 2;

struct QAreaAllocatorNode
{
    QAreaAllocatorNode(QAreaAllocatorNode *parent);
    ~QAreaAllocatorNode();
    inline bool isLeaf();

    QAreaAllocatorNode *parent;
    QAreaAllocatorNode *left;
    QAreaAllocatorNode *right;
    int split; // only valid for inner nodes.
    SplitType splitType;
    bool isOccupied; // only valid for leaf nodes.
};

QAreaAllocatorNode::QAreaAllocatorNode(QAreaAllocatorNode *parent)
    : parent(parent)
    , left(0)
    , right(0)
    , isOccupied(false)
{
}

QAreaAllocatorNode::~QAreaAllocatorNode()
{
    delete left;
    delete right;
}

bool QAreaAllocatorNode::isLeaf()
{
    Q_ASSERT((left != 0) == (right != 0));
    return !left;
}


QAreaAllocator::QAreaAllocator(const QSize &size, const QSize &padding) : m_size(size), m_padding(padding)
{
    m_root = new QAreaAllocatorNode(0);
}

QAreaAllocator::~QAreaAllocator()
{
    delete m_root;
}

QRect QAreaAllocator::allocate(const QSize &size)
{
    QPoint point;
    bool result = allocateInNode(size+m_padding, point, QRect(QPoint(0, 0), m_size), m_root);
    return result ? QRect(point, size+m_padding) : QRect();
}

bool QAreaAllocator::deallocate(const QRect &rect)
{
    return deallocateInNode(rect.topLeft(), m_root);
}

bool QAreaAllocator::allocateInNode(const QSize &size, QPoint &result, const QRect &currentRect, QAreaAllocatorNode *node)
{
    if (size.width() > currentRect.width() || size.height() > currentRect.height())
        return false;

    if (node->isLeaf()) {
        if (node->isOccupied)
            return false;
        if (size.width() + maxMargin >= currentRect.width() && size.height() + maxMargin >= currentRect.height()) {
            //Snug fit, occupy entire rectangle.
            node->isOccupied = true;
            result = currentRect.topLeft();
            return true;
        }
        // TODO: Reuse nodes.
        // Split node.
        node->left = new QAreaAllocatorNode(node);
        node->right = new QAreaAllocatorNode(node);
        QRect splitRect = currentRect;
        if ((currentRect.width() - size.width()) * currentRect.height() < (currentRect.height() - size.height()) * currentRect.width()) {
            node->splitType = HorizontalSplit;
            node->split = currentRect.top() + size.height();
            splitRect.setHeight(size.height());
        } else {
            node->splitType = VerticalSplit;
            node->split = currentRect.left() + size.width();
            splitRect.setWidth(size.width());
        }
        return allocateInNode(size, result, splitRect, node->left);
    } else {
        // TODO: avoid unnecessary recursion.
        //  has been split.
        QRect leftRect = currentRect;
        QRect rightRect = currentRect;
        if (node->splitType == HorizontalSplit) {
            leftRect.setHeight(node->split - leftRect.top());
            rightRect.setTop(node->split);
        } else {
            leftRect.setWidth(node->split - leftRect.left());
            rightRect.setLeft(node->split);
        }
        if (allocateInNode(size, result, leftRect, node->left))
            return true;
        if (allocateInNode(size, result, rightRect, node->right))
            return true;
        return false;
    }
}

bool QAreaAllocator::deallocateInNode(const QPoint &pos, QAreaAllocatorNode *node)
{
    while (!node->isLeaf()) {
        //  has been split.
        int cmp = node->splitType == HorizontalSplit ? pos.y() : pos.x();
        node = cmp < node->split ? node->left : node->right;
    }
    if (!node->isOccupied)
        return false;
    node->isOccupied = false;
    mergeNodeWithNeighbors(node);
    return true;
}

void QAreaAllocator::mergeNodeWithNeighbors(QAreaAllocatorNode *node)
{
    bool done = false;
    QAreaAllocatorNode *parent = 0;
    QAreaAllocatorNode *current = 0;
    QAreaAllocatorNode *sibling;
    while (!done) {
        Q_ASSERT(node->isLeaf());
        Q_ASSERT(!node->isOccupied);
        if (node->parent == 0)
            return; // No neighbours.

        SplitType splitType = SplitType(node->parent->splitType);
        done = true;

        /* Special case. Might be faster than going through the general code path.
        // Merge with sibling.
        parent = node->parent;
        sibling = (node == parent->left ? parent->right : parent->left);
        if (sibling->isLeaf() && !sibling->isOccupied) {
            Q_ASSERT(!sibling->right);
            node = parent;
            parent->isOccupied = false;
            delete parent->left;
            delete parent->right;
            parent->left = parent->right = 0;
            done = false;
            continue;
        }
        */

        // Merge with left neighbour.
        current = node;
        parent = current->parent;
        while (parent && current == parent->left && parent->splitType == splitType) {
            current = parent;
            parent = parent->parent;
        }

        if (parent && parent->splitType == splitType) {
            Q_ASSERT(current == parent->right);
            Q_ASSERT(parent->left);

            QAreaAllocatorNode *neighbor = parent->left;
            while (neighbor->right && neighbor->splitType == splitType)
                neighbor = neighbor->right;

            if (neighbor->isLeaf() && neighbor->parent->splitType == splitType && !neighbor->isOccupied) {
                // Left neighbour can be merged.
                parent->split = neighbor->parent->split;

                parent = neighbor->parent;
                sibling = neighbor == parent->left ? parent->right : parent->left;
                QAreaAllocatorNode **nodeRef = &m_root;
                if (parent->parent) {
                    if (parent == parent->parent->left)
                        nodeRef = &parent->parent->left;
                    else
                        nodeRef = &parent->parent->right;
                }
                sibling->parent = parent->parent;
                *nodeRef = sibling;
                parent->left = parent->right = 0;
                delete parent;
                delete neighbor;
                done = false;
            }
        }

        // Merge with right neighbour.
        current = node;
        parent = current->parent;
        while (parent && current == parent->right && parent->splitType == splitType) {
            current = parent;
            parent = parent->parent;
        }

        if (parent && parent->splitType == splitType) {
            Q_ASSERT(current == parent->left);
            Q_ASSERT(parent->right);

            QAreaAllocatorNode *neighbor = parent->right;
            while (neighbor->left && neighbor->splitType == splitType)
                neighbor = neighbor->left;

            if (neighbor->isLeaf() && neighbor->parent->splitType == splitType && !neighbor->isOccupied) {
                // Right neighbour can be merged.
                parent->split = neighbor->parent->split;

                parent = neighbor->parent;
                sibling = neighbor == parent->left ? parent->right : parent->left;
                QAreaAllocatorNode **nodeRef = &m_root;
                if (parent->parent) {
                    if (parent == parent->parent->left)
                        nodeRef = &parent->parent->left;
                    else
                        nodeRef = &parent->parent->right;
                }
                sibling->parent = parent->parent;
                *nodeRef = sibling;
                parent->left = parent->right = 0;
                delete parent;
                delete neighbor;
                done = false;
            }
        }
    } // end while(!done)
}

} // namespace QGL


constexpr QSize QAtlasManager::padding;

QAtlasManager::QAtlasManager(QSize defAtlasSize)
    : m_atlas(0)
{
    ensureTextureAtlasSize(defAtlasSize);
}


QAtlasManager::~QAtlasManager()
{
    Q_ASSERT(m_atlas == 0);
}

void QAtlasManager::ensureTextureAtlasSize(QSize reqAtlasSize)
{
    QOpenGLContext *gl = QOpenGLContext::currentContext();
    Q_ASSERT(gl);
    int max;
    gl->functions()->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);

    int w = qMin(max, qt_envInt("QGEOM_ATLAS_WIDTH", quint32(reqAtlasSize.width())));
    int h = qMin(max, qt_envInt("QGEOM_ATLAS_HEIGHT", quint32(reqAtlasSize.height())));

    m_atlas_size_limit = qt_envInt("QGEOM_ATLAS_SIZE_LIMIT", qMax(w, h) / 2);
    m_atlas_size = QSize(w, h);

    qCDebug(QGEOM_LOG_INFO, "texture atlas dimensions: %dx%d", w, h);
}

void QAtlasManager::invalidate(QSize reqAtlasSize)
{
    if (m_atlas) {
        m_atlas->invalidate();
        m_atlas->deleteLater();
        m_atlas = 0;
    }
    if (reqAtlasSize.isValid() && reqAtlasSize != m_atlas_size)
        ensureTextureAtlasSize(reqAtlasSize);
}

QTexture *QAtlasManager::create(const QImage &image)
{
    if (image.width() > m_atlas_size_limit || image.height() > m_atlas_size_limit)
        return 0;
    if (!m_atlas)
        m_atlas = new QTextureAtlas(m_atlas_size);
    // texture may be null for atlas allocation failure
    return m_atlas->create(image);
}

GLuint QAtlasManager::atlasTextureId() const { return m_atlas->textureId(); }

QSize QAtlasManager::atlasTextureSize() const { return m_atlas_size; }

QTextureAtlas::QTextureAtlas(const QSize &size)
    : m_allocator(size, QAtlasManager::padding)
    , m_texture_id(0)
    , m_size(size)
    , m_atlas_transient_image_threshold(0)
    , m_allocated(false)
{

    m_internalFormat = GL_RGBA;
    m_externalFormat = GL_BGRA;

#ifndef QT_OPENGL_ES
    if (QOpenGLContext::currentContext()->isOpenGLES()) {
#endif

#if defined(Q_OS_ANDROID)
    QString *deviceName =
            static_cast<QString *>(QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("AndroidDeviceName"));
    static bool wrongfullyReportsBgra8888Support = deviceName != 0
                                                    && (deviceName->compare(QLatin1String("samsung SM-T211"), Qt::CaseInsensitive) == 0
                                                        || deviceName->compare(QLatin1String("samsung SM-T210"), Qt::CaseInsensitive) == 0
                                                        || deviceName->compare(QLatin1String("samsung SM-T215"), Qt::CaseInsensitive) == 0);
#else
    static bool wrongfullyReportsBgra8888Support = false;
    // The Raspberry Pi (both 1 and 2) GPU refuses framebuffers with BGRA color attachments.
    const GLubyte *renderer = QOpenGLContext::currentContext()->functions()->glGetString(GL_RENDERER);
    if (renderer && strstr((const char *) renderer, "VideoCore IV"))
        wrongfullyReportsBgra8888Support = true;
#endif // ANDROID

    if (qEnvironmentVariableIsSet("QGEOM_ATLAS_NO_BGRA_WORKAROUNDS"))
        wrongfullyReportsBgra8888Support = false;

    const char *ext = (const char *) QOpenGLContext::currentContext()->functions()->glGetString(GL_EXTENSIONS);
    if (ext && !wrongfullyReportsBgra8888Support
            && (strstr(ext, "GL_EXT_bgra")
                || strstr(ext, "GL_EXT_texture_format_BGRA8888")
                || strstr(ext, "GL_IMG_texture_format_BGRA8888"))) {
        m_internalFormat = m_externalFormat = GL_BGRA;
#if defined(Q_OS_DARWIN) && !defined(Q_OS_OSX)
    } else if (ext && strstr(ext, "GL_APPLE_texture_format_BGRA8888")) {
        m_internalFormat = GL_RGBA;
        m_externalFormat = GL_BGRA;
#endif // IOS || TVOS
    } else {
        m_internalFormat = m_externalFormat = GL_RGBA;
    }

#ifndef QT_OPENGL_ES
    }
#endif

    m_use_bgra_fallback = qEnvironmentVariableIsSet("QGEOM_ATLAS_USE_BGRA_FALLBACK");
    m_debug_overlay = qEnvironmentVariableIsSet("QGEOM_ATLAS_OVERLAY");

    // images smaller than this will retain their QImage.
    // by default no images are retained (favoring memory)
    // set to a very large value to retain all images (allowing quick removal from the atlas)
    m_atlas_transient_image_threshold = qt_envInt("QGEOM_ATLAS_TRANSIENT_IMAGE_THRESHOLD", 0);
}

QTextureAtlas::~QTextureAtlas()
{
    Q_ASSERT(!m_texture_id);
}

void QTextureAtlas::invalidate()
{
    if (m_texture_id && QOpenGLContext::currentContext())
        QOpenGLContext::currentContext()->functions()->glDeleteTextures(1, &m_texture_id);
    m_texture_id = 0;
}

QTexture *QTextureAtlas::create(const QImage &image)
{
    // No need to lock, as manager already locked it.
    QRect rect = m_allocator.allocate(QSize(image.width(), image.height()));
    if (rect.width() > 0 && rect.height() > 0) {
        QTexture *t = new QTexture(this, rect, image);
        m_pending_uploads << t;
        return t;
    }
    return 0;
}


int QTextureAtlas::textureId() const
{
    if (!m_texture_id) {
        Q_ASSERT(QOpenGLContext::currentContext());
        QOpenGLContext::currentContext()->functions()->glGenTextures(1, &const_cast<QTextureAtlas *>(this)->m_texture_id);
    }

    return m_texture_id;
}

static void swizzleBGRAToRGBA(QImage *image)
{
    const int width = image->width();
    const int height = image->height();
    uint *p = (uint *) image->bits();
    int stride = image->bytesPerLine() / 4;
    for (int i = 0; i < height; ++i) {
        for (int x = 0; x < width; ++x)
            p[x] = ((p[x] << 16) & 0xff0000) | ((p[x] >> 16) & 0xff) | (p[x] & 0xff00ff00);
        p += stride;
    }
}

void QTextureAtlas::upload(QTexture *texture)
{
    const QImage &image = texture->image();
    const QRect &r = texture->atlasSubRect();

    QImage tmp(r.width(), r.height(), QImage::Format_ARGB32_Premultiplied);
    {
        QPainter p(&tmp);
        p.setCompositionMode(QPainter::CompositionMode_Source);

        int w = r.width();
        int h = r.height();
        int iw = image.width();
        int ih = image.height();

        p.drawImage(1, 1, image);
        p.drawImage(1, 0, image, 0, 0, iw, 1);
        p.drawImage(1, h - 1, image, 0, ih - 1, iw, 1);
        p.drawImage(0, 1, image, 0, 0, 1, ih);
        p.drawImage(w - 1, 1, image, iw - 1, 0, 1, ih);
        p.drawImage(0, 0, image, 0, 0, 1, 1);
        p.drawImage(0, h - 1, image, 0, ih - 1, 1, 1);
        p.drawImage(w - 1, 0, image, iw - 1, 0, 1, 1);
        p.drawImage(w - 1, h - 1, image, iw - 1, ih - 1, 1, 1);
        if (m_debug_overlay) {
            p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
            p.fillRect(0, 0, iw, ih, QBrush(QColor::fromRgbF(1, 0, 1, 0.5)));
        }
    }

    if (m_externalFormat == GL_RGBA)
        swizzleBGRAToRGBA(&tmp);
    QOpenGLContext::currentContext()->functions()->glTexSubImage2D(GL_TEXTURE_2D, 0,
                                                                   r.x(), r.y(), r.width(), r.height(),
                                                                   m_externalFormat, GL_UNSIGNED_BYTE, tmp.constBits());
}

void QTextureAtlas::uploadBgra(QTexture *texture)
{
    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    const QRect &r = texture->atlasSubRect();
    QImage image = texture->image();

    if (image.isNull())
        return;

    if (image.format() != QImage::Format_ARGB32_Premultiplied
            && image.format() != QImage::Format_RGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    if (m_debug_overlay) {
        QPainter p(&image);
        p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        p.fillRect(0, 0, image.width(), image.height(), QBrush(QColor::fromRgbF(0, 1, 1, 0.5)));
    }

    QVarLengthArray<quint32, 512> tmpBits(qMax(image.width() + 2, image.height() + 2));
    int iw = image.width();
    int ih = image.height();
    int bpl = image.bytesPerLine() / 4;
    const quint32 *src = (const quint32 *) image.constBits();
    quint32 *dst = tmpBits.data();

    // top row, padding corners
    dst[0] = src[0];
    memcpy(dst + 1, src, iw * sizeof(quint32));
    dst[1 + iw] = src[iw-1];
    funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, r.x(), r.y(), iw + 2, 1, m_externalFormat, GL_UNSIGNED_BYTE, dst);

    // bottom row, padded corners
    const quint32 *lastRow = src + bpl * (ih - 1);
    dst[0] = lastRow[0];
    memcpy(dst + 1, lastRow, iw * sizeof(quint32));
    dst[1 + iw] = lastRow[iw-1];
    funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, r.x(), r.y() + ih + 1, iw + 2, 1, m_externalFormat, GL_UNSIGNED_BYTE, dst);

    // left column
    for (int i=0; i<ih; ++i)
        dst[i] = src[i * bpl];
    funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, r.x(), r.y() + 1, 1, ih, m_externalFormat, GL_UNSIGNED_BYTE, dst);

    // right column
    for (int i=0; i<ih; ++i)
        dst[i] = src[i * bpl + iw - 1];
    funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, r.x() + iw + 1, r.y() + 1, 1, ih, m_externalFormat, GL_UNSIGNED_BYTE, dst);

    // Inner part of the image....
    if (bpl != iw) {
        int sy = r.y() + 1;
        int ey = sy + r.height() - 2;
        for (int y = sy; y < ey; ++y) {
            funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, r.x() + 1, y, r.width() - 2, 1, m_externalFormat, GL_UNSIGNED_BYTE, src);
            src += bpl;
        }
    } else {
        funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, r.x() + 1, r.y() + 1, r.width() - 2, r.height() - 2, m_externalFormat, GL_UNSIGNED_BYTE, src);
    }
}

void QTextureAtlas::bind(QOpenGLTexture::Filter filtering)
{
    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    if (!m_allocated) {
        m_allocated = true;

        while (funcs->glGetError() != GL_NO_ERROR) ;

        funcs->glGenTextures(1, &m_texture_id);
        funcs->glBindTexture(GL_TEXTURE_2D, m_texture_id);
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if !defined(QT_OPENGL_ES_2)
        if (!QOpenGLContext::currentContext()->isOpenGLES())
            funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
#endif
        funcs->glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_size.width(), m_size.height(), 0, m_externalFormat, GL_UNSIGNED_BYTE, 0);

#if 0
        QImage pink(m_size.width(), m_size.height(), QImage::Format_ARGB32_Premultiplied);
        pink.fill(0);
        QPainter p(&pink);
        QLinearGradient redGrad(0, 0, m_size.width(), 0);
        redGrad.setColorAt(0, Qt::black);
        redGrad.setColorAt(1, Qt::red);
        p.fillRect(0, 0, m_size.width(), m_size.height(), redGrad);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        QLinearGradient blueGrad(0, 0, 0, m_size.height());
        blueGrad.setColorAt(0, Qt::black);
        blueGrad.setColorAt(1, Qt::blue);
        p.fillRect(0, 0, m_size.width(), m_size.height(), blueGrad);
        p.end();

        funcs->glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_size.width(), m_size.height(), 0, m_externalFormat, GL_UNSIGNED_BYTE, pink.constBits());
#endif

        GLenum errorCode = funcs->glGetError();
        if (errorCode == GL_OUT_OF_MEMORY) {
            qDebug("QTextureAtlas: texture atlas allocation failed, out of memory");
            funcs->glDeleteTextures(1, &m_texture_id);
            m_texture_id = 0;
        } else if (errorCode != GL_NO_ERROR) {
            qDebug("QTextureQAtlas: texture atlas allocation failed, code=%x", errorCode);
            funcs->glDeleteTextures(1, &m_texture_id);
            m_texture_id = 0;
        }
    } else {
        funcs->glBindTexture(GL_TEXTURE_2D, m_texture_id);
    }

    if (m_texture_id == 0)
        return;

    // Upload all pending images..
    for (int i=0; i<m_pending_uploads.size(); ++i) {
        QTexture *t = m_pending_uploads.at(i);
        if (m_externalFormat == GL_BGRA &&
                !m_use_bgra_fallback) {
            uploadBgra(t);
        } else {
            upload(t);
        }
        const QSize textureSize = t->textureSize();
        if (textureSize.width() > m_atlas_transient_image_threshold ||
                textureSize.height() > m_atlas_transient_image_threshold)
            t->releaseImage();
    }

    GLenum f = filtering == QOpenGLTexture::Nearest ? GL_NEAREST : GL_LINEAR;
    funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);

    m_pending_uploads.clear();
}

void QTextureAtlas::release()
{
    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    funcs->glBindTexture(GL_TEXTURE_2D, 0);
}

void QTextureAtlas::remove(QTexture *t)
{
    QRect atlasRect = t->atlasSubRect();
    m_allocator.deallocate(atlasRect);
    m_pending_uploads.removeOne(t);
}



QTexture::QTexture(QTextureAtlas *atlas, const QRect &textureRect, const QImage &image)
    : m_allocated_rect(textureRect)
    , m_image(image)
    , m_filtering(QOpenGLTexture::Linear)
    , m_mipmaps(QOpenGLTexture::GenerateMipMaps)
    , m_atlas(atlas)
    , m_nonatlas_texture(0)
{
    float w = atlas->size().width();
    float h = atlas->size().height();
    QRect nopad = atlasSubRectWithoutPadding();
    m_texture_coords_rect = QRectF(nopad.x() / w,
                                   nopad.y() / h,
                                   nopad.width() / w,
                                   nopad.height() / h);
}

QTexture::~QTexture()
{
    m_atlas->remove(this);
    if (m_nonatlas_texture)
        delete m_nonatlas_texture;
}

void QTexture::bind()
{
    m_atlas->bind(filtering());
}

QOpenGLTexture *QTexture::removedFromAtlas() const
{
    if (m_nonatlas_texture) {
        m_nonatlas_texture->setAutoMipMapGenerationEnabled(mipmapFiltering());
        m_nonatlas_texture->setMinMagFilters(filtering(), filtering());
        return m_nonatlas_texture;
    }

    if (!m_image.isNull()) {
        m_nonatlas_texture = new QOpenGLTexture(m_image);
        m_nonatlas_texture->setAutoMipMapGenerationEnabled(mipmapFiltering());
        m_nonatlas_texture->setMinMagFilters(filtering(), filtering());
    } else {
        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
        // bind the atlas texture as an fbo and extract the texture..

        // First extract the currently bound fbo so we can restore it later.
        GLint currentFbo;
        f->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFbo);

        // Create an FBO and bind the atlas texture into it.
        GLuint fbo;
        f->glGenFramebuffers(1, &fbo);
        f->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_atlas->textureId(), 0);

        // Create the target texture, QTexture below will deal with the texparams, so we don't
        // need to worry about those here.
        m_nonatlas_texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
        m_nonatlas_texture->bind();
        QRect r = atlasSubRectWithoutPadding();
        // and copy atlas into our texture.
        while (f->glGetError() != GL_NO_ERROR) ;
        f->glCopyTexImage2D(GL_TEXTURE_2D, 0, m_atlas->internalFormat(), r.x(), r.y(), r.width(), r.height(), 0);
        // BGRA may have been rejected by some GLES implementations
        if (f->glGetError() != GL_NO_ERROR)
            f->glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r.x(), r.y(), r.width(), r.height(), 0);

        // cleanup: unbind our atlas from the fbo, rebind the old default and delete the fbo.
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        f->glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) currentFbo);
        f->glDeleteFramebuffers(1, &fbo);
    }

    m_nonatlas_texture->setAutoMipMapGenerationEnabled(mipmapFiltering());
    m_nonatlas_texture->setMinMagFilters(filtering(), filtering());
    return m_nonatlas_texture;
}

QT_END_NAMESPACE
