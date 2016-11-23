QT += widgets

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

CONFIG += c++11
INCLUDEPATH += ../../ext ..

SOURCES += \
    main.cpp \
    QtEffectsLibrary.cpp \
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
    ../../pugixml/pugixml.cpp

HEADERS += \
    QtEffectsLibrary.h \
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
    ../TLFXXMLLoader.h

include(qgeometrydata/qgeometrydata.pri)
