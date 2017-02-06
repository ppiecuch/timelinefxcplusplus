QT += opengl widgets
CONFIG += debug

DESTDIR = build-$$[QMAKE_SPEC]

CONFIG(debug, debug|release) {
	DIR = dbg
} else {
	DIR = rel
}

OBJECTS_DIR = $$DESTDIR/$$TARGET-$$DIR/obj
MOC_DIR = $$DESTDIR/$$TARGET-$$DIR/moc
RCC_DIR = $$DESTDIR/$$TARGET-$$DIR/qrc
UI_DIR = $$DESTDIR/$$TARGET-$$DIR/ui

ICON = tlfx.icns
RESOURCES += data.qrc

CONFIG += c++11
INCLUDEPATH += ../../ext ..

SOURCES += \
    ../../ext/pugixml.cpp \
    ../../ext/debug_font.cpp \
    ../../ext/vogl_miniz.cpp \
    ../../ext/vogl_miniz_zip.cpp \
    ../TLFXAnimImage.cpp \
    ../TLFXAttributeNode.cpp \
    ../TLFXEffect.cpp \
    ../TLFXEffectsLibrary.cpp \
    ../TLFXEmitter.cpp \
    ../TLFXEmitterArray.cpp \
    ../TLFXEntity.cpp \
    ../TLFXMatrix2.cpp \
    ../TLFXParticle.cpp \
    ../TLFXParticleManager.cpp \
    ../TLFXPugiXMLLoader.cpp \
    ../TLFXVector2.cpp \
    ../TLFXXMLLoader.cpp \
    QtEffectsLibrary.cpp \
    main.cpp

HEADERS += \
    ../../ext/pugixml.hpp \
    ../../ext/debug_font.h \
    ../../ext/qopenglerrorcheck.h \
    ../../ext/vogl_miniz.h \
    ../../ext/vogl_miniz_zip.h \
    ../TLFXAnimImage.h \
    ../TLFXAttributeNode.h \
    ../TLFXEffect.h \
    ../TLFXEffectsLibrary.h \
    ../TLFXEmitter.h \
    ../TLFXEmitterArray.h \
    ../TLFXEntity.h \
    ../TLFXMatrix2.h \
    ../TLFXParticle.h \
    ../TLFXParticleManager.h \
    ../TLFXPugiXMLLoader.h \
    ../TLFXVector2.h \
    ../TLFXXMLLoader.h \
    QtEffectsLibrary.h

include(qgeometry/qgeometry.pri)
include(imageviewer/imageviewer.pri)
