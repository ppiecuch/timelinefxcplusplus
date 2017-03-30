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

#include "qglbuilder.h"
#include "qglbuilder_p.h"
#include "qglmaterial.h"
#include "qglpainter.h"
#include "qlogicalvertex.h"
#include "qgeometrydata.h"
#include "qtriangulator.h"
#include "qvector_utils_p.h"

#include <QDebug>
#include <QVector2d>
#include <QPainterPath>

#include <QtGui/private/qtriangulatingstroker_p.h>
#include <QtGui/private/qbezier_p.h>

QT_BEGIN_NAMESPACE

/*!
    \class QGLBuilder
    \brief The QGLBuilder class constructs geometry for efficient display.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::geometry

    \tableofcontents

    Use a QGLBuilder to build up vertex, index, texture and other data
    during application initialization.  The optimized() function
    returns an optimized geometry which can be efficiently and flexibly
    displayed during frames of rendering.  It is suited to writing loaders
    for 3D models, and for programatically creating geometry.

    \section1 Geometry Building

    QGLBuilder makes the job of getting triangles on the GPU simple.  It
    calculates indices and normals for you, then uploads the data.  While
    it has addQuads() and other functions to deal with quads, all data is
    represented as triangles for portability.

    The simplest way to use QGLBuilder is to send a set of geometry
    values to it using QGeometryData in the constructor:

    \code
    MyView::MyView() : QGLView()
    {
        // in the constructor construct a builder on the stack
        QGLBuilder builder;
        QGeometryData triangle;
        QVector3D a(2, 2, 0);
        QVector3D b(-2, 2, 0);
        QVector3D c(0, -2, 0);
        triangle.appendVertex(a, b, c);

        // When adding geometry, QGLBuilder automatically creates lighting normals
        builder << triangle;

        // obtain the scene from the builder
        m_scene = builder.optimized();
    }
    \endcode

    Then during rendering the scene is used to display the results:
    \code
    MyView::paintGL(QGLPainter *painter)
    {
        Q_FOREACH(QGeometryData gd, m_scene)
            gd->draw(painter);
    }
    \endcode

    QGLBuilder automatically generates index values and normals
    on-the-fly during geometry building.  During building, simply send
    primitives to the builder as a sequence of vertices, and
    vertices that are the same will be referenced by a single index
    automatically.

    Primitives will have standard normals generated automatically
    based on vertex winding.

    Consider the following code for OpenGL to draw a quad with corner
    points A, B, C and D :

    \code
    float vertices[12] =
    {
        -1.0, -1.0, -1.0,   // A
        1.0, -1.0, -1.0,    // B
        1.0, 1.0, 1.0,      // C
        -1.0, 1.0, 1.0      // D
    };
    float normals[12] = { 0.0f };
    for (int i = 0; i < 12; i += 3)
    {
        normals[i] = 0.0;
        normals[i+1] = -sqrt(2.0);
        normals[i+2] = sqrt(2.0);
    }
    GLuint indices[6] = {
        0, 1, 2,     // triangle A-B-C
        0, 2, 3      // triangle A-C-D
    };
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glNormalPointer(3, GL_FLOAT, 0, normals);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, indices);
    \endcode

    With QGLBuilder this code becomes:

    \code
    float vertices[12] =
    {
        -1.0, -1.0, -1.0,   // A
        1.0, -1.0, -1.0,    // B
        1.0, 1.0, 1.0,      // C
        -1.0, 1.0, 1.0      // D
    };
    QGLBuilder quad;
    QGeometryData data;
    data.appendVertexArray(QArray<QVector3D>::fromRawData(
            reinterpret_cast<const QVector3D*>(vertices), 4));
    quad.addQuads(data);
    \endcode

    The data primitive is added to the list, as two triangles, indexed to
    removed the redundant double storage of B & C - just the same as the
    OpenGL code.

    QGLBuilder will also calculate a normal for the quad and apply it
    to the vertices.

    In this trivial example the indices are easily calculated, however
    in more complex geometry it is easy to introduce bugs by trying
    to manually control indices.  Extra work is required to generate,
    track and store the index values correctly.

    Bugs such as trying to index two vertices with different data -
    one with texture data and one without - into one triangle can
    easily result.  The picture becomes more difficult when smoothing
    groups are introduced - see below.

    Using indices is always preferred since it saves space on the GPU,
    and makes the geometry perform faster during application run time.

    \section2 Removing Epsilon Errors

    Where vertices are generated by modelling packages or tools, or
    during computation in code, very frequently rounding errors will
    result in several vertices being generated that are actually
    the same vertex but are separated by tiny amounts.  At best these
    duplications waste space on the GPU but at worst can introduce
    visual artifacts that mar the image displayed.

    Closing paths, generating solids of rotation, or moving model
    sections out and back can all introduce these types of epsilon
    errors, resulting in "cracks" or artifacts on display.

    QGLBuilder's index generation process uses a fuzzy match that
    coalesces all vertex values at a point - even if they are out by
    a tiny amount - and references them with a single index.

    \section2 Lighting Normals and Null Triangles

    QGLBuilder functions calculate lighting normals, when building
    geometry.  This saves the application programmer from having to write
    code to calculate them.  Normals for each triangle (a, b, c) are
    calculated as the QVector3D::normal(a, b, c).

    If lighting normals are explicitly supplied when using QGLBuilder,
    then this calculation is not done.  This may save on build time.

    As an optimization, QGLBuilder skips null triangles, that is ones
    with zero area, where it can.  Such triangles generate no fragments on
    the GPU, and thus do not display but nonetheless can take up space
    and processing power.

    Null triangles can easily occur when calculating vertices results
    in two vertices coinciding, or three vertices lying on the same line.

    This skipping is done using the lighting normals cross-product.  If the
    cross-product is a null vector then the triangle is null.

    When lighting normals are specified explicitly the skipping
    optimization is suppressed, so if for some reason null triangles are
    required to be retained, then specify normals for each logical vertex.

    See the documentation below of the individual addTriangle() and other
    functions for more details.

    \section2 Raw Triangle Mode

    Where generation of indices and normals is not needed - for example if
    porting an existing application, it is possible to do a raw import of
    triangle data, without using any of QGLBuilder's processing.

    To do this ensure that indices are placed in the QGeometryData passed to
    the addTriangles() function, and this will trigger \b{raw triangle} mode.

    When adding triangles in this way ensure that all appropriate values
    have been correctly set, and that the normals, indices and other data
    are correctly calculated, since no checking is done.

    When writing new applications, simply leave construction of normals and
    indices to the QGLBuilder

    \section1 Using Sections

    During initialization of the QGLBuilder, while accumulating
    geometry, the geometry data in a QGLBuilder is placed into
    sections - there must be at least one section.

    Call the newSection() function to create a new section:

    \snippet qt3d/builder/builder.cpp 3

    Here separate sections for the rounded outside cylinder and flat top and
    bottom of the soup can model makes for the appearance of a sharp edge
    between them.  If the sides and top and bottom were in the same section
    QGLBuilder would attempt to average the normals around the edge resulting
    in an unrealistic effect.

    In 3D applications this concept is referred to as
    \l{http://www.google.com/search?smoothing+groups}{smoothing groups}.  Within
    a section (smoothing group) all normals are averaged making it appear
    as one smoothly shaded surface.

    The can has 3 smoothing groups - bottom, top and sides.

    This mesh of a Q is a faceted model - it has 0 smoothing groups:

    \image faceted-q.png

    To create geometry with a faceted appearance call newSection() with
    an argument of QGL::Faceted thus \c{newSection(QGL::Faceted)}.

    Faceted geometry is suitable for small models, where hard edges are
    desired between every face - a dice, gem or geometric solid for example.

    If no section has been created when geometry is added a new section is
    created automatically.  This section will have its smoothing set
    to QGL::Smooth.

    To create a faceted appearance rather than accepting the automatically
    created section the << operator can also be used:

    \code
    QGLBuilder builder;
    QGeometryData triangles;
    triangles.appendVertices(a, b, c);
    builder << QGL::Faceted << triangles;
    \endcode

    \section2 Geometry Data in a Section

    Management of normals and vertices for smoothing, and other data is
    handled automatically by the QGLBuilder instance.

    Within a section, incoming geometry data will be coalesced and
    indices created to reference the fewest possible copies of the vertex
    data.  For example, in smooth geometry all copies of a vertex are
    coalesced into one, and referenced by indices.

    One of the few exceptions to this is the case where texture data forms
    a \e seam and a copy of a vertex must be created to carry the two
    texture coordinates either side of the seam.

    \image texture-seam.png

    Coalescing has the effect of packing geometry data into the
    smallest space possible thus improving cache coherence and performance.

    Again all this is managed automatically by QGLBuilder and all
    that is required is to create smooth or faceted sections, and add
    geometry to them.

    Each QGLSection references a contiguous range of vertices in a
    QGLBuilder.

    \section1 Finalizing

    Once the geometry has been accumulated in the QGLBuilder instance,  the
    optimized() method must be called to retrieve the optimized
    geometry.  This function serves to normalize the geometry and optimize
    it for display.

    \code
    MyView::MyView() : QGLView()
    {
        // in the constructor construct a builder on the stack
        QGLBuilder builder;

        // add geometry as shown above
        builder << triangles;

        // obtain the scene from the builder & take ownership
        m_scene = builder.optimized();
    }

    MyView::~MyView()
    {
        // recover all scene resources
        delete m_scene;
    }
    \endcode

*/

QGLBuilderPrivate::QGLBuilderPrivate(QGLBuilder *parent)
    : currentSection(0)
    , defThreshold(5)
    , q(parent)
{
}

QGLBuilderPrivate::~QGLBuilderPrivate()
{
    qDeleteAll(sections);
}

/*!
    Construct a new empty QGLBuilder.
*/
QGLBuilder::QGLBuilder()
    : dptr(new QGLBuilderPrivate(this))
{
}

/*!
    Destroys this QGLBuilder recovering any resources.
*/
QGLBuilder::~QGLBuilder()
{
    delete dptr;
}

/*!
    Helper function to calculate the normal for and set it on vertices
    in \a i, \a j and \a k in triangle data \a p.  If the triangle in
    data \a p is a null triangle (area == 0) then the function returns
    false, otherwise it returns true.
*/
static inline void setNormals(int i, int j, int k, QGeometryData &p, const QVector3D &n)
{
    p.normal(i) = n;
    p.normal(j) = n;
    p.normal(k) = n;
}

static bool qCalculateNormal(int i, int j, int k, QGeometryData &p, QVector3D *vec = 0)
{
    QVector3D norm;
    QVector3D *n = &norm;
    if (vec)
        n = vec;
    bool nullTriangle = false;
    *n = QVector3D::crossProduct(p.vertexAt(j) - p.vertexAt(i),
                                   p.vertexAt(k) - p.vertexAt(j));
    if (qFskIsNull(n->x()))
        n->setX(0.0f);
    if (qFskIsNull(n->y()))
        n->setY(0.0f);
    if (qFskIsNull(n->z()))
        n->setZ(0.0f);
    if (n->isNull())
    {
        nullTriangle = true;
    }
    else
    {
        setNormals(i, j, k, p, *n);
    }
    return nullTriangle;
}

/*!
    \internal
    Helper function to actually add the vertices to geometry.
*/
void QGLBuilderPrivate::addTriangle(int i, int j, int k,
                                    const QGeometryData &p, int &count)
{
    if (currentSection == 0)
        q->newSection();
    QLogicalVertex a(p, i);
    QLogicalVertex b(p, j);
    QLogicalVertex c(p, k);
    currentSection->append(a, b, c);
    count += 3;
}

/*!
    Add \a triangles - a series of one or more triangles - to this builder.

    The data is broken into groups of 3 vertices, each processed as a triangle.

    If \a triangles has less than 3 vertices this function exits without
    doing anything.  Any vertices at the end of the list under a multiple
    of 3 are ignored.

    If no normals are supplied in \a triangles, a normal is calculated; as
    the cross-product \c{(b - a) x (c - a)}, for each group of 3
    logical vertices \c{a(triangle, i), b(triangle, i+1), c(triangle, i+2)}.

    In the case of a degenerate triangle, where the cross-product is null,
    that triangle is skipped.  Supplying normals suppresses this behaviour
    (and means any degenerate triangles will be added to the geometry).

    \b{Raw Triangle Mode}

    If \a triangles has indices specified then no processing of any kind is
    done and all the geometry is simply dumped in to the builder.

    This \b{raw triangle} mode is for advanced use, and it is assumed that
    the user knows what they are doing, in particular that the indices
    supplied are correct, and normals are supplied and correct.

    Normals are not calculated in raw triangle mode, and skipping of null
    triangles is likewise not performed.  See the section on
    \l{raw-triangle-mode}{raw triangle mode}
    in the class documentation above.

    \sa addQuads(), operator>>()
*/
void QGLBuilder::addTriangles(const QGeometryData &triangles)
{
    if (triangles.count() < 3)
        return;
    if (triangles.indexCount() > 0)
    {
        // raw triangle mode
        if (dptr->currentSection == 0)
            newSection();
        dptr->currentSection->appendGeometry(triangles);
        dptr->currentSection->appendIndices(triangles.indices());
    }
    else
    {
        QGeometryData t = triangles;
        bool calcNormal = !t.hasField(QGL::Normal);
        if (calcNormal)
        {
            QVector3DArray nm(t.count());
            t.appendNormalArray(nm);
        }
        bool skip = false;
        int k = 0;
        for (int i = 0; i < t.count() - 2; i += 3)
        {
            if (calcNormal)
                skip = qCalculateNormal(i, i+1, i+2, t);
            if (!skip)
                dptr->addTriangle(i, i+1, i+2, t, k);
        }
    }
}

/*!
    Add \a quads - a series of one or more quads - to this builder.

    If \a quads has less than four vertices this function exits without
    doing anything.

    One normal per quad is calculated if \a quads does not have normals.
    For this reason quads should have all four vertices in the same plane.
    If the vertices do not lie in the same plane, use addTriangleStrip()
    to add two adjacent triangles instead.

    Since internally \l{geometry-building}{quads are stored as two triangles},
    each quad is actually divided in half into two triangles.

    Degenerate triangles are skipped in the same way as addTriangles().

    \sa addTriangles(), addTriangleStrip()
*/
void QGLBuilder::addQuads(const QGeometryData &quads)
{
    if (quads.count() < 4)
        return;
    QGeometryData q = quads;
    bool calcNormal = !q.hasField(QGL::Normal);
    if (calcNormal)
    {
        QVector3DArray nm(q.count());
        q.appendNormalArray(nm);
    }
    bool skip = false;
    int k = 0;
    QVector3D norm;
    for (int i = 0; i < q.count(); i += 4)
    {
        if (calcNormal)
            skip = qCalculateNormal(i, i+1, i+2, q, &norm);
        if (!skip)
            dptr->addTriangle(i, i+1, i+2, q, k);
        if (skip)
            skip = qCalculateNormal(i, i+2, i+3, q, &norm);
        if (!skip)
        {
            if (calcNormal)
                setNormals(i, i+2, i+3, q, norm);
            dptr->addTriangle(i, i+2, i+3, q, k);
        }
    }
}

/*!
    Adds to this section a set of connected triangles defined by \a fan.

    N triangular faces are generated, where \c{N == fan.count() - 2}. Each
    face contains the 0th vertex in \a fan, followed by the i'th and i+1'th
    vertex - where i takes on the values from 1 to \c{fan.count() - 1}.

    If \a fan has less than three vertices this function exits without
    doing anything.

    This function is similar to the OpenGL mode GL_TRIANGLE_FAN.  It
    generates a number of triangles all sharing one common vertex, which
    is the 0'th vertex of the \a fan.

    Normals are calculated as for addTriangle(), given the above ordering.
    There is no requirement or assumption that all triangles lie in the
    same plane.  Degenerate triangles are skipped in the same way as
    addTriangles().

    \sa addTriangulatedFace()
*/
void QGLBuilder::addTriangleFan(const QGeometryData &fan)
{
    if (fan.count() < 3)
        return;
    QGeometryData f = fan;
    bool calcNormal = !f.hasField(QGL::Normal);
    if (calcNormal)
    {
        QVector3DArray nm(f.count());
        f.appendNormalArray(nm);
    }
    int k = 0;
    bool skip = false;
    for (int i = 1; i < f.count() - 1; ++i)
    {
        if (calcNormal)
            skip = qCalculateNormal(0, i, i+1, f);
        if (!skip)
            dptr->addTriangle(0, i, i+1, f, k);
    }
}

/*!
    Adds to this section a set of connected triangles defined by \a strip.

    N triangular faces are generated, where \c{N == strip.count() - 2}.
    The triangles are generated from vertices 0, 1, & 2, then 2, 1 & 3,
    then 2, 3 & 4, and so on.  In other words every second triangle has
    the first and second vertices switched, as a new triangle is generated
    from each successive set of three vertices.

    If \a strip has less than three vertices this function exits without
    doing anything.

    Normals are calculated as for addTriangle(), given the above ordering.

    This function is very similar to the OpenGL mode GL_TRIANGLE_STRIP.  It
    generates triangles along a strip whose two sides are the even and odd
    vertices.

    \sa addTriangulatedFace()
*/
void QGLBuilder::addTriangleStrip(const QGeometryData &strip)
{
    if (strip.count() < 3)
        return;
    QGeometryData s = strip;
    bool calcNormal = !s.hasField(QGL::Normal);
    if (calcNormal)
    {
        QVector3DArray nm(s.count());
        s.appendNormalArray(nm);
    }
    bool skip = false;
    int k = 0;
    for (int i = 0; i < s.count() - 2; ++i)
    {
        if (i % 2)
        {
            if (calcNormal)
                skip = qCalculateNormal(i+1, i, i+2, s);
            if (!skip)
                dptr->addTriangle(i+1, i, i+2, s, k);
        }
        else
        {
            if (calcNormal)
                skip = qCalculateNormal(i, i+1, i+2, s);
            if (!skip)
                dptr->addTriangle(i, i+1, i+2, s, k);
        }
    }
}

/*!
    Adds to this section a set of quads defined by \a strip.

    If \a strip has less than four vertices this function exits without
    doing anything.

    The first quad is formed from the 0'th, 2'nd, 3'rd and 1'st vertices.
    The second quad is formed from the 2'nd, 4'th, 5'th and 3'rd vertices,
    and so on, as shown in this diagram:

    \image quads.png

    One normal per quad is calculated if \a strip does not have normals.
    For this reason quads should have all four vertices in the same plane.
    If the vertices do not lie in the same plane, use addTriangles() instead.

    Since internally \l{geometry-building}{quads are stored as two triangles},
    each quad is actually divided in half into two triangles.

    Degenerate triangles are skipped in the same way as addTriangles().

    \sa addQuads(), addTriangleStrip()
*/
void QGLBuilder::addQuadStrip(const QGeometryData &strip)
{
    if (strip.count() < 4)
        return;
    QGeometryData s = strip;
    bool calcNormal = !s.hasField(QGL::Normal);
    if (calcNormal)
    {
        QVector3DArray nm(s.count());
        s.appendNormalArray(nm);
    }
    bool skip = false;
    QVector3D norm;
    int k = 0;
    for (int i = 0; i < s.count() - 3; i += 2)
    {
        if (calcNormal)
            skip = qCalculateNormal(i, i+2, i+3, s, &norm);
        if (!skip)
            dptr->addTriangle(i, i+2, i+3, s, k);
        if (skip)
            skip = qCalculateNormal(i, i+3, i+1, s, &norm);
        if (!skip)
        {
            if (calcNormal)
                setNormals(i, i+3, i+1, s, norm);
            dptr->addTriangle(i, i+3, i+1, s, k);
        }
    }
}

/*!
    Adds to this section a polygonal face made of triangular sub-faces,
    defined by \a face.  The 0'th vertex is used for the center, while
    the subsequent vertices form the perimeter of the face, which must
    at minimum be a triangle.

    If \a face has less than four vertices this function exits without
    doing anything.

    This function provides functionality similar to the OpenGL mode GL_POLYGON,
    except it divides the face into sub-faces around a \b{central point}.
    The center and perimeter vertices must lie in the same plane (unlike
    triangle fan).  If they do not normals will be incorrectly calculated.

    \image triangulated-face.png

    Here the sub-faces are shown divided by green lines.  Note how this
    function handles some re-entrant (non-convex) polygons, whereas
    addTriangleFan will not support such polygons.

    If required, the center point can be calculated using the center() function
    of QGeometryData:

    \code
    QGeometryData face;
    face.appendVertex(perimeter.center()); // perimeter is a QGeometryData
    face.appendVertices(perimeter);
    builder.addTriangulatedFace(face);
    \endcode

    N sub-faces are generated where \c{N == face.count() - 2}.

    Each triangular sub-face consists of the center; followed by the \c{i'th}
    and \c{((i + 1) % N)'th} vertex.  The last face generated then is
    \c{(center, face[N - 1], face[0]}, the closing face.  Note that the closing
    face is automatically created, unlike addTriangleFan().

    If no normals are supplied in the vertices of \a face, normals are
    calculated as per addTriangle().  One normal is calculated, since a
    face's vertices lie in the same plane.

    Degenerate triangles are skipped in the same way as addTriangles().

    \sa addTriangleFan(), addTriangles()
*/
void QGLBuilder::addTriangulatedFace(const QGeometryData &face)
{
    if (face.count() < 4)
        return;
    QGeometryData f;
    f.appendGeometry(face);
    int cnt = f.count();
    bool calcNormal = !f.hasField(QGL::Normal);
    if (calcNormal)
    {
        QVector3DArray nm(cnt);
        f.appendNormalArray(nm);
    }
    bool skip = false;
    QVector3D norm;
    int k = 0;
    for (int i = 1; i < cnt; ++i)
    {
        int n = i + 1;
        if (n == cnt)
            n = 1;
        if (calcNormal)
        {
            skip = qCalculateNormal(0, i, n, f);
            if (norm.isNull() && !skip)
            {
                norm = f.normalAt(0);
                for (int i = 0; i < cnt; ++i)
                    f.normal(i) = norm;
            }
        }
        if (!skip)
            dptr->addTriangle(0, i, n, f, k);
    }
}

/*!
    Add a series of quads by 'interleaving' \a top and \a bottom.

    This function behaves like quadStrip(), where the odd-numbered vertices in
    the input primitive are from \a top and the even-numbered vertices from
    \a bottom.

    It is trivial to do extrusions using this function:

    \code
    // create a series of quads for an extruded edge along -Y
    addQuadsInterleaved(topEdge, topEdge.translated(QVector3D(0, -1, 0));
    \endcode

    N quad faces are generated where \c{N == min(top.count(), bottom.count() - 1}.
    If \a top or \a bottom has less than 2 elements, this functions does
    nothing.

    Each face is formed by the \c{i'th} and \c{(i + 1)'th}
    vertices of \a bottom, followed by the \c{(i + 1)'th} and \c{i'th}
    vertices of \a top.

    If the vertices in \a top and \a bottom are the perimeter vertices of
    two polygons then this function can be used to generate quads which form
    the sides of a \l{http://en.wikipedia.org/wiki/Prism_(geometry)}{prism}
    with the polygons as the prisms top and bottom end-faces.

    \image quad-extrude.png

    In the diagram above, the \a top is shown in orange, and the \a bottom in
    dark yellow.  The first generated quad, (a, b, c, d) is generated in
    the order shown by the blue arrow.

    To create such a extruded prismatic solid, complete with top and bottom cap
    polygons, given just the top edge do this:
    \code
    QGeometryData top = buildTopEdge();
    QGeometryData bottom = top.translated(QVector3D(0, 0, -1));
    builder.addQuadsInterleaved(top, bottom);
    builder.addTriangulatedFace(top);
    builder.addTriangulatedFace(bottom.reversed());
    \endcode
    The \a bottom QGeometryData must be \b{reversed} so that the correct
    winding for an outward facing polygon is obtained.
*/
void QGLBuilder::addQuadsInterleaved(const QGeometryData &top,
                                     const QGeometryData &bottom)
{
    if (top.count() < 2 || bottom.count() < 2)
        return;
    QGeometryData zipped = bottom.interleavedWith(top);
    bool calcNormal = !zipped.hasField(QGL::Normal);
    if (calcNormal)
    {
        QVector3DArray nm(zipped.count());
        zipped.appendNormalArray(nm);
    }
    bool skip = false;
    QVector3D norm;
    int k = 0;
    for (int i = 0; i < zipped.count() - 2; i += 2)
    {
        if (calcNormal)
            skip = qCalculateNormal(i, i+2, i+3, zipped, &norm);
        if (!skip)
            dptr->addTriangle(i, i+2, i+3, zipped, k);
        if (skip)
            skip = qCalculateNormal(i, i+3, i+1, zipped, &norm);
        if (!skip)
        {
            if (calcNormal)
                setNormals(i, i+3, i+1, zipped, norm);
            dptr->addTriangle(i, i+3, i+1, zipped, k);
        }
    }
}

/*!
    \fn void QGLBuilder::addPane(QSizeF size)
    Convenience function to create a quad centered on the origin,
    lying in the Z=0 plane, with width (x dimension) and height
    (y dimension) specified by \a size.
*/

/*!
    \fn void QGLBuilder::addPane(float size)
    Convenience method to add a single quad of dimensions \a size wide by
    \a size high in the z = 0 plane, centered on the origin.  The quad has
    texture coordinates of (0, 0) at the bottom left and (1, 1) at the top
    right.  The default value for \a size is 1.0, resulting in a quad
    from QVector3D(-0.5, -0.5, 0.0) to QVector3D(0.5, 0.5, 0.0).
*/

static inline void warnIgnore(int secCount, QGLSection *s, int vertCount, int indexCount,
                              const char *msg)
{
    qWarning("Ignoring section %d (%p) with %d vertices and"
             " %d indexes - %s", secCount, s, vertCount, indexCount, msg);
}

/*!
    Finish the building of this geometry, optimize it for rendering, and return a
    optimized geometry list.

    This function does the following:
    \list
        \li packs all geometry data from sections into optimized instances
        \li returns the list of optimized geometry
    \endlist
*/
QList<QGeometryData> QGLBuilder::optimized()
{
    QGeometryData g;
    QMap<quint32, QGeometryData> geos;
    QMap<QGLSection*, int> offsets;
    for (int i = 0; i < dptr->sections.count(); ++i)
    {
        // pack sections that have the same fields into one geometry
        QGLSection *s = dptr->sections.at(i);
        QGL::IndexArray indices = s->indices();
        int icnt = indices.size();
        int vcnt = s->count();
        if (icnt == 0 || vcnt == 0)
        {
            if (!qgetenv("Q_WARN_EMPTY_MESH").isEmpty())
            {
                if (vcnt == 0)
                    warnIgnore(i, s, vcnt, icnt, "vertex count zero");
                else
                    warnIgnore(i, s, vcnt, icnt, "index count zero");
            }
            continue;
        }
        s->normalizeNormals();
        int sectionOffset = 0;
        int sectionIndexOffset = 0;
        if (geos.contains(s->fields()))
        {
            QGeometryData &gd = geos[s->fields()];
            sectionOffset = gd.count();
            sectionIndexOffset = gd.indexCount();
            offsets.insert(s, sectionIndexOffset);
            gd.appendGeometry(*s);
            for (int i = 0; i < icnt; ++i)
                indices[i] += sectionOffset;
            gd.appendIndices(indices);
        }
        else
        {
            g = QGeometryData(*s);
            geos.insert(s->fields(), g);
        }
    }
    return geos.values();
}

/*!
    Creates a new section with smoothing mode set to \a smooth.  By default
    \a smooth is QGL::Smooth.

    A section must be created before any geometry can be added
    to the builder.  However one is created automatically by addTriangle()
    and the other add functions.
*/
void QGLBuilder::newSection(QGL::Smoothing smooth)
{
    new QGLSection(this, smooth);  // calls addSection
}

void QGLBuilder::addSection(QGLSection *sec)
{
    dptr->currentSection = sec;
    sec->setMapThreshold(dptr->defThreshold);
    dptr->sections.append(sec);
}

/*!
    \internal
    Returns the current section, in which new geometry is being added.
*/
QGLSection *QGLBuilder::currentSection() const
{
    return dptr->currentSection;
}

/*!
    \internal
    Returns a list of the sections of the geometry in this builder.
*/
QList<QGLSection*> QGLBuilder::sections() const
{
    return dptr->sections;
}

/*!
    \internal
    Test function only.
*/
void QGLBuilder::setDefaultThreshold(int t)
{
    dptr->defThreshold = t;
}

/*!
    \relates QGLBuilder
    Convenience operator for creating a new section in \a builder with \a smoothing.

    \code
    // equivalent to builder.newSection(QGL::Faceted)
    builder << QGL::Faceted;
    \endcode
*/
QGLBuilder& operator<<(QGLBuilder& builder, const QGL::Smoothing& smoothing)
{
    builder.newSection(smoothing);
    return builder;
}

/*!
    \relates QGLBuilder
    Convenience operator for adding \a triangles to the \a builder.

    \code
    // equivalent to builder.addTriangles(triangles);
    builder << triangles;
    \endcode
*/
QGLBuilder& operator<<(QGLBuilder& builder, const QGeometryData& triangles)
{
    builder.addTriangles(triangles);
    return builder;
}



/*!
    \internal
    \class QGLSection
    \brief The QGLSection class clusters like geometry in a QGLBuilder.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::geometry

    QGLSection instances partition a QGLBuilder into related sections,
    while the builder is being initialized with geometry data.

    Once the builder is initialized, and geometry building is complete
    the QGLSection instances are destroyed and the data is uploaded to the
    graphics hardware.

    The QGLSection class is a work horse for the QGLBuilder, and it
    takes care of automatically managing vertex data.  As such
    for usual use cases, its functionality will not need to be referenced
    directly.  For low-level access to geometry, QGLSection provides a
    range of accessors to reference geometry data during scene building.

    Within a section, incoming geometry data will be coalesced and
    indexes created to reference the fewest possible copies of the vertex
    data.  For example, in smooth geometry all copies of a vertex are
    coalesced into one, and referenced by indices - except in the case
    where texture data forms a \e seam and a copy must be created to carry
    the two texture coordinates of the seam.

    This is handled automatically by QGLSection, to pack data into the
    smallest space possible thus improving cache coherence and performance.

    All the vertices in a QGLSection are treated with the same
    \l{QGL::Smoothing}{smoothing}, and have the same
    \l{QLogicalVertex::Type}{data types}.

    Each QGLSection references a contiguous range of vertices in a
    QGLBuilder.

    A QGLBuilder instance has the \l{QGLBuilder::newSection()}{newSection()}
    function which creates a new QGLSection to reference its data.  Use this
    to construct new QGLSection instances, or alternatively construct
    a new QGLSection() and pass a non-null QGLBuilder pointer.

    These functions all return QVector values. QVector instances are
    implicitly shared, thus the copies are inexpensive unless a
    non-const function is called on them, triggering a copy-on-write.

    Generally for adding geometry, use append().  This function simply
    calls virtual protected functions appendSmooth() (for smoothed vertices)
    and appendFaceted() (for faceted vertices).  See QGLBuilder for a
    discussion of smoothing.
*/

// allow QVector3D's to be stored in a QMap
inline bool operator<(const QVector3D &a, const QVector3D &b)
{
    if (qFskCompare(a.x(), b.x()))
    {
        if (qFskCompare(a.y(), b.y()))
        {
            if (qFskCompare(a.z(), b.z()))
            {
                return false;  // equal so not less-than
            }
            else
            {
                return a.z() < b.z();
            }
        }
        else
        {
            return a.y() < b.y();
        }
    }
    else
    {
        return a.x() < b.x();
    }
}

static inline bool qSameDirection(const QVector3D &a , const QVector3D &b)
{
    bool res = false;
    if (!a.isNull() && !b.isNull())
    {
        float dot = QVector3D::dotProduct(a, b);
        res = qFskCompare(dot, a.length() * b.length());
    }
    return res;
}

class QGLSectionPrivate
{
public:
    QGLSectionPrivate(const QVector3DArray *ary)
        : index(0)
        , vec_data(ary)
        , it(vec_map.end())
        , map_threshold(5)
        , number_mapped(0)
        , start_ptr(-1)
        , end_ptr(-1)
    {
        normIndices.fill(-1, 32);
    }

    ~QGLSectionPrivate() {}

    bool normalAccumulated(int index, const QVector3D &norm) const
    {
        if (index >= normIndices.size())
            return false;
        int ptr = normIndices.at(index);
        while (ptr != -1)
        {
            int val_ptr = normPtrs.at(ptr);
            //if (normValues.at(val_ptr) == norm)
            if (qSameDirection(normValues.at(val_ptr), norm))
                return true;
            ptr = normPtrs.at(ptr+1);
        }
        return false;
    }

    void accumulateNormal(int index, const QVector3D &norm)
    {
        int new_norm_index = normValues.size();
        normValues.append(norm);
        if (normIndices.size() <= index)
        {
            int old_size = normIndices.size();
            normIndices.extend(32);
            for (int i = old_size; i < normIndices.size(); ++i)
                normIndices[i] = -1;
        }
        int new_norm_ptr = normPtrs.size();
        normPtrs.append(new_norm_index);  // even ptrs point to vector value
        normPtrs.append(-1);              // odd ptrs point to next in normPtr linked list
        if (normIndices.at(index) == -1)
        {
            normIndices[index] = new_norm_ptr;
        }
        else
        {
            int norm_ptr = normIndices.at(index);
            while (normPtrs.at(norm_ptr + 1) != -1)
            {
                norm_ptr = normPtrs.at(norm_ptr + 1);
            }
            normPtrs[norm_ptr+1] = new_norm_ptr;
        }
    }

    void mapVertex(const QVector3D &v, int ix)
    {
        Q_UNUSED(ix);
        Q_UNUSED(v);
        Q_ASSERT(vec_data->at(ix) == v);
        if ((vec_data->size() - number_mapped) > map_threshold)
        {
            int to_map = vec_data->size() - number_mapped;
            QArray<int, 100> shuffle(to_map, -1);
            for (int i = number_mapped, k = 0; i < vec_data->size(); ++i, ++k)
                shuffle[k] = i;
            for (int n = to_map; n > 1; --n)
            {
                int k = qrand() % n;
                int tmp = shuffle[k];
                shuffle[k] = shuffle[n - 1];
                shuffle[n - 1] = tmp;
            }
            for (int i = 0; i < to_map; ++i)
                vec_map.insertMulti(vec_data->at(shuffle.at(i)), shuffle.at(i));
            number_mapped += to_map;
        }
    }

    int nextIndex()
    {
        int result = -1;
        if (end_ptr != -1)
        {
            // first look through the unmapped items
            while (start_ptr <= end_ptr && result == -1)
            {
                // search from the end and beginning, favouring the end - most often
                // its in the last few we added, sometimes in the first ones
                if (qFskCompare(vec_data->at(end_ptr--), target))
                    result = end_ptr+1;
                else if (start_ptr <= end_ptr && qFskCompare(vec_data->at(end_ptr--), target))
                    result = end_ptr+1;
                else if (start_ptr <= end_ptr && qFskCompare(vec_data->at(start_ptr++), target))
                    result = start_ptr-1;
            }
            // if that found nothing, have a look at the map
            if (result == -1)
            {
                start_ptr = -1;
                end_ptr = -1;
                it = vec_map.constEnd();
                if (vec_map.size() > 0)
                    it = vec_map.find(target);
            }
        }
        if (it != vec_map.constEnd())
        {
            // if there was something in the map see if its still on target
            if (qFskCompare(it.key(), target))
            {
                result = it.value();
                ++it;  // increment to find more insertMulti instances
            }
            else
            {
                // not on target - flag that we're done here
                it = vec_map.constEnd();
            }
        }
        return result;
    }

    int findVertex(const QVector3D &v)
    {
        end_ptr = vec_data->size() - 1;   // last one not in QMap
        start_ptr = number_mapped;        // first one not in QMap
        target = v;
        return nextIndex();
    }

    // mapper
    int index;
    QVector3D target;
    const QVector3DArray *vec_data;
    QMap<QVector3D, int> vec_map;
    QMap<int, int> index_map;
    QMap<QVector3D,int>::const_iterator it;
    int map_threshold;   // if more than this is unmapped, do a mapping run
    int number_mapped;    // how many vertices have been mapped
    int start_ptr;
    int end_ptr;

    QArray<int, 32> normIndices;
    QArray<int, 32> normPtrs;
    QArray<QVector3D, 32> normValues;
};

/*!
    \internal
    Construct a new QGLSection on \a builder, and with smoothing \a s.
    By default the smoothing is QGL::Smooth.

    See QGLBuilder for a discussion of smoothing.

    The pointer \a list must be non-null, and in debug mode, unless QT_NO_DEBUG is
    defined, this function will assert if \a list is null.

    The following lines of code have identical effect:
    \code
    QGLSection *s = myDisplayList->newSection(QGL::Faceted);
    QGLSection *s2 = new QGLSection(myDisplayList, QGL::Faceted);
    \endcode
*/
QGLSection::QGLSection(QGLBuilder *builder, QGL::Smoothing s)
    : m_smoothing(s)
    , d(0)
{
    Q_ASSERT(builder);
    enableField(QGL::Position);
    Q_ASSERT(vertexData());
    d = new QGLSectionPrivate(vertexData());
    builder->addSection(this);
}

/*!
    \internal
    Destroy this QGLSection, recovering any resources.
*/
QGLSection::~QGLSection()
{
    delete d;
}

/*!
    \internal
    Reserve capacity for \a amount items.  This may avoid realloc
    overhead when a large number of items will be appended.
*/
void QGLSection::reserve(int amount)
{
    QGeometryData::reserve(amount);
    d->normIndices.reserve(amount);
    d->normPtrs.reserve(amount * 2);
    d->normValues.reserve(amount);
}

/*!
    \internal
    Adds the logical vertices \a a, \a b and \c to this section.  All
    should have the same fields.  This function is exactly equivalent to
    \code
        append(a); append(b); append(c);
    \endcode

    \sa appendSmooth(), appendFaceted()
*/
void QGLSection::append(const QLogicalVertex &a, const QLogicalVertex &b, const QLogicalVertex &c)
{
    Q_ASSERT(a.fields() == b.fields() && b.fields() == c.fields());
    if (!a.hasField(QGL::Normal))
    {
        appendFaceted(a, b, c);
    }
    else
    {
        if (m_smoothing == QGL::Smooth)
            appendSmooth(a, b, c);
        else
            appendFaceted(a, b, c);
    }
}

/*!
    \internal
    Adds the logical vertex \a lv to this section.

    Otherwise, if the \a lv does have a lighting normal; then the
    vertex processing depends on the smoothing property of this section.
    If this section has smoothing QGL::Smooth, then the append will be done
    by calling appendSmooth(); or if this section has smoothing QGL::Faceted,
    then the append will be done by calling appendFaceted().

    \sa appendSmooth(), appendFaceted()
*/
void QGLSection::append(const QLogicalVertex &lv)
{
    if (!lv.hasField(QGL::Normal))
    {
        appendFaceted(lv);
    }
    else
    {
        if (m_smoothing == QGL::Smooth)
            appendSmooth(lv);
        else
            appendFaceted(lv);
    }
}

static bool qCompareByAttributes(const QLogicalVertex &a, const QLogicalVertex &b)
{
    static const quint32 ATTRS_AND_TEXTURES = (0xFFFFFFFF << QGL::TextureCoord0);
    quint32 af = a.fields() & ATTRS_AND_TEXTURES;
    quint32 bf = b.fields() & ATTRS_AND_TEXTURES;
    if (af != bf)
        return false;
    quint32 flds = af | bf;
    const quint32 mask = 0x01;
    flds >>= QGL::TextureCoord0;
    for (int i = QGL::TextureCoord0; flds; ++i, flds >>= 1)
    {
        if (flds & mask)
        {
            QGL::VertexAttribute attr = static_cast<QGL::VertexAttribute>(i);
            if (attr < QGL::CustomVertex0)
            {
                if (!qFskCompare(a.texCoord(attr), b.texCoord(attr)))
                    return false;
            }
            else
            {
                QVariant v1 = a.attribute(attr);
                QVariant v2 = b.attribute(attr);
                if (v1.type() == (QVariant::Type)QMetaType::Float)
                    return qFskCompare(v1.toFloat(), v2.toFloat());
                else if (v1.type() == QVariant::Vector2D)
                    return qFskCompare(qvariant_cast<QVector2D>(v1), qvariant_cast<QVector2D>(v2));
                else if (v1.type() == QVariant::Vector3D)
                    return qFskCompare(qvariant_cast<QVector3D>(v1), qvariant_cast<QVector3D>(v2));
                else
                    return v1 == v2;
            }
        }
    }
    return true;
}

int QGLSection::appendOne(const QLogicalVertex &lv)
{
#ifndef QT_NO_DEBUG_STREAM
    if (count() && lv.fields() != fields())
    {
        qDebug() << "Warning: adding" << lv << "fields:" << lv.fields()
                << "fields do not match existing:" << fields()
                << "create new section first?";
    }
#endif
    int index = appendVertex(lv);
    d->mapVertex(lv.vertex(), index);
    appendIndex(index);
    return index;
}

/*!
    \internal
    Adds the logical vertex \a lv to this section of a builder.

    Two QLogicalVertex instances a and b are treated as being duplicates for
    the purpose of smoothing, if \c{qFuzzyCompare(a.vertex(), b.vertex())} is
    true

    All duplicate occurrences of a vertex are coalesced, that is replaced
    by a GL index referencing the one copy.

    In order to draw \a lv as part of a smooth continuous surface, with
    no distinct edge, duplicates of vertex \a lv are coalesced into one
    (within this section) and the normal for that one set to the average of
    the incoming unique normals.

    The incoming vertex \a lv is not treated as a duplicate if \a lv has
    different texture coordinates or attributes.  This occurs for example
    in the case of a texture seam, where two different texture coordinates
    are required at the same point on the geometry.

    In that case a new duplicate vertex is added to carry the unique
    texture coordinates or attributes.  When new vertex copies are added in
    this way all copies receive the averaged normals.

    Call this function to add the vertices of a smooth face to the section
    of a builder, or use:

    \code
    myDisplayList->newSection(QGLBuilder::Smooth);
    myDisplayList->addTriangle(a, b, c);
    \endcode

    In smooth surfaces, the vertex and its normal is only sent to the
    graphics hardware once (not once per face), thus smooth geometry may
    consume fewer resources.

    \sa appendFaceted(), updateTexCoord(), QGLBuilder::newSection()
*/
void QGLSection::appendSmooth(const QLogicalVertex &lv)
{
    Q_ASSERT(lv.hasField(QGL::Position));
    Q_ASSERT(lv.hasField(QGL::Normal));

    int found_index = d->findVertex(lv.vertex());
    bool coalesce = false;
    if (found_index == -1)
    {
        int newIndex = appendOne(lv);
        d->accumulateNormal(newIndex, lv.normal());
    }
    else
    {
        while (!coalesce && found_index != -1)
        {
            if (qCompareByAttributes(lv, logicalVertexAt(found_index)))
                coalesce = true;
            else
                found_index = d->nextIndex();
        }
        if (!coalesce)  // texture or attributes prevented coalesce
        {
            // new vert to carry tex/attrib data
            d->accumulateNormal(appendOne(lv), lv.normal());
        }
        else
        {
            appendIndex(found_index);
            while (found_index != -1)
            {
                if (!d->normalAccumulated(found_index, lv.normal()))
                {
                    normal(found_index) += lv.normal();
                    d->accumulateNormal(found_index, lv.normal());
                }
                found_index = d->nextIndex();
            }
        }
    }
}


void QGLSection::appendSmooth(const QLogicalVertex &lv, int index)
{
    Q_ASSERT(lv.hasField(QGL::Position));
    Q_ASSERT(lv.hasField(QGL::Normal));

    int found_index = -1;
    QMap<int, int>::const_iterator it = d->index_map.constFind(index);
    if (it != d->index_map.constEnd())
        found_index = it.value();
    if (found_index == -1)
    {
        int newIndex = appendVertex(lv);
        d->index_map.insert(index, newIndex);
        appendIndex(newIndex);
        d->accumulateNormal(newIndex, lv.normal());
    }
    else
    {
        appendIndex(found_index);
        if (!d->normalAccumulated(found_index, lv.normal()))
        {
            normal(found_index) += lv.normal();
            d->accumulateNormal(found_index, lv.normal());
        }
    }
}

/*!
    \internal
    Add the logical vertex \a lv to this section of a builder.

    The vertex will be drawn as a distinct edge, instead of just part of a
    continuous smooth surface.  To acheive this the vertex value of \a lv
    is duplicated for each unique normal in the current section.

    Note that duplication is only for unique normals: if a vertex is already
    present with a given normal it is coalesced and simply referenced by index.
    As for appendSmooth() vertices are not coalesced in this way if \a lv
    has a different texture coordinate or attribute than its duplicate.

    In faceted surfaces, the vertex is sent to the graphics hardware once for
    each normal it has, and thus may consume more resources.

    \sa appendSmooth(), updateTexCoord(), QGLBuilder::newSection()
*/
void QGLSection::appendFaceted(const QLogicalVertex &lv)
{
    Q_ASSERT(lv.hasField(QGL::Position));
    int found_index = d->findVertex(lv.vertex());
    bool coalesce = false;
    while (!coalesce && found_index != -1)
    {
        if (logicalVertexAt(found_index) == lv)
            coalesce = true;
        else
            found_index = d->nextIndex();
    }
    if (coalesce) // found
    {
        appendIndex(found_index);
    }
    else
    {
        appendOne(lv);
    }
}

/*!
    \internal
    Returns the current map threshold for this section.  The threshold is the
    point at which the section switches from using a plain QArray - with
    linear performance ie O(n) - to using a QMap - with approx O(log n).  These
    structures are used for looking up vertices during the index generation and
    normals calculation.

    The default value is 50.

    \sa setMapThreshold()
*/
int QGLSection::mapThreshold() const
{
    return d->map_threshold;
}

/*!
    \internal
    Sets the current map threshold to \a t for this section.

    \sa mapThreshold()
*/
void QGLSection::setMapThreshold(int t)
{
    d->map_threshold = t;
}

#ifndef QT_NO_DEBUG_STREAM
/*!
    \internal
    Output a string representation of \a section to a debug stream \a dbg.
    \relates QGLSection
*/
QDebug operator<<(QDebug dbg, const QGLSection &section)
{
    dbg.space()
            << "QGLSection(" << &section
            << "- count:" << section.count()
            << "- smoothing mode:" << (section.smoothing() == QGL::Smooth ?
                                       "QGL::Smooth" : "QGL::Faceted") << "\n";
    QGL::IndexArray indices = section.indices();
    for (int i = 0; i < section.count(); ++i)
    {
        int ix = indices[i];
        dbg << section.logicalVertexAt(ix) << "\n";
    }
    dbg << ")\n";
    return dbg.space();
}
#endif



/*!
    \function qQuadPlane
    \brief The qQuadPlane constructs a 3D model of a large flat plane.
    When drawing a large flat plane, such as a wall or a floor, it is desirable
    to decompose it into smaller units to make the shading effects look correct.
    This function composes a plane out of a configurable number of quads.

    Construct a new quad plane with \a size, subdivided \a level times.  By default
    the plane is 100.0f x 100.0f, and is subdivided 3 times - that is into an
    8 x 8 grid.

    It is centered on the origin, and lies in the z = 0 plane.
*/
QGeometryData qQuadPlane(QSizeF size, int level)
{
    if (level > 8)
        level = 8;
    if (level < 1)
        level = 1;
    int divisions = 1;
    for ( ; level--; divisions *= 2) {}  // integer 2**n
    QSizeF div = size / float(divisions);
    QSizeF half = size / 2.0f;
    QGLBuilder builder;
    QGeometryData zip, zip2;
    for (int yy = 0; yy <= divisions; ++yy)
    {
        float y = half.height() - float(yy) * div.height();
        float texY = float(yy) / divisions;
        for (int xx = 0; xx <= divisions; ++xx)
        {
            float x = half.width() - float(xx) * div.width();
            float texX = float(xx) / divisions;
            zip.appendVertex(QVector3D(x, y, 0));
            zip.appendTexCoord(QVector2D(1.0f - texX, 1.0f - texY));
        }
        if (yy > 0)
            builder.addQuadsInterleaved(zip, zip2);
        zip2 = zip;
        zip2.detach();
        zip.clear();
    }
    return builder.optimized().at(0);
}


/*!
    \function qCheckerQuadPlane
    \brief The qCheckerQuadPlane constructs a 3D model of a large flat plane.
    When drawing a large flat plane, such as a wall or a floor, it is desirable
    to decompose it into smaller units to make the shading effects look correct.
    This function composes a plane out of a configurable number of quads.

    Construct a new checkered quad plane with \a size, subdivided \a level times. 
    By default the plane is 100.0f x 100.0f, and is subdivided 3 times - that is 
    into an 8 x 8 grid. Default color1 is black and color2 is white.

    It is centered on the origin, and lies in the z = 0 plane.
    
    x---xx---xx---x
    |---||   ||---|
    |---||   ||---|
    x---xx---xx---x
    x---xx---xx---x
    |   ||---||   |
    |   ||---||   |
    x---xx---xx---x
*/
QGeometryData qCheckerQuadPlane(QSizeF size, QPointF origin, int level, QColor color1, QColor color2)
{
    const QColor colors[2] = { color1, color2 }; int colorIndex = 0;
    if (level > 8)
        level = 8;
    if (level < 1)
        level = 1;
    int divisions = 1;
    for ( ; level--; divisions *= 2) {}  // integer 2**n
    QSizeF div = size / float(divisions);
    QSizeF half = size / 2.0f;
    QGLBuilder builder; builder << QGL::NoSmoothing; // we have a few degradeted quads
    QGeometryData zip, zip2;
    for (int yy = 0; yy < divisions; ++yy)
    {
        float y = origin.y() + half.height() - float(yy) * div.height(), y2 = y - div.height();
        float texY = float(yy) / divisions, texY2 = texY + 1./divisions;
        colorIndex = yy%2; for (int xx = 0; xx <= divisions; ++xx)
        {
            float x = origin.x() + half.width() - float(xx) * div.width();
            float texX = float(xx) / divisions;
            zip.appendVertex(QVector3D(x, y, 0));
            zip.appendTexCoord(QVector2D(1.0f - texX, 1.0f - texY));
            zip.appendColor(colors[colorIndex%2]);
            zip2.appendVertex(QVector3D(x, y2, 0));
            zip2.appendTexCoord(QVector2D(1.0f - texX, 1.0f - texY2));
            zip2.appendColor(colors[colorIndex%2]);
            // duplicated if this is not first/last vertex
            if (xx > 0 && xx < divisions) {
                ++colorIndex;
                zip.appendVertex(QVector3D(x, y, 0));
                zip.appendTexCoord(QVector2D(1.0f - texX, 1.0f - texY));
                zip.appendColor(colors[colorIndex%2]);
                zip2.appendVertex(QVector3D(x, y2, 0));
                zip2.appendTexCoord(QVector2D(1.0f - texX, 1.0f - texY2));
                zip2.appendColor(colors[colorIndex%2]);
            }
        }
        builder.addQuadsInterleaved(zip, zip2);
        zip.clear();
        zip2.clear();
    }
    return builder.optimized().at(0);
}


void extrudeCaps(QGeometryData &data,QTriangleSet &set,bool invFace,qreal offset)
{
	int facecount = set.vertices.count()>>1;
	const qreal * vert = set.vertices.data();

	qreal normal=1;
	if (invFace) {
		normal=-1;
	}

	int startindex=data.count();
	data.appendVertexArray(QVector3DArray(facecount));
	data.appendNormalArray(QVector3DArray(facecount));
	for (int i=0;i<facecount;i++) {
		data.vertex(startindex+i)=QVector3D(vert[0],vert[1],offset);
		data.normal(startindex+i)=QVector3D(0,0,normal);
		vert++;
		vert++;
	}
	int *ind=(int *)set.indices.data();

	if (invFace) {
		for (int i=0;i<set.indices.size();i++) {
			data.appendIndex(ind[i]+startindex);
		}
	} else {
		for (int i=set.indices.size()-1;i>-1;i--) {
			data.appendIndex(ind[i]+startindex);
		}
	}
}

void extrudeFacture(QGeometryData &data,QPolylineSet &set,qreal extrude)
{
	int linecount = set.vertices.count()>>1;
	const qreal * vert = set.vertices.data();

	int indexstart=data.count();
	data.appendVertexArray(QVector3DArray(linecount*2));
	data.appendNormalArray(QVector3DArray(linecount*2));

	for (int i=0;i<linecount;i++) {
		data.vertex(indexstart+i)=QVector3D(vert[0],vert[1],-extrude*0.5);
		data.normal(indexstart+i)=QVector3D(0,0,0);
		vert++;
		vert++;
	}
	vert=set.vertices.data();
	for (int i=0;i<linecount;i++) {
		data.vertex(indexstart+linecount+i)=QVector3D(vert[0],vert[1],extrude*0.5);
		data.normal(indexstart+linecount+i)=QVector3D(0,0,0);
		vert++;
		vert++;
	}

	int *ind=(int *)set.indices.data();
	vert=set.vertices.data();
	QVector3D nn;
	int addindex=data.count();

	for (int i=0;i<set.indices.size()-1;i++) {
		if ((ind[i]<0) || (ind[i+1]<0)) continue;
		int id1=ind[i]+indexstart;
		int id10=ind[i+1]+indexstart;
		int id2=ind[i]+indexstart+linecount;
		int id20=ind[i+1]+indexstart+linecount;
		nn=QVector3D(vert[ind[i+1]*2+1]-vert[ind[i]*2+1],vert[ind[i]*2]-vert[ind[i+1]*2],0);
		nn.normalize();
		if (!data.normal(id1).isNull()) {
			if (QVector3D::dotProduct(data.normal(id1),nn)<0.5) {
				QVector3D vec=data.vertexAt(id1);
				data.appendVertex(vec);
				data.appendNormal(QVector3D(0,0,0));
				id1=addindex;
				addindex++;
				vec=data.vertexAt(id2);
				data.appendVertex(vec);
				data.appendNormal(QVector3D(0,0,0));
				id2=addindex;
				addindex++;
			}
		}
		QVector3D &nm1=data.normal(id1);
		QVector3D &nm10=data.normal(id10);
		data.appendIndices(id1,id10,id2);
		data.appendIndices(id10,id20,id2);
		if (!nm1.isNull()) nm1=(nn+nm1)*0.5; else nm1=nn;
		data.normal(id2)=nm1;
		if (!nm10.isNull()) nm10=(nn+nm10)*0.5; else nm10=nn;
		data.normal(id20)=nm10;
	}
}

QGeometryData qExtrude(const qreal *polygon, int count, qreal extrude, Qt::ExtrudeFlags flag, uint hint, const QTransform &matrix)
{
	QGeometryData data;

	QTriangleSet face = qTriangulate(polygon,count,hint,matrix);
	qreal extr=extrude;
	if (extr<=0) extr=0;

	int fasecount=face.vertices.count()>>1;
	const qreal * vert;
	if ((flag & Qt::CapStart)!=0) {
		extrudeCaps(data,face,false,extr*0.5);
	}

	if ((flag & Qt::CapEnd)!=0) {
		extrudeCaps(data,face,true,-extr*0.5);
	}

	if (extr>0) {
		QPolylineSet line = qPolyline(polygon,count,hint,matrix);
		extrudeFacture(data,line,extr);
	}

	return data;
}

QGeometryData qExtrude(const QVectorPath &path, qreal extrude, Qt::ExtrudeFlags flag, const QTransform &matrix, qreal lod)
{
	QGeometryData data;

	QTriangleSet face = qTriangulate(path,matrix,lod);
	qreal extr=extrude;
	if (extr<=0) extr=0;

	int fasecount=face.vertices.count()>>1;
	const qreal * vert;
	if ((flag & Qt::CapStart)!=0) {
		extrudeCaps(data,face,false,extr*0.5);
	}

	if ((flag & Qt::CapEnd)!=0) {
		extrudeCaps(data,face,true,-extr*0.5);
	}

	int *ind;
	if (extr>0) {
		QPolylineSet line = qPolyline(path,matrix,lod);
		extrudeFacture(data,line,extr);
	}

	return data;
}

QGeometryData qExtrude(const QPainterPath &path, qreal extrude, Qt::ExtrudeFlags flag, const QTransform &matrix, qreal lod)
{
	QGeometryData data;

	QTriangleSet face = qTriangulate(path,matrix,lod);
	qreal extr=extrude;
	if (extr<=0) extr=0;

	int fasecount=face.vertices.count()>>1;
	const qreal * vert;
	if ((flag & Qt::CapStart)!=0) {
		extrudeCaps(data,face,false,extr*0.5);
	}

	if ((flag & Qt::CapEnd)!=0) {
		extrudeCaps(data,face,true,-extr*0.5);
	}

	int *ind;
	if (extr>0) {
		QPolylineSet line = qPolyline(path,matrix,lod);
		extrudeFacture(data,line,extr);
	}

	return data;
}

QGeometryData qExtrude(const QGeometryData &path, qreal extrude, Qt::ExtrudeFlags flag, uint hint, const QTransform &matrix)
{
	QGeometryData data;

	QTriangleSet face = qTriangulate(path,hint,matrix);
	qreal extr=extrude;
	if (extr<=0) extr=0;

	int fasecount=face.vertices.count()>>1;
	const qreal * vert;
	if ((flag & Qt::CapStart)!=0) {
		extrudeCaps(data,face,false,extr*0.5);
	}

	if ((flag & Qt::CapEnd)!=0) {
		extrudeCaps(data,face,true,-extr*0.5);
	}

	int *ind;
	if (extr>0) {
		QPolylineSet line = qPolyline(path,hint,matrix);
		extrudeFacture(data,line,extr);
	}

	return data;
}




QGeometryData qtGeometryDataForPainterPath(const QPainterPath &_value, qreal lod)
{
	const QVectorPath &path=qtVectorPathForPath(_value);
	return qtGeometryDataForVectorPath(path,lod);
}

QGeometryData qtGeometryDataForVectorPath(const QVectorPath &_path, qreal lod)
{
	QGeometryData data;

	uint hint = _path.hints();
	// Curved paths will be converted to complex polygons.
	hint &= ~QVectorPath::CurvedShapeMask;
	bool neednew=true;

	const qreal *p = _path.points();
	const QPainterPath::ElementType *e = _path.elements();
	if (e) {
		for (int i = 0; i < _path.elementCount(); ++i, ++e, p += 2) {
			switch (*e) {
case QPainterPath::MoveToElement:
	neednew=false;
	data.appendVertex(QVector3D(p[0],p[1],0));
	break;
case QPainterPath::LineToElement:
	if (!neednew) {
		data.appendIndex(data.count()-1);
		data.appendIndex(data.count());
	}
	data.appendVertex(QVector3D(p[0],p[1],0));
	break;
case QPainterPath::CurveToElement:
	{
		qreal pts[8];
		for (int j = 0; j < 4; ++j) {
			pts[2 * j + 0]=p[2 * j - 2];
			pts[2 * j + 1]=p[2 * j - 1];
		}
		for (int j = 0; j < 8; ++j)
			pts[j] *= lod;
		QBezier bezier = QBezier::fromPoints(QPointF(pts[0], pts[1]), QPointF(pts[2], pts[3]), QPointF(pts[4], pts[5]), QPointF(pts[6], pts[7]));
		QPolygonF poly = bezier.toPolygon();
		// Skip first point, it already exists in 'm_vertices'.
		for (int j = 1; j < poly.size(); ++j) {
			if (!neednew) {
				data.appendIndex(data.count()-1);
				data.appendIndex(data.count());
			}
			data.appendVertex(QVector3D(poly.at(j).x(),poly.at(j).y(),0));
		}
	}
	i += 2;
	e += 2;
	p += 4;
	break;
default:
	Q_ASSERT_X(0, "qtGeometryForVectorPath::QVectorPath", "Unexpected element type.");
	break;
			}
		}
	} else {
		for (int j = 0; j < _path.elementCount(); ++j, p += 2) {
			if (j>0) {
				data.appendIndex(data.count()-1);
				data.appendIndex(data.count());
			}
			data.appendVertex(QVector3D(p[0],p[1],0));
		}
	}

//	QTriangleSet face = qTriangulate(_path);

	return data;
}



QGLText::QGLText()
: m_text()
, m_font()
, m_extrude(1.0)
, m_stroke(1.0)
, m_strokeExtrude(0)
, m_caps(QGLText::CapAll)
, m_align(Qt::AlignLeft | Qt::AlignBottom)
{
	m_font.setStyleStrategy(QFont::OpenGLCompatible);
}

QGLText::QGLText(QString _text,QFont _font)
: m_extrude(1.0)
, m_stroke(1.0)
, m_strokeExtrude(0)
, m_caps(QGLText::CapAll)
, m_align(Qt::AlignLeft | Qt::AlignBottom)
{
	m_text=_text;
	m_font=_font;
	m_font.setStyleStrategy(QFont::OpenGLCompatible);
}

QFont QGLText::font() const
{
	return m_font;
}

void QGLText::setFont(QFont _font)
{
	m_font=_font;
	m_font.setStyleStrategy(QFont::OpenGLCompatible);
}

QString QGLText::text() const
{
	return m_text;
}

void QGLText::setText(QString _text)
{
	m_text=_text;
}

qreal QGLText::extrude() const
{
	return m_extrude;
}

void QGLText::setExtrude(qreal _val)
{
	m_extrude=_val;
}

qreal QGLText::stroke() const
{
	return m_stroke;
}

void QGLText::setStroke(qreal _val)
{
	m_stroke=_val;
}

qreal QGLText::strokeExtrude() const
{
	return m_strokeExtrude;
}

void QGLText::setStrokeExtrude(qreal _val)
{
	m_strokeExtrude=_val;
}

QGLText::textCaps QGLText::caps() const
{
	return m_caps;
}

void QGLText::setCaps(QGLText::textCaps _val)
{
	m_caps=_val;
}

Qt::Alignment QGLText::Align() const
{
	return m_align;
}

void QGLText::setAlign(Qt::Alignment _val)
{
	m_align=_val;
}

QGeometryData QGLText::buildText() const
{
	QGeometryData data;
	if ((m_font.styleStrategy() & QFont::OpenGLCompatible)==0) return data;

	QFontMetrics metr(m_font);
	qreal xpos=metr.width(m_text);
	qreal ypos=metr.height();

	QPainterPath pathText;

	switch (m_align & 0xF)
	{
	case Qt::AlignRight: xpos=-xpos; break;
	case Qt::AlignLeft: xpos=0; break;
	case Qt::AlignHCenter:  xpos=-(xpos*0.5); break;
	default: xpos=0;
	}
	switch (m_align & 0xF0)
	{
	case Qt::AlignTop: ypos=-ypos; break;
	case Qt::AlignBottom: ypos=0; break;
	case Qt::AlignVCenter: ypos=-(ypos*0.5); break;
	default: ypos=0;
	}

	pathText.addText(xpos,ypos,m_font,m_text);
	QMatrix4x4 matr;
	matr.rotate(180,QVector3D(1,0,0));

	if (pathText.elementCount()>0) {
		if ((m_caps & CapLine)==0) {
			data=qExtrude(pathText,m_extrude,(Qt::ExtrudeFlags)m_caps);
		} else {
			data=qtGeometryDataForPainterPath(pathText);
		}
	}
	for (int i=0;i<data.vertices().count();i++) {
		data.vertex(i) = matr.map(data.vertexAt(i));
	}

	for (int i=0;i<data.normals().count();i++) {
		data.normal(i) = matr.map(data.normalAt(i));
	}

	/**
    QTriangulatingStroker stroker;
	stroker.process(qtVectorPathForPath(pathText),QPen(Qt::SolidPattern,0.1,Qt::NoPen),QRectF());
	const float * vert=stroker.vertices();
	for (int i=0;i<(stroker.vertexCount()>>1);i++) {
		data.appendVertex(QVector3D(vert[0],vert[1],0));
		vert++;
		vert++;
	}

	QVector3DArray nm(data.count());

	for (int i=0;i<nm.count();i++) {
		nm[0]=QVector3D(0,1,0);
	}

	data.appendNormalArray(nm);

	for (int i=0;i<((stroker.vertexCount()>>2)-1);i++) {
			data.appendIndices(i,i+1,i+2);
			data.appendIndices(i+3,i+1,i+2);
	}
	**/

	return data;
}

QGLBuilder& operator<<(QGLBuilder& _builder, const QGLText& _text)
{
	QGeometryData data=_text.buildText();
    _builder.addTriangles(data);
	return _builder;
}

QT_END_NAMESPACE
