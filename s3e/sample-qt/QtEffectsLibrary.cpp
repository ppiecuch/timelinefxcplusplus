
#include "QtEffectsLibrary.h"
#include "TLFXPugiXMLLoader.h"

#include <QOpenGLTexture>
#include <QImage>

#include <stdint.h>
#include <cmath>

#include "qgeometry/qgeometrydata.h"

TLFX::XMLLoader* QtEffectsLibrary::CreateLoader() const
{
    return new TLFX::PugiXMLLoader(0);
}

TLFX::AnimImage* QtEffectsLibrary::CreateImage() const
{
    return new QtImage();
}



bool QtImage::Load( const char *filename )
{
    QOpenGLTexture *texture = new QOpenGLTexture(QImage(filename).mirrored());
    texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
    texture->setMagnificationFilter(QOpenGLTexture::Linear);

    return true;
}

QtImage::QtImage()
    : _texture(NULL)
{

}

QtImage::~QtImage()
{
    if (_texture)
        delete _texture;
}

QOpenGLTexture* QtImage::GetTexture() const
{
    return _texture;
}

QtParticleManager::QtParticleManager( int particles /*= particleLimit*/, int layers /*= 1*/ )
    : TLFX::ParticleManager(particles, layers)
    , _lastSprite(NULL)
    , _lastAdditive(true)
{

}

void QtParticleManager::DrawSprite( TLFX::AnimImage* sprite, float px, float py, float frame, float x, float y, float rotation, float scaleX, float scaleY, unsigned char r, unsigned char g, unsigned char b, float a , bool additive )
{
    Q_ASSERT(frame == 0);

    unsigned char alpha = (unsigned char)(a * 255);
    if (alpha == 0 || scaleX == 0 || scaleY == 0) return;

    if (sprite != _lastSprite || additive != _lastAdditive)
        Flush();

    Batch batch;
    batch.px = px;
    batch.py = py;
    batch.frame = frame;
    batch.x = x;
    batch.y = y;
    batch.rotation = rotation;
    batch.scaleX = scaleX;
    batch.scaleY = scaleY;
    batch.color = QColor(r, g, b, alpha);
    _batch.push_back(batch);

    _lastSprite = sprite;
    _lastAdditive = additive;
}

void QtParticleManager::Flush()
{
    if (!_batch.empty() && _lastSprite)
    {
        int count = _batch.size();
        int count4 = count * 4;

        QGeometryData batch;

        for (auto it = _batch.begin(); it != _batch.end(); ++it)
        {
            //uvs[index + 0] = {0, 0};
            batch.appendTexCoord(QVector2D(0, 0));
            //uvs[index + 1] = {1.0f, 0}
            batch.appendTexCoord(QVector2D(1.0, 0));
            //uvs[index + 2] = {1.0f, 1.0f};
            batch.appendTexCoord(QVector2D(1.0, 1.0));
            //uvs[index + 3] = {0, 1.0f};
            batch.appendTexCoord(QVector2D(0, 1.0));

            /*
            verts[index + 0].x = it->px - it->x * it->scaleX;
            verts[index + 0].y = it->py - it->y * it->scaleY;
            //verts[index + 0].z = 1.0f;
            verts[index + 1].x = verts[index + 0].x + _lastSprite->GetWidth() * it->scaleX;
            verts[index + 1].y = verts[index + 0].y;
            //verts[index + 1].z = 1.0f;
            verts[index + 2].x = verts[index + 1].x;
            verts[index + 2].y = verts[index + 1].y + _lastSprite->GetHeight() * it->scaleY;
            //verts[index + 2].z = 1.0f;
            verts[index + 3].x = verts[index + 0].x;
            verts[index + 3].y = verts[index + 2].y;
            //verts[index + 3].z = 1.0f;
            */

            float x0 = -it->x * it->scaleX;
            float y0 = -it->y * it->scaleY;
            float x1 = x0;
            float y1 = (-it->y + _lastSprite->GetHeight()) * it->scaleY;
            float x2 = (-it->x + _lastSprite->GetWidth()) * it->scaleX;
            float y2 = y1;
            float x3 = x2;
            float y3 = y0;

            float cos = cosf(it->rotation / 180.f * (float)M_PI);
            float sin = sinf(it->rotation / 180.f * (float)M_PI);

            //verts[index + 0] = {it->px + x0 * cos - y0 * sin, it->py + x0 * sin + y0 * cos};
            //verts[index + 0].z = 1.0f;
            batch.appendVertex(QVector3D(it->px + x0 * cos - y0 * sin, it->py + x0 * sin + y0 * cos, 0));
            //verts[index + 1] = {it->px + x1 * cos - y1 * sin, it->py + x1 * sin + y1 * cos};
            //verts[index + 1].z = 1.0f;
            batch.appendVertex(QVector3D(it->px + x1 * cos - y1 * sin, it->py + x1 * sin + y1 * cos, 0));
            //verts[index + 2] = {it->px + x2 * cos - y2 * sin, it->py + x2 * sin + y2 * cos};
            //verts[index + 2].z = 1.0f;
            batch.appendVertex(QVector3D(it->px + x2 * cos - y2 * sin, it->py + x2 * sin + y2 * cos, 0));
            //verts[index + 3] = {it->px + x3 * cos - y3 * sin, it->py + x3 * sin + y3 * cos};
            //verts[index + 3].z = 1.0f;
            batch.appendVertex(QVector3D(it->px + x3 * cos - y3 * sin, it->py + x3 * sin + y3 * cos, 0));

            for (int i = 0; i < 4; ++i) {
                batch.appendColor(QColor(it->color.red(), it->color.green(), it->color.blue(), it->color.alpha()));
                indices[index + i] = index + i;
            }
        }

    	glTexCoordPointer(2, GL_FLOAT, sizeof(QVector3D), vertices->t);
    	glVertexPointer(3, GL_FLOAT, sizeof(QVector3D), vertices->v);
    	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QVector3D), &vertices->c);
    	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glTexture2D(static_cast<QtImage*>(_lastSprite)->GetTexture());
        glDisable(DEPTH_WRITE_DISABLED);
        gl(_lastAdditive ? CIwMaterial::ALPHA_ADD : CIwMaterial::ALPHA_BLEND);

        glDrawArray(IW_GX_QUAD_LIST, indices, count4);

        _batch.clear();
    }
}
