#include <QDebug>
#include <QElapsedTimer>
#include <QDirIterator>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QSettings>
#include <QMutex>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QWindow>
#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <QOpenGLPaintDevice>
#include <QOpenGLFramebufferObject>
#include <QGLFramebufferObject>
#include <QFileDialog>
#include <QApplication>

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "qgeometry/qglbuilder.h"
#include "qgeometry/qglsurface.h"
#include "qgeometry/qglpainter.h"
#include "imageviewer/imageviewer.h"

#include "QtEffectsLibrary.h"

#include <TLFXEffectsLibrary.h>
#include <TLFXParticleManager.h>
#include <TLFXEffect.h>

#include "debug_font.h"

static QPixmap toPixmap(const QImage &im, int x, int y, int width, int height, float buffer_scale = 1);
static void convertFromGLImage(QImage &img, int w, int h, bool alpha_format, bool include_alpha);
static QImage qt_gl_read_framebuffer(const QSize &size, bool alpha_format = true, bool include_alpha = true);
static QImage texture_to_image(const QSize &size, GLuint texture);

// == Qt Window ==

static int const CAPTURED_FRAMES_NUM   = 30;    // 30 captures
static float const AVG_TIME            = 0.5;   // 500 millisecondes

class FPSComputer
{
    float history[CAPTURED_FRAMES_NUM]; 
    int indx; long int total;
    float timestamp, average, last;
    const float step;
    
	QElapsedTimer time_, timer_;
    
public:
    FPSComputer(void)
    : indx(0), total(0)
    , timestamp(0)
    , step(AVG_TIME / CAPTURED_FRAMES_NUM)
    , average(0), last(0)
    {
        time_.start(); timer_.start();
        for (int i = 0; i < CAPTURED_FRAMES_NUM; ++i) history[i] = 0;
    }

    float ComputeFPS()
    {
        const float delta_time = timer_.restart()/1000.;
        const float total_time = time_.elapsed()/1000.;

        float fpsFrame = 1. / delta_time;
        if (total_time - last > step)
        {
            last = total_time;
            ++indx %= CAPTURED_FRAMES_NUM;
            average -= history[indx];
            history[indx] = fpsFrame / CAPTURED_FRAMES_NUM;
            average += history[indx];
            ++total;
        }
        return average;
    }

    float ComputeFPS(float delta_time, float total_time)
    {
        float fpsFrame = 1. / delta_time;
        if (total_time - last > step)
        {
            last = total_time;
            ++indx %= CAPTURED_FRAMES_NUM;
            average -= history[indx];
            history[indx] = fpsFrame / CAPTURED_FRAMES_NUM;
            average += history[indx];
            ++total;
        }
        return average;
    }
    
    float GetLastAverage(void) { return average; }
    long int GetTotalFrames(void) { return total; }
};

static FPSComputer fps;


const int Bg_cnt = 3;
static struct {
    float color[4];
    QtParticleManager::GlobalBlendModeType blend_mode;
    bool invert, checker;
} bg[Bg_cnt] = {
    {{0, 0, 0, 1}, QtParticleManager::FromEffectBlendMode, true, false},
    {{0.99, 0.96, 0.89, 1}, QtParticleManager::AlphaBlendMode, false, true},
    {{1, 1, 1, 1}, QtParticleManager::AlphaBlendMode, false, false}
};

class Window : public QWindow, protected QOpenGLFunctions
{
	Q_OBJECT
private:
	bool m_done, m_update_pending, m_auto_refresh;
	QOpenGLContext *m_context;
	QOpenGLPaintDevice *m_device;
    
    QString m_curr_library;
    
    QtEffectsLibrary *m_effects;
    QtParticleManager *m_pm;
    quint32 m_curr_effect; // currently selected effect
    quint32 m_curr_bg; // current background style
    bool m_draw_grid; // draw additional grid
    qint32 m_msg_line; // line with blending message

    QMutex guard;
    
    QOpenGLFramebufferObject *m_fbo;
    QGLFramebufferObjectSurface *m_surf;
    QGeometryData m_grid;
    QGLPainter m_p;
    QSize m_size;
    QMatrix4x4 m_projm;
public:
	QPoint cursorPos;
public:
	Window(QString library = "", QWindow *parent = 0) : QWindow(parent)
		, m_update_pending(false)
		, m_auto_refresh(true)
		, m_context(0)
		, m_device(0)
        , m_fbo(0), m_surf(0)
        , m_curr_library(library)
        , m_effects(0), m_pm(0), m_curr_effect(0), m_curr_bg(0), m_draw_grid(false)
		, m_done(false) {
		setSurfaceType(QWindow::OpenGLSurface);
        setMinimumSize(QSize(400,200));
	}
	~Window() { delete m_surf; delete m_fbo; delete m_device; }
	
	void setAutoRefresh(bool a) { m_auto_refresh = a; renderLater(); }
	
	void render(QPainter *painter) {
		Q_UNUSED(painter);

        fps.ComputeFPS();
        
        m_pm->SetScreenSize(m_size.width(), m_size.height());
        
        guard.lock();

        m_pm->Update();

        m_p.begin(m_surf);
        m_p.projectionMatrix() = m_projm;
        m_p.setStandardEffect(QGL::VertColorTexture2D);
        glClearColor(0,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT);
        m_pm->DrawParticles();
        m_pm->Flush();
        //m_effects->Debug(&m_p);
        m_p.disableEffect();
        m_p.end();

        glBindTexture(GL_TEXTURE_2D,m_fbo->texture());
        QGeometryData fbo_vert;
        fbo_vert.appendVertex(QVector3D(-1,-1,0));//0
        fbo_vert.appendVertex(QVector3D(1,-1,0)); //1
        fbo_vert.appendVertex(QVector3D(1,1,0));  //2
        fbo_vert.appendVertex(QVector3D(-1,1,0)); //3
        fbo_vert.appendTexCoord(QVector2D(0,0));
        fbo_vert.appendTexCoord(QVector2D(1,0));
        fbo_vert.appendTexCoord(QVector2D(1,1));
        fbo_vert.appendTexCoord(QVector2D(0,1));

        fbo_vert.appendIndices(0,1,2);
        fbo_vert.appendIndices(2,3,0);
        
        m_p.begin(this);
        glClearColor(bg[m_curr_bg].color[0], 
            bg[m_curr_bg].color[1], 
            bg[m_curr_bg].color[2], 
            bg[m_curr_bg].color[3]);
		glClear(GL_COLOR_BUFFER_BIT);

        // draw transparent background grid
        if (m_draw_grid) {
            m_p.projectionMatrix() = QMatrix4x4(); // -1 .. 1
            m_p.setStandardEffect(QGL::FlatPerVertexColor);
            m_grid.draw(&m_p, 0, m_grid.indexCount());
        }

        // draw rendered particles quad
        m_p.projectionMatrix() = QMatrix4x4();
        m_p.setStandardEffect(QGL::FlatReplaceTexture2D);
        glEnable( GL_BLEND );
        // ALPHA_ADD
        glBlendFunc( GL_SRC_ALPHA, GL_ONE );
        fbo_vert.draw(&m_p,0,6,QGL::Triangles);
        m_p.disableEffect();
        m_p.end();
        m_fbo->release();

        guard.unlock();

        dbgSetStatusLine(QString("Running effect: [%1]%2 | blending: %3 | atlas: %4x%5 | FPS:%6")
        .arg(m_effects->AllEffects().size())
        .arg(m_effects->AllEffects().size()?QFileInfo(m_effects->AllEffects()[m_curr_effect].c_str()).fileName():"n/a")
        .arg(m_pm->GlobalBlendModeInfo())
        .arg(m_effects->TextureAtlasSize().width())
        .arg(m_effects->TextureAtlasSize().height())
        .arg(qRound(fps.GetLastAverage())).toLatin1().constData()
        );
        dbgFlush();
    }
	
    void loadCurrLibrary()
    {
        if (m_curr_library.isEmpty())
        {
            if (!m_effects->Load(":/data/particles/data.xml")) // default res. effect
                qWarning() << "Failed to load :/data/particles/data.xml resources.";
        } 
        else if (!m_effects->LoadLibrary(m_curr_library.toUtf8().constData()))
        {
            m_curr_library = ""; // no library loaded
            m_effects->Load(":/data/particles/data.xml"); // try with embedded one
        }
        if(!m_effects->UploadTextures())
            // cannot initialize library
            m_effects->ClearAll();
    }

	void initialize() {
		qDebug() << "OpenGL infos with gl functions:";
		qDebug() << "-------------------------------";
		qDebug() << " Renderer:" << (const char*)glGetString(GL_RENDERER);
		qDebug() << " Vendor:" << (const char*)glGetString(GL_VENDOR);
		qDebug() << " OpenGL Version:" << (const char*)glGetString(GL_VERSION);
		qDebug() << " GLSL Version:" << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);

        setTitle(QString("Qt %1 - %2 (%3)").arg(QT_VERSION_STR).arg((const char*)glGetString(GL_VERSION)).arg((const char*)glGetString(GL_RENDERER)));

        m_surf = new QGLFramebufferObjectSurface;
        ensureFbo();

        m_effects = new QtEffectsLibrary();
        loadCurrLibrary();
        
        m_pm = new QtParticleManager(&m_p);
        m_pm->SetOrigin(0, 0);

        if (m_effects->AllEffects().size())
        {
            TLFX::Effect *eff = m_effects->GetEffect(m_effects->AllEffects()[m_curr_effect].c_str());
            TLFX::Effect *cpy  = new TLFX::Effect(*eff, m_pm);
            cpy->SetPosition(0, 0);

            m_pm->AddEffect(cpy);
        } else
            qWarning() << "No effects found in the library";

        dbgLoadFont();
        dbgAppendMessage(" >: next effect");
        dbgAppendMessage(" <: previous effect");
        dbgAppendMessage(" b: switch background");
        dbgAppendMessage(" g: show grid");
        dbgAppendMessage(" t: toggle foreground");
        dbgAppendMessage(" m: toggle blending mode");
        dbgAppendMessage(" p: toggle pause");
        dbgAppendMessage(" r: restart effect");
        dbgAppendMessage(" s: show texture atlas");
        dbgAppendMessage(" i: switch texture atlas quality");
        dbgAppendMessage(" o: open effect file");
        dbgSetPixelRatio(devicePixelRatio());

        dbgSetInvert(bg[m_curr_bg].invert);
        m_pm->SetGlobalBlendMode(bg[m_curr_bg].blend_mode);
    }
	void update() { renderLater(); }
	void render() {
		if (!m_device) m_device = new QOpenGLPaintDevice;
		m_device->setSize(size());
		QPainter painter(m_device);
		render(&painter);
	}
	void resizeEvent(QResizeEvent *event) {
        m_size = event->size();
        guard.lock();
        m_projm.setToIdentity();
        m_projm.ortho(0, m_size.width(), m_size.height(), 0, -10, 10);
        m_grid = qCheckerQuadPlane(QSize(2,2), QPoint(0,0), QSizeF(m_size.width()/22,m_size.height()/22), QColor(0x80,0x80,0x80,0x80), QColor(0xc0,0xc0,0xc0,0x80));
        guard.unlock();
	}
	void mousePressEvent(QMouseEvent *event) {
		cursorPos = QPoint(event->x(), event->y());
		Qt::KeyboardModifiers modifiers = event->modifiers();
		if (event->buttons() & Qt::LeftButton) { }
	}
	void mouseReleaseEvent(QMouseEvent *event) {
		cursorPos = QPoint(event->x(), event->y());
		Qt::KeyboardModifiers modifiers = event->modifiers();
		if (event->button() == Qt::LeftButton) { }
	}
	void mouseMoveEvent(QMouseEvent *event) {
		cursorPos = QPoint(event->x(), event->y());
	}
	void keyPressEvent(QKeyEvent* event) {
		switch(event->key()) {
        case Qt::Key_Q:
		case Qt::Key_Escape: close(); break;
		case Qt::Key_S: {
            ImageViewer imv;
            imv.openImage(texture_to_image(m_effects->TextureAtlasSize(), m_effects->TextureAtlas()));
            bool auto_refresh = m_auto_refresh;
            setAutoRefresh(false); // pause animation
            imv.exec();
            setAutoRefresh(auto_refresh);
        } break;
		case Qt::Key_I: {
            static int max_atlas_tex_size[] = {512,1024,2048, -1};
            const int area = m_effects->TextureAtlasSize().width()*m_effects->TextureAtlasSize().height();
            int *m=max_atlas_tex_size; while(*++m!=-1) {
                if (*m * *m > area) break;
            }
            if (*m==-1) 
                m=max_atlas_tex_size; // from the beginning
            guard.lock();
            m_effects->ClearAll(QSize(*m, *m));
            // reload library
            loadCurrLibrary();
            TLFX::Effect *eff = m_effects->GetEffect(m_effects->AllEffects()[m_curr_effect].c_str());
            TLFX::Effect *cpy = new TLFX::Effect(*eff, m_pm);
            cpy->SetPosition(0, 0);
            m_pm->Reset();
            m_pm->AddEffect(cpy);
            guard.unlock();
        } break;
		case Qt::Key_G:
            m_draw_grid = !m_draw_grid;
            break;
		case Qt::Key_B:
            ++m_curr_bg %= Bg_cnt; 
            // update default settings for this visual:
            dbgSetInvert(bg[m_curr_bg].invert);
            m_pm->SetGlobalBlendMode(bg[m_curr_bg].blend_mode);
            break;
		case Qt::Key_M: m_pm->ToggleGlobalBlendMode(); break;
		case Qt::Key_P: m_pm->TogglePause(); break;
		case Qt::Key_T: dbgToggleInvert(); break;
		case Qt::Key_O: {
            bool auto_refresh = m_auto_refresh;
            setAutoRefresh(false); // pause animation (prevent open file dialog stucking)
            QSettings settings;
            QString openPath = settings.value("LastOpenPath", QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)).toString();
            QString fileName = QFileDialog::getOpenFileName(0,
                tr("Open Effects"), openPath, tr("Effect Files (*.eff)"));
            if (!fileName.isNull())
            {
                QString path = QFileInfo(fileName).path(); // store path for next time
                settings.setValue("LastOpenPath", path);
                guard.lock();
                m_effects->ClearAll();
                if (m_effects->LoadLibrary(fileName.toUtf8().constData())) {
                    m_curr_library = fileName;
                    m_curr_effect = 0;
                    if (m_effects->AllEffects().size())
                    {
                        TLFX::Effect *eff = m_effects->GetEffect(m_effects->AllEffects()[m_curr_effect].c_str());
                        TLFX::Effect *cpy = new TLFX::Effect(*eff, m_pm);
                        cpy->SetPosition(0, 0);
                        
                        m_pm->Reset();
                        m_pm->AddEffect(cpy);
                    } else
                        qWarning() << "No effects found in the library";
                } else
                        qWarning() << "Failed to load the library" << fileName;
                guard.unlock();
                setAutoRefresh(auto_refresh);
            }
        } break;
		case Qt::Key_R: {
            guard.lock();
            TLFX::Effect *eff = m_effects->GetEffect(m_effects->AllEffects()[m_curr_effect].c_str());
            TLFX::Effect *cpy = new TLFX::Effect(*eff, m_pm);
            m_pm->Reset();
            m_pm->AddEffect(cpy);
            guard.unlock();
        } break;
        case Qt::Key_Greater: 
        case Qt::Key_Period: {
            guard.lock();
            ++m_curr_effect;
            if (m_curr_effect == m_effects->AllEffects()[m_curr_effect].size()) 
                m_curr_effect = 0;
            TLFX::Effect *eff = m_effects->GetEffect(m_effects->AllEffects()[m_curr_effect].c_str());
            TLFX::Effect *cpy = new TLFX::Effect(*eff, m_pm);
            m_pm->Reset();
            m_pm->AddEffect(cpy);
            guard.unlock();
        } break;
        case Qt::Key_Less: 
        case Qt::Key_Comma: {
            guard.lock();
            if (m_curr_effect == 0) 
                m_curr_effect = m_effects->AllEffects()[m_curr_effect].size() - 1;
            else 
                --m_curr_effect;
            TLFX::Effect *eff = m_effects->GetEffect(m_effects->AllEffects()[m_curr_effect].c_str());
            TLFX::Effect *cpy = new TLFX::Effect(*eff, m_pm);
            m_pm->Reset();
            m_pm->AddEffect(cpy);
            guard.unlock();
        } break;
		default: event->ignore();
			break;
		}
	}
	void quit() { m_done = true; }
	bool done() const { return m_done; }
protected:
	void closeEvent(QCloseEvent *event) { quit(); }
	bool event(QEvent *event) {
		switch (event->type()) {
		case QEvent::UpdateRequest:
			m_update_pending = false;
			renderNow();
			return true;
		default:
			return QWindow::event(event);
		}
	}
	void exposeEvent(QExposeEvent *event) {
		Q_UNUSED(event);
		if (isExposed()) renderNow();
	}
    void ensureFbo()
    {
        if (m_fbo && m_fbo->size() != size() * devicePixelRatio()) {
            delete m_fbo;
            m_fbo = 0;
        }
        if (!m_fbo) {
            m_fbo = new QOpenGLFramebufferObject(size() * devicePixelRatio(), QOpenGLFramebufferObject::CombinedDepthStencil);
            m_surf->setFramebufferObject(m_fbo);
        }
    }
public slots:
	void renderLater() {
		if (!m_update_pending) {
			m_update_pending = true;
			QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
		}
	}
	void renderNow() {
		if (!isExposed()) return;
		bool needsInitialize = false;
		if (!m_context) {
			m_context = new QOpenGLContext(this);
			m_context->setFormat(requestedFormat());
			m_context->create();
			needsInitialize = true;
		}
		m_context->makeCurrent(this);
		if (needsInitialize) {
			initializeOpenGLFunctions();
			initialize();
		}
		render();
		m_context->swapBuffers(this);
		if (m_auto_refresh) renderLater();
	}
};



////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {

    QApplication app(argc, argv);
    app.setApplicationName("TLFXSample");
    app.setOrganizationName("KomSoft Oprogramowanie");
    app.setOrganizationDomain("komsoft.ath.cx");

	QSurfaceFormat surface_format = QSurfaceFormat::defaultFormat();
	surface_format.setAlphaBufferSize( 0 );
	surface_format.setDepthBufferSize( 0 );
	// surface_format.setRedBufferSize( 8 );
	// surface_format.setBlueBufferSize( 8 );
	// surface_format.setGreenBufferSize( 8 );
	// surface_format.setOption( QSurfaceFormat::DebugContext );
	// surface_format.setProfile( QSurfaceFormat::NoProfile );
	// surface_format.setRenderableType( QSurfaceFormat::OpenGLES );
	// surface_format.setSamples( 4 );
	// surface_format.setStencilBufferSize( 8 );
	// surface_format.setSwapBehavior( QSurfaceFormat::DefaultSwapBehavior );
	// surface_format.setSwapInterval( 1 );
	// surface_format.setVersion( 2, 0 );
	QSurfaceFormat::setDefaultFormat( surface_format );

    QString library; if (QCoreApplication::arguments().count() > 1) 
        library = QCoreApplication::arguments().at(1);
    Window window(library);
    window.show();
	window.resize(800, 600);

    return app.exec();
}


// Qt image/opengl functions:
static QPixmap toPixmap(const QImage &im, int x, int y, int width, int height, float buffer_scale) {

    int _width = float(im.width())/float(buffer_scale);
    int _height = float(im.height())/float(buffer_scale);

    Q_ASSERT(x < _width);
    Q_ASSERT(y < _height);
    Q_ASSERT(width <= _width);
    Q_ASSERT(height <= height);

    if (buffer_scale == 1)
        return QPixmap::fromImage(im.mirrored(false, true).copy(x, y, width, height));
    else
        return QPixmap::fromImage(im
            .mirrored(false, true)
            .scaled(_width, _height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            .copy(x, y, width, height));
}

static void convertFromGLImage(QImage &img, int w, int h, bool alpha_format, bool include_alpha) {
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        // OpenGL gives RGBA; Qt wants ARGB
        uint *p = (uint*)img.bits();
        uint *end = p + w*h;
        if (alpha_format && include_alpha) {
        while (p < end) {
            uint a = *p << 24;
            *p = (*p >> 8) | a;
            p++;
        }
        } else {
        // This is an old legacy fix for PowerPC based Macs, which
        // we shouldn't remove
        while (p < end) {
            *p = 0xff000000 | (*p>>8);
            ++p;
        }
        }
    } else {
        // OpenGL gives ABGR (i.e. RGBA backwards); Qt wants ARGB
        for (int y = 0; y < h; y++) {
        uint *q = (uint*)img.scanLine(y);
        for (int x=0; x < w; ++x) {
            const uint pixel = *q;
            if (alpha_format && include_alpha) {
            *q = ((pixel << 16) & 0xff0000) | ((pixel >> 16) & 0xff)
                | (pixel & 0xff00ff00);
            } else {
            *q = 0xff000000 | ((pixel << 16) & 0xff0000)
                | ((pixel >> 16) & 0xff) | (pixel & 0x00ff00);
            }
            
            q++;
        }
        }
        
    }
    img = img.mirrored();
}

static QImage qt_gl_read_framebuffer(const QSize &size, bool alpha_format, bool include_alpha) {
    QImage img(size, (alpha_format && include_alpha) ? QImage::Format_ARGB32
                : QImage::Format_RGB32);
    int w = size.width();
    int h = size.height();
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
    convertFromGLImage(img, w, h, alpha_format, include_alpha);
    return img;
}

//
static QImage texture_to_image(const QSize &size, GLuint texture)
{
   const int nWidth = size.width();
   const int nHeight = size.height();

   QGLFramebufferObject *_out_fbo = new QGLFramebufferObject( nWidth, nHeight, GL_TEXTURE_2D ); 
   if( !_out_fbo )
       return QImage();

   _out_fbo->bind( );

   glViewport( 0, 0, nWidth, nHeight );   
   glMatrixMode( GL_PROJECTION );
   glLoadIdentity();  
   glOrtho( 0.0, nWidth, 0.0, nHeight, -1.0, 1.0 );
   glMatrixMode( GL_MODELVIEW );
   glLoadIdentity( );
   glEnable( GL_TEXTURE_2D );
   
   _out_fbo->drawTexture( QPointF(0.0,0.0), texture, GL_TEXTURE_2D );
#if 0
#endif
   _out_fbo->release();
   return _out_fbo->toImage();
}

#include "main.moc"
