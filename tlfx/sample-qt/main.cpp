#include <QDebug>
#include <QDirIterator>
#include <QMouseEvent>
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

class Window : public QWindow, protected QOpenGLFunctions
{
	Q_OBJECT
private:
	bool m_done, m_update_pending, m_auto_refresh;
	QOpenGLContext *m_context;
	QOpenGLPaintDevice *m_device;
    
    TLFX::EffectsLibrary *m_effects;
    QtParticleManager *m_pm;
    int m_curr_effect; // currently selected effect

    QGLPainter m_p;
    QSize m_size;
public:
	QPoint cursorPos;
public:
	Window(QWindow *parent = 0) : QWindow(parent)
		, m_update_pending(false)
		, m_auto_refresh(true)
		, m_context(0)
		, m_device(0)
        , m_effects(0), m_pm(0), m_curr_effect(0)
		, m_done(false) {
		setSurfaceType(QWindow::OpenGLSurface);
	}
	~Window() { delete m_device; }
	
	void setAutoRefresh(bool a) { m_auto_refresh = a; }
	
	void render(QPainter *painter) {
		Q_UNUSED(painter);

        m_pm->SetScreenSize(m_size.width(), m_size.height());

        glClearColor(0,1,1,1);
        
        m_pm->Update();

        m_p.begin(this);
        QMatrix4x4 projm;
        projm.ortho(0, m_size.width(), 0, m_size.height(), -10, 10);
        m_p.projectionMatrix() = projm;
        m_pm->DrawParticles();
        m_pm->Flush();
        m_p.disableEffect();
        m_p.end();    
    
        glColor4f(1, 1, 1, 1);
        dbgSetStatusLine(QString("Running effect: %1").arg(m_effects->AllEffects()[m_curr_effect].c_str()).toLatin1().constData());
        dbgFlush();
    }
	
	void initialize() {
		qDebug() << "OpenGL infos with gl functions:";
		qDebug() << "-------------------------------";
		qDebug() << " Renderer:" << (const char*)glGetString(GL_RENDERER);
		qDebug() << " Vendor:" << (const char*)glGetString(GL_VENDOR);
		qDebug() << " OpenGL Version:" << (const char*)glGetString(GL_VERSION);
		qDebug() << " GLSL Version:" << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);

        glEnable( GL_TEXTURE_2D );

        setTitle(QString("%1 (%2)").arg((const char*)glGetString(GL_VERSION)).arg((const char*)glGetString(GL_RENDERER)));

        m_effects = new QtEffectsLibrary();
        m_effects->Load(":/data/particles/data.xml");

        m_pm = new QtParticleManager(&m_p);
        m_pm->SetOrigin(0, 0);

        TLFX::Effect *eff = m_effects->GetEffect("Area Effects/Swirly Balls");
        TLFX::Effect *copy = new TLFX::Effect(*eff, m_pm);

        copy->SetPosition(0, 0);
        m_pm->AddEffect(copy);

        dbgLoadFont();
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
		case Qt::Key_Escape: quit(); break;
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
	surface_format.setAlphaBufferSize( 8 );
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
