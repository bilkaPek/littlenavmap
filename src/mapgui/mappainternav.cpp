/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "mapgui/mappainternav.h"

#include "common/symbolpainter.h"
#include "common/mapcolors.h"
#include "common/unit.h"
#include "mapgui/mapwidget.h"

#include <QElapsedTimer>

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;
using namespace maptypes;

MapPainterNav::MapPainterNav(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale)
  : MapPainter(mapWidget, mapQuery, mapScale)
{
}

MapPainterNav::~MapPainterNav()
{
}

void MapPainterNav::render(const PaintContext *context)
{
  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();

  setRenderHints(context->painter);

  // Airways -------------------------------------------------
  bool drawAirway = context->mapLayer->isAirway() &&
                    (context->objectTypes.testFlag(maptypes::AIRWAYJ) ||
                     context->objectTypes.testFlag(maptypes::AIRWAYV));

  context->szFont(context->textSizeNavaid);

  if(drawAirway)
  {
    // Draw airway lines
    const QList<MapAirway> *airways = query->getAirways(curBox, context->mapLayer, context->drawFast);
    if(airways != nullptr)
      paintAirways(context, airways, context->drawFast);
  }

  // Waypoints -------------------------------------------------
  bool drawWaypoint = context->mapLayer->isWaypoint() && context->objectTypes.testFlag(maptypes::WAYPOINT);
  if(drawWaypoint || drawAirway)
  {
    // If airways are drawn we also have to go through waypoints
    const QList<MapWaypoint> *waypoints = query->getWaypoints(curBox, context->mapLayer, context->drawFast);
    if(waypoints != nullptr)
      paintWaypoints(context, waypoints, drawWaypoint, context->drawFast);
  }

  // VOR -------------------------------------------------
  if(context->mapLayer->isVor() && context->objectTypes.testFlag(maptypes::VOR))
  {
    const QList<MapVor> *vors = query->getVors(curBox, context->mapLayer, context->drawFast);
    if(vors != nullptr)
      paintVors(context, vors, context->drawFast);
  }

  // NDB -------------------------------------------------
  if(context->mapLayer->isNdb() && context->objectTypes.testFlag(maptypes::NDB))
  {
    const QList<MapNdb> *ndbs = query->getNdbs(curBox, context->mapLayer, context->drawFast);
    if(ndbs != nullptr)
      paintNdbs(context, ndbs, context->drawFast);
  }

  // Marker -------------------------------------------------
  if(context->mapLayer->isMarker() && context->objectTypes.testFlag(maptypes::ILS))
  {
    const QList<MapMarker> *markers = query->getMarkers(curBox, context->mapLayer, context->drawFast);
    if(markers != nullptr)
      paintMarkers(context, markers, context->drawFast);
  }
}

/* Draw airways and texts */
void MapPainterNav::paintAirways(const PaintContext *context, const QList<MapAirway> *airways, bool fast)
{
  QFontMetrics metrics = context->painter->fontMetrics();

  // Used to combine texts of different airway lines with the same coordinates into one text
  // Key is line coordinates as text (avoid floating point compare) and value is index into texts and airwayIndex
  QHash<QString, int> lines;
  // Contains combined text for overlapping airway lines
  QList<QString> texts;
  // points to index or airway in airway list
  QList<int> airwayIndex;

  for(int i = 0; i < airways->size(); i++)
  {
    const MapAirway& airway = airways->at(i);

    if(airway.type == maptypes::JET && !context->objectTypes.testFlag(maptypes::AIRWAYJ))
      continue;
    if(airway.type == maptypes::VICTOR && !context->objectTypes.testFlag(maptypes::AIRWAYV))
      continue;

    if(airway.type == maptypes::VICTOR)
      context->painter->setPen(QPen(mapcolors::airwayVictorColor, 1.5));
    else if(airway.type == maptypes::JET)
      context->painter->setPen(QPen(mapcolors::airwayJetColor, 1.5));
    else if(airway.type == maptypes::BOTH)
      context->painter->setPen(QPen(mapcolors::airwayBothColor, 1.5));

    // Get start and end point of airway segment in screen coordinates
    int x1, y1, x2, y2;
    bool visible1 = wToS(airway.from, x1, y1);
    bool visible2 = wToS(airway.to, x2, y2);

    if(!visible1 && !visible2)
      // Check bounding rect for visibility
      visible1 = airway.bounding.overlaps(context->viewportRect);

    if(visible1 || visible2)
    {
      // Draw line if both points are visible or line intersects screen coordinates
      GeoDataCoordinates from(airway.from.getLonX(), airway.from.getLatY(), 0, DEG);
      GeoDataCoordinates to(airway.to.getLonX(), airway.to.getLatY(), 0, DEG);
      GeoDataLineString line;
      line.setTessellate(true);
      line << from << to;
      context->painter->drawPolyline(line);

      if(!fast)
      {
        // Build text index
        QString text;
        if(context->mapLayer->isAirwayIdent())
          text += airway.name;
        if(context->mapLayer->isAirwayInfo())
        {
          text += QString(tr(" / ")) + maptypes::airwayTypeToShortString(airway.type);
          if(airway.minAltitude)
            text += QString(tr(" / ")) + Unit::altFeet(airway.minAltitude, true /*addUnit*/, true /*narrow*/);
        }

        if(!text.isEmpty())
        {
          QString firstStr = line.first().toString(GeoDataCoordinates::Decimal, 3);
          QString lastStr = line.last().toString(GeoDataCoordinates::Decimal, 3);

          // Create string key for index by using the coordinates
          QString lineTextKey = firstStr + "|" + lastStr;

          // Does it already exist in the map?
          int index = lines.value(lineTextKey, -1);
          if(index == -1)
            // Try with reversed coordinates
            index = lines.value(lastStr + "|" + firstStr, -1);

          if(index != -1)
          {
            // Index already found - add the new text to the present one
            QString oldTxt(texts.at(index));
            if(oldTxt != text)
              texts[index] = texts.at(index) + ", " + text;
          }
          else if(!text.isEmpty())
          {
            // Neither with forward nor reversed coordinates found - insert a new entry
            texts.append(text);
            lines.insert(lineTextKey, texts.size() - 1);
            airwayIndex.append(i);
          }
        }
      }
    }
  }

  // Draw texts ----------------------------------------
  int i = 0;
  context->painter->setPen(mapcolors::airwayTextColor);
  for(const QString& text : texts)
  {
    const MapAirway& airway = airways->at(airwayIndex.at(i));
    int xt = -1, yt = -1;
    float textBearing;
    if(findTextPos(airway.from, airway.to, context->painter, metrics.width(text), metrics.height() * 2,
                   xt, yt, &textBearing))
    {
      float rotate;
      if(textBearing > 180.f)
        rotate = textBearing + 90.f;
      else
        rotate = textBearing - 90.f;

      context->painter->translate(xt, yt);
      context->painter->rotate(rotate);
      context->painter->drawText(-context->painter->fontMetrics().width(text) / 2,
                                 context->painter->fontMetrics().ascent(), text);
      context->painter->resetTransform();
    }
    i++;
  }
}

/* Draw waypoints. If airways are enabled corresponding waypoints are drawn too */
void MapPainterNav::paintWaypoints(const PaintContext *context, const QList<MapWaypoint> *waypoints,
                                   bool drawWaypoint, bool drawFast)
{
  bool drawAirwayV = context->mapLayer->isAirway() && context->objectTypes.testFlag(maptypes::AIRWAYV);
  bool drawAirwayJ = context->mapLayer->isAirway() && context->objectTypes.testFlag(maptypes::AIRWAYJ);

  for(const MapWaypoint& waypoint : *waypoints)
  {
    // If waypoints are off, airways are on and waypoint has no airways skip it
    if(!(drawWaypoint || (drawAirwayV && waypoint.hasVictorAirways) || (drawAirwayJ && waypoint.hasJetAirways)))
      continue;

    int x, y;
    bool visible = wToS(waypoint.position, x, y);

    if(visible)
    {
      int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());
      symbolPainter->drawWaypointSymbol(context->painter, QColor(), x, y, size, false, drawFast);

      // If airways are drawn force display of the respecive waypoints
      if(context->mapLayer->isWaypointName() ||
         (context->mapLayer->isAirwayIdent() && (drawAirwayV || drawAirwayJ)))
        symbolPainter->drawWaypointText(context->painter, waypoint, x, y, textflags::IDENT, size, false);
    }
  }
}

void MapPainterNav::paintVors(const PaintContext *context, const QList<MapVor> *vors, bool drawFast)
{
  for(const MapVor& vor : *vors)
  {
    int x, y;
    bool visible = wToS(vor.position, x, y);

    if(visible)
    {
      int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getVorSymbolSize());
      symbolPainter->drawVorSymbol(context->painter, vor, x, y,
                                   size, false, drawFast,
                                   context->mapLayerEffective->isVorLarge() ? size * 5 : 0);

      textflags::TextFlags flags;

      if(context->mapLayer->isVorInfo())
        flags = textflags::IDENT | textflags::TYPE | textflags::FREQ;
      else if(context->mapLayer->isVorIdent())
        flags = textflags::IDENT;

      symbolPainter->drawVorText(context->painter, vor, x, y, flags, size, false);
    }
  }
}

void MapPainterNav::paintNdbs(const PaintContext *context, const QList<MapNdb> *ndbs, bool drawFast)
{
  for(const MapNdb& ndb : *ndbs)
  {
    int x, y;
    bool visible = wToS(ndb.position, x, y);

    if(visible)
    {
      int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getNdbSymbolSize());
      symbolPainter->drawNdbSymbol(context->painter, x, y, size, false, drawFast);

      textflags::TextFlags flags;

      if(context->mapLayer->isNdbInfo())
        flags = textflags::IDENT | textflags::TYPE | textflags::FREQ;
      else if(context->mapLayer->isNdbIdent())
        flags = textflags::IDENT;

      symbolPainter->drawNdbText(context->painter, ndb, x, y, flags, size, false);
    }
  }
}

void MapPainterNav::paintMarkers(const PaintContext *context, const QList<MapMarker> *markers, bool drawFast)
{
  for(const MapMarker& marker : *markers)
  {
    int x, y;
    bool visible = wToS(marker.position, x, y);

    if(visible)
    {
      int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getMarkerSymbolSize());
      symbolPainter->drawMarkerSymbol(context->painter, marker, x, y, size, drawFast);

      if(context->mapLayer->isMarkerInfo())
      {
        QString type = marker.type.toLower();
        type[0] = type.at(0).toUpper();
        x -= size / 2 + 2;
        symbolPainter->textBox(context->painter, {type}, mapcolors::markerSymbolColor, x, y,
                               textatt::BOLD | textatt::RIGHT, 0);
      }
    }
  }
}
