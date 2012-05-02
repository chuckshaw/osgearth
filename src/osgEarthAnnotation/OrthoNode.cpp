/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
* Copyright 2008-2012 Pelican Mapping
* http://osgearth.org
*
* osgEarth is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include <osgEarthAnnotation/OrthoNode>
#include <osgEarthAnnotation/AnnotationUtils>
#include <osgEarthAnnotation/Decluttering>
#include <osgEarthSymbology/Color>
#include <osgEarth/ThreadingUtils>
#include <osgEarth/CullingUtils>
#include <osgEarth/MapNode>
#include <osgText/Text>
#include <osg/ComputeBoundsVisitor>
#include <osgUtil/IntersectionVisitor>
#include <osg/OcclusionQueryNode>
#include <osg/Point>
#include <osg/Depth>

using namespace osgEarth;
using namespace osgEarth::Annotation;

//------------------------------------------------------------------------

namespace
{
    struct OrthoOQNode : public osg::OcclusionQueryNode
    {
        OrthoOQNode( const std::string& name ) 
        {
            setName( name );
            setVisibilityThreshold(1);
            setDebugDisplay(true);
            setCullingActive(false);
        }

        virtual osg::BoundingSphere computeBound() const
        {
            {
                // Need to make this routine thread-safe. Typically called by the update
                //   Visitor, or just after the update traversal, but could be called by
                //   an application thread or by a non-osgViewer application.
                Threading::ScopedMutexLock lock( _computeBoundMutex );

                // This is the logical place to put this code, but the method is const. Cast
                //   away constness to compute the bounding box and modify the query geometry.
                OrthoOQNode* nonConstThis = const_cast<OrthoOQNode*>( this );

                osg::ref_ptr<osg::Vec3Array> v = new osg::Vec3Array(1);
                (*v)[0].set( _xform->getMatrix().getTrans() );

                osg::Geometry* geom = static_cast< osg::Geometry* >( nonConstThis->_queryGeode->getDrawable( 0 ) );
                geom->setVertexArray( v.get() );
                geom->getPrimitiveSetList().clear();
                geom->addPrimitiveSet( new osg::DrawArrays(GL_POINTS,0,1) );
                nonConstThis->getQueryStateSet()->setAttributeAndModes(new osg::Point(15), 1);
                nonConstThis->getQueryStateSet()->setBinNumber(INT_MAX);

                geom = static_cast< osg::Geometry* >( nonConstThis->_debugGeode->getDrawable( 0 ) );
                geom->setVertexArray( v.get() );
                geom->getPrimitiveSetList().clear();
                geom->addPrimitiveSet( new osg::DrawArrays(GL_POINTS,0,1) );
                nonConstThis->getDebugStateSet()->setAttributeAndModes(new osg::Point(15), 1);
                osg::Depth* d = new osg::Depth( osg::Depth::LEQUAL, 0.f, 1.f, false );
                nonConstThis->getDebugStateSet()->setAttributeAndModes( d, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);
                (*dynamic_cast<osg::Vec4Array*>(geom->getColorArray()))[0].set(1,0,0,1);
            }

            return Group::computeBound();
        }

        osg::MatrixTransform* _xform;
    };
}

//------------------------------------------------------------------------


OrthoNode::OrthoNode(MapNode*        mapNode,
                     const GeoPoint& position ) :

PositionedAnnotationNode( mapNode ),
_mapSRS                 ( mapNode ? mapNode->getMapSRS() : 0L ),
_horizonCulling         ( false )
{
    init();
    if ( mapNode && mapNode->isGeocentric() )
    {
        setHorizonCulling( true );
    }
    setPosition( position );
}

OrthoNode::OrthoNode(MapNode*          mapNode,
                     const osg::Vec3d& position ) :

PositionedAnnotationNode( mapNode ),
_mapSRS                 ( mapNode ? mapNode->getMapSRS() : 0L ),
_horizonCulling         ( false )
{
    init();
    if ( mapNode && mapNode->isGeocentric() )
    {
        setHorizonCulling( true );
    }
    setPosition( GeoPoint(_mapSRS.get(), position) );
}

OrthoNode::OrthoNode(const SpatialReference* mapSRS,
                     const GeoPoint&         position ) :

_mapSRS        ( mapSRS ),
_horizonCulling( false )
{
    init();
    if ( _mapSRS.valid() && _mapSRS->isGeographic() && !_mapSRS->isPlateCarre() )
    {
        setHorizonCulling( true );
    }
    setPosition( position );
}

OrthoNode::OrthoNode() :
_mapSRS        ( 0L ),
_horizonCulling( false )
{
    init();
}

void
OrthoNode::init()
{
    _switch = new osg::Switch();

    // install it, but deactivate it until we can get it to work.
#if 0
    OrthoOQNode* oq = new OrthoOQNode("");
    oq->setQueriesEnabled(false);
#else
    _oq = new osg::Group();
#endif

    _oq->addChild( _switch );
    this->addChild( _oq );

    _autoxform = new AnnotationUtils::OrthoNodeAutoTransform();
    _autoxform->setAutoRotateMode( osg::AutoTransform::ROTATE_TO_SCREEN );
    _autoxform->setAutoScaleToScreen( true );
    _autoxform->setCullingActive( false ); // for the first pass
    _switch->addChild( _autoxform );

    _matxform = new osg::MatrixTransform();
    _switch->addChild( _matxform );

    //oq->_xform = _matxform;
    //_oq = oq;

    _switch->setSingleChildOn( 0 );

    _attachPoint = new osg::Group();

    _autoxform->addChild( _attachPoint );
    _matxform->addChild( _attachPoint );

    this->getOrCreateStateSet()->setMode( GL_LIGHTING, 0 );
}

void
OrthoNode::traverse( osg::NodeVisitor& nv )
{
    osgUtil::CullVisitor* cv = 0L;

    if ( nv.getVisitorType() == osg::NodeVisitor::CULL_VISITOR )
    {
        cv = static_cast<osgUtil::CullVisitor*>( &nv );

        // make sure that we're NOT using the AutoTransform if this node is in the decluttering bin;
        // the decluttering bin automatically manages screen space transformation.
        bool declutter = cv->getCurrentRenderBin()->getName() == OSGEARTH_DECLUTTER_BIN;
        if ( declutter && _switch->getValue(0) == 1 )
        {
            _switch->setSingleChildOn( 1 );
        }
        else if ( !declutter && _switch->getValue(0) == 0 )
        {
            _switch->setSingleChildOn( 0 );
        }

        // If decluttering is enabled, update the auto-transform but not its children.
        // This is necessary to support picking/selection. An optimization would be to
        // disable this pass when picking is not in use
        if ( declutter )
        {
            static_cast<AnnotationUtils::OrthoNodeAutoTransform*>(_autoxform)->acceptCullNoTraverse( cv );
        }

        // turn off small feature culling
        cv->setSmallFeatureCullingPixelSize(0.0f);

        AnnotationNode::traverse( nv );

        this->setCullingActive( true );
    }
    
    // For an intersection visitor, ALWAYS traverse the autoxform instead of the 
    // matrix transform. The matrix transform is only used in combination with the 
    // decluttering engine, so it cannot properly support picking of decluttered
    // objects
    else if ( 
        nv.getVisitorType() == osg::NodeVisitor::NODE_VISITOR &&
        dynamic_cast<osgUtil::IntersectionVisitor*>( &nv ) )
    {
        if ( static_cast<AnnotationUtils::OrthoNodeAutoTransform*>(_autoxform)->okToIntersect() )
        {
            _autoxform->accept( nv );
        }
    }

    else
    {
        AnnotationNode::traverse( nv );
    }
}

osg::BoundingSphere
OrthoNode::computeBound() const
{
    return osg::BoundingSphere(_matxform->getMatrix().getTrans(), 1000.0);
}

bool
OrthoNode::setPosition( const osg::Vec3d& position )
{
    return setPosition( GeoPoint(_mapSRS.get(), position) );
}

bool
OrthoNode::setPosition( const GeoPoint& position )
{
    if ( _mapSRS.valid() )
    {
        // first transform the point to the map's SRS:
        GeoPoint mapPos = _mapSRS.valid() ? position.transform(_mapSRS.get()) : position;
        if ( !mapPos.isValid() )
            return false;

        _mapPosition = mapPos;
    }
    else
    {
        _mapPosition = position;
    }

    // make sure the node is set up for auto-z-update if necessary:
    configureForAltitudeMode( _mapPosition.altitudeMode() );

    // and update the node.
    if ( !updateTransforms(_mapPosition) )
        return false;

    return true;
}

bool
OrthoNode::updateTransforms( const GeoPoint& p, osg::Node* patch )
{
    if ( _mapSRS.valid() )
    {
        // make sure the point is absolute to terrain
        GeoPoint absPos(p);
        if ( !makeAbsolute(absPos, patch) )
            return false;

        osg::Matrixd local2world;
        if ( !absPos.createLocalToWorld(local2world) )
            return false;

        // apply the local tangent plane offset:
        local2world.preMult( osg::Matrix::translate(_localOffset) );

        // update the xforms:
        _autoxform->setPosition( local2world.getTrans() );
        _matxform->setMatrix( local2world );

        CullNodeByHorizon* culler = dynamic_cast<CullNodeByHorizon*>(this->getCullCallback());
        if ( culler )
            culler->_world = local2world.getTrans();
    }
    else
    {
        osg::Vec3d absPos = p.vec3d() + _localOffset;
        _autoxform->setPosition( absPos );
        _matxform->setMatrix( osg::Matrix::translate(absPos) );
    }

    dirtyBound();
    return true;
}

GeoPoint
OrthoNode::getPosition() const
{
    return _mapPosition;
}

void
OrthoNode::setLocalOffset( const osg::Vec3d& offset )
{
    _localOffset = offset;
    setPosition( _mapPosition );
}

const osg::Vec3d&
OrthoNode::getLocalOffset() const
{
    return _localOffset;
}

void
OrthoNode::setHorizonCulling( bool value )
{
    if ( _horizonCulling != value && _mapSRS.valid() )
    {
        _horizonCulling = value;

        if ( _horizonCulling )
        {
            osg::Vec3d world = _autoxform->getPosition();
            this->setCullCallback( new CullNodeByHorizon(world, _mapSRS->getEllipsoid()) );
        }
        else
        {
            this->removeCullCallback( this->getCullCallback() );
        }
    }
}

void
OrthoNode::reclamp( const TileKey& key, osg::Node* tile, const Terrain* terrain )
{
    // first verify that the label position intersects the tile:
    if ( key.getExtent().contains( _mapPosition.x(), _mapPosition.y() ) )
    {
        updateTransforms( _mapPosition, tile );
    }
}
