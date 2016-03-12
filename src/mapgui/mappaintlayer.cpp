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

#include "mapgui/mappaintlayer.h"
#include "mapgui/navmapwidget.h"
#include "maplayersettings.h"
#include "mappainterairport.h"
#include "mappaintermark.h"
#include "mappainternav.h"
#include "mapquery.h"
#include "mapscale.h"
#include "geo/calculations.h"
#include <cmath>
#include <marble/MarbleModel.h>
#include <marble/GeoDataPlacemark.h>
#include <marble/GeoDataDocument.h>
#include <marble/GeoDataTreeModel.h>
#include <marble/MarbleWidgetPopupMenu.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/GeoDataStyle.h>
#include <marble/GeoDataIconStyle.h>
#include <marble/GeoDataCoordinates.h>
#include <marble/GeoPainter.h>
#include <marble/LayerInterface.h>
#include <marble/ViewportParams.h>
#include <marble/MarbleLocale.h>
#include <marble/MarbleWidget.h>
#include <marble/ViewportParams.h>

#include <QContextMenuEvent>
#include <QElapsedTimer>

using namespace Marble;
using namespace atools::geo;

MapPaintLayer::MapPaintLayer(NavMapWidget *widget, MapQuery *mapQueries)
  : mapQuery(mapQueries), navMapWidget(widget)
{
  initLayers();

  mapScale = new MapScale();
  mapPainterNav = new MapPainterNav(navMapWidget, mapQuery, mapScale);

  mapPainterAirport = new MapPainterAirport(navMapWidget, mapQuery, mapScale);

  mapPainterMark = new MapPainterMark(navMapWidget, mapQuery, mapScale);
}

MapPaintLayer::~MapPaintLayer()
{
  delete mapPainterNav;
  delete mapPainterAirport;
  delete mapPainterMark;
  delete layers;
  delete mapScale;
  delete mapFont;
}

void MapPaintLayer::preDatabaseLoad()
{
  databaseLoadStatus = true;
}

void MapPaintLayer::postDatabaseLoad()
{
  databaseLoadStatus = true;
}

void MapPaintLayer::initLayers()
{
  if(layers != nullptr)
    delete layers;

  layers = new MapLayerSettings();

  MapLayer defLayer = MapLayer(0).airports().airportName().airportIdent().
                      airportSoft().airportNoRating().airportOverviewRunway().airportSource(layer::ALL).
                      vor().ndb().waypoint().marker();
  layers->
  append(defLayer.clone(0.3f).airportDiagram().airportDiagramDetail().airportDiagramDetail2().
         airportSymbolSize(20).airportInfo().
         waypointSymbolSize(14).waypointName().
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         markerSymbolSize(24).markerInfo()).

  append(defLayer.clone(1.f).airportDiagram().airportDiagramDetail().airportSymbolSize(20).airportInfo().
         waypointSymbolSize(14).waypointName().
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         markerSymbolSize(24).markerInfo()).

  append(defLayer.clone(5.f).airportDiagram().airportSymbolSize(20).airportInfo().
         waypointSymbolSize(10).waypointName().
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         markerSymbolSize(24).markerInfo()).

  append(defLayer.clone(25.f).airportSymbolSize(18).airportInfo().
         waypointSymbolSize(8).
         vorSymbolSize(22).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(22).ndbIdent().ndbInfo().
         markerSymbolSize(24)).

  append(defLayer.clone(50.f).airportSymbolSize(18).airportInfo().
         waypoint(false).
         vorSymbolSize(20).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(20).ndbIdent().ndbInfo().
         marker(false)).

  append(defLayer.clone(100.f).airportSymbolSize(14).
         waypoint(false).
         vorSymbolSize(16).vorIdent().
         ndbSymbolSize(14).ndbIdent().
         marker(false)).

  append(defLayer.clone(150.f).airportSymbolSize(10).minRunwayLength(2500).
         airportOverviewRunway(false).airportName(false).
         waypoint(false).
         vorSymbolSize(10).
         ndbSymbolSize(10).
         marker(false)).

  append(defLayer.clone(300.f).airportSymbolSize(10).
         airportOverviewRunway(false).airportName(false).airportSource(layer::MEDIUM).
         vor(false).ndb(false).waypoint(false).marker(false)).

  append(defLayer.clone(1200.f).airportSymbolSize(10).
         airportOverviewRunway(false).airportName(false).airportSource(layer::LARGE).
         vor(false).ndb(false).waypoint(false).marker(false));

  layers->finishAppend();
  qDebug() << *layers;
}

bool MapPaintLayer::render(GeoPainter *painter, ViewportParams *viewport,
                           const QString& renderPos, GeoSceneLayer *layer)
{
  Q_UNUSED(renderPos);
  Q_UNUSED(layer);

  if(!databaseLoadStatus)
  {
    mapScale->update(viewport, navMapWidget->distance());

    if(mapFont == nullptr)
#if defined(Q_OS_WIN32)
      mapFont = new QFont("Arial", painter->font().pointSize());
#else
      mapFont = new QFont("Helvetica", painter->font().pointSize());
#endif
    mapFont->setBold(true);
    painter->setFont(*mapFont);

    mapLayer = layers->getLayer(static_cast<float>(navMapWidget->distance()));

    if(mapLayer != nullptr)
    {
      if(mapLayer->isAirportDiagram())
      {
        mapPainterAirport->paint(mapLayer, painter, viewport);
        mapPainterNav->paint(mapLayer, painter, viewport);
      }
      else
      {
        mapPainterNav->paint(mapLayer, painter, viewport);
        mapPainterAirport->paint(mapLayer, painter, viewport);
      }
      mapPainterMark->paint(mapLayer, painter, viewport);
    }
  }

  return true;
}
