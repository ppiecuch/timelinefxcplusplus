
#include "QtEffectsLibrary.h"
#include "TLFXPugiXMLLoader.h"

#include <QFile>
#include <QOpenGLContext>
#include <QOpenGLTexture>
#include <QImage>
#include <QScreen>
#include <QGuiApplication>

#include <stdint.h>
#include <cmath>

#include "qgeometry/qglpainter.h"
#include "qgeometry/qglbuilder.h"

#include "vogl_miniz_zip.h"

QtEffectsLibrary::QtEffectsLibrary()
{
    if (qApp == 0)
        qWarning("[QtEffectsLibrary] Application is not initialized.");
    else
        SetUpdateFrequency(qApp->primaryScreen()->refreshRate());
}

bool LoadLibrary(const char *library, const char *filename /* = 0 */, bool compile /* = true */)
{
    mz_zip_archive zip_archive;
    
    // Now try to open the archive.
    memset(&zip_archive, 0, sizeof(zip_archive));
    mz_bool status = mz_zip_reader_init_file(&zip_archive, library);
    if (!status)
    {
        printf("mz_zip_reader_init_file() failed!\n");
        return false;
    }
  
    // Get and print information about each file in the archive.
    for (int i = 0; i < (int)mz_zip_get_num_files(&zip_archive); i++)
    {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_file_stat(&zip_archive, i, &file_stat))
        {
            printf("mz_zip_file_stat() failed!\n");
            mz_zip_reader_end(&zip_archive);
            return false;
        }
        printf("Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u, Is Dir: %u\n", file_stat.m_filename, file_stat.m_comment, (uint)file_stat.m_uncomp_size, (uint)file_stat.m_comp_size, mz_zip_is_file_a_directory(&zip_archive, i));
    }
    
    // Try to extract all the files to the heap.
    size_t uncomp_size;
    void *p = mz_zip_extract_file_to_heap(&zip_archive, filename, &uncomp_size, 0);
    if (!p)
    {
        printf("mz_zip_extract_file_to_heap() failed!\n");
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    printf("Successfully extracted file \"%s\", size %u\n", filename, (uint)uncomp_size);

    // We're done.
    mz_free(p);
      
    // Close the archive, freeing any resources it was using
    mz_zip_reader_end(&zip_archive);
    return true;
}

TLFX::XMLLoader* QtEffectsLibrary::CreateLoader() const
{
    return new TLFX::PugiXMLLoader(0);
}

TLFX::AnimImage* QtEffectsLibrary::CreateImage() const
{
    return new QtImage();
}

//Textures DB
QMap<QString, QImage> s_textureDB;
QMap<QString, QSharedPointer<QOpenGLTexture>> s_openGLTextureDB;

bool QtImage::Load( const char *filename )
{
    QFile f(filename);
    if (!f.exists())
        f.setFileName(QString(":/data/%1").arg(filename));
    if (!f.exists()) {
        qDebug() << "Failed to load image: " << filename;
        return false;
    }
    _texture = new QOpenGLTexture(QImage(f.fileName()).mirrored());
    _texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
    _texture->setMagnificationFilter(QOpenGLTexture::Linear);

    _image = f.fileName();

    return true;
}

QtImage::QtImage()
    : _texture(0)
{
}

QtImage::~QtImage()
{
    if (_texture)
        delete _texture;
}

QtParticleManager::QtParticleManager( QGLPainter *p, int particles /*= particleLimit*/, int layers /*= 1*/ )
    : TLFX::ParticleManager(particles, layers)
    , _lastSprite(0)
    , _lastAdditive(true), _forceBlend(false)
    , _p(p)
{
}

void QtParticleManager::DrawSprite( TLFX::Particle *p, TLFX::AnimImage* sprite, float px, float py, float frame, float x, float y, float rotation, float scaleX, float scaleY, unsigned char r, unsigned char g, unsigned char b, float a , bool additive )
{
    if(frame > 1)
        qWarning() << "[DrawSprite] Invalid frame " << frame;

    #define qFF(C) C*(255.999)

    quint8 alpha = qFF(a);
    if (alpha == 0 || scaleX == 0 || scaleY == 0) return;

    if ((sprite != _lastSprite) || ((additive != _lastAdditive) && !_forceBlend))
        Flush();

    //uvs[index + 0] = {0, 0};
    batch.appendTexCoord(QVector2D(0, 0));
    //uvs[index + 1] = {1.0f, 0}
    batch.appendTexCoord(QVector2D(1, 0));
    //uvs[index + 2] = {1.0f, 1.0f};
    batch.appendTexCoord(QVector2D(1, 1));
    //uvs[index + 3] = {0, 1.0f};
    batch.appendTexCoord(QVector2D(0, 1));

    /*
    verts[index + 0].x = px - x * scaleX;
    verts[index + 0].y = py - y * scaleY;
    //verts[index + 0].z = 1.0f;
    verts[index + 1].x = verts[index + 0].x + _lastSprite->GetWidth() * scaleX;
    verts[index + 1].y = verts[index + 0].y;
    //verts[index + 1].z = 1.0f;
    verts[index + 2].x = verts[index + 1].x;
    verts[index + 2].y = verts[index + 1].y + _lastSprite->GetHeight() * scaleY;
    //verts[index + 2].z = 1.0f;
    verts[index + 3].x = verts[index + 0].x;
    verts[index + 3].y = verts[index + 2].y;
    //verts[index + 3].z = 1.0f;
    */

    float x0 = -x * scaleX;
    float y0 = -y * scaleY;
    float x1 = x0;
    float y1 = (-y + sprite->GetHeight()) * scaleY;
    float x2 = (-x + sprite->GetWidth()) * scaleX;
    float y2 = y1;
    float x3 = x2;
    float y3 = y0;

    float cos = cosf(rotation / 180.f * M_PI);
    float sin = sinf(rotation / 180.f * M_PI);

    //verts[index + 0] = {px + x0 * cos - y0 * sin, py + x0 * sin + y0 * cos};
    //verts[index + 0].z = 1.0f;
    batch.appendVertex(QVector3D(px + x0 * cos - y0 * sin, py + x0 * sin + y0 * cos, 0));
    //verts[index + 1] = {px + x1 * cos - y1 * sin, py + x1 * sin + y1 * cos};
    //verts[index + 1].z = 1.0f;
    batch.appendVertex(QVector3D(px + x1 * cos - y1 * sin, py + x1 * sin + y1 * cos, 0));
    //verts[index + 2] = {px + x2 * cos - y2 * sin, py + x2 * sin + y2 * cos};
    //verts[index + 2].z = 1.0f;
    batch.appendVertex(QVector3D(px + x2 * cos - y2 * sin, py + x2 * sin + y2 * cos, 0));
    //verts[index + 3] = {px + x3 * cos - y3 * sin, py + x3 * sin + y3 * cos};
    //verts[index + 3].z = 1.0f;
    batch.appendVertex(QVector3D(px + x3 * cos - y3 * sin, py + x3 * sin + y3 * cos, 0));

    for (int i = 0; i < 4; ++i) 
    {
        batch.appendColor(QColor(r, g, b, alpha));
    }
    
    _lastSprite = sprite;
    _lastAdditive = additive;
}

void QtParticleManager::Flush()
{
    if (batch.count() && _lastSprite)
    {
        Q_ASSERT(_p);

        glDisable( GL_DEPTH );
        glEnable( GL_BLEND );
        glEnable( GL_TEXTURE_2D );
        dynamic_cast<QtImage*>(_lastSprite)->GetTexture()->bind();
        if (_lastAdditive && !_forceBlend) {
            // ALPHA_ADD
            glBlendFunc( GL_SRC_ALPHA, GL_ONE );
        } else {
            // ALPHA_BLEND
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        }

        QGLBuilder builder;
        builder.addQuads(batch);
        QList<QGeometryData> opt = builder.optimized();
        Q_FOREACH(QGeometryData gd, opt) {
            gd.draw(_p, 0, gd.indexCount());
        }
        dynamic_cast<QtImage*>(_lastSprite)->GetTexture()->release();
        batch = QGeometryData(); // clear batch data
    }
}
