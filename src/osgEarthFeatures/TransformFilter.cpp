/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2010 Pelican Mapping
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
#include <osgEarthFeatures/TransformFilter>
#include <osg/ClusterCullingCallback>

using namespace osgEarth;
using namespace osgEarth::Features;
using namespace osgEarth::Symbology;

TransformFilter::TransformFilter() :
_makeGeocentric( false ),
_localize( false ),
_heightOffset( 0.0 )
{
    // nop
}

TransformFilter::TransformFilter(const SpatialReference* outputSRS ) :
_outputSRS( outputSRS ),
_makeGeocentric( false ),
_localize( false ),
_heightOffset( 0.0 )
{
    //NOP
}

bool
TransformFilter::push( Feature* input, const FilterContext& context )
{
    if ( !input || !input->getGeometry() )
        return true;

    Geometry* container = input->getGeometry();
    if ( container )
    {
        GeometryIterator iter( container );
        while( iter.hasMore() )
        {
            Geometry* geom = iter.next();
            bool success = context.profile()->getSRS()->transformPoints( _outputSRS.get(), geom, false );
            
            // todo: handle errors
            // if ( !success ) return false;

            if ( _makeGeocentric && _outputSRS->isGeographic() )
            {
                const osg::EllipsoidModel* em = context.profile()->getSRS()->getEllipsoid();
                for( int i=0; i<geom->size(); i++ )
                {
                    double x, y, z;
                    em->convertLatLongHeightToXYZ(
                        osg::DegreesToRadians( (*geom)[i].y() ),
                        osg::DegreesToRadians( (*geom)[i].x() ),
                        (*geom)[i].z() + _heightOffset,
                        x, y, z );
                    (*geom)[i].set( x, y, z );                    
                    _bbox.expandBy( x, y, z );
                }
            }
            else
            {
                for( int i=0; i<geom->size(); i++ )
                {
                    if ( _heightOffset != 0.0 )
                        (*geom)[i].z() += _heightOffset;
                    _bbox.expandBy( (*geom)[i] );
                }
            }
        }
    }

    return true;
}

namespace
{
    void
    localizeGeometry( Feature* input, const osg::Matrixd& refFrame )
    {
        if ( input && input->getGeometry() )
        {
            GeometryIterator iter( input->getGeometry() );
            while( iter.hasMore() )
            {
                Geometry* geom = iter.next();
                for( int i=0; i<geom->size(); i++ )
                {
                    (*geom)[i] = (*geom)[i] * refFrame;
                }
            }
        }
    }
}

FilterContext
TransformFilter::push( FeatureList& input, const FilterContext& incx )
{
    _bbox = osg::BoundingBoxd();

    bool ok = true;
    for( FeatureList::iterator i = input.begin(); i != input.end(); i++ )
        if ( !push( i->get(), incx ) )
            ok = false;

    FilterContext outcx( incx );
    outcx.isGeocentric() = _makeGeocentric;
    outcx.profile() = new FeatureProfile( 
        incx.profile()->getExtent().transform( _outputSRS.get() ) );

    // set the reference frame to shift data to the centroid. This will
    // prevent floating point precision errors in the openGL pipeline for
    // properly gridded data.
    if ( _bbox.valid() && _localize )
    {       
        osg::Matrixd localizer = osg::Matrixd::translate( -_bbox.center() );
        for( FeatureList::iterator i = input.begin(); i != input.end(); i++ ) {
            localizeGeometry( i->get(), localizer );
        }
        outcx.setReferenceFrame( localizer );
    }

    return outcx;
}
