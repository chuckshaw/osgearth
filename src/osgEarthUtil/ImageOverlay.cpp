#include <osgEarthUtil/ImageOverlay>

#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/Texture2D>
#include <osgEarthSymbology/MeshSubdivider>
#include <osg/io_utils>

using namespace osgEarth;
using namespace osgEarth::Util;
using namespace osgEarth::Symbology;

/***************************************************************************/

void clampLatitude(osg::Vec2d& l)
{
    l.y() = osg::clampBetween( l.y(), -90.0, 90.0);
}

ImageOverlay::ImageOverlay(const osg::EllipsoidModel* ellipsoid, osg::Image* image):
_lowerLeft(10,10),
_lowerRight(20, 10),
_upperRight(20,20),
_upperLeft(10, 20),
_image(image),
_ellipsoid(ellipsoid),
_dirty(false),
_alpha(1.0f)
{
    _geode = new osg::Geode;
    addChild( _geode );    
    
    //Create the texture
    osg::Texture2D* texture = new osg::Texture2D(_image.get());
    texture->setResizeNonPowerOfTwoHint(false);
    _geode->getOrCreateStateSet()->setTextureAttributeAndModes(0, texture, osg::StateAttribute::ON);    

    init();    
    setNumChildrenRequiringUpdateTraversal( 1 );    
}

void
ImageOverlay::init()
{
    OpenThreads::ScopedLock< OpenThreads::Mutex > lock(_mutex);    

    double height = 0;
    osg::Geometry* geometry = new osg::Geometry();
    osg::Vec3d ll;
    _ellipsoid->convertLatLongHeightToXYZ(osg::DegreesToRadians(_lowerLeft.y()), osg::DegreesToRadians(_lowerLeft.x()), height, ll.x(), ll.y(), ll.z());

    osg::Vec3d lr;
    _ellipsoid->convertLatLongHeightToXYZ(osg::DegreesToRadians(_lowerRight.y()), osg::DegreesToRadians(_lowerRight.x()), height, lr.x(), lr.y(), lr.z());

    osg::Vec3d ur;
    _ellipsoid->convertLatLongHeightToXYZ(osg::DegreesToRadians(_upperRight.y()), osg::DegreesToRadians(_upperRight.x()), height, ur.x(), ur.y(), ur.z());

    osg::Vec3d ul;
    _ellipsoid->convertLatLongHeightToXYZ(osg::DegreesToRadians(_upperLeft.y()), osg::DegreesToRadians(_upperLeft.x()), height, ul.x(), ul.y(), ul.z());


    osg::Vec3Array* verts = new osg::Vec3Array(4);
    (*verts)[0] = ll;
    (*verts)[1] = lr;
    (*verts)[2] = ur;
    (*verts)[3] = ul;

    geometry->setVertexArray( verts );

    osg::Vec4Array* colors = new osg::Vec4Array(1);
    (*colors)[0] = osg::Vec4(1,1,1,_alpha);

    geometry->setColorArray( colors );
    geometry->setColorBinding( osg::Geometry::BIND_OVERALL );

     GLuint tris[6] = { 0, 1, 2,
                        0, 2, 3
                      };        
    geometry->addPrimitiveSet(new osg::DrawElementsUInt( GL_TRIANGLES, 6, tris ) );

    bool flip = _image->getOrigin()==osg::Image::TOP_LEFT;

    osg::Vec2Array* texcoords = new osg::Vec2Array(4);
    (*texcoords)[0].set(0.0f,flip ? 1.0 : 0.0f);
    (*texcoords)[1].set(1.0f,flip ? 1.0 : 0.0f);
    (*texcoords)[2].set(1.0f,flip ? 0.0 : 1.0f);
    (*texcoords)[3].set(0.0f,flip ? 0.0 : 1.0f);
    geometry->setTexCoordArray(0, texcoords);
        
    MeshSubdivider ms;
    ms.run(osg::DegreesToRadians(5.0), *geometry);            

    _geode->removeDrawables(0, _geode->getNumDrawables() );

    _geode->addDrawable( geometry );

    _geometry = _geometry;

    _dirty = false;
}

osg::Image*
ImageOverlay::getImage() const
{
    return _image.get();
}

void ImageOverlay::setImage( osg::Image* image )
{
    if (_image != image)
    {
        _image = image;
        osg::Texture2D* texture = dynamic_cast<osg::Texture2D*>(_geode->getOrCreateStateSet()->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
        if (texture)
        {
            texture->setImage( _image.get() );
        }
        dirty();
    }
}


float
ImageOverlay::getAlpha() const
{
    return _alpha;
}

void
ImageOverlay::setAlpha(float alpha)
{
    if (_alpha != alpha)
    {
        _alpha = osg::clampBetween(alpha, 0.0f, 1.0f);
        dirty();
    }
}

void
ImageOverlay::clampLatitudes()
{
    clampLatitude( _lowerLeft );
    clampLatitude( _lowerRight );
    clampLatitude( _upperLeft );
    clampLatitude( _upperRight );
}


osg::Vec2d
ImageOverlay::getCenter() const
{
    return (_lowerLeft + _lowerRight + _upperRight + _upperLeft) / 4.0;
}

void
ImageOverlay::setCenter(double lon_deg, double lat_deg)
{
    osg::Vec2d center = getCenter();
    osg::Vec2d newCenter(lon_deg, lat_deg);
    osg::Vec2d offset =  newCenter - center;
    setCorners(_lowerLeft += offset, _lowerRight += offset,
               _upperLeft += offset, _upperRight += offset);    
}

void
ImageOverlay::setNorth(double value_deg)
{
    _upperRight.y() = value_deg;
    _upperLeft.y()  = value_deg;
    clampLatitudes();
    dirty();
}

void
ImageOverlay::setSouth(double value_deg)
{
    _lowerRight.y() = value_deg;
    _lowerLeft.y() = value_deg;
    clampLatitudes();
    dirty();
}

void
ImageOverlay::setEast(double value_deg)
{
    _upperRight.x() = value_deg;
    _lowerRight.x() = value_deg;
    dirty();
}

void
ImageOverlay::setWest(double value_deg)
{
    _lowerLeft.x() = value_deg;
    _upperLeft.x() = value_deg;
    dirty();
}

void
ImageOverlay::setCorners(const osg::Vec2d& lowerLeft, const osg::Vec2d& lowerRight, 
        const osg::Vec2d& upperLeft, const osg::Vec2d& upperRight)
{
    _lowerLeft = lowerLeft;
    _lowerRight = lowerRight;
    _upperLeft = upperLeft;
    _upperRight = upperRight;
    clampLatitudes();
    
    dirty();
}

osgEarth::Bounds
ImageOverlay::getBounds() const
{
    osgEarth::Bounds bounds;
    bounds.expandBy(_lowerLeft.x(), _lowerLeft.y());
    bounds.expandBy(_lowerRight.x(), _lowerRight.y());
    bounds.expandBy(_upperLeft.x(), _upperLeft.y());
    bounds.expandBy(_upperRight.x(), _upperRight.y());
    return bounds;
}

void ImageOverlay::setBounds(const osgEarth::Bounds &extent)
{
    setCorners(osg::Vec2d(extent.xMin(), extent.yMin()), osg::Vec2d(extent.xMax(), extent.yMin()),
               osg::Vec2d(extent.xMin(), extent.yMax()), osg::Vec2d(extent.xMax(), extent.yMax()));
}

void
ImageOverlay::setLowerLeft(double lon_deg, double lat_deg)
{
    _lowerLeft = osg::Vec2d(lon_deg, lat_deg);
    clampLatitudes();
    dirty();
}

void
ImageOverlay::setLowerRight(double lon_deg, double lat_deg)
{
    _lowerRight = osg::Vec2d(lon_deg, lat_deg);
    clampLatitudes();
    dirty();
}

void
ImageOverlay::setUpperRight(double lon_deg, double lat_deg)
{
    _upperRight = osg::Vec2d(lon_deg, lat_deg);
    clampLatitudes();
    dirty();
}

void
ImageOverlay::setUpperLeft(double lon_deg, double lat_deg)
{
    _upperLeft = osg::Vec2d(lon_deg, lat_deg);
    clampLatitudes();
    dirty();
}

void
ImageOverlay::traverse(osg::NodeVisitor &nv)
{ 
    if (nv.getVisitorType() == osg::NodeVisitor::UPDATE_VISITOR && _dirty)
    {
        init();        
    }
    osg::Group::traverse(nv);
}

void ImageOverlay::dirty()
{
    OpenThreads::ScopedLock< OpenThreads::Mutex > lock(_mutex);
    _dirty = true;
}