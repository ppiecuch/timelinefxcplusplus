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

#include "qgraphicstransform3d.h"

QT_BEGIN_NAMESPACE

/*!
    \class QGraphicsTransform3D
    \brief The QGraphicsTransform3D class is an abstract base class for building 3D transformations.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::graphicsview

    QGraphicsTransform3D lets you create and control advanced transformations
    that can be configured independently using specialized properties.
    Scene nodes have an associated list of transforms, which are applied
    in order, one at a time, to the modelview matrix.  Transformations are
    computed in true 3D space using QMatrix4x4.

    QGraphicsTransform3D is particularly useful for animations. Whereas
    QGLPainter::modelViewMatrix() lets you assign any transform directly,
    there is no direct way to interpolate between two different
    transformations (e.g., when transitioning between two states, each for
    which the item has a different arbitrary transform assigned). Using
    QGraphicsTransform3D you can interpolate the property values of each
    independent transformation. The resulting operation is then combined into a
    single transform which is applied to the modelview matrix during drawing.

    If you want to create your own configurable transformation, you can create
    a subclass of QGraphicsTransform3D (or any or the existing subclasses), and
    reimplement the pure virtual applyTo() function, which takes a pointer to a
    QMatrix4x4. Each operation you would like to apply should be exposed as
    properties (e.g., customTransform->setVerticalShear(2.5)). Inside you
    reimplementation of applyTo(), you can modify the provided transform
    respectively.

    \sa QGraphicsScale3D, QGraphicsRotation3D, QGraphicsTranslation3D
    \sa QGraphicsBillboardTransform
*/

/*!
    \fn QGraphicsTransform3D::QGraphicsTransform3D(QObject *parent)

    Constructs a 3D transformation and attaches it to \a parent.
*/

/*!
    \fn QGraphicsTransform3D::~QGraphicsTransform3D()

    Destroys this 3D transformation.
*/

/*!
    \fn void QGraphicsTransform3D::applyTo(QMatrix4x4 *matrix) const

    Applies the effect of this transformation to the specified
    modelview \a matrix.
*/

/*!
    \fn QGraphicsTransform3D *QGraphicsTransform3D::clone(QObject *parent) const

    Clones a copy of this transformation and attaches the clone to \a parent.
*/

/*!
    \fn void QGraphicsTransform3D::transformChanged()

    Signal that is emitted whenever any of the transformation's
    parameters are changed.
*/



/*!
    \class QGraphicsRotation3D
    \brief The QGraphicsRotation3D class supports arbitrary rotation around an axis in 3D space.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::graphicsview

    \sa QGraphicsTranslation3D, QGraphicsScale3D
*/

/*!
    \qmltype Rotation3D
    \instantiates QGraphicsRotation3D
    \brief The Rotation3D item supports arbitrary rotation around an axis in 3D space.
    \since 4.8
    \ingroup qt3d::qml3d

    Frequently a user will create and item in the 3d world and immediately
    wish to apply a rotation to that item before it is displayed, or,
    optionally, perform an animation on that rotation parameter based on
    user inputs, or other events.  Such an rotation can easily be defined
    in QML using the following code:

    \code
    Item3D {
        id: helicoptor
        mesh:  {source: "bellUH1.3ds"}
        effect: Effect {}
        cullFaces: "CullBackFaces"

        transform: [
                        Rotation3D {
                                id: rotate1
                                angle: 5
                                axis: Qt.vector3d(1, 0, 0)
                        },
                        Rotation3D {
                                id: rotate2
                                angle: 5
                                axis: Qt.vector3d(0, 1, 0)
                        },
                        Rotation3D {
                                id: rotate3
                                angle: 45
                                axis: Qt.vector3d(0, 0, 1)
                        }
                ]

       SequentialAnimation {
            NumberAnimation {target: rotate1; property: "angle"; to : 360.0; duration: 3000; easing:"easeOutQuad" }
        }
    }
    \endcode

    Notice here that we create a list of rotations for the \c transform
    property of the container item.  By doing this we allow rotations
    around each of the axes individually in a manner which is conducive
    to animation and interaction.

    Each of the rotations has an \c axis property which is a QVector3D.
    This vector contains a value for each of the three components
    corresponding to x, y, and z.  In the above example, we first
    rotate by 5 degrees about the x axis, then 5 degrees about the y
    axis, and finally by 45 degrees about the z axis.

    By giving each rotation a unique \c id users can then refer to these
    rotations in the QML source in order to perform rotational animations.

    \sa Translation3D, Scale3D
*/

class QGraphicsRotation3DPrivate
{
public:
    QGraphicsRotation3DPrivate() : axis(0, 0, 1), angle(0) {}

    QVector3D origin;
    QVector3D axis;
    float angle;
};

/*!
    Create a 3D rotation transformation and attach it to \a parent.
*/
QGraphicsRotation3D::QGraphicsRotation3D(QObject *parent)
    : QGraphicsTransform3D(parent)
    , d_ptr(new QGraphicsRotation3DPrivate)
{
}

/*!
    Destroy this 3D rotation transformation.
*/
QGraphicsRotation3D::~QGraphicsRotation3D()
{
}

/*!
    \property QGraphicsRotation3D::origin
    \brief the origin about which to rotate.

    The default value for this property is (0, 0, 0).
*/

/*!
    \qmlproperty vector3D Rotation3D::origin

    The origin about which to rotate.  The default value for this
    property is (0, 0, 0).
*/

QVector3D QGraphicsRotation3D::origin() const
{
    Q_D(const QGraphicsRotation3D);
    return d->origin;
}

void QGraphicsRotation3D::setOrigin(const QVector3D &value)
{
    Q_D(QGraphicsRotation3D);
    if (d->origin != value) {
        d->origin = value;
        emit transformChanged();
        emit originChanged();
    }
}

/*!
    \property QGraphicsRotation3D::angle
    \brief the angle to rotate around the axis, in degrees anti-clockwise.

    The default value for this property is 0.
*/

/*!
    \qmlproperty real Rotation3D::angle

    The angle to rotate around the axis, in degrees anti-clockwise.
    The default value for this property is 0.
*/

float QGraphicsRotation3D::angle() const
{
    Q_D(const QGraphicsRotation3D);
    return d->angle;
}

void QGraphicsRotation3D::setAngle(float value)
{
    Q_D(QGraphicsRotation3D);
    if (d->angle != value) {
        d->angle = value;
        emit transformChanged();
        emit angleChanged();
    }
}

/*!
    \property QGraphicsRotation3D::axis
    \brief the axis to rotate around.

    The default value for this property is (0, 0, 1); i.e. the z-axis.
*/

/*!
    \qmlproperty vector3D Rotation3D::axis

    The axis to rotate around.  The default value for this property
    is (0, 0, 1); i.e. the z-axis.
*/

QVector3D QGraphicsRotation3D::axis() const
{
    Q_D(const QGraphicsRotation3D);
    return d->axis;
}

void QGraphicsRotation3D::setAxis(const QVector3D &value)
{
    Q_D(QGraphicsRotation3D);
    if (d->axis != value) {
        d->axis = value;
        emit transformChanged();
        emit axisChanged();
    }
}

/*!
    \internal
*/
void QGraphicsRotation3D::applyTo(QMatrix4x4 *matrix) const
{
    Q_D(const QGraphicsRotation3D);
    matrix->translate(d->origin);
    matrix->rotate(d->angle, d->axis.x(), d->axis.y(), d->axis.z());
    matrix->translate(-d->origin);
}

/*!
    \internal
*/
QGraphicsTransform3D *QGraphicsRotation3D::clone(QObject *parent) const
{
    Q_D(const QGraphicsRotation3D);
    QGraphicsRotation3D *copy = new QGraphicsRotation3D(parent);
    copy->setOrigin(d->origin);
    copy->setAxis(d->axis);
    copy->setAngle(d->angle);
    return copy;
}

/*!
    \fn void QGraphicsRotation3D::originChanged()

    Signal that is emitted when origin() changes.
*/

/*!
    \fn void QGraphicsRotation3D::angleChanged()

    Signal that is emitted when angle() changes.
*/

/*!
    \fn void QGraphicsRotation3D::axisChanged()

    Signal that is emitted when axis() changes.
*/



/*!
    \class QGraphicsTranslation3D
    \brief The QGraphicsTranslation3D class supports translation of 3D items.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::graphicsview

    QGraphicsTranslation3D is derived directly from QGraphicsTransform, and
    provides a \l translate property to specify the 3D vector to
    apply to incoming co-ordinates.

    The \l progress property can be used to perform animation along a
    translation vector by varying the progress value between 0 and 1.
    Overshoot animations are also possible by setting the progress
    value to something outside this range.  The default progress
    value is 1.

    \sa QGraphicsRotation3D, QGraphicsScale3D
*/

/*!
    \qmltype Translation3D
    \instantiates QGraphicsTranslation3D
    \brief The Translation3D item supports translation of items in 3D.
    \since 4.8
    \ingroup qt3d::qml3d

    3D items in QML are typically positioned directly as follows:

    \code
    Item3D {
        mesh: Mesh { source: "chair.3ds" }
        position: Qt.vector3d(0, 5, 10)
    }
    \endcode

    However, it can sometimes be useful to translate an object along a
    vector under the control of an animation.  The Translate3D
    element can be used for this purpose.  The following example
    translates the object along a straight-line path 5 units to
    the right of its original position, and then back again:

    \code
    Item3D {
        mesh: Mesh { source: "chair.3ds" }
        position: Qt.vector3d(0, 5, 10)
        transform: [
            Translation3D {
                translate: Qt.vector3d(5, 0, 0)
                SequentialAnimation on progress {
                    running: true
                    loops: Animation.Infinite
                    NumberAnimation { to : 1.0; duration: 300 }
                    NumberAnimation { to : 0.0; duration: 300 }
                }
            }
        ]
    }
    \endcode

    \sa Rotation3D, Scale3D
*/

class QGraphicsTranslation3DPrivate
{
public:
    QGraphicsTranslation3DPrivate() : progress(1.0f) {}

    QVector3D translate;
    float progress;
};

/*!
    Constructs a new translation and attaches it to \a parent.
*/
QGraphicsTranslation3D::QGraphicsTranslation3D(QObject *parent)
    : QGraphicsTransform3D(parent), d_ptr(new QGraphicsTranslation3DPrivate)
{
}

/*!
    Destroys this translation.
*/
QGraphicsTranslation3D::~QGraphicsTranslation3D()
{
}

/*!
    \property QGraphicsTranslation3D::translate
    \brief the translation to apply to incoming co-ordinates.

    The default value for this property is (0, 0, 0).
*/

/*!
    \qmlproperty vector3D Translation3D::translate

    The translation to apply to incoming co-ordinates.  The default value
    for this property is (0, 0, 0).
*/

QVector3D QGraphicsTranslation3D::translate() const
{
    Q_D(const QGraphicsTranslation3D);
    return d->translate;
}

void QGraphicsTranslation3D::setTranslate(const QVector3D &value)
{
    Q_D(QGraphicsTranslation3D);
    if (d->translate != value) {
        d->translate = value;
        emit transformChanged();
        emit translateChanged();
    }
}

/*!
    \property QGraphicsTranslation3D::progress
    \brief the progress along the translation vector, from 0 to 1.

    The default value for this property is 1.

    This property can be used to perform animation along a translation
    vector by varying the progress between 0 and 1.  Overshoot animations
    are also possible by setting the value to something outside this range.
*/

/*!
    \qmlproperty real Translation3D::progress

    The progress along the translation vector, from 0 to 1.  The default
    value for this property is 1.

    This property can be used to perform animation along a translation
    vector by varying the progress between 0 and 1.  Overshoot animations
    are also possible by setting the value to something outside this range.
*/

float QGraphicsTranslation3D::progress() const
{
    Q_D(const QGraphicsTranslation3D);
    return d->progress;
}

void QGraphicsTranslation3D::setProgress(float value)
{
    Q_D(QGraphicsTranslation3D);
    if (d->progress != value) {
        d->progress = value;
        emit transformChanged();
        emit progressChanged();
    }
}

/*!
    \internal
*/
void QGraphicsTranslation3D::applyTo(QMatrix4x4 *matrix) const
{
    Q_D(const QGraphicsTranslation3D);
    matrix->translate(d->translate * d->progress);
}

/*!
    \internal
*/
QGraphicsTransform3D *QGraphicsTranslation3D::clone(QObject *parent) const
{
    Q_D(const QGraphicsTranslation3D);
    QGraphicsTranslation3D *copy = new QGraphicsTranslation3D(parent);
    copy->setTranslate(d->translate);
    copy->setProgress(d->progress);
    return copy;
}

/*!
    \fn void QGraphicsTranslation3D::translateChanged()

    Signal that is emitted when translate() changes.
*/

/*!
    \fn void QGraphicsTranslation3D::progressChanged()

    Signal that is emitted when progress() changes.
*/



/*!
    \class QGraphicsScale3D
    \brief The QGraphicsScale3D class supports scaling of items in 3D.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::graphicsview

    \sa QGraphicsRotation3D, QGraphicsTranslation3D
*/

/*!
    \qmltype Scale3D
    \instantiates QGraphicsScale3D
    \brief The Scale3D item supports scaling of items in 3D.
    \since 4.8
    \ingroup qt3d::qml3d

    3D items in QML can have a simple scale applied directly as follows:

    \code
    Item3D {
        mesh: Mesh { source: "chair.3ds" }
        scale: 0.5
    }
    \endcode

    An alternative is to use Scale3D to apply a transform directly
    to an item as part of a sequence of transformations:

    \code
    Item3D {
        mesh: Mesh { source: "chair.3ds" }
        transform: [
            Translation3D { translate: Qt.vector3d(5, 0, 0) },
            Scale3D { scale: 0.5 }
        ]
    }
    \endcode

    This allows the application writer to control exactly when the
    scale occurs relative to other transformations.  In the example
    above, the item is first translated by 5 units along the x-axis,
    and then the co-ordinates are scaled by half.  This is distinct
    from the following example which scales the object to half its
    original size and then translates it by 5 units along the x-axis:

    \code
    Item3D {
        mesh: Mesh { source: "chair.3ds" }
        transform: [
            Scale3D { scale: 0.5 },
            Translation3D { translate: Qt.vector3d(5, 0, 0) }
        ]
    }
    \endcode

    The scale property on the item itself is applied before
    any of the transforms.  So the previous example is equivalent to:

    \code
    Item3D {
        mesh: Mesh { source: "chair.3ds" }
        scale: 0.5
        transform: [
            Translation3D { translate: Qt.vector3d(5, 0, 0) }
        ]
    }
    \endcode

    Scale values can also affect the x, y, and z axes by different amounts
    by using a \c{vector3D} value:

    \code
    Item3D {
        mesh: Mesh { source: "chair.3ds" }
        transform: [
            Scale3D { scale: Qt.vector3d(0.5, 0.2, 1.0) },
            Translation3D { translate: Qt.vector3d(5, 0, 0) }
        ]
    }
    \endcode

    \sa Rotation3D, Translation3D
*/

class QGraphicsScale3DPrivate
{
public:
    QGraphicsScale3DPrivate()
        : scale(1, 1, 1)
        , isIdentityScale(true)
        , isIdentityOrigin(true)
    {}

    QVector3D origin;
    QVector3D scale;
    bool isIdentityScale;
    bool isIdentityOrigin;
};

/*!
    Construct a 3D scale transform and attach it to \a parent.
*/
QGraphicsScale3D::QGraphicsScale3D(QObject *parent)
    : QGraphicsTransform3D(parent), d_ptr(new QGraphicsScale3DPrivate)
{
}

/*!
    Destroy this 3D scale transform.
*/
QGraphicsScale3D::~QGraphicsScale3D()
{
}

/*!
    \property QGraphicsScale3D::origin
    \brief the origin about which to scale.

    The default value for this property is (0, 0, 0).
*/

/*!
    \qmlproperty vector3D Scale3D::origin

    The origin about which to scale.  The default value for this
    property is (0, 0, 0).
*/

QVector3D QGraphicsScale3D::origin() const
{
    Q_D(const QGraphicsScale3D);
    return d->origin;
}

void QGraphicsScale3D::setOrigin(const QVector3D &value)
{
    Q_D(QGraphicsScale3D);

    // Optimise for the common case of setting the origin to 0, 0, 0
    // Also minimise the number of floating point compares required
    bool changed = false;
    QVector3D v = value;

    // Are we about to set to 0, 0, 0 ...?
    // Normalise inbound value & record in bool to save on compares
    bool isSetToZeroOrigin = false;
    if (qFuzzyIsNull(v.x()) && qFuzzyIsNull(v.y()) && qFuzzyIsNull(v.z()))
    {
        v = QVector3D(0, 0, 0);
        isSetToZeroOrigin = true;
    }
    if (!isSetToZeroOrigin)
    {
        if (d->origin != v)
        {
            d->origin = v;
            d->isIdentityOrigin = false;
            changed = true;
        }
    }
    else
    {
        if (!d->isIdentityOrigin)
        {
            d->origin = v;
            d->isIdentityOrigin = true;
            changed = true;
        }
    }
    if (changed)
    {
        emit transformChanged();
        emit originChanged();
    }
}

/*!
    \property QGraphicsScale3D::scale
    \brief the amount with which to scale each component.

    The default value for this property is QVector3D(1, 1, 1).
*/

/*!
    \qmlproperty vector3D Scale3D::scale

    The amount with which to scale each component.  The default value for
    this property is (1, 1, 1).

    This property can be specified as either a vector3D or a single
    floating-point value.  A single floating-point value will set
    the x, y, and z scale components to the same value.  In other words,
    the following two transformations are equivalent:

    \code
    Scale3D { scale: 2 }
    Scale3D { scale: Qt.vector3d(2, 2, 2) }
    \endcode
*/

QVector3D QGraphicsScale3D::scale() const
{
    Q_D(const QGraphicsScale3D);
    return d->scale;
}

void QGraphicsScale3D::setScale(const QVector3D &value)
{
    Q_D(QGraphicsScale3D);

    // Optimise for the common case of setting the scale to 1, 1, 1
    // Also minimise the number of floating point compares required
    bool changed = false;
    QVector3D v = value;

    // Are we about to set to 1, 1, 1 ...?
    // Normalise inbound value & record in bool to save on compares
    bool isSetToIdentity = false;
    if (qFuzzyIsNull(v.x() - 1.0f) && qFuzzyIsNull(v.y() - 1.0f) && qFuzzyIsNull(v.z() - 1.0f))
    {
        v = QVector3D(1, 1, 1);
        isSetToIdentity = true;
    }
    if (!isSetToIdentity)
    {
        if (d->scale != v)
        {
            d->scale = v;
            d->isIdentityScale = false;
            changed = true;
        }
    }
    else
    {
        if (!d->isIdentityScale)
        {
            d->scale = v;
            d->isIdentityScale = true;
            changed = true;
        }
    }
    if (changed)
    {
        emit transformChanged();
        emit scaleChanged();
    }
}

/*!
    \internal
*/
void QGraphicsScale3D::applyTo(QMatrix4x4 *matrix) const
{
    Q_D(const QGraphicsScale3D);
    if (!d->isIdentityScale)
    {
        if (d->isIdentityOrigin)
        {
            matrix->scale(d->scale);
        }
        else
        {
            matrix->translate(d->origin);
            matrix->scale(d->scale);
            matrix->translate(-d->origin);
        }
    }
}

/*!
    \internal
*/
QGraphicsTransform3D *QGraphicsScale3D::clone(QObject *parent) const
{
    Q_D(const QGraphicsScale3D);
    QGraphicsScale3D *copy = new QGraphicsScale3D(parent);
    copy->setOrigin(d->origin);
    copy->setScale(d->scale);
    return copy;
}

/*!
    \fn void QGraphicsScale3D::originChanged()

    Signal that is emitted when origin() changes.
*/

/*!
    \fn void QGraphicsScale3D::scaleChanged()

    Signal that is emitted when scale() changes.
*/



/*!
    \class QGraphicsTransform3D
    \brief The QGraphicsTransform3D class is an abstract base class for building 3D transformations.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::graphicsview

    QGraphicsTransform3D lets you create and control advanced transformations
    that can be configured independently using specialized properties.
    Scene nodes have an associated list of transforms, which are applied
    in order, one at a time, to the modelview matrix.  Transformations are
    computed in true 3D space using QMatrix4x4.

    QGraphicsTransform3D is particularly useful for animations. Whereas
    QGLPainter::modelViewMatrix() lets you assign any transform directly,
    there is no direct way to interpolate between two different
    transformations (e.g., when transitioning between two states, each for
    which the item has a different arbitrary transform assigned). Using
    QGraphicsTransform3D you can interpolate the property values of each
    independent transformation. The resulting operation is then combined into a
    single transform which is applied to the modelview matrix during drawing.

    If you want to create your own configurable transformation, you can create
    a subclass of QGraphicsTransform3D (or any or the existing subclasses), and
    reimplement the pure virtual applyTo() function, which takes a pointer to a
    QMatrix4x4. Each operation you would like to apply should be exposed as
    properties (e.g., customTransform->setVerticalShear(2.5)). Inside you
    reimplementation of applyTo(), you can modify the provided transform
    respectively.

    \sa QGraphicsScale3D, QGraphicsRotation3D, QGraphicsTranslation3D
    \sa QGraphicsBillboardTransform
*/

/*!
    \fn QGraphicsTransform3D::QGraphicsTransform3D(QObject *parent)

    Constructs a 3D transformation and attaches it to \a parent.
*/

/*!
    \fn QGraphicsTransform3D::~QGraphicsTransform3D()

    Destroys this 3D transformation.
*/

/*!
    \fn void QGraphicsTransform3D::applyTo(QMatrix4x4 *matrix) const

    Applies the effect of this transformation to the specified
    modelview \a matrix.
*/

/*!
    \fn QGraphicsTransform3D *QGraphicsTransform3D::clone(QObject *parent) const

    Clones a copy of this transformation and attaches the clone to \a parent.
*/

/*!
    \fn void QGraphicsTransform3D::transformChanged()

    Signal that is emitted whenever any of the transformation's
    parameters are changed.
*/



/*!
    \class QGraphicsBillboardTransform
    \brief The QGraphicsBillboardTransform class implements a transformation that causes objects to face the camera.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::graphicsview

    Sometimes it can be useful to make an object face towards the camera
    no matter what orientation the scene is in.  The common name for
    this technique is "billboarding".

    When applied as a transformation, this class will replace the top-left
    3x3 part of the transformation matrix with the identity.  This has the
    effect of removing the rotation and scale components from the current
    world co-ordinate orientation.
*/

/*!
    \qmltype BillboardTransform
    \instantiates QGraphicsBillboardTransform
    \brief The BillboardTransform item implements a transformation that causes objects to face the camera.
    \since 4.8
    \ingroup qt3d::qml3d

    Sometimes it can be useful to make an object face towards the camera
    no matter what orientation the scene is in.  The common name for
    this technique is "billboarding".

    When applied as a transformation, this class will replace the top-left
    3x3 part of the transformation matrix with the identity.  This has the
    effect of removing the rotation and scale components from the current
    world co-ordinate orientation.  In QML, this can be used as follows
    to orient a pane to point towards the viewer:

    \code
    Item3D {
        mesh: Mesh { source: "pane.obj" }
        position: Qt.vector3d(2, 0, -20)
        transform: BillboardTransform {}
        effect: Effect { texture: "picture.jpg" }
    }
    \endcode

    Because the billboard transformation will strip any further
    alterations to the matrix, it will usually be the last element
    in the \c transform list (transformations are applied to the matrix in
    reverse order of their appearance in \c transform):

    \code
    Item3D {
        mesh: Mesh { source: "pane.obj" }
        position: Qt.vector3d(2, 0, -20)
        transform: [
            Scale3D { scale: 0.5 },
            Rotation3D { angle: 30 },
            BillboardTransform {}
        ]
        effect: Effect { texture: "picture.jpg" }
    }
    \endcode

    The \c scale property is applied to the matrix after \c transform has
    performed the billboard transformation, so the above can also be written
    as follows:

    \code
    Item3D {
        mesh: Mesh { source: "pane.obj" }
        position: Qt.vector3d(2, 0, -20)
        scale: 0.5
        transform: [
            Rotation3D { angle: 30 },
            BillboardTransform {}
        ]
        effect: Effect { texture: "picture.jpg" }
    }
    \endcode

    By default the billboard transform will cause the object to
    face directly at the camera no matter how the world co-ordinate
    system is rotated.  Sometimes the billboard needs to stay at right
    angles to the "ground plane" even if the user's viewpoint is
    elevated.  This behavior can be enabled using the preserveUpVector
    property:

    \code
    Pane {
        position: Qt.vector3d(2, 0, -20)
        transform: BillboardTransform { preserveUpVector: true }
        effect: Effect { texture: "picture.jpg" }
    }
    \endcode
*/

class QGraphicsBillboardTransformPrivate
{
public:
    QGraphicsBillboardTransformPrivate() : preserveUpVector(false) {}

    bool preserveUpVector;
};

/*!
    Construct a billboard transform and attach it to \a parent.
*/
QGraphicsBillboardTransform::QGraphicsBillboardTransform(QObject *parent)
    : QGraphicsTransform3D(parent), d_ptr(new QGraphicsBillboardTransformPrivate)
{
}

/*!
    Destroy this billboard transform.
*/
QGraphicsBillboardTransform::~QGraphicsBillboardTransform()
{
}

/*!
    \property QGraphicsBillboardTransform::preserveUpVector
    \brief true to preserve the up orientation.

    The default value for this property is false, which indicates that
    the object being transformed should always face directly to the camera
    This is also known as a "spherical billboard".

    If the value for this property is true, then the object will have
    its up orientation preserved.  This is also known as a "cylindrical
    billboard".
*/

/*!
    \qmlproperty bool BillboardTransform::preserveUpVector

    This property specifies whether the billboard transform should
    preserve the "up vector" so that objects stay at right angles
    to the ground plane in the scene.

    The default value for this property is false, which indicates that
    the object being transformed should always face directly to the camera
    This is also known as a "spherical billboard".

    If the value for this property is true, then the object will have
    its up orientation preserved.  This is also known as a "cylindrical
    billboard".
*/

bool QGraphicsBillboardTransform::preserveUpVector() const
{
    Q_D(const QGraphicsBillboardTransform);
    return d->preserveUpVector;
}

void QGraphicsBillboardTransform::setPreserveUpVector(bool value)
{
    Q_D(QGraphicsBillboardTransform);
    if (d->preserveUpVector != value) {
        d->preserveUpVector = value;
        emit transformChanged();
        emit preserveUpVectorChanged();
    }
}

/*!
    \internal
*/
void QGraphicsBillboardTransform::applyTo(QMatrix4x4 *matrix) const
{
    Q_D(const QGraphicsBillboardTransform);
    if (!d->preserveUpVector) {
        // Replace the top-left 3x3 of the matrix with the identity.
        // The technique is "Cheating Spherical Billboards", described here:
        // http://www.lighthouse3d.com/opengl/billboarding/index.php?billCheat
        (*matrix)(0, 0) = 1.0f;
        (*matrix)(0, 1) = 0.0f;
        (*matrix)(0, 2) = 0.0f;
        (*matrix)(1, 0) = 0.0f;
        (*matrix)(1, 1) = 1.0f;
        (*matrix)(1, 2) = 0.0f;
        (*matrix)(2, 0) = 0.0f;
        (*matrix)(2, 1) = 0.0f;
        (*matrix)(2, 2) = 1.0f;
    } else {
        // Replace some of the top-left 3x3 of the matrix with the identity,
        // but leave the up vector component in the second column as-is.
        // The technique is "Cheating Cylindrical Billboards", described here:
        // http://www.lighthouse3d.com/opengl/billboarding/index.php?billCheat1
        (*matrix)(0, 0) = 1.0f;
        (*matrix)(0, 2) = 0.0f;
        (*matrix)(1, 0) = 0.0f;
        (*matrix)(1, 2) = 0.0f;
        (*matrix)(2, 0) = 0.0f;
        (*matrix)(2, 2) = 1.0f;
    }
    matrix->optimize();
}

/*!
    \internal
*/
QGraphicsTransform3D *QGraphicsBillboardTransform::clone(QObject *parent) const
{
    Q_D(const QGraphicsBillboardTransform);
    QGraphicsBillboardTransform *copy = new QGraphicsBillboardTransform(parent);
    copy->setPreserveUpVector(d->preserveUpVector);
    return copy;
}

/*!
    \fn void QGraphicsBillboardTransform::preserveUpVectorChanged()

    Signal that is emitted when preserveUpVector() changes.
*/

QT_END_NAMESPACE
