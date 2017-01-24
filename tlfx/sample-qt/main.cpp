#include <QDebug>
#include <QElapsedTimer>
#include <QDirIterator>
#include <QMouseEvent>
#include <QMutex>
#include <QPainter>
#include <QWindow>
#include <QOpenGLContext>
#include <QOpenGLPaintDevice>
#include <QOpenGLFunctions>
#include <QApplication>

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "qgeometry/qglbuilder.h"
#include "qgeometry/qglpainter.h"

#include "QtEffectsLibrary.h"

#include <TLFXEffectsLibrary.h>
#include <TLFXParticleManager.h>
#include <TLFXEffect.h>

#include "debug_font.h"

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
    bool force_blend;
    bool invert;
} bg[Bg_cnt] = {
    {{0, 0, 0, 1}, false, true},
    {{0.99, 0.96, 0.89, 1}, true, false},
    {{1, 1, 1, 1}, true, false}
};

class Window : public QWindow, protected QOpenGLFunctions
{
	Q_OBJECT
private:
	bool m_done, m_update_pending, m_auto_refresh;
	QOpenGLContext *m_context;
	QOpenGLPaintDevice *m_device;
    
    TLFX::EffectsLibrary *m_effects;
    QtParticleManager *m_pm;
    quint32 m_curr_effect; // currently selected effect
    quint32 m_curr_bg; // current background style
    qint32 m_msg_line; // line with blending message

    QMutex guard;
    
    QGLPainter m_p;
    QSize m_size;
    QMatrix4x4 m_projm;
public:
	QPoint cursorPos;
public:
	Window(QWindow *parent = 0) : QWindow(parent)
		, m_update_pending(false)
		, m_auto_refresh(true)
		, m_context(0)
		, m_device(0)
        , m_effects(0), m_pm(0), m_curr_effect(0), m_curr_bg(0)
		, m_done(false) {
		setSurfaceType(QWindow::OpenGLSurface);
	}
	~Window() { delete m_device; }
	
	void setAutoRefresh(bool a) { m_auto_refresh = a; }
	
	void render(QPainter *painter) {
		Q_UNUSED(painter);

        fps.ComputeFPS();
        
        m_pm->SetScreenSize(m_size.width(), m_size.height());

        glClearColor(bg[m_curr_bg].color[0], bg[m_curr_bg].color[1], bg[m_curr_bg].color[2], bg[m_curr_bg].color[3]);
        
        guard.lock();

        m_pm->Update();

        m_p.begin(this);
        m_p.projectionMatrix() = m_projm;
        m_p.setStandardEffect(QGL::VertColorTexture2D);
        m_pm->DrawParticles();
        m_pm->Flush();
        m_p.disableEffect();
        m_p.end();

        guard.unlock();

        dbgSetStatusLine(QString("Running effect: %1 | blending: %2 | FPS:%3")
        .arg(m_effects->AllEffects()[m_curr_effect].c_str())
        .arg(m_pm->IsForceBlend()?"force blend":"effect blend")
        .arg(qRound(fps.GetLastAverage())).toLatin1().constData()
        );
        dbgFlush();
    }
	
	void initialize() {
		qDebug() << "OpenGL infos with gl functions:";
		qDebug() << "-------------------------------";
		qDebug() << " Renderer:" << (const char*)glGetString(GL_RENDERER);
		qDebug() << " Vendor:" << (const char*)glGetString(GL_VENDOR);
		qDebug() << " OpenGL Version:" << (const char*)glGetString(GL_VERSION);
		qDebug() << " GLSL Version:" << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);

        setTitle(QString("Qt %1 - %2 (%3)").arg(QT_VERSION_STR).arg((const char*)glGetString(GL_VERSION)).arg((const char*)glGetString(GL_RENDERER)));

        m_effects = new QtEffectsLibrary();
        m_effects->Load(":/data/particles/data.xml");

        m_pm = new QtParticleManager(&m_p);
        m_pm->SetOrigin(0, 0);

        TLFX::Effect *eff = m_effects->GetEffect(m_effects->AllEffects()[m_curr_effect].c_str());
        TLFX::Effect *copy = new TLFX::Effect(*eff, m_pm);
        copy->SetPosition(0, 0);

        m_pm->AddEffect(copy);

        dbgLoadFont();
        dbgAppendMessage(" >: next effect");
        dbgAppendMessage(" <: previous effect");
        dbgAppendMessage(" b: switch background");
        dbgAppendMessage(" t: toggle foreground");
        dbgAppendMessage(" m: toggle blending mode");
        dbgAppendMessage(" p: toggle pause");
        dbgAppendMessage(" r: restart effect");
        dbgSetPixelRatio(devicePixelRatio());

        dbgSetInvert(bg[m_curr_bg].invert);
        m_pm->SetForceBlend(bg[m_curr_bg].force_blend);
    }
	void update() { renderLater(); }
	void render() {
		if (!m_device) m_device = new QOpenGLPaintDevice;
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		m_device->setSize(size());
		QPainter painter(m_device);
		render(&painter);
	}
	void resizeEvent(QResizeEvent *event) {
        m_size = event->size();
        guard.lock();
        m_projm.setToIdentity();
        m_projm.ortho(0, m_size.width(), m_size.height(), 0, -10, 10);
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
		case Qt::Key_B: 
            ++m_curr_bg %= Bg_cnt; 
            // update default settings for this visual:
            dbgSetInvert(bg[m_curr_bg].invert);
            m_pm->SetForceBlend(bg[m_curr_bg].force_blend);
            break;
		case Qt::Key_M: m_pm->ToggleForceBlend(); break;
		case Qt::Key_P: m_pm->TogglePause(); break;
		case Qt::Key_T: dbgToggleInvert(); break;
		case Qt::Key_R: {
            guard.lock();
            m_pm->Reset();
            TLFX::Effect *eff = m_effects->GetEffect(m_effects->AllEffects()[m_curr_effect].c_str());
            TLFX::Effect *copy = new TLFX::Effect(*eff, m_pm);
            m_pm->AddEffect(copy);
            guard.unlock();
        } break;
        case Qt::Key_Greater: 
        case Qt::Key_Period: {
            ++m_curr_effect;
            if (m_curr_effect == m_effects->AllEffects()[m_curr_effect].size()) 
                m_curr_effect = 0;
            guard.lock();
            m_pm->Reset();
            TLFX::Effect *eff = m_effects->GetEffect(m_effects->AllEffects()[m_curr_effect].c_str());
            TLFX::Effect *copy = new TLFX::Effect(*eff, m_pm);
            m_pm->AddEffect(copy);
            guard.unlock();
        } break;
        case Qt::Key_Less: 
        case Qt::Key_Comma: {
            if (m_curr_effect == 0) 
                m_curr_effect = m_effects->AllEffects()[m_curr_effect].size() - 1;
            else 
                --m_curr_effect;
            guard.lock();
            m_pm->Reset();
            TLFX::Effect *eff = m_effects->GetEffect(m_effects->AllEffects()[m_curr_effect].c_str());
            TLFX::Effect *copy = new TLFX::Effect(*eff, m_pm);
            m_pm->AddEffect(copy);
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

	QSurfaceFormat surface_format = QSurfaceFormat::defaultFormat();
	surface_format.setAlphaBufferSize( 0 );
	surface_format.setDepthBufferSize( 24 );
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

    Window window;
    window.show();
	window.resize(800, 600);

    return app.exec();
}


#include "main.moc"
