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
#include <osgEarth/MapNodeOptions>
#include <osg/Notify>
#include <OpenThreads/Thread>

using namespace osgEarth;

std::string MapNodeOptions::OPTIONS_TAG = "__osgEarth::MapNodeOptions";

//----------------------------------------------------------------------------

ResourcePolicy::ResourcePolicy()
{
    //nop
}

bool
ResourcePolicy::reserveTextureImageUnit( int& out_unit )
{
    //TODO
    out_unit = -1;
    return false;
}

bool
ResourcePolicy::reserveTextureImageUnit( int unit )
{
    std::set<int>::const_iterator i = _reservedTUIs.find( unit );
    if ( i != _reservedTUIs.end() ) {
        _reservedTUIs.insert( unit );
        return true;
    }
    return false;
}

void 
ResourcePolicy::releaseTextureImageUnit( int unit )
{
    std::set<int>::iterator i = _reservedTUIs.find( unit );
    if ( i != _reservedTUIs.end() )
        _reservedTUIs.erase( i );
}

bool 
ResourcePolicy::isTextureImageUnitReserved( int unit ) const
{
    return _reservedTUIs.find( unit ) != _reservedTUIs.end();
}

//----------------------------------------------------------------------------

static TerrainOptions s_defaultTerrainOptions;

//----------------------------------------------------------------------------

MapNodeOptions::MapNodeOptions( const Config& conf ) :
ConfigOptions( conf ),
_proxySettings( ProxySettings() ),
_cacheOnly( false ),
_enableLighting( true ),
_terrainOptions( 0L )
{
    mergeConfig( conf );
}

MapNodeOptions::MapNodeOptions( const TerrainOptions& to ) :
_proxySettings( ProxySettings() ),
_cacheOnly( false ),
_enableLighting( true ),
_terrainOptions( 0L )
{
    setTerrainOptions( to );
}


MapNodeOptions::~MapNodeOptions()
{
    if ( _terrainOptions )
    {
        delete _terrainOptions;
        _terrainOptions = 0L;
    }
}

Config
MapNodeOptions::getConfig() const
{
    Config conf; // start with a fresh one since this is a FINAL object  // = ConfigOptions::getConfig();
    conf.key() = "options";

    conf.updateObjIfSet( "proxy", _proxySettings );
    conf.updateIfSet( "cache_only", _cacheOnly );
    conf.updateIfSet( "lighting", _enableLighting );
    conf.updateIfSet( "terrain", _terrainOptionsConf );

    return conf;
}

void
MapNodeOptions::mergeConfig( const Config& conf )
{
    ConfigOptions::mergeConfig( conf );

    conf.getObjIfSet( "proxy", _proxySettings );
    conf.getIfSet( "cache_only", _cacheOnly );
    conf.getIfSet( "lighting", _enableLighting );

    if ( conf.hasChild( "terrain" ) )
    {
        _terrainOptionsConf = conf.child( "terrain" );
        if ( _terrainOptions )
        {
            delete _terrainOptions;
            _terrainOptions = 0L;
        }
    }
}

void
MapNodeOptions::setTerrainOptions( const TerrainOptions& options )
{
    _terrainOptionsConf = options.getConfig();
    if ( _terrainOptions )
    {
        delete _terrainOptions;
        _terrainOptions = 0L;
    }
}

const TerrainOptions&
MapNodeOptions::getTerrainOptions() const
{
    if ( _terrainOptionsConf.isSet() )
    {
        if ( !_terrainOptions )
        {
            const_cast<MapNodeOptions*>(this)->_terrainOptions = new TerrainOptions( _terrainOptionsConf.value() );
        }
        return *_terrainOptions;
    }
    else
    {
        return s_defaultTerrainOptions;
    }
}
