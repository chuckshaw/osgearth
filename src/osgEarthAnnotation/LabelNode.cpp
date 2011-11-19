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

#include <osgEarthAnnotation/LabelNode>
#include <osgEarthAnnotation/Decluttering>
#include <osgEarthAnnotation/AnnotationUtils>
#include <osgEarthSymbology/Color>
#include <osgText/Text>
#include <osg/Depth>

using namespace osgEarth;
using namespace osgEarth::Annotation;
using namespace osgEarth::Symbology;


//-------------------------------------------------------------------

LabelNode::LabelNode(MapNode*            mapNode,
                     const osg::Vec3d&   position,
                     const std::string&  text,
                     const TextSymbol*   symbol ) :

OrthoNode( mapNode->getMap()->getProfile()->getSRS(), position ),
_text    ( text ),
_geode   ( 0L )
{
    init( symbol );
}

LabelNode::LabelNode(const SpatialReference* mapSRS,
                     const osg::Vec3d&       position,
                     const std::string&      text,
                     const TextSymbol*       symbol ) :

OrthoNode( mapSRS, position ),
_text( text )
{
    init( symbol );
}

void
LabelNode::init( const TextSymbol* symbol )
{
    // The following setup will result is a proper dynamic bounding box for the text.
    // If you just use osgText's rotate-to-screen and SCREEN_COORDS setup, you do not
    // get a proper bounds.
    osg::Drawable* t = AnnotationUtils::createTextDrawable( _text, symbol, osg::Vec3(0,0,0) );

    osg::StateSet* stateSet = t->getOrCreateStateSet();
    stateSet->setAttributeAndModes( new osg::Depth(osg::Depth::ALWAYS, 0, 1, false), 1 );

    _geode = new osg::Geode();
    _geode->addDrawable( t );

    this->attach( _geode );
}

void
LabelNode::setAnnotationData( AnnotationData* data )
{
    OrthoNode::setAnnotationData( data );

    // override this method so we can attach the anno data to the drawables.
    const osg::Geode::DrawableList& list = _geode->getDrawableList();
    for( osg::Geode::DrawableList::const_iterator i = list.begin(); i != list.end(); ++i )
    {
        i->get()->setUserData( data );
    }
}
