
#include "QtEffectsLibrary.h"
#include "TLFXPugiXMLLoader.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QOpenGLContext>
#include <QOpenGLTexture>
#include <QImage>
#include <QBitmap>
#include <QScreen>
#include <QDesktopWidget>
#include <QApplication>

#include <stdint.h>
#include <cmath>

#include "qgeometry/qglpainter.h"
#include "qgeometry/qglbuilder.h"

#include "vogl_miniz_zip.h"

/** KDE effect code */
static void __toGray(QImage &img, float value);
static void __toGray2(QImage &img);
static void __colorize(QImage &img, const QColor &col, float value);
static void __toMonochrome(QImage &img, const QColor &black,
                               const QColor &white, float value);
static void __deSaturate(QImage &img, float value);
static void __toGamma(QImage &img, float value);
static void __semiTransparent(QImage &img);
static QImage __doublePixels(const QImage &src);
static void __overlay(QImage &src, QImage &overlay);
static void __overlay(QImage &src, QImage &overlay);

bool QtImage::Load()
{
    return true;
}


QtEffectsLibrary::QtEffectsLibrary() : _atlas(0)
{
    if (qApp == 0)
        qWarning() << "[QtEffectsLibrary] Application is not initialized.";
    else
        SetUpdateFrequency(qApp->primaryScreen()->refreshRate());

    _atlas = new QAtlasManager;
}

QtEffectsLibrary::~QtEffectsLibrary()
{
    delete _atlas;
}

bool QtEffectsLibrary::LoadLibrary(const char *library, const char *filename /* = 0 */, bool compile /* = true */)
{
    QString libraryinfo = filename;

    struct zip_archive_t {
        zip_archive_t() { memset(&za, 0, sizeof(mz_zip_archive)); }
        ~zip_archive_t() { mz_zip_reader_end(&za); }
        mz_bool init_file(const char *fn) { return mz_zip_reader_init_file(&za, fn); }
        mz_zip_archive * operator &() { return &za; }
        mz_zip_archive za;
    } zip_archive;

    // Now try to open the archive.
    mz_bool status = zip_archive.init_file(library);
    if (!status)
    {
        qWarning() << "[QtEffectsLibrary] Cannot open effects library" << library;
        return false;
    }
    
    if (libraryinfo.isEmpty())
    {
        // Try to locate effect data file.
        for (int i = 0; i < (int)mz_zip_get_num_files(&zip_archive); i++)
        {
            mz_zip_archive_file_stat file_stat;
            if (!mz_zip_file_stat(&zip_archive, i, &file_stat))
            {
                qWarning() << "[QtEffectsLibrary] Cannot read effects library!";
                return false;
            }
            if(libraryinfo.isEmpty() && strcasestr(file_stat.m_filename, "data.xml"))
            {
                libraryinfo = file_stat.m_filename;
                break;
            }
            // qDebug("Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u, Is Dir: %u\n", file_stat.m_filename, file_stat.m_comment, (uint)file_stat.m_uncomp_size, (uint)file_stat.m_comp_size, mz_zip_is_file_a_directory(&zip_archive, i));
        }
    }

    if (libraryinfo.isEmpty())
    {
        qWarning() << "[QtEffectsLibrary] Cannot find library description file!";
        return false;
    }

    // Keep library we are using for effects
    _library = library;

    return Load(libraryinfo.toUtf8().constData(), compile);
}

TLFX::XMLLoader* QtEffectsLibrary::CreateLoader() const
{
    return new TLFX::PugiXMLLoader(_library.isEmpty()?0:_library.toUtf8().constData());
}

TLFX::AnimImage* QtEffectsLibrary::CreateImage() const
{
    return new QtImage();
}

bool QtEffectsLibrary::ensureTextureSize(int &w, int &h)
{
    // for texture bigger then atlas fix the size to textureLimit:
    if (w > _atlas->atlasTextureSize().width() || h > _atlas->atlasTextureSize().height())
    {
        qreal scale = qMin( _atlas->atlasTextureSizeLimit()/qreal(w),
                            _atlas->atlasTextureSizeLimit()/qreal(h) );
        w *= scale; h *= scale;
        return false; // donot scale - keep size
    // if greater than atlas limit, make the size scaled :
    } else if (w > _atlas->atlasTextureSizeLimit() || h > _atlas->atlasTextureSizeLimit())
    {
        qreal scale = qMin( _atlas->atlasTextureSizeLimit()/qreal(w),
                            _atlas->atlasTextureSizeLimit()/qreal(h) );
        w *= scale; h *= scale;
    }
    return true; // scale
}

bool QtEffectsLibrary::UploadTextures()
{
    // try calculate best fit into current atlas texture:
    int minw = 0, maxw = 0, minh = 0, maxh = 0;
    Q_FOREACH(TLFX::AnimImage *shape, _shapeList)
    {
        const int w = shape->GetWidth();
        const int h = shape->GetHeight();
        if (w < minw) minw = w;
        if (h < minh) minh = h;
        if (w > maxw) maxw = w;
        if (h > maxh) maxh = h;
    }
#define SC(x) (sc/(1.1+(x-minw)/(maxw-minw))) // 1-2
    qreal sc = 1.5; bool done = false; while(!done && sc > 0)
    {
        QGL::QAreaAllocator m_allocator(_atlas->atlasTextureSize(), _atlas->padding);
        qDebug() << "[QtEffectsLibrary] Scaling texture atlas with" << sc;
        done = true; Q_FOREACH(TLFX::AnimImage *shape, _shapeList)
        {
            const int anim_size = powf(2, ceilf(log2f(shape->GetFramesCount())));
            const int anim_square = sqrtf(anim_size);
            int w = shape->GetWidth()*anim_square;
            int h = shape->GetHeight()*anim_square;
            if(ensureTextureSize(w, h))
            {
                w *= SC(w);
                h *= SC(h);
            }
            QRect rc = m_allocator.allocate(QSize(w, h));
            if (rc.width() == 0 || rc.height() == 0) 
            {
                sc -= 0.05; done = false; break; // next step
            }
        }
    }
    if (!done) {
        qDebug() << "[QtEffectsLibrary] Cannot build texture atlas.";
        return false;
    }
    if (!_library.isEmpty()) 
    {
        struct zip_archive_t {
            zip_archive_t() { memset(&za, 0, sizeof(mz_zip_archive)); }
            ~zip_archive_t() { mz_zip_reader_end(&za); }
            mz_bool init_file(const char *fn) { return mz_zip_reader_init_file(&za, fn); }
            mz_zip_archive * operator &() { return &za; }
            mz_zip_archive za;
        } zip_archive;
        
        // Now try to open the archive.
        mz_bool status = zip_archive.init_file(_library.toUtf8().constData());
        if (status)
        {
            Q_FOREACH(TLFX::AnimImage *shape, _shapeList)
            {
                const char *filename = shape->GetFilename();
                const int anim_size = powf(2, ceilf(log2f(shape->GetFramesCount())));
                const int anim_square = sqrtf(anim_size);
                int w = shape->GetWidth()*anim_square;
                int h = shape->GetHeight()*anim_square;
                if(ensureTextureSize(w, h))
                {
                    w *= SC(w);
                    h *= SC(h);
                }

                if (filename==0 || strlen(filename)==0)
                {
                    qWarning() << "[QtEffectsLibrary] Empty image filename";
                    continue;
                }
                // Try to extract all the files to the heap.
                QStringList variants; 
                variants
                    << filename
                    << QFileInfo(filename).fileName()
                    << QFileInfo(QString(filename).replace("\\","/")).fileName();
                Q_FOREACH(QString fn, variants)
                {
                    size_t uncomp_size;
                    void *p = mz_zip_extract_file_to_heap(&zip_archive, fn.toUtf8().constData(), &uncomp_size, 0);
                    if (p == 0) 
                    {
                        continue; // Try next name
                    }

                    qDebug() << "[QtEffectsLibrary] Successfully extracted file" << fn << uncomp_size << "bytes";

                    QImage img = QImage::fromData((const uchar *)p, uncomp_size);

                    if (img.isNull())
                    {
                        qWarning() << "[QtEffectsLibrary] Failed to create image:" << filename;
                        return false;
                    }
                    else
                    {
                        switch (shape->GetImportOpt()) {
                            case QtImage::impGreyScale:  __toGray2(img); break;
                            case QtImage::impFullColour: break;
                            case QtImage::impPassThrough: break;
                            default: break;
                        }

                        // scale images to fit atlas
                        QTexture *texture = _atlas->create(img.scaled(QSize(w,h)));
                        dynamic_cast<QtImage*>(shape)->SetTexture(texture, filename);
                        if (texture == 0) {
                            qWarning() << "[QtEffectsLibrary] Failed to create texture for image" << filename << img.size() << QString("%1 frames").arg(shape->GetFramesCount());
                            return false;
                        } else
                            break;
                    }                    
                    // We're done.
                    mz_free(p);
                }
                if (0 == dynamic_cast<QtImage*>(shape)->GetTexture()) 
                {
                    qWarning() << "[QtEffectsLibrary] Failed to extract file" << filename;
                    return false;
                }
            }
            return true;
        } else {
            qWarning() << "[QtEffectsLibrary] Cannot open library file" << _library;
            return false;
        }
    } else {
        Q_FOREACH(TLFX::AnimImage *shape, _shapeList)
        {
            const char *filename = shape->GetFilename();
            const int anim_size = powf(2, ceilf(log2f(shape->GetFramesCount())));
            const int anim_square = sqrtf(anim_size);
            int w = shape->GetWidth()*anim_square;
            int h = shape->GetHeight()*anim_square;
            if(ensureTextureSize(w, h))
            {
                w *= SC(w);
                h *= SC(h);
            }

            QFile f(filename);
            if (!f.exists())
                f.setFileName(QString(":/data/%1").arg(filename));
            if (!f.exists()) {
                qWarning() << "[QtImage] Failed to load image:" << filename;
                return false;
            }
            QImage img(f.fileName());
            if (img.isNull())
            {
                qWarning() << "[QtImage] Failed to load image:" << filename;
                return false;
            }
            else
            {
                switch (shape->GetImportOpt()) {
                    case QtImage::impGreyScale:  __toGray2(img); break;
                    case QtImage::impFullColour: break;
                    case QtImage::impPassThrough: break;
                    default: break;
                }

                // scale images to fit atlas
                QTexture *texture = _atlas->create(img.scaled(QSize(w,h)));
                dynamic_cast<QtImage*>(shape)->SetTexture(texture, filename);
                if (texture == 0) {
                    qWarning() << "[QtImage] Failed to create texture for image" << filename << img.size() << QString("%1 frames").arg(shape->GetFramesCount());
                    return false;
                }
            }
        }
    }
    return true;
}

void QtEffectsLibrary::Debug(QGLPainter *p)
{
    Q_FOREACH(TLFX::AnimImage *sprite, _shapeList)
    {
        if (sprite->GetFramesCount() != 64) continue;
        
        // draw texture quad:
        
        glDisable( GL_DEPTH );
        glEnable( GL_BLEND );
        glDisable( GL_ALPHA_TEST );
        glEnable( GL_TEXTURE_2D );
        dynamic_cast<QtImage*>(sprite)->GetTexture()->bind();
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

        static qreal f = 0;
        
        QRectF rc = dynamic_cast<QtImage*>(sprite)->GetTexture()->normalizedTextureSubRect();
        const int anim_size = powf(2, ceilf(log2f(sprite->GetFramesCount())));
        const int anim_square = sqrtf(anim_size);
        const int anim_frame = int(round(f)) % sprite->GetFramesCount(); f += 0.1;
        const int gr = anim_frame / anim_square, gc = anim_frame % anim_square;
        const float cw = rc.width()/anim_square, ch = rc.height()/anim_square;
        rc = QRectF(rc.x()+gc*cw, rc.y()+gr*ch, cw, ch);
        
        QGeometryData batch;
        batch.appendVertex(QVector3D(0,0,0));
        batch.appendVertex(QVector3D(sprite->GetWidth(),0,0));
        batch.appendVertex(QVector3D(sprite->GetWidth(),sprite->GetHeight(),0));
        batch.appendVertex(QVector3D(0,sprite->GetHeight(),0));
        batch.appendTexCoord(QVector2D(rc.x(), rc.y()));
        batch.appendTexCoord(QVector2D(rc.x()+rc.width(), rc.y()));
        batch.appendTexCoord(QVector2D(rc.x()+rc.width(), rc.y()+rc.height()));
        batch.appendTexCoord(QVector2D(rc.x(), rc.y()+rc.height()));
        batch.appendColor(Qt::white);
        batch.appendColor(Qt::white);
        batch.appendColor(Qt::white);
        batch.appendColor(Qt::white);
        batch.appendIndices(0,1,2);
        batch.appendIndices(2,3,0);
        batch.draw(p,0,6,QGL::Triangles);
        dynamic_cast<QtImage*>(sprite)->GetTexture()->release();
        
        return;
    }
}

QtParticleManager::QtParticleManager( QGLPainter *p, int particles /*= particleLimit*/, int layers /*= 1*/ )
    : TLFX::ParticleManager(particles, layers)
    , _lastTexture(0)
    , _lastAdditive(true), _globalBlend(FromEffectBlendMode)
    , _p(p)
{
}

static void __build_tiles(QSize grid_size, unsigned int total_frames, 
                          QPointF tex_origin=QPointF(0,0), QSizeF tex_size=QSizeF(1,1)) 
{
    QVector<QRectF> frames;
    for(int fr=0; fr<grid_size.height() /* rows */; fr++)
        for(int fc=0; fc<grid_size.width() /* cols */; fc++) {
            const float cw = tex_size.width()/grid_size.width(), ch = tex_size.height()/grid_size.height();
            frames.push_back(QRectF(tex_origin.x()+fc*cw, tex_origin.y()+fr*ch, cw, ch));
            if (frames.size() == total_frames) break;
        }
}
	
void QtParticleManager::DrawSprite( TLFX::Particle *p, TLFX::AnimImage* sprite, float px, float py, float frame, float x, float y, float rotation, float scaleX, float scaleY, unsigned char r, unsigned char g, unsigned char b, float a , bool additive )
{
    #define qFF(C) C*(255.999)

    quint8 alpha = qFF(a);
    if (alpha == 0 || scaleX == 0 || scaleY == 0) return;

    if ((_lastTexture && dynamic_cast<QtImage*>(sprite)->GetTexture()->textureId() != _lastTexture->textureId()) 
        || (additive != _lastAdditive))
        Flush();

    QRectF rc = dynamic_cast<QtImage*>(sprite)->GetTexture()->normalizedTextureSubRect();
    // calculate frame position in atlas:
    if (sprite && sprite->GetFramesCount() > 1)
    {
        const int anim_size = powf(2, ceilf(log2f(sprite->GetFramesCount())));
        const int anim_square = sqrtf(anim_size);
        const int anim_frame = floorf(frame);
        if(anim_frame >= sprite->GetFramesCount()) {
            qWarning() << "[QtParticleManager] Out of range:" << frame << anim_frame << "frames:" << sprite->GetFramesCount();
        }
        const int gr = anim_frame / anim_square, gc = anim_frame % anim_square;
        const float cw = rc.width()/anim_square, ch = rc.height()/anim_square;
        rc = QRectF(rc.x()+gc*cw, rc.y()+gr*ch, cw, ch);
        //qDebug() << sprite->GetFilename() << anim_frame << dynamic_cast<QtImage*>(sprite)->GetTexture()->normalizedTextureSubRect() << rc;
    }

    //uvs[index + 0] = {0, 0};
    batch.appendTexCoord(QVector2D(rc.x(), rc.y()));
    //uvs[index + 1] = {1.0f, 0}
    batch.appendTexCoord(QVector2D(rc.x()+rc.width(), rc.y()));
    //uvs[index + 2] = {1.0f, 1.0f};
    batch.appendTexCoord(QVector2D(rc.x()+rc.width(), rc.y()+rc.height()));
    //uvs[index + 3] = {0, 1.0f};
    batch.appendTexCoord(QVector2D(rc.x(), rc.y()+rc.height()));

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

    _lastTexture = dynamic_cast<QtImage*>(sprite)->GetTexture();
    switch(_globalBlend)
    {
        case FromEffectBlendMode: _lastAdditive = additive; break;
        case AddBlendMode: _lastAdditive = true; break;
        case AlphaBlendMode: _lastAdditive = false; break;
    }
}

void QtParticleManager::Flush()
{
    if (batch.count())
    {
        Q_ASSERT(_p);

        glDisable( GL_DEPTH );
        glEnable( GL_BLEND );
        glDisable( GL_ALPHA_TEST );
        if (_lastTexture) {
            glEnable( GL_TEXTURE_2D );
            _lastTexture->bind();
        } else
            glDisable( GL_TEXTURE_2D );
        if (_lastAdditive) {
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
        if(_lastTexture)
            _lastTexture->release();
        batch = QGeometryData(); // clear batch data
    }
}


// Image utilities:
// ----------------


struct KIEImgEdit
{
    QImage& img;
    QVector <QRgb> colors;
    unsigned int*  data;
    unsigned int   pixels;

    KIEImgEdit(QImage& _img):img(_img)
    {
	if (img.depth() > 8)
        {
            //Code using data and pixels assumes that the pixels are stored
            //in 32bit values and that the image is not premultiplied
            if ((img.format() != QImage::Format_ARGB32) &&
                (img.format() != QImage::Format_RGB32))
            {
                img = img.convertToFormat(QImage::Format_ARGB32);
            }
            data   = (unsigned int*)img.bits();
	    pixels = img.width()*img.height();
	}
	else
	{
	    pixels = img.colorCount();
	    colors = img.colorTable();
	    data   = (unsigned int*)colors.data();
	}
    }

    ~KIEImgEdit()
    {
	if (img.depth() <= 8)
	    img.setColorTable(colors);
    }
};


static void __toGray(QImage &img, float value)
{
    if(value == 0.0)
        return;

    KIEImgEdit ii(img);
    QRgb *data = ii.data;
    QRgb *end = data + ii.pixels;

    unsigned char gray;
    if(value == 1.0){
        while(data != end){
            gray = qGray(*data);
            *data = qRgba(gray, gray, gray, qAlpha(*data));
            ++data;
        }
    }
    else{
        unsigned char val = (unsigned char)(255.0*value);
        while(data != end){
            gray = qGray(*data);
            *data = qRgba((val*gray+(0xFF-val)*qRed(*data)) >> 8,
                          (val*gray+(0xFF-val)*qGreen(*data)) >> 8,
                          (val*gray+(0xFF-val)*qBlue(*data)) >> 8,
                          qAlpha(*data));
            ++data;
        }
    }
}

//For Local loc:Int = 0 Until pixmapcopy.capacity Step 4
//    p = pixmapcopy.pixels + loc
//    c = Min((p[0] *.3) + (p[1] *.59) + (p[2] *.11), p[3])
//    p[0] = 255
//    p[1] = p[0]
//    p[2] = p[0]
//    p[3] = c
//Next
static void __toGray2(QImage &img)
{
    KIEImgEdit ii(img);
    QRgb *data = ii.data;
    QRgb *end = data + ii.pixels;

    unsigned char gray;
    while(data != end){
        gray = qMin((qRed(*data)*30+qGreen(*data)*59+qBlue(*data)*11)/100, qAlpha(*data));
        *data = qRgba(255, 255, 255, gray);
        ++data;
    }
}

static void __colorize(QImage &img, const QColor &col, float value)
{
    if(value == 0.0)
        return;

    KIEImgEdit ii(img);
    QRgb *data = ii.data;
    QRgb *end = data + ii.pixels;

    float rcol = col.red(), gcol = col.green(), bcol = col.blue();
    unsigned char red, green, blue, gray;
    unsigned char val = (unsigned char)(255.0*value);
    while(data != end){
        gray = qGray(*data);
        if(gray < 128){
            red = static_cast<unsigned char>(rcol/128*gray);
            green = static_cast<unsigned char>(gcol/128*gray);
            blue = static_cast<unsigned char>(bcol/128*gray);
        }
        else if(gray > 128){
            red = static_cast<unsigned char>((gray-128)*(2-rcol/128)+rcol-1);
            green = static_cast<unsigned char>((gray-128)*(2-gcol/128)+gcol-1);
            blue = static_cast<unsigned char>((gray-128)*(2-bcol/128)+bcol-1);
        }
        else{
            red = static_cast<unsigned char>(rcol);
            green = static_cast<unsigned char>(gcol);
            blue = static_cast<unsigned char>(bcol);
        }

        *data = qRgba((val*red+(0xFF-val)*qRed(*data)) >> 8,
                      (val*green+(0xFF-val)*qGreen(*data)) >> 8,
                      (val*blue+(0xFF-val)*qBlue(*data)) >> 8,
                      qAlpha(*data));
        ++data;
    }
}

static void __toMonochrome(QImage &img, const QColor &black,
                               const QColor &white, float value)
{
    if(value == 0.0)
        return;

    KIEImgEdit ii(img);
    QRgb *data = ii.data;
    QRgb *end = data + ii.pixels;

    // Step 1: determine the average brightness
    double values = 0.0, sum = 0.0;
    bool grayscale = true;
    while(data != end){
        sum += qGray(*data)*qAlpha(*data) + 255*(255-qAlpha(*data));
        values += 255;
        if((qRed(*data) != qGreen(*data) ) || (qGreen(*data) != qBlue(*data)))
            grayscale = false;
        ++data;
    }
    double medium = sum/values;

    // Step 2: Modify the image
    unsigned char val = (unsigned char)(255.0*value);
    int rw = white.red(), gw = white.green(), bw = white.blue();
    int rb = black.red(), gb = black.green(), bb = black.blue();
    data = ii.data;

    if(grayscale){
        while(data != end){
            if(qRed(*data) <= medium)
                *data = qRgba((val*rb+(0xFF-val)*qRed(*data)) >> 8,
                              (val*gb+(0xFF-val)*qGreen(*data)) >> 8,
                              (val*bb+(0xFF-val)*qBlue(*data)) >> 8,
                              qAlpha(*data));
            else
                *data = qRgba((val*rw+(0xFF-val)*qRed(*data)) >> 8,
                              (val*gw+(0xFF-val)*qGreen(*data)) >> 8,
                              (val*bw+(0xFF-val)*qBlue(*data)) >> 8,
                              qAlpha(*data));
            ++data;
        }
    }
    else{
        while(data != end){
            if(qGray(*data) <= medium) 
                *data = qRgba((val*rb+(0xFF-val)*qRed(*data)) >> 8,
                              (val*gb+(0xFF-val)*qGreen(*data)) >> 8,
                              (val*bb+(0xFF-val)*qBlue(*data)) >> 8,
                              qAlpha(*data));
            else
                *data = qRgba((val*rw+(0xFF-val)*qRed(*data)) >> 8,
                              (val*gw+(0xFF-val)*qGreen(*data)) >> 8,
                              (val*bw+(0xFF-val)*qBlue(*data)) >> 8,
                              qAlpha(*data));
            ++data;
        }
    }
}

static void __deSaturate(QImage &img, float value)
{
    if(value == 0.0)
        return;

    KIEImgEdit ii(img);
    QRgb *data = ii.data;
    QRgb *end = data + ii.pixels;

    QColor color;
    int h, s, v;
    while(data != end){
        color.setRgb(*data);
        color.getHsv(&h, &s, &v);
        color.setHsv(h, (int) (s * (1.0 - value) + 0.5), v);
	*data = qRgba(color.red(), color.green(), color.blue(),
                      qAlpha(*data));
        ++data;
    }
}

static void __toGamma(QImage &img, float value)
{
    KIEImgEdit ii(img);
    QRgb *data = ii.data;
    QRgb *end = data + ii.pixels;

    float gamma = 1/(2*value+0.5);
    while(data != end){
        *data = qRgba(static_cast<unsigned char>
                      (pow(static_cast<float>(qRed(*data))/255 , gamma)*255),
                      static_cast<unsigned char>
                      (pow(static_cast<float>(qGreen(*data))/255 , gamma)*255),
                      static_cast<unsigned char>
                      (pow(static_cast<float>(qBlue(*data))/255 , gamma)*255),
                      qAlpha(*data));
        ++data;
    }
}


static bool painterSupportsAntialiasing()
{
#ifdef QT_WIDGETS_LIB
   QPaintEngine* const pe = QApplication::desktop()->paintEngine();
   if (pe)
	   return pe && pe->hasFeature(QPaintEngine::Antialiasing);
	else
	   // apparently QApplication::desktop()->paintEngine() is null on windows
	   // but we can assume the paint engine supports antialiasing there, right?
	   return true;
#else
	return true;
#endif
}
static void __semiTransparent(QImage &img)
{
    int x, y;
    if(img.depth() == 32){
        if(img.format() == QImage::Format_ARGB32_Premultiplied)
            img = img.convertToFormat(QImage::Format_ARGB32);
        int width  = img.width();
	int height = img.height();

        if(painterSupportsAntialiasing()){
            unsigned char *line;
            for(y=0; y<height; ++y){
                if(QSysInfo::ByteOrder == QSysInfo::BigEndian)
                    line = img.scanLine(y);
                else
                    line = img.scanLine(y) + 3;
                for(x=0; x<width; ++x){
                    *line >>= 1;
                    line += 4;
                }
            }
        }
        else{
            for(y=0; y<height; ++y){
                QRgb* line = (QRgb*)img.scanLine(y);
                for(x=(y%2); x<width; x+=2)
                    line[x] &= 0x00ffffff;
            }
        }
    }
    else{
        if (img.depth() == 8) {
            if (painterSupportsAntialiasing()) {
                // not running on 8 bit, we can safely install a new colorTable
                QVector<QRgb> colorTable = img.colorTable();
                for (int i = 0; i < colorTable.size(); ++i) {
                    colorTable[i] = (colorTable[i] & 0x00ffffff) | ((colorTable[i] & 0xfe000000) >> 1);
                }
                img.setColorTable(colorTable);
                return;
            }
        }
        // Insert transparent pixel into the clut.
        int transColor = -1;

        // search for a color that is already transparent
        for(x=0; x<img.colorCount(); ++x){
            // try to find already transparent pixel
            if(qAlpha(img.color(x)) < 127){
                transColor = x;
                break;
            }
        }

        // FIXME: image must have transparency
        if(transColor < 0 || transColor >= img.colorCount())
            return;

	img.setColor(transColor, 0);
        unsigned char *line;
        if(img.depth() == 8){
            for(y=0; y<img.height(); ++y){
                line = img.scanLine(y);
                for(x=(y%2); x<img.width(); x+=2)
                    line[x] = transColor;
            }
	}
        else{
            bool setOn = (transColor != 0);
            if(img.format() == QImage::Format_MonoLSB){
                for(y=0; y<img.height(); ++y){
                    line = img.scanLine(y);
                    for(x=(y%2); x<img.width(); x+=2){
                        if(!setOn)
                            *(line + (x >> 3)) &= ~(1 << (x & 7));
                        else
                            *(line + (x >> 3)) |= (1 << (x & 7));
                    }
                }
            }
            else{
                for(y=0; y<img.height(); ++y){
                    line = img.scanLine(y);
                    for(x=(y%2); x<img.width(); x+=2){
                        if(!setOn)
                            *(line + (x >> 3)) &= ~(1 << (7-(x & 7)));
                        else
                            *(line + (x >> 3)) |= (1 << (7-(x & 7)));
                    }
                }
            }
        }
    }
}

static void __semiTransparent(QPixmap &pix)
{
    if (painterSupportsAntialiasing()) {
        QImage img=pix.toImage();
        __semiTransparent(img);
        pix = QPixmap::fromImage(img);
        return;
    }

    QImage img;
    if (!pix.mask().isNull())
      img = pix.mask().toImage();
    else
    {
        img = QImage(pix.size(), QImage::Format_Mono);
        img.fill(1);
    }

    for (int y=0; y<img.height(); y++)
    {
        QRgb* line = (QRgb*)img.scanLine(y);
        QRgb pattern = (y % 2) ? 0x55555555 : 0xaaaaaaaa;
        for (int x=0; x<(img.width()+31)/32; x++)
            line[x] &= pattern;
    }
    QBitmap mask;
    mask = QBitmap::fromImage(img);
    pix.setMask(mask);
}

static QImage __doublePixels(const QImage &src)
{
    int w = src.width();
    int h = src.height();

    QImage dst( w*2, h*2, src.format() );

    if (src.depth() == 1)
    {
        qDebug() << "image depth 1 not supported\n";
        return QImage();
    }

    int x, y;
    if (src.depth() == 32)
    {
	QRgb* l1, *l2;
	for (y=0; y<h; ++y)
	{
	    l1 = (QRgb*)src.scanLine(y);
	    l2 = (QRgb*)dst.scanLine(y*2);
	    for (x=0; x<w; ++x)
	    {
			l2[x*2] = l2[x*2+1] = l1[x];
	    }
	    memcpy(dst.scanLine(y*2+1), l2, dst.bytesPerLine());
	}
    } else
    {
		for (x=0; x<src.colorCount(); ++x)
	    	dst.setColor(x, src.color(x));

		const unsigned char *l1;
		unsigned char *l2;
		for (y=0; y<h; ++y)
		{
		    l1 = src.scanLine(y);
		    l2 = dst.scanLine(y*2);
		    for (x=0; x<w; ++x)
		    {
				l2[x*2] = l1[x];
				l2[x*2+1] = l1[x];
		    }
		    memcpy(dst.scanLine(y*2+1), l2, dst.bytesPerLine());
		}
    }
    return dst;
}

static void __overlay(QImage &src, QImage &overlay)
{
    if (src.depth() != overlay.depth())
    {
		qDebug() << "Image depth src (" << src.depth() << ") != overlay " << "(" << overlay.depth() << ")!\n";
		return;
    }
    if (src.size() != overlay.size())
    {
		qDebug() << "Image size src != overlay\n";
		return;
    }
    if (src.format() == QImage::Format_ARGB32_Premultiplied)
        src = src.convertToFormat(QImage::Format_ARGB32);

    if (overlay.format() == QImage::Format_RGB32)
    {
		qDebug() << "Overlay doesn't have alpha buffer!\n";
		return;
    }
    else if (overlay.format() == QImage::Format_ARGB32_Premultiplied)
        overlay = overlay.convertToFormat(QImage::Format_ARGB32);

    int i, j;

    // We don't do 1 bpp

    if (src.depth() == 1)
    {
		qDebug() << "1bpp not supported!\n";
		return;
    }

    // Overlay at 8 bpp doesn't use alpha blending

    if (src.depth() == 8)
    {
		if (src.colorCount() + overlay.colorCount() > 255)
		{
		    qDebug() << "Too many colors in src + overlay!\n";
		    return;
		}
	
		// Find transparent pixel in overlay
		int trans;
		for (trans=0; trans<overlay.colorCount(); trans++)
		{
		    if (qAlpha(overlay.color(trans)) == 0)
		    {
			qDebug() << "transparent pixel found at " << trans << "\n";
			break;
		    }
		}
		if (trans == overlay.colorCount())
		{
		    qDebug() << "transparent pixel not found!\n";
		    return;
		}
	
		// Merge color tables
		int nc = src.colorCount();
		src.setColorCount(nc + overlay.colorCount());
		for (i=0; i<overlay.colorCount(); ++i)
		{
		    src.setColor(nc+i, overlay.color(i));
		}
	
		// Overwrite nontransparent pixels.
		unsigned char *oline, *sline;
		for (i=0; i<src.height(); ++i)
		{
		    oline = overlay.scanLine(i);
		    sline = src.scanLine(i);
		    for (j=0; j<src.width(); ++j)
		    {
				if (oline[j] != trans)
			    	sline[j] = oline[j]+nc;
		    }
		}
    }

    // Overlay at 32 bpp does use alpha blending

    if (src.depth() == 32)
    {
	QRgb* oline, *sline;
	int r1, g1, b1, a1;
	int r2, g2, b2, a2;

	for (i=0; i<src.height(); ++i)
	{
	    oline = (QRgb*)overlay.scanLine(i);
	    sline = (QRgb*)src.scanLine(i);

	    for (j=0; j<src.width(); ++j)
	    {
		r1 = qRed(oline[j]);
		g1 = qGreen(oline[j]);
		b1 = qBlue(oline[j]);
		a1 = qAlpha(oline[j]);

		r2 = qRed(sline[j]);
		g2 = qGreen(sline[j]);
		b2 = qBlue(sline[j]);
		a2 = qAlpha(sline[j]);

		r2 = (a1 * r1 + (0xff - a1) * r2) >> 8;
		g2 = (a1 * g1 + (0xff - a1) * g2) >> 8;
		b2 = (a1 * b1 + (0xff - a1) * b2) >> 8;
		a2 = qMax(a1, a2);

		sline[j] = qRgba(r2, g2, b2, a2);
	    }
	}
    }

    return;
}
