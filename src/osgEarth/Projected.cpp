/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2009 Pelican Ventures, Inc.
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

#include <osgEarth/Projected>

using namespace osgEarth;

const std::string ProjectedTileKey::TYPE_CODE = "L";

ProjectedTileKey::ProjectedTileKey( const std::string key_string, const TileGridProfile& profile ):
TileKey(key_string, profile)
{
    //NOP
}

ProjectedTileKey::ProjectedTileKey( const ProjectedTileKey& rhs):
TileKey(rhs)
{
}

TileKey*
ProjectedTileKey::getSubkey( unsigned int quadrant ) const
{
    if ( !subkeys[quadrant].valid() )
        const_cast<ProjectedTileKey*>(this)->subkeys[quadrant] = new ProjectedTileKey( key + (char)('0' + quadrant), profile );
    return subkeys[quadrant].get();
}

TileKey*
ProjectedTileKey::getParentKey() const
{
    if (getLevelOfDetail() == 1) return NULL;
    return new ProjectedTileKey(key.substr(0, key.length()-1), profile);
}

unsigned int
ProjectedTileKey::getLevelOfDetail() const
{
    return (unsigned int)key.length();
}

bool
ProjectedTileKey::getGeoExtents(
            double& xmin,
            double& ymin,
            double& xmax,
            double& ymax ) const
{
    double width =  profile.xMax() - profile.xMin();
    double height = profile.yMax() - profile.yMin();

    ymax = profile.yMax();
    xmin = profile.xMin();

    for( unsigned int lod = 0; lod < getLevelOfDetail(); lod++ )
    {
        width /= 2.0;
        height /= 2.0;

        char digit = key[lod];
        switch( digit )
        {
        case '1': xmin += width; break;
        case '2': ymax -= height; break;
        case '3': xmin += width; ymax -= height; break;
        }
    }

    ymin = ymax - height;
    xmax = xmin + width;
    return true;
}


void ProjectedTileKey::getPixelExtents(
                                       unsigned int& xmin,
                                       unsigned int& ymin,
                                       unsigned int& xmax,
                                       unsigned int& ymax) const
{
    unsigned int lod = getLevelOfDetail();
    unsigned int px = 0, py = 0;
    unsigned int delta = getMapSizePixels() >> 1;
    for( unsigned int i=0; i<lod; i++ )
    {
        switch( key[i] ) {
            case '1': px += delta; break;
            case '2': py += delta; break;
            case '3': px += delta; py += delta; break;
        }
        delta >>= 1;
    }
    xmin = px;
    ymin = py;
    xmax = px + (delta << 1);
    ymax = py + (delta << 1);
}

void ProjectedTileKey::getTileXY(
                                 unsigned int& out_tile_x,
                                 unsigned int& out_tile_y) const
{
    unsigned int xmin, ymin, xmax, ymax;
    getPixelExtents( xmin, ymin, xmax, ymax );
    out_tile_x = xmin/profile.pixelsPerTile();
    out_tile_y = ymin/profile.pixelsPerTile();
}

osgTerrain::TileID
ProjectedTileKey::getTileId() const
{
    return osgTerrain::TileID();
}