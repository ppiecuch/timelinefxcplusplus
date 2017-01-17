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

#include "qgltexture2d.h"
#include "qgltexture2d_p.h"
#include "qglpainter_p.h"
#include "qglext_p.h"
#include "qdownloadmanager.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>

QT_BEGIN_NAMESPACE

/*!
    \class QGLTexture2D
    \brief The QGLTexture2D class represents a 2D texture object for GL painting operations.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::textures

    QGLTexture2D contains a QImage and settings for texture filters,
    wrap modes, and mipmap generation.  When bind() is called, this
    information is uploaded to the GL server if it has changed since
    the last time bind() was called.

    To enable a texture so that it will be used in the next draw call,
    use bind().  To disable the texture so that it will no longer be
    used in drawing call release().  To remove the texture from the
    GL server altogether call cleanupResources().

    Once a QGLTexture2D object is created, it can be bound to multiple
    GL contexts.  Internally, a separate texture identifier is created
    for each context.  This makes QGLTexture2D easier to use than
    raw GL texture identifiers because the application does not need
    to be as concerned with whether the texture identifier is valid
    in the current context.  The application merely calls bind() and
    QGLTexture2D will create a new texture identifier for the context
    if necessary.  When cleanupResources() is called the texture will
    be removed from the GL server, and will no longer be available to
    any of the multiple contexts.

    QGLTexture2D internally points to a reference-counted object that
    represents the current texture state.  If the QGLTexture2D is copied,
    the internal pointer is the same.  Modifications to one QGLTexture2D
    copy will affect all of the other copies in the system.

    When the QGLTexture2D object is deleted, no attempt is made to remove
    the texture resources from the GL server.  The application programmer
    must call cleanupResources().  The reason is that the relevant GL
    context must be current (or be made current) for GL resources to be
    removed, but the context is likely to be in a different thread.

    The application programmer thus has control over the life-cycle of the
    GL resources, and has the option to keep the QGLTexture2D object around
    with its attached image in heap memory; whilst not consuming resources
    on the GPU.  This is useful in larger scenes, or when used with LOD
    and higher-level culling.

    QGLTexture2D can also be used for uploading 1D textures into the
    GL server by specifying an image() with a height of 1.

    \sa QGLTextureCube
*/

QGLTexture2DPrivate::QGLTexture2DPrivate()
{
    horizontalWrap = QGL::Repeat;
    verticalWrap = QGL::Repeat;
    bindOptions = QGLTexture2D::DefaultBindOption;
#if !defined(QT_OPENGL_ES)
    mipmapSupported = false;
    mipmapSupportedKnown = false;
#endif
    imageGeneration = 0;
    parameterGeneration = 0;
    sizeAdjusted = false;
    downloadManager = 0;
}

QGLTexture2DPrivate::~QGLTexture2DPrivate()
{
    if (!textureInfo.empty()) {
        for (QList<QGLTexture2DTextureInfo*>::iterator It=textureInfo.begin(); It!=textureInfo.end(); ++It) {
            if ((*It)->isLiteral==false && (*It)->tex.textureId()) {
                QGLTexture2D::toBeDeletedLater((*It)->tex.context(), (*It)->tex.textureId());
            }
        }
    }
    qDeleteAll(textureInfo);
}

/*!
    Constructs a null texture object and attaches it to \a parent.

    \sa isNull()
*/
QGLTexture2D::QGLTexture2D(QObject *parent)
    : QObject(parent), d_ptr(new QGLTexture2DPrivate())
{
}

/*!
    Destroys this texture object.  Call cleanupResources() before destroying the object
    in order to remove GL resources from the server - this is not done automatically,
    as destruction is not guaranteed to occur in the same thread as the context.
*/
QGLTexture2D::~QGLTexture2D()
{
}

/*!
    Returns true if this texture object is null; that is, image()
    is null and textureId() is zero.
*/
bool QGLTexture2D::isNull() const
{
    Q_D(const QGLTexture2D);
    return d->image.isNull() && !d->textureInfo.isEmpty();
}

/*!
    Returns true if this texture has an alpha channel; false if the
    texture is fully opaque.
*/
bool QGLTexture2D::hasAlphaChannel() const
{
    Q_D(const QGLTexture2D);
    if (!d->image.isNull())
        return d->image.hasAlphaChannel();

    if (!d->textureInfo.isEmpty())
    {
        QGLTexture2DTextureInfo *info = d->textureInfo.first();
        return info->tex.hasAlpha();
    }
    return false;
}

/*!
    Returns the size of this texture.  If the underlying OpenGL
    implementation requires texture sizes to be a power of two,
    then this function may return the next power of two equal
    to or greater than requestedSize()

    The adjustment to the next power of two will only occur when
    an OpenGL context is available, so if the actual texture size
    is needed call this function when a context is available.

    \sa setSize(), requestedSize()
*/
QSize QGLTexture2D::size() const
{
    Q_D(const QGLTexture2D);
    if (!d->sizeAdjusted)
    {
        QGLTexture2DPrivate *that = const_cast<QGLTexture2DPrivate*>(d);
        that->adjustForNPOTTextureSize();
    }
    return d->size;
}

static inline bool isFloatChar(char c)
{
    return c == '.' || (c >= '0' && c <= '9');
}

void QGLTexture2DPrivate::adjustForNPOTTextureSize()
{
    if (QOpenGLContext::currentContext() && !sizeAdjusted)
    {
        bool ok = false;
        QByteArray verString(reinterpret_cast<const char *>(glGetString(GL_VERSION)));
        QByteArray verStringCleaned;

        // the strings look like "2.1 some random vendor chars" - and toFloat does not deal with junk so clean it first
        for (int c = 0; c < verString.size() && isFloatChar(verString.at(c)); ++c)
            verStringCleaned.append(verString.at(c));
        float verNum = verStringCleaned.toFloat(&ok);

        // With OpenGL 2.0 support for NPOT textures is mandatory - before that it was only by extension.
        if (!ok || verNum < 2.0)
        {
            QGLTextureExtensions *te = QGLTextureExtensions::extensions();
            if (!te) {
                size = QGL::nextPowerOfTwo(size);
            } else
            if (!te->npotTextures)
            {
                if (!ok)
                    qWarning() << "Could not read GL_VERSION - string was:" << verString << "- assuming no NPOT support";
                size = QGL::nextPowerOfTwo(size);
            }
        }
        sizeAdjusted = true;
    }
}

/*!
    Sets the size of this texture to \a value.  Also sets the
    requestedSize to \a value.

    Note that the underlying OpenGL implementation may require texture sizes
    to be a power of two.  If that is the case, then \b{when the texture is bound}
    this will be detected, and while requestedSize() will remain at \a value,
    the size() will be set to the next power of two equal to or greater than
    \a value.

    For this reason to get a definitive value of the actual size of the underlying
    texture, query the size after bind() has been done.

    \sa size(), requestedSize()
*/
void QGLTexture2D::setSize(const QSize& value)
{
    Q_D(QGLTexture2D);
    if (d->requestedSize == value)
        return;
    d->size = value;
    d->sizeAdjusted = false;
    d->adjustForNPOTTextureSize();
    d->requestedSize = value;
    ++(d->imageGeneration);
}

/*!
    Returns the size that was previously set with setSize() before
    it was rounded to a power of two.

    \sa size(), setSize()
*/
QSize QGLTexture2D::requestedSize() const
{
    Q_D(const QGLTexture2D);
    return d->requestedSize;
}

/*!
    Returns the image that is currently associated with this texture.
    The image may not have been uploaded into the GL server yet.
    Uploads occur upon the next call to bind().

    \sa setImage()
*/
QImage QGLTexture2D::image() const
{
    Q_D(const QGLTexture2D);
    return d->image;
}

/*!
    Sets the \a image that is associated with this texture.  The image
    will be uploaded into the GL server the next time bind() is called.

    If setSize() or setImage() has been called previously, then \a image
    will be scaled to size() when it is uploaded.

    If \a image is null, then this function is equivalent to clearImage().

    \sa image(), setSize(), setPixmap()
*/
void QGLTexture2D::setImage(const QImage& image)
{
    Q_D(QGLTexture2D);
    d->compressedData = QByteArray(); // Clear compressed file data.
    if (image.isNull()) {
        // Don't change the imageGeneration, because we aren't actually
        // changing the image in the GL server, only the client copy.
        d->image = image;
    } else {
        if (!d->size.isValid())
            setSize(image.size());
        d->image = image;
        ++(d->imageGeneration);
    }
}

/*!
    Sets the image that is associated with this texture to \a pixmap.

    This is a convenience that calls setImage() after converting
    \a pixmap into a QImage.  It may be more efficient on some
    platforms than the application calling QPixmap::toImage().

    \sa setImage()
*/
void QGLTexture2D::setPixmap(const QPixmap& pixmap)
{
    QImage image = pixmap.toImage();
    if (pixmap.depth() == 16 && !image.hasAlphaChannel()) {
        // If the system depth is 16 and the pixmap doesn't have an alpha channel
        // then we convert it to RGB16 in the hope that it gets uploaded as a 16
        // bit texture which is much faster to access than a 32-bit one.
        image = image.convertToFormat(QImage::Format_RGB16);
    }
    setImage(image);
}

/*!
    Clears the image() that is associated with this texture, but the
    GL texture will retain its current value.  This can be used to
    release client-side memory that is no longer required once the
    image has been uploaded into the GL server.

    The following code will queue \c image to be uploaded, immediately
    force it to be uploaded into the current GL context, and then
    clear the client copy:

    \code
    texture.setImage(image);
    texture.bind();
    texture.clearImage()
    \endcode

    \sa image(), setImage()
*/
void QGLTexture2D::clearImage()
{
    Q_D(QGLTexture2D);
    d->image = QImage();
}

#ifndef GL_GENERATE_MIPMAP_SGIS
#define GL_GENERATE_MIPMAP_SGIS       0x8191
#define GL_GENERATE_MIPMAP_HINT_SGIS  0x8192
#endif

/*!
    Sets this texture to the contents of a compressed image file
    at \a path.  Returns true if the file exists and has a supported
    compressed format; false otherwise.

    The DDS, ETC1, PVRTC2, and PVRTC4 compression formats are
    supported, assuming that the GL implementation has the
    appropriate extension.

    \sa setImage(), setSize()
*/
bool QGLTexture2D::setCompressedFile(const QString &path)
{
    Q_D(QGLTexture2D);
    d->image = QImage();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        qWarning("QGLTexture2D::setCompressedFile(%s): File could not be read",
                 qPrintable(path));
        return false;
    }
    QByteArray data = f.readAll();
    f.close();

    bool hasAlpha, isFlipped;
    if (!QGLBoundTexture::canBindCompressedTexture
            (data.constData(), data.size(), 0, &hasAlpha, &isFlipped)) {
        qWarning("QGLTexture2D::setCompressedFile(%s): Format is not supported",
                 path.toLocal8Bit().constData());
        return false;
    }

    QFileInfo fi(path);
    d->url = QUrl::fromLocalFile(fi.absoluteFilePath());

    // The 3DS loader expects the flip state to be set before bind().
    if (isFlipped)
        d->bindOptions &= ~QGLTexture2D::InvertedYBindOption;
    else
        d->bindOptions |= QGLTexture2D::InvertedYBindOption;

    d->compressedData = data;
    ++(d->imageGeneration);
    return true;
}

/*!
    Returns the url that was last set with setUrl.
*/
QUrl QGLTexture2D::url() const
{
    Q_D(const QGLTexture2D);
    return d->url;
}

/*!
    Sets this texture to have the contents of the image stored at \a url.
*/
void QGLTexture2D::setUrl(const QUrl &url)
{
    Q_D(QGLTexture2D);
    if (d->url == url)
        return;
    d->url = url;

    if (url.isEmpty())
    {
        d->image = QImage();
    }
    else
    {
        if (url.scheme() == QLatin1String("file") || url.scheme().toLower() == QLatin1String("qrc"))
        {
            QString fileName = url.toLocalFile();

            // slight hack since there doesn't appear to be a QUrl::toResourcePath() function
            // to convert qrc:///foo into :/foo
            if (url.scheme().toLower() == QLatin1String("qrc")) {
                // strips off any qrc: prefix and any excess slashes and replaces it with :/
                QUrl tempUrl(url);
                tempUrl.setScheme(QString());
                fileName = QLatin1Char(':')+tempUrl.toString();
            }

            if (fileName.endsWith(QLatin1String(".dds"), Qt::CaseInsensitive))
            {
                setCompressedFile(fileName);
            }
            else
            {
                QImage im(fileName);
                if (im.isNull())
                    qWarning("Could not load texture: %s", qPrintable(fileName));
                setImage(im);
            }
        }
        else
        {
            if (!d->downloadManager) {
                d->downloadManager = new QDownloadManager(this);
                connect (d->downloadManager,SIGNAL(downloadComplete(QByteArray*)),this, SLOT(textureRequestFinished(QByteArray*)));
            }

            //Create a temporary image that will be used until the Url is loaded.
            static QImage tempImg(128,128, QImage::Format_RGB32);
            QColor fillcolor(Qt::gray);
            tempImg.fill(fillcolor.rgba());
            setImage(tempImg);

            //Issue download request.
            if (!d->downloadManager->downloadAsset(url)) {
                qWarning("Unable to issue texture download request.");
            }
        }
    }
}

/*!
    Returns the options to use when binding the image() to an OpenGL
    context for the first time.  The default options are
    QGLTexture2D::LinearFilteringBindOption |
    QGLTexture2D::InvertedYBindOption | QGLTexture2D::MipmapBindOption.

    \sa setBindOptions()
*/
QGLTexture2D::BindOptions QGLTexture2D::bindOptions() const
{
    Q_D(const QGLTexture2D);
    return d->bindOptions;
}

/*!
    Sets the \a options to use when binding the image() to an
    OpenGL context.  If the image() has already been bound,
    then changing the options will cause it to be recreated
    from image() the next time bind() is called, unless the
    only change was to the LinearFilteringBindOption, which
    can be applied without re-creation.

    \sa bindOptions(), bind()
*/
void QGLTexture2D::setBindOptions(QGLTexture2D::BindOptions options)
{
    Q_D(QGLTexture2D);
    uint optionDelta = d->bindOptions ^ options;
    if (optionDelta) {
        // special case: only LinearFiltering option changed
        // --> treat this just as a parameter change
        if (optionDelta == LinearFilteringBindOption) {
            ++(d->parameterGeneration);
        } else {
            // all other options trigger re-upload
            ++(d->imageGeneration);
        }
        d->bindOptions = options;
    }
}

/*!
    Returns the wrapping mode for horizontal texture co-ordinates.
    The default value is QGL::Repeat.

    \sa setHorizontalWrap(), verticalWrap()
*/
QGL::TextureWrap QGLTexture2D::horizontalWrap() const
{
    Q_D(const QGLTexture2D);
    return d->horizontalWrap;
}

/*!
    Sets the wrapping mode for horizontal texture co-ordinates to \a value.

    The \a value will not be applied to the texture in the GL
    server until the next call to bind().

    \sa horizontalWrap(), setVerticalWrap()
*/
void QGLTexture2D::setHorizontalWrap(QGL::TextureWrap value)
{
    Q_D(QGLTexture2D);
    if (d->horizontalWrap != value) {
        d->horizontalWrap = value;
        ++(d->parameterGeneration);
    }
}

/*!
    Returns the wrapping mode for vertical texture co-ordinates.
    The default value is QGL::Repeat.

    \sa setVerticalWrap(), horizontalWrap()
*/
QGL::TextureWrap QGLTexture2D::verticalWrap() const
{
    Q_D(const QGLTexture2D);
    return d->verticalWrap;
}

/*!
    Sets the wrapping mode for vertical texture co-ordinates to \a value.

    If \a value is not supported by the OpenGL implementation, it will be
    replaced with a value that is supported.  If the application desires a
    very specific \a value, it can call verticalWrap() to check that
    the specific value was actually set.

    The \a value will not be applied to the texture in the GL
    server until the next call to bind().

    \sa verticalWrap(), setHorizontalWrap()
*/
void QGLTexture2D::setVerticalWrap(QGL::TextureWrap value)
{
    Q_D(QGLTexture2D);
    if (d->verticalWrap != value) {
        d->verticalWrap = value;
        ++(d->parameterGeneration);
    }
}

/*!
    Binds this texture to the 2D texture target.

    If this texture object is not associated with an identifier in
    the current context, then a new identifier will be created,
    and image() uploaded into the GL server.

    To remove the resources from the GL server when this texture is
    no longer required, call cleanupResources().

    If setImage(), setSize() or setBindOptions() were called since
    the last upload, then image() will be re-uploaded to the GL
    server, except for the special case where setBindOptions()
    changed only the LinearFilteringBindOption.

    Returns false if the texture could not be bound for some reason.

    \sa release(), textureId(), setImage()
*/
bool QGLTexture2D::bind() const
{
    Q_D(const QGLTexture2D);
    return const_cast<QGLTexture2DPrivate *>(d)->bind(GL_TEXTURE_2D);
}

bool QGLTexture2DPrivate::bind(GLenum target)
{
    // Get the current context.  If we don't have one, then we
    // cannot bind the texture.
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx)
        return false;

    if (ctx)
    {
        QOpenGLFunctions functions(ctx);
        if (!functions.hasOpenGLFeature(QOpenGLFunctions::NPOTTextures))
        {
            QSize oldSize = size;
            size = QGL::nextPowerOfTwo(size);
            if (size != oldSize)
                ++imageGeneration;
        }
    }

    if ((bindOptions & QGLTexture2D::MipmapBindOption) ||
            horizontalWrap != QGL::ClampToEdge ||
            verticalWrap != QGL::ClampToEdge) {
        // This accounts for the broken Intel HD 3000 graphics support at least
        // under OSX which claims to support POTT, but actually doesn't.
        QSize oldSize = size;
        size = QGL::nextPowerOfTwo(size);
        if (size != oldSize)
            ++imageGeneration;
    }

    adjustForNPOTTextureSize();

    //Find information for the block...
    QGLTexture2DTextureInfo *texInfo = 0;
    if (!textureInfo.isEmpty()) {
        int i=0;
        QOpenGLContext *ictx=0;
        do {
            if (i>=textureInfo.size()) {
                texInfo = 0;
                ictx = 0;
            } else {
                texInfo = textureInfo.at(i);
                ictx = const_cast<QOpenGLContext*>(textureInfo.at(i)->tex.context());
                if (texInfo->isLiteral)
                    return false;
            }
            i++;
        } while (texInfo!=0 && !QOpenGLContext::areSharing(ictx, ctx));
    }

    //If texInfo is 0, we need to create a new info block.
    if (!texInfo) {
        texInfo = new QGLTexture2DTextureInfo
            (0, 0, imageGeneration - 1, parameterGeneration - 1);
        textureInfo.push_back(texInfo);
    }

    if (!texInfo->tex.textureId() || imageGeneration != texInfo->imageGeneration) {
        // Create the texture contents and upload a new image.
        texInfo->tex.setOptions(bindOptions);
        if (!compressedData.isEmpty()) {
            texInfo->tex.bindCompressedTexture
                (compressedData.constData(), compressedData.size());
        } else {
            texInfo->tex.startUpload(ctx, target, image.size());
            bindImages(texInfo);
            texInfo->tex.finishUpload(target);
        }
        texInfo->imageGeneration = imageGeneration;
    } else {
        // Bind the existing texture to the texture target.
        glBindTexture(target, texInfo->tex.textureId());
    }

    // If the parameter generation has changed, then alter the parameters.
    if (parameterGeneration != texInfo->parameterGeneration) {
        texInfo->parameterGeneration = parameterGeneration;
        uint v = ((bindOptions & QGLTexture2D::LinearFilteringBindOption) ? GL_LINEAR : GL_NEAREST);
        q_glTexParameteri(target, GL_TEXTURE_MIN_FILTER, v);
        q_glTexParameteri(target, GL_TEXTURE_MAG_FILTER, v);
        q_glTexParameteri(target, GL_TEXTURE_WRAP_S, horizontalWrap);
        q_glTexParameteri(target, GL_TEXTURE_WRAP_T, verticalWrap);
    }

    // Texture is ready to be used.
    return true;
}

bool QGLTexture2DPrivate::cleanupResources()
{
    if (!textureInfo.empty()) {
        QOpenGLContext *ctx = QOpenGLContext::currentContext();
        if (ctx)
        {
            for (QList<QGLTexture2DTextureInfo*>::iterator It=textureInfo.begin(); It!=textureInfo.end();) {
                QGLTexture2DTextureInfo *texInfo = *It;
                QOpenGLContext *ictx = const_cast<QOpenGLContext*>(texInfo->tex.context());
                Q_ASSERT(ictx!=0);
                if (QOpenGLContext::areSharing(ictx, ctx)) {
                    if (!texInfo->isLiteral && texInfo->tex.textureId()) {
                        GLuint id = texInfo->tex.textureId();
                        glDeleteTextures(1, &id);
                        texInfo->tex.clearId();
                    }
                    It = textureInfo.erase(It);
                } else {
                    ++It;
                }
            }
        }
    }
    if (!textureInfo.empty()) {
        if (!url.isEmpty())
            qWarning("Texture '%s':",url.toString().toLatin1().constData());
        else
            qWarning("Texture (created from Image):");
        qWarning("  cleanupResources() was called from wrong context. Some OpenGL resources are not released.");
        return false;
    }
    return true;
}

void QGLTexture2DPrivate::bindImages(QGLTexture2DTextureInfo *info)
{
    QSize scaledSize(size);
#if defined(QT_OPENGL_ES_2)
    if ((bindOptions & QGLTexture2D::MipmapBindOption) ||
            horizontalWrap != QGL::ClampToEdge ||
            verticalWrap != QGL::ClampToEdge) {
        // ES 2.0 does not support NPOT textures when mipmaps are in use,
        // or if the wrap mode isn't ClampToEdge.
        scaledSize = QGL::nextPowerOfTwo(scaledSize);
    }
#endif
    if (!image.isNull())
        info->tex.uploadFace(GL_TEXTURE_2D, image, scaledSize);
    else if (size.isValid())
        info->tex.createFace(GL_TEXTURE_2D, scaledSize);
}

/*!
    Releases the texture associated with the 2D texture target.
    This is equivalent to \c{glBindTexture(GL_TEXTURE_2D, 0)}.

    \sa bind()
*/
void QGLTexture2D::release() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

/*!
    Returns the identifier associated with this texture object in
    the current context.

    Returns zero if the texture has not previously been bound to
    the 2D texture target in the current context with bind().

    \sa bind()
*/
GLuint QGLTexture2D::textureId() const
{
    Q_D(const QGLTexture2D);
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx)
        return 0;

    QGLTexture2DTextureInfo *info = 0;
    if (!d->textureInfo.isEmpty()) {
        int i=0;
        QOpenGLContext *ictx = 0;
        do {
            if (i>d->textureInfo.size()){
                info = 0;
                ictx = 0;
            } else {
                info = d->textureInfo.at(i);
                ictx = const_cast<QOpenGLContext*>(info->tex.context());
            }
            i++;
        } while (info != 0 && !QOpenGLContext::areSharing(ictx, ctx));
    }
    return info ? info->tex.textureId() : 0;
}

/*!
    Cleans up the resources associated with the QGLTexture2D.

    Calling this function to clean up resources must be done manually within
    the users' code.  Cleanup should be undertaken on application close, or
    when the QOpenGLContext associated with these resources is otherwise being
    destroyed.

    The user must ensure that the current thread in "owns" the associated
    QOpenGLContext, and that this context has been made current prior to cleanup.
    If these conditions are satisfied the user may simply call the cleanupResources()
    function.

    In multi-threaded applications where the user cannot ensure the context exists in
    the current thread, or that the context can be made current, the relevant context is
    generally available from within paint function, so the cleanupResources function
    should be called at the start of painting.  An example of an application where
    scenario arises is in multi-threaded QML2 application which uses a separate thread
    for rendering.

    If the context is not current or available the function will generate a
    warning and fail to cleanup the resources associated with that context.

    This does not remove any client side image that may be stored:
    for that, see clearImage().

    \sa bind()
    \sa release()
*/
bool QGLTexture2D::cleanupResources()
{
    Q_D(QGLTexture2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return d->cleanupResources();
}

/*!
    Constructs a QGLTexture2D object that wraps the supplied literal
    texture identifier \a id, with the dimensions specified by \a size.

    The \a id is assumed to have been created by the application in
    the current GL context, and it will be destroyed by the application
    after the returned QGLTexture2D object is destroyed.

    This function is intended for interfacing to existing code that
    uses raw GL texture identifiers.  The returned QGLTexture2D can
    only be used with the current GL context.

    \sa textureId()
*/
QGLTexture2D *QGLTexture2D::fromTextureId(GLuint id, const QSize& size)
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!id || !ctx)
        return 0;

    QGLTexture2D *texture = new QGLTexture2D();
    if (!size.isNull())
        texture->setSize(size);

    QGLTexture2DTextureInfo *info = new QGLTexture2DTextureInfo
        (ctx, id, texture->d_ptr->imageGeneration,
         texture->d_ptr->parameterGeneration, true);
    texture->d_ptr->textureInfo.push_back(info);
    return texture;
}

/*!
    This slot is used to recieve signals from the QDownloadManager class
    which provides Url download capability across the network.

    A successful download will have a valid QByteArray stored in \a assetData,
    while a failed download (due to network error, etc), will result in
    a NULL value.

    Successful downloads use the image data downloaded into \a assetData,
    which is converted to an image/texture, and emit a textureUpdated()
    signal.

    \sa QDownloadManager
*/
void QGLTexture2D::textureRequestFinished(QByteArray* assetData)
{
    //Ensure valid asset data exists.
    if (!assetData) {
        qWarning("DownloadManager request failed. Texture not loaded.");
    } else {
        //Convert asset data to an image.
        QImage texImage;
        texImage.loadFromData(*assetData);

        setSize(texImage.size());
        setImage(texImage.mirrored());

        emit textureUpdated();
    }

    if (assetData)
        delete assetData;
}


Q_GLOBAL_STATIC(QToBeDeleted,getPendingObject)
Q_GLOBAL_STATIC(QMutex,getPendingResourceMutex)

QToBeDeleted::QToBeDeleted(QObject *parent) : QObject(parent) {}

QToBeDeleted::~QToBeDeleted()
{
    for (PendingResourcesMapIter It=m_ToBeDeleted.begin(); It!=m_ToBeDeleted.end(); ++It) {
        QOpenGLContext* context = It.key();
        qWarning("OPENGL RESOURCE LEAK !");
        qWarning("  context %p:",context);
        QList<GLuint>& rPendingList = It.value();
        foreach (GLuint res, rPendingList) {
            qWarning("    resource %u",res);
        }
    }
}

void QToBeDeleted::processPendingResourceDeallocations()
{
    if (!m_ToBeDeleted.isEmpty()) {
        QOpenGLContext* currContext = QOpenGLContext::currentContext();
        Q_ASSERT(currContext != 0);
        bool bDeletedSomething = false;
        do {
            bDeletedSomething = false;
            for (PendingResourcesMapIter It=m_ToBeDeleted.begin(); It!=m_ToBeDeleted.end(); ++It) {
                QOpenGLContext* context = It.key();
                if (currContext==context || QOpenGLContext::areSharing(currContext,context)) {
                    QList<GLuint>& rPendingList = It.value();
                    foreach (GLuint res, rPendingList) {
                        glDeleteTextures(1,&res);
                    }
                    bDeletedSomething = true;
                }
                if (bDeletedSomething) {
                    m_ToBeDeleted.erase(It);
                    break;
                }
            }
        } while (bDeletedSomething);
    }
}

void QToBeDeleted::mournGLContextDeath()
{
    QMutexLocker locker(getPendingResourceMutex());
    processPendingResourceDeallocations();
}

void QGLTexture2D::toBeDeletedLater(QOpenGLContext* context, GLuint textureId)
{
    QMutexLocker locker(getPendingResourceMutex());
    QToBeDeleted* pObj = getPendingObject();
    PendingResourcesMapIter It = pObj->m_ToBeDeleted.find(context);
    if (It == pObj->m_ToBeDeleted.end()) {
        QObject::connect(context,SIGNAL(aboutToBeDestroyed()),pObj,SLOT(mournGLContextDeath()),Qt::DirectConnection);
        It = pObj->m_ToBeDeleted.insert(context,QList<GLuint>());
    }
    Q_ASSERT(It != pObj->m_ToBeDeleted.end());
    It.value().append(textureId);
}

void QGLTexture2D::processPendingResourceDeallocations()
{
    QMutexLocker locker(getPendingResourceMutex());
    QToBeDeleted* pObj = getPendingObject();
    pObj->processPendingResourceDeallocations();
}

/*!
    \fn QGLTexture2D::textureUpdated()
    Signals that some property of this texture has changed in a manner
    that may require that the parent material class be updated.
*/



inline static bool isPowerOfTwo(int x)
{
    // Assumption: x >= 1
    return x == (x & -x);
}

QGLTextureExtensions::QGLTextureExtensions(QOpenGLContext *ctx)
    : npotTextures(false)
    , generateMipmap(false)
    , bgraTextureFormat(false)
    , ddsTextureCompression(false)
    , etc1TextureCompression(false)
    , pvrtcTextureCompression(false)
    , compressedTexImage2D(0)
{
    Q_UNUSED(ctx);
    QGLExtensionChecker extensions(reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)));
    if (extensions.match("GL_ARB_texture_non_power_of_two"))
        npotTextures = true;
    if (extensions.match("GL_SGIS_generate_mipmap"))
        generateMipmap = true;
    if (extensions.match("GL_EXT_bgra"))
        bgraTextureFormat = true;
    if (extensions.match("GL_EXT_texture_compression_s3tc"))
        ddsTextureCompression = true;
    if (extensions.match("GL_OES_compressed_ETC1_RGB8_texture"))
        etc1TextureCompression = true;
    if (extensions.match("GL_IMG_texture_compression_pvrtc"))
        pvrtcTextureCompression = true;
#if defined(QT_OPENGL_ES_2)
    npotTextures = true;
    generateMipmap = true;
#endif
#if !defined(QT_OPENGL_ES)
    if (extensions.match("GL_ARB_texture_compression")) {
        compressedTexImage2D = (q_glCompressedTexImage2DARB)
            ctx->getProcAddress("glCompressedTexImage2DARB");
    }
#else
    compressedTexImage2D = glCompressedTexImage2D;
#endif

}

struct QGLTEHelper
{
    QGLTextureExtensions *d;
    QGLTEHelper() : d(0) {}
};

Q_GLOBAL_STATIC(QGLTEHelper, qt_qgltehelper)

QGLTextureExtensions::~QGLTextureExtensions()
{
}

QGLTextureExtensions *QGLTextureExtensions::extensions()
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx)
        return 0;
    if (!qt_qgltehelper()->d)
        qt_qgltehelper()->d = new QGLTextureExtensions(ctx);
    return qt_qgltehelper()->d;
}

QGLBoundTexture::QGLBoundTexture()
    : m_options(QGLTexture2D::DefaultBindOption)
    , m_hasAlpha(false)
    , m_context(0)
    , m_resourceId(0)
{
}

QGLBoundTexture::~QGLBoundTexture()
{
}

//#define QGL_BIND_TEXTURE_DEBUG

void QGLBoundTexture::startUpload(QOpenGLContext *ctx, GLenum target, const QSize &imageSize)
{
    Q_UNUSED(imageSize);

    QGLTextureExtensions *extensions = QGLTextureExtensions::extensions();
    if (!extensions)
        return;

#ifdef QGL_BIND_TEXTURE_DEBUG
    printf("QGLBoundTexture::startUpload(), imageSize=(%d,%d), options=%x\n",
           imageSize.width(), imageSize.height(), int(m_options));
    time.start();
#endif

#ifndef QT_NO_DEBUG
    // Reset the gl error stack...
    while (glGetError() != GL_NO_ERROR) ;
#endif

    // Create the texture id for the target, which should be one of
    // GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP.
    if (m_resourceId) {
        glBindTexture(target, 0);   // Just in case texture is bound.
        glDeleteTextures(1, &m_resourceId);
    }
    m_resourceId = 0;
    glGenTextures(1, &m_resourceId);
    glBindTexture(target, m_resourceId);
    m_context = ctx;

    GLuint filtering = m_options & QGLTexture2D::LinearFilteringBindOption ? GL_LINEAR : GL_NEAREST;

#ifdef QGL_BIND_TEXTURE_DEBUG
    printf(" - setting options (%d ms)\n", time.elapsed());
#endif
    q_glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filtering);

    if (extensions->generateMipmap && (m_options & QGLTexture2D::MipmapBindOption))
    {
#if !defined(QT_OPENGL_ES_2)
        glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
        q_glTexParameteri(target, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
#else
        glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
#endif
        q_glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
                m_options & QGLTexture2D::LinearFilteringBindOption
                        ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST);
    } else {
        q_glTexParameteri(target, GL_TEXTURE_MIN_FILTER, filtering);
        m_options &= ~QGLTexture2D::MipmapBindOption;
    }
}

// map from Qt's ARGB endianness-dependent format to GL's big-endian RGBA layout
static inline void qt_gl_byteSwapImage(QImage &img, GLenum pixel_type)
{
    const int width = img.width();
    const int height = img.height();

    if (pixel_type == GL_UNSIGNED_INT_8_8_8_8_REV
        || (pixel_type == GL_UNSIGNED_BYTE && QSysInfo::ByteOrder == QSysInfo::LittleEndian))
    {
        for (int i = 0; i < height; ++i) {
            uint *p = (uint *) img.scanLine(i);
            for (int x = 0; x < width; ++x)
                p[x] = ((p[x] << 16) & 0xff0000) | ((p[x] >> 16) & 0xff) | (p[x] & 0xff00ff00);
        }
    } else {
        for (int i = 0; i < height; ++i) {
            uint *p = (uint *) img.scanLine(i);
            for (int x = 0; x < width; ++x)
                p[x] = (p[x] << 8) | ((p[x] >> 24) & 0xff);
        }
    }
}

// #define QGL_BIND_TEXTURE_DEBUG

void QGLBoundTexture::uploadFace
    (GLenum target, const QImage &image, const QSize &scaleSize, GLenum format)
{
    GLenum internalFormat(format);

    // Resolve the texture-related extensions for the current context.
    QGLTextureExtensions *extensions = QGLTextureExtensions::extensions();
    if (!extensions)
        return;

    // Adjust the image size for scaling and power of two.
    QSize size = (!scaleSize.isEmpty() ? scaleSize : image.size());
    if (!extensions->npotTextures)
        size = QGL::nextPowerOfTwo(size);
    QImage img(image);
    if (size != image.size()) {
#ifdef QGL_BIND_TEXTURE_DEBUG
            printf(" - scaling up to %dx%d (%d ms) \n", size.width(), size.height(), time.elapsed());
#endif
        img = img.scaled(size);
    }
    m_size = size;

    QImage::Format target_format = img.format();
    bool premul = m_options & QGLTexture2D::PremultipliedAlphaBindOption;
    GLenum externalFormat;
    GLuint pixel_type = GL_UNSIGNED_BYTE;
    if (extensions->bgraTextureFormat) {
        externalFormat = GL_BGRA;
        // under OpenGL 1.2 with this extension apparently the pixel format might be
        // some GL_UNSIGNED_INT_8_8_8_8_REV - that is 5 years plus out of date, so
        // don't do that.
#ifdef QGL_BIND_TEXTURE_DEBUG
        qWarning("Checking for old image formats now not supported");
#endif
    } else {
        externalFormat = GL_RGBA;
    }

    switch (target_format) {
    case QImage::Format_ARGB32:
        if (premul) {
            img = img.convertToFormat(target_format = QImage::Format_ARGB32_Premultiplied);
#ifdef QGL_BIND_TEXTURE_DEBUG
            printf(" - converting ARGB32 -> ARGB32_Premultiplied (%d ms) \n", time.elapsed());
#endif
        }
        break;
    case QImage::Format_ARGB32_Premultiplied:
        if (!premul) {
            img = img.convertToFormat(target_format = QImage::Format_ARGB32);
#ifdef QGL_BIND_TEXTURE_DEBUG
            printf(" - converting ARGB32_Premultiplied -> ARGB32 (%d ms)\n", time.elapsed());
#endif
        }
        break;
    case QImage::Format_RGB16:
        pixel_type = GL_UNSIGNED_SHORT_5_6_5;
        externalFormat = GL_RGB;
        internalFormat = GL_RGB;
        break;
    case QImage::Format_RGB32:
        break;
    default:
        if (img.hasAlphaChannel()) {
            img = img.convertToFormat(premul
                                      ? QImage::Format_ARGB32_Premultiplied
                                      : QImage::Format_ARGB32);
#ifdef QGL_BIND_TEXTURE_DEBUG
            printf(" - converting to 32-bit alpha format (%d ms)\n", time.elapsed());
#endif
        } else {
            img = img.convertToFormat(QImage::Format_RGB32);
#ifdef QGL_BIND_TEXTURE_DEBUG
            printf(" - converting to 32-bit (%d ms)\n", time.elapsed());
#endif
        }
    }

    if (m_options & QGLTexture2D::InvertedYBindOption) {
#ifdef QGL_BIND_TEXTURE_DEBUG
            printf(" - flipping bits over y (%d ms)\n", time.elapsed());
#endif
        if (img.isDetached()) {
            int ipl = img.bytesPerLine() / 4;
            int h = img.height();
            for (int y=0; y<h/2; ++y) {
                int *a = (int *) img.scanLine(y);
                int *b = (int *) img.scanLine(h - y - 1);
                for (int x=0; x<ipl; ++x)
                    qSwap(a[x], b[x]);
            }
        } else {
            // Create a new image and copy across.  If we use the
            // above in-place code then a full copy of the image is
            // made before the lines are swapped, which processes the
            // data twice.  This version should only do it once.
            img = img.mirrored();
        }
    }

    if (externalFormat == GL_RGBA) {
#ifdef QGL_BIND_TEXTURE_DEBUG
            printf(" - doing byte swapping (%d ms)\n", time.elapsed());
#endif
        // The only case where we end up with a depth different from
        // 32 in the switch above is for the RGB16 case, where we set
        // the format to GL_RGB
        Q_ASSERT(img.depth() == 32);
        qt_gl_byteSwapImage(img, pixel_type);
    }
#ifdef QT_OPENGL_ES
    // OpenGL/ES requires that the internal and external formats be
    // identical.
    internalFormat = externalFormat;
#endif
#ifdef QGL_BIND_TEXTURE_DEBUG
    printf(" - uploading, image.format=%d, externalFormat=0x%x, internalFormat=0x%x, pixel_type=0x%x\n",
           img.format(), externalFormat, internalFormat, pixel_type);
#endif

    const QImage &constRef = img; // to avoid detach in bits()...
    glTexImage2D(target, 0, internalFormat, img.width(), img.height(), 0, externalFormat,
                 pixel_type, constRef.bits());

    m_hasAlpha = (internalFormat != GL_RGB);
}

void QGLBoundTexture::createFace
    (GLenum target, const QSize &size, GLenum format)
{
    glTexImage2D(target, 0, format, size.width(),
                 size.height(), 0, format, GL_UNSIGNED_BYTE, 0);
    m_hasAlpha = (format != GL_RGB);
}

void QGLBoundTexture::finishUpload(GLenum target)
{
    Q_UNUSED(target);

#if defined(QT_OPENGL_ES_2)
    // OpenGL/ES 2.0 needs to generate mipmaps after all cubemap faces
    // have been uploaded.
    if (m_options & QGLTexture2D::MipmapBindOption) {
#ifdef QGL_BIND_TEXTURE_DEBUG
        printf(" - generating mipmaps (%d ms)\n", time.elapsed());
#endif
        glGenerateMipmap(target);
    }
#endif

#ifndef QT_NO_DEBUG
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        qWarning(" - texture upload failed, error code 0x%x, enum: %d (%x)\n", error, target, target);
    }
#endif

#ifdef QGL_BIND_TEXTURE_DEBUG
    static int totalUploadTime = 0;
    totalUploadTime += time.elapsed();
    printf(" - upload done in (%d ms) time=%d\n", time.elapsed(), totalUploadTime);
#endif
}

// DDS format structure
struct DDSFormat {
    quint32 dwSize;
    quint32 dwFlags;
    quint32 dwHeight;
    quint32 dwWidth;
    quint32 dwLinearSize;
    quint32 dummy1;
    quint32 dwMipMapCount;
    quint32 dummy2[11];
    struct {
        quint32 dummy3[2];
        quint32 dwFourCC;
        quint32 dummy4[5];
    } ddsPixelFormat;
};

// compressed texture pixel formats
#define FOURCC_DXT1  0x31545844
#define FOURCC_DXT2  0x32545844
#define FOURCC_DXT3  0x33545844
#define FOURCC_DXT4  0x34545844
#define FOURCC_DXT5  0x35545844

#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#endif

// PVR header format for container files that store textures compressed
// with the ETC1, PVRTC2, and PVRTC4 encodings.  Format information from the
// PowerVR SDK at http://www.imgtec.com/powervr/insider/powervr-sdk.asp
// "PVRTexTool Reference Manual, version 1.11f".
struct PvrHeader
{
    quint32 headerSize;
    quint32 height;
    quint32 width;
    quint32 mipMapCount;
    quint32 flags;
    quint32 dataSize;
    quint32 bitsPerPixel;
    quint32 redMask;
    quint32 greenMask;
    quint32 blueMask;
    quint32 alphaMask;
    quint32 magic;
    quint32 surfaceCount;
};

#define PVR_MAGIC               0x21525650      // "PVR!" in little-endian

#define PVR_FORMAT_MASK         0x000000FF
#define PVR_FORMAT_PVRTC2       0x00000018
#define PVR_FORMAT_PVRTC4       0x00000019
#define PVR_FORMAT_ETC1         0x00000036

#define PVR_HAS_MIPMAPS         0x00000100
#define PVR_TWIDDLED            0x00000200
#define PVR_NORMAL_MAP          0x00000400
#define PVR_BORDER_ADDED        0x00000800
#define PVR_CUBE_MAP            0x00001000
#define PVR_FALSE_COLOR_MIPMAPS 0x00002000
#define PVR_VOLUME_TEXTURE      0x00004000
#define PVR_ALPHA_IN_TEXTURE    0x00008000
#define PVR_VERTICAL_FLIP       0x00010000

#ifndef GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG
#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG      0x8C00
#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG      0x8C01
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG     0x8C02
#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG     0x8C03
#endif

#ifndef GL_ETC1_RGB8_OES
#define GL_ETC1_RGB8_OES                        0x8D64
#endif

bool QGLBoundTexture::canBindCompressedTexture
    (const char *buf, int len, const char *format, bool *hasAlpha,
     bool *isFlipped)
{
    if (QSysInfo::ByteOrder != QSysInfo::LittleEndian) {
        // Compressed texture loading only supported on little-endian
        // systems such as x86 and ARM at the moment.
        return false;
    }
    if (!format) {
        // Auto-detect the format from the header.
        if (len >= 4 && !qstrncmp(buf, "DDS ", 4)) {
            *hasAlpha = true;
            *isFlipped = true;
            return true;
        } else if (len >= 52 && !qstrncmp(buf + 44, "PVR!", 4)) {
            const PvrHeader *pvrHeader =
                reinterpret_cast<const PvrHeader *>(buf);
            *hasAlpha = (pvrHeader->alphaMask != 0);
            *isFlipped = ((pvrHeader->flags & PVR_VERTICAL_FLIP) != 0);
            return true;
        }
    } else {
        // Validate the format against the header.
        if (!qstricmp(format, "DDS")) {
            if (len >= 4 && !qstrncmp(buf, "DDS ", 4)) {
                *hasAlpha = true;
                *isFlipped = true;
                return true;
            }
        } else if (!qstricmp(format, "PVR") || !qstricmp(format, "ETC1")) {
            if (len >= 52 && !qstrncmp(buf + 44, "PVR!", 4)) {
                const PvrHeader *pvrHeader =
                    reinterpret_cast<const PvrHeader *>(buf);
                *hasAlpha = (pvrHeader->alphaMask != 0);
                *isFlipped = ((pvrHeader->flags & PVR_VERTICAL_FLIP) != 0);
                return true;
            }
        }
    }
    return false;
}

bool QGLBoundTexture::bindCompressedTexture
    (const char *buf, int len, const char *format)
{
    if (QSysInfo::ByteOrder != QSysInfo::LittleEndian) {
        // Compressed texture loading only supported on little-endian
        // systems such as x86 and ARM at the moment.
        return false;
    }
#if !defined(QT_OPENGL_ES)
    QGLTextureExtensions *extensions = QGLTextureExtensions::extensions();
    if (!extensions)
        return false;
    if (!extensions->compressedTexImage2D) {
        qWarning("QOpenGLContext::bindTexture(): The GL implementation does "
                 "not support texture compression extensions.");
        return false;
    }
#endif
    if (!format) {
        // Auto-detect the format from the header.
        if (len >= 4 && !qstrncmp(buf, "DDS ", 4))
            return bindCompressedTextureDDS(buf, len);
        else if (len >= 52 && !qstrncmp(buf + 44, "PVR!", 4))
            return bindCompressedTexturePVR(buf, len);
    } else {
        // Validate the format against the header.
        if (!qstricmp(format, "DDS")) {
            if (len >= 4 && !qstrncmp(buf, "DDS ", 4))
                return bindCompressedTextureDDS(buf, len);
        } else if (!qstricmp(format, "PVR") || !qstricmp(format, "ETC1")) {
            if (len >= 52 && !qstrncmp(buf + 44, "PVR!", 4))
                return bindCompressedTexturePVR(buf, len);
        }
    }
    return false;
}

bool QGLBoundTexture::bindCompressedTexture
    (const QString& fileName, const char *format)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QByteArray contents = file.readAll();
    file.close();
    return bindCompressedTexture
        (contents.constData(), contents.size(), format);
}

bool QGLBoundTexture::bindCompressedTextureDDS(const char *buf, int len)
{
    QGLTextureExtensions *extensions = QGLTextureExtensions::extensions();
    if (!extensions)
        return false;

    // Bail out if the necessary extension is not present.
    if (!extensions->ddsTextureCompression) {
        qWarning("QGLBoundTexture::bindCompressedTextureDDS(): DDS texture compression is not supported.");
        return false;
    }

    const DDSFormat *ddsHeader = reinterpret_cast<const DDSFormat *>(buf + 4);
    if (!ddsHeader->dwLinearSize) {
        qWarning("QGLBoundTexture::bindCompressedTextureDDS(): DDS image size is not valid.");
        return false;
    }

    int blockSize = 16;
    GLenum format;

    switch(ddsHeader->ddsPixelFormat.dwFourCC) {
    case FOURCC_DXT1:
        format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        blockSize = 8;
        break;
#if !defined(QT_OPENGL_ES_2)
    case FOURCC_DXT3:
        format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        break;
    case FOURCC_DXT5:
        format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        break;
#endif
    default:
        qWarning("QGLBoundTexture::bindCompressedTextureDDS(): DDS image format not supported.");
        return false;
    }

    const GLubyte *pixels =
        reinterpret_cast<const GLubyte *>(buf + ddsHeader->dwSize + 4);

    if (m_resourceId) {
        glBindTexture(GL_TEXTURE_2D, 0);    // Just in case it is bound.
        glDeleteTextures(1, &m_resourceId);
    }
    q_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    q_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    m_resourceId = 0;
    glGenTextures(1, &m_resourceId);
    glBindTexture(GL_TEXTURE_2D, m_resourceId);

    int size;
    int offset = 0;
    int available = len - int(ddsHeader->dwSize + 4);
    int w = ddsHeader->dwWidth;
    int h = ddsHeader->dwHeight;

    // load mip-maps
    for (int i = 0; i < (int) ddsHeader->dwMipMapCount; ++i) {
        if (w == 0) w = 1;
        if (h == 0) h = 1;

        size = ((w+3)/4) * ((h+3)/4) * blockSize;
        if (size > available)
            break;
        extensions->compressedTexImage2D
            (GL_TEXTURE_2D, i, format, w, h, 0, size, pixels + offset);
        offset += size;
        available -= size;

        // half size for each mip-map level
        w = w/2;
        h = h/2;
    }

    // DDS images are not inverted.
    m_options &= ~QGLTexture2D::InvertedYBindOption;

    m_size = QSize(ddsHeader->dwWidth, ddsHeader->dwHeight);
    m_hasAlpha = false;
    return true;
}

bool QGLBoundTexture::bindCompressedTexturePVR(const char *buf, int len)
{
    QGLTextureExtensions *extensions = QGLTextureExtensions::extensions();
    if (!extensions)
        return false;

    // Determine which texture format we will be loading.
    const PvrHeader *pvrHeader = reinterpret_cast<const PvrHeader *>(buf);
    GLenum textureFormat;
    quint32 minWidth, minHeight;
    switch (pvrHeader->flags & PVR_FORMAT_MASK) {
    case PVR_FORMAT_PVRTC2:
        if (pvrHeader->alphaMask)
            textureFormat = GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
        else
            textureFormat = GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
        minWidth = 16;
        minHeight = 8;
        break;

    case PVR_FORMAT_PVRTC4:
        if (pvrHeader->alphaMask)
            textureFormat = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
        else
            textureFormat = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
        minWidth = 8;
        minHeight = 8;
        break;

    case PVR_FORMAT_ETC1:
        textureFormat = GL_ETC1_RGB8_OES;
        minWidth = 4;
        minHeight = 4;
        break;

    default:
        qWarning("QGLBoundTexture::bindCompressedTexturePVR(): PVR image format 0x%x not supported.", int(pvrHeader->flags & PVR_FORMAT_MASK));
        return false;
    }

    // Bail out if the necessary extension is not present.
    if (textureFormat == GL_ETC1_RGB8_OES) {
        if (!extensions->etc1TextureCompression) {
            qWarning("QGLBoundTexture::bindCompressedTexturePVR(): ETC1 texture compression is not supported.");
            return false;
        }
    } else {
        if (!extensions->pvrtcTextureCompression) {
            qWarning("QGLBoundTexture::bindCompressedTexturePVR(): PVRTC texture compression is not supported.");
            return false;
        }
    }

    // Boundary check on the buffer size.
    quint32 bufferSize = pvrHeader->headerSize + pvrHeader->dataSize;
    if (bufferSize > quint32(len)) {
        qWarning("QGLBoundTexture::bindCompressedTexturePVR(): PVR image size is not valid.");
        return false;
    }

    // Create the texture.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (m_resourceId) {
        glBindTexture(GL_TEXTURE_2D, 0);    // Just in case it is bound.
        glDeleteTextures(1, &m_resourceId);
    }
    m_resourceId = 0;
    glGenTextures(1, &m_resourceId);
    glBindTexture(GL_TEXTURE_2D, m_resourceId);
    if (pvrHeader->mipMapCount) {
        if ((m_options & QGLTexture2D::LinearFilteringBindOption) != 0) {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        } else {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        }
    } else if ((m_options & QGLTexture2D::LinearFilteringBindOption) != 0) {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    } else {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    // Load the compressed mipmap levels.
    const GLubyte *buffer =
        reinterpret_cast<const GLubyte *>(buf + pvrHeader->headerSize);
    bufferSize = pvrHeader->dataSize;
    quint32 level = 0;
    quint32 width = pvrHeader->width;
    quint32 height = pvrHeader->height;
    while (bufferSize > 0 && level <= pvrHeader->mipMapCount) {
        quint32 size =
            (qMax(width, minWidth) * qMax(height, minHeight) *
             pvrHeader->bitsPerPixel) / 8;
        if (size > bufferSize)
            break;
        extensions->compressedTexImage2D
            (GL_TEXTURE_2D, GLint(level), textureFormat,
             GLsizei(width), GLsizei(height), 0, GLsizei(size), buffer);
        width /= 2;
        height /= 2;
        buffer += size;
        ++level;
    }

    // Restore the default pixel alignment for later texture uploads.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    // Set the invert flag for the texture.  The "vertical flip"
    // flag in PVR is the opposite sense to our sense of inversion.
    if ((pvrHeader->flags & PVR_VERTICAL_FLIP) != 0)
        m_options &= ~QGLTexture2D::InvertedYBindOption;
    else
        m_options |= QGLTexture2D::InvertedYBindOption;

    m_size = QSize(pvrHeader->width, pvrHeader->height);
    m_hasAlpha = (pvrHeader->alphaMask != 0);
    return true;
}


QT_END_NAMESPACE
