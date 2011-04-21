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
#include <osgEarthFeatures/ConvertTypeFilter>
#include <list>
#include <deque>

using namespace osgEarth;
using namespace osgEarth::Features;
using namespace osgEarth::Symbology;


ConvertTypeFilter::ConvertTypeFilter() :
_toType( Geometry::TYPE_UNKNOWN )
{
    //NOP
}

ConvertTypeFilter::ConvertTypeFilter( const Geometry::Type& toType ) :
_toType( toType )
{
    // NOP
}

ConvertTypeFilter::ConvertTypeFilter( const ConvertTypeFilter& rhs ) :
_toType( rhs._toType )
{
    //NOP
}

bool
ConvertTypeFilter::push( Feature* input, const FilterContext& context )
{
    if ( !input || !input->getGeometry() )
        return true;

    if ( input->getGeometry()->getComponentType() == _toType )
        return true;

    Geometry* geom = input->getGeometry()->cloneAs( _toType );
    input->setGeometry( geom );

    return true;
}


FilterContext
ConvertTypeFilter::push( FeatureList& input, const FilterContext& context )
{
    if ( !isSupported() )
    {
        OE_WARN << "ConvertTypeFilter support not enabled" << std::endl;
        return context;
    }

    bool ok = true;
    for( FeatureList::iterator i = input.begin(); i != input.end(); ++i )
        if ( !push( i->get(), context ) )
            ok = false;

    return context;
}
