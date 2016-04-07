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
#include "settings/settings.h"
#include "gui/mainwindow.h"
#include "mapgui/mapscale.h"
#include "common/formatter.h"
#include "geo/calculations.h"
#include "common/maptools.h"
#include "common/mapcolors.h"
#include "route/routecontroller.h"
#include <QContextMenuEvent>
#include <QMenu>
#include <QSettings>
#include <QToolTip>

#include <marble/MarbleLocale.h>
#include <marble/GeoDataDocument.h>
#include <marble/GeoDataIconStyle.h>
#include <marble/GeoDataPlacemark.h>
#include <marble/GeoDataStyle.h>
#include <marble/GeoDataTreeModel.h>
#include <marble/MarbleModel.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/ViewportParams.h>
#include <marble/MarbleModel.h>
#include <marble/ElevationModel.h>
#include "common/coordinateconverter.h"
#include "maplayer.h"
#include "maptooltip.h"
#include "ui_mainwindow.h"

#include <gui/actiontextsaver.h>

using namespace Marble;

NavMapWidget::NavMapWidget(MainWindow *parent, MapQuery *query)
  : Marble::MarbleWidget(parent), parentWindow(parent), mapQuery(query)
{
  installEventFilter(this);

  // Set the map quality to gain speed
  // TODO configuration option
  setMapQualityForViewContext(HighQuality, Still);
  setMapQualityForViewContext(LowQuality, Animation);

  MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::NauticalSystem);
  inputHandler()->setInertialEarthRotationEnabled(false);

  mapTooltip = new MapTooltip(this, mapQuery);
  paintLayer = new MapPaintLayer(this, mapQuery);
  addLayer(paintLayer);

  MarbleWidgetInputHandler *input = inputHandler();
  input->setMouseButtonPopupEnabled(Qt::RightButton, false);
  input->setMouseButtonPopupEnabled(Qt::LeftButton, false);
  setContextMenuPolicy(Qt::CustomContextMenu);

  connect(this, &NavMapWidget::customContextMenuRequested, this, &NavMapWidget::contextMenu);
}

NavMapWidget::~NavMapWidget()
{
  delete paintLayer;
  delete mapTooltip;
  // highlightMapObjects.deleteAllObjects();
}

void NavMapWidget::setTheme(const QString& theme, int index)
{
  setMapThemeId(theme);

  if(index == OSM_HILLSHADING)
  {
    setShowTerrain(true);
    setShowRelief(true);
    setPropertyValue("hillshading", true);
  }
  else
  {
    setShowTerrain(false);
    setShowRelief(false);
    setPropertyValue("hillshading", false);
  }

  if(index == SIMPLE || index == POLITICAL || index == BLUE_MARBLE || index == ATLAS)
  {
    setPropertyValue("places", true);
    setPropertyValue("cities", true);
    setPropertyValue("otherplaces", false);
    setPropertyValue("terrain", false);
    setPropertyValue("urbanareas", false);
    setPropertyValue("borders", true);
  }
}

void NavMapWidget::setShowMapPois(bool show)
{
  qDebug() << "setShowMapPois" << show;
  setShowPlaces(show);
  setShowCities(show);
  setShowOtherPlaces(show);
}

void NavMapWidget::setShowMapFeatures(maptypes::MapObjectTypes type, bool show)
{
  qDebug() << "setShowMapFeatures" << type << "show" << show;
  paintLayer->setShowMapFeatures(type, show);
}

void NavMapWidget::setDetailFactor(int factor)
{
  qDebug() << "setDetailFactor" << factor;
  paintLayer->setDetailFactor(factor);
}

RouteController *NavMapWidget::getRouteController() const
{
  return parentWindow->getRouteController();
}

void NavMapWidget::getRouteDragPoints(atools::geo::Pos& from, atools::geo::Pos& to, QPoint& cur)
{
  from = routeDragFrom;
  to = routeDragTo;
  cur = routeDragCur;
}

void NavMapWidget::preDatabaseLoad()
{
  paintLayer->preDatabaseLoad();
}

void NavMapWidget::postDatabaseLoad()
{
  paintLayer->postDatabaseLoad();
}

void NavMapWidget::historyNext()
{
  const MapPosHistoryEntry& entry = history.next();
  if(entry.pos.isValid())
  {
    setZoom(entry.zoom);
    centerOn(entry.pos.getLonX(), entry.pos.getLatY(), false);
  }
}

void NavMapWidget::historyBack()
{
  const MapPosHistoryEntry& entry = history.back();
  if(entry.pos.isValid())
  {
    setZoom(entry.zoom);
    centerOn(entry.pos.getLonX(), entry.pos.getLatY(), false);
  }
}

void NavMapWidget::saveState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();

  s->setValue("Map/MarkLonX", static_cast<double>(markPos.getLonX()));
  s->setValue("Map/MarkLatY", static_cast<double>(markPos.getLatY()));

  s->setValue("Map/HomeLonX", static_cast<double>(homePos.getLonX()));
  s->setValue("Map/HomeLatY", static_cast<double>(homePos.getLatY()));
  s->setValue("Map/HomeZoom", homeZoom);
  history.saveState("Map/History");

  QByteArray bytesDistMarker;
  QDataStream ds(&bytesDistMarker, QIODevice::WriteOnly);
  ds.setVersion(QDataStream::Qt_5_5);
  ds << distanceMarkers;
  s->setValue("Map/DistanceMarkers", bytesDistMarker);

  QByteArray bytesRangeMarker;
  QDataStream ds2(&bytesRangeMarker, QIODevice::WriteOnly);
  ds2.setVersion(QDataStream::Qt_5_5);
  ds2 << rangeMarkers;
  s->setValue("Map/RangeMarkers", bytesRangeMarker);
}

void NavMapWidget::restoreState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();

  if(s->contains("Map/MarkLonX") && s->contains("Map/MarkLatY"))
  {
    markPos = atools::geo::Pos(s->value("Map/MarkLonX").toDouble(), s->value("Map/MarkLatY").toDouble());
    emit markChanged(markPos);
  }

  if(s->contains("Map/HomeLonX") && s->contains("Map/HomeLatY") && s->contains("Map/HomeZoom"))
  {
    homePos = atools::geo::Pos(s->value("Map/HomeLonX").toDouble(), s->value("Map/HomeLatY").toDouble());
    homeZoom = s->value("Map/HomeZoom").toInt();
    emit homeChanged(markPos);
  }
  history.restoreState("Map/History");

  QByteArray bytesDistMark(s->value("Map/DistanceMarkers").toByteArray());
  QDataStream ds(&bytesDistMark, QIODevice::ReadOnly);
  ds.setVersion(QDataStream::Qt_5_5);
  ds >> distanceMarkers;

  QByteArray bytesRangeMark(s->value("Map/RangeMarkers").toByteArray());
  QDataStream ds2(&bytesRangeMark, QIODevice::ReadOnly);
  ds2.setVersion(QDataStream::Qt_5_5);
  ds2 >> rangeMarkers;
}

void NavMapWidget::showSavedPos()
{
  const MapPosHistoryEntry& currentPos = history.current();

  if(currentPos.pos.isValid())
    centerOn(currentPos.pos.getLonX(), currentPos.pos.getLatY(), false);

  if(currentPos.zoom != -1)
    setZoom(currentPos.zoom);
}

void NavMapWidget::showPos(const atools::geo::Pos& pos, int zoomValue)
{
  qDebug() << "NavMapWidget::showPoint" << pos;
  setZoom(zoomValue);
  centerOn(pos.getLonX(), pos.getLatY(), false);
}

void NavMapWidget::showRect(const atools::geo::Rect& rect)
{
  qDebug() << "NavMapWidget::showRect" << rect;
  centerOn(GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(), rect.getWest(),
                            GeoDataCoordinates::Degree), false);
}

void NavMapWidget::showMark()
{
  qDebug() << "NavMapWidget::showMark" << markPos;
  if(markPos.isValid())
  {
    setZoom(2700);
    centerOn(markPos.getLonX(), markPos.getLatY(), false);
  }
}

void NavMapWidget::showHome()
{
  qDebug() << "NavMapWidget::showHome" << markPos;
  if(homeZoom != -1)
    setZoom(homeZoom);

  if(homePos.isValid())
    centerOn(homePos.getLonX(), homePos.getLatY(), false);
}

void NavMapWidget::changeMark(const atools::geo::Pos& pos)
{
  markPos = pos;
  emit markChanged(markPos);

  update();
}

void NavMapWidget::changeRouteHighlight(const QList<RouteMapObject>& routeHighlight)
{
  routeHighlightMapObjects = routeHighlight;
  update();
}

void NavMapWidget::routeChanged()
{
  paintLayer->routeChanged();
  updateRouteScreenLines();
  update();
}

void NavMapWidget::changeHighlight(const maptypes::MapSearchResult& positions)
{
  highlightMapObjects = positions;
  update();
}

void NavMapWidget::changeHome()
{
  homePos = atools::geo::Pos(centerLongitude(), centerLatitude());
  homeZoom = zoom();
  update();
}

void NavMapWidget::updateRouteFromDrag(QPoint newPoint, MouseStates state, int leg, int point)
{
  qDebug() << "End route drag" << newPoint << "state" << state << "leg" << leg << "point" << point;

  maptypes::MapSearchResult result;
  getAllNearestMapObjects(newPoint.x(), newPoint.y(), 10, result);

  CoordinateConverter conv(viewport());

  // Get objects from cache - alread present objects will be skipped
  mapQuery->getNearestObjects(conv, paintLayer->getMapLayer(), false,
                              paintLayer->getShownMapFeatures() &
                              (maptypes::AIRPORT_ALL | maptypes::VOR | maptypes::NDB | maptypes::WAYPOINT),
                              newPoint.x(), newPoint.y(), 10, result);

  int totalSize = result.airports.size() + result.vors.size() + result.ndbs.size() + result.waypoints.size();

  atools::geo::Pos pos = conv.sToW(newPoint.x(), newPoint.y());
  if(totalSize == 0)
    // Add userpoint
    qDebug() << "add userpoint";
  else if(totalSize == 1)
    // Add single navaid
    qDebug() << "add navaid";
  else
  {
    qDebug() << "add navaids" << totalSize;
    QMenu menu;
    menu.addAction(new QAction("What?", this));
    menu.addAction(new QAction("Else?", this));

    QAction *action = menu.exec(QCursor::pos());
  }

}

void NavMapWidget::contextMenu(const QPoint& point)
{
  Q_UNUSED(point);
  qInfo() << "tableContextMenu";

  if(mouseState != NONE)
    return;

  Ui::MainWindow *ui = parentWindow->getUi();

  atools::gui::ActionTextSaver textSaver({ui->actionMapMeasureDistance, ui->actionMapMeasureRhumbDistance,
                                          ui->actionMapRangeRings, ui->actionMapNavaidRange,
                                          ui->actionShowInSearch, ui->actionRouteAdd,
                                          ui->actionRouteDeleteWaypoint, ui->actionRouteAirportStart,
                                          ui->actionRouteAirportDest, ui->actionRouteParkingStart});
  Q_UNUSED(textSaver);

  QMenu menu;
  menu.addAction(ui->actionMapSetMark);
  menu.addAction(ui->actionMapSetHome);
  menu.addSeparator();
  menu.addAction(ui->actionMapMeasureDistance);
  menu.addAction(ui->actionMapMeasureRhumbDistance);
  menu.addAction(ui->actionMapHideDistanceMarker);
  menu.addSeparator();
  menu.addAction(ui->actionMapRangeRings);
  menu.addAction(ui->actionMapNavaidRange);
  menu.addAction(ui->actionMapHideOneRangeRing);
  menu.addAction(ui->actionMapHideRangeRings);
  menu.addSeparator();
  menu.addAction(ui->actionRouteAdd);
  menu.addAction(ui->actionRouteDeleteWaypoint);
  menu.addSeparator();
  menu.addAction(ui->actionRouteAirportStart);
  menu.addAction(ui->actionRouteParkingStart);
  menu.addAction(ui->actionRouteAirportDest);
  menu.addSeparator();
  menu.addAction(ui->actionShowInSearch);

  int distMarkerIndex = getNearestDistanceMarkerIndex(point.x(), point.y(), 10);
  int rangeMarkerIndex = getNearestRangeMarkerIndex(point.x(), point.y(), 10);

  qreal lon, lat;
  bool visible = geoCoordinates(point.x(), point.y(), lon, lat);
  atools::geo::Pos pos(lon, lat);

  // Disable all menu items that depend on position
  ui->actionMapSetMark->setEnabled(visible);
  ui->actionMapSetHome->setEnabled(visible);
  ui->actionMapMeasureDistance->setEnabled(visible);
  ui->actionMapMeasureRhumbDistance->setEnabled(visible);
  ui->actionMapRangeRings->setEnabled(visible);
  ui->actionMapNavaidRange->setEnabled(visible);

  ui->actionMapHideRangeRings->setEnabled(!rangeMarkers.isEmpty() || !distanceMarkers.isEmpty());
  ui->actionMapHideOneRangeRing->setEnabled(visible && rangeMarkerIndex != -1);
  ui->actionMapHideDistanceMarker->setEnabled(visible && distMarkerIndex != -1);

  ui->actionMapNavaidRange->setEnabled(false);
  ui->actionShowInSearch->setEnabled(false);
  ui->actionRouteAdd->setEnabled(true);
  ui->actionRouteAirportStart->setEnabled(false);
  ui->actionRouteAirportDest->setEnabled(false);
  ui->actionRouteDeleteWaypoint->setEnabled(false);
  ui->actionRouteParkingStart->setEnabled(false);

  maptypes::MapSearchResult result;
  getAllNearestMapObjects(point.x(), point.y(), 10, result);
  // maptypes::MapObjectTypes selectedSearchType = maptypes::NONE, selectedRangeType = maptypes::NONE;
  maptypes::MapAirport *airport = nullptr;
  maptypes::MapVor *vor = nullptr;
  maptypes::MapNdb *ndb = nullptr;
  maptypes::MapWaypoint *waypoint = nullptr;
  maptypes::MapUserpoint *userpoint = nullptr;
  maptypes::MapParking *parking = nullptr;

  if(!result.airports.isEmpty())
    airport = &result.airports.first();
  if(!result.parkings.isEmpty() && result.parkings.first().type != "FUEL")
    parking = &result.parkings.first();
  if(!result.vors.isEmpty())
    vor = &result.vors.first();
  if(!result.ndbs.isEmpty())
    ndb = &result.ndbs.first();
  if(!result.waypoints.isEmpty())
    waypoint = &result.waypoints.first();
  if(!result.userPoints.isEmpty())
    userpoint = &result.userPoints.first();

  QString searchText;
  // Collect information from the search result
  if(airport != nullptr)
    searchText = maptypes::airportText(*airport);
  else if(parking != nullptr)
    searchText = maptypes::parkingName(parking->name) + " " + QString::number(parking->number);
  else if(vor != nullptr)
    searchText = maptypes::vorText(*vor);
  else if(ndb != nullptr)
    searchText = maptypes::ndbText(*ndb);
  else if(waypoint != nullptr)
    searchText = maptypes::waypointText(*waypoint);
  else if(userpoint != nullptr)
    searchText = maptypes::userpointText(*userpoint);

  int delRouteIndex = -1;
  maptypes::MapObjectTypes delType = maptypes::NONE;
  QString delSearchText;
  // Collect information from the search result
  if(airport != nullptr && airport->routeIndex != -1)
  {
    delSearchText = maptypes::airportText(*airport);
    delRouteIndex = airport->routeIndex;
    delType = maptypes::AIRPORT;
  }
  else if(vor != nullptr && vor->routeIndex != -1)
  {
    delSearchText = maptypes::vorText(*vor);
    delRouteIndex = vor->routeIndex;
    delType = maptypes::VOR;
  }
  else if(ndb != nullptr && ndb->routeIndex != -1)
  {
    delSearchText = maptypes::ndbText(*ndb);
    delRouteIndex = ndb->routeIndex;
    delType = maptypes::NDB;
  }
  else if(waypoint != nullptr && waypoint->routeIndex != -1)
  {
    delSearchText = maptypes::waypointText(*waypoint);
    delRouteIndex = waypoint->routeIndex;
    delType = maptypes::WAYPOINT;
  }
  else if(userpoint != nullptr && userpoint->routeIndex != -1)
  {
    delSearchText = maptypes::userpointText(*userpoint);
    delRouteIndex = userpoint->routeIndex;
    delType = maptypes::USER;
  }

  // Update "set airport as start/dest"
  if(airport != nullptr)
  {
    ui->actionRouteAirportStart->setEnabled(true);
    ui->actionRouteAirportStart->setText(ui->actionRouteAirportStart->text().arg(searchText));
    ui->actionRouteAirportDest->setEnabled(true);
    ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(searchText));
  }
  else
  {
    ui->actionRouteAirportStart->setText(ui->actionRouteAirportStart->text().arg(QString()));
    ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(QString()));
  }

  // Update "set parking start"
  if(parking != nullptr)
  {
    ui->actionRouteParkingStart->setEnabled(true);
    ui->actionRouteParkingStart->setText(ui->actionRouteParkingStart->text().arg(searchText));
  }
  else
    ui->actionRouteParkingStart->setText(ui->actionRouteParkingStart->text().arg(QString()));

  // Update "show in search" and "add to route"
  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || airport != nullptr)
  {
    ui->actionShowInSearch->setEnabled(true);
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(searchText));
    ui->actionRouteAdd->setText(ui->actionRouteAdd->text().arg(searchText));
  }
  else
  {
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(QString()));
    ui->actionRouteAdd->setText(ui->actionRouteAdd->text().arg("Position"));
  }

  // Update "delete in route"
  if(delRouteIndex != -1)
  {
    ui->actionRouteDeleteWaypoint->setEnabled(true);
    ui->actionRouteDeleteWaypoint->setText(ui->actionRouteDeleteWaypoint->text().arg(delSearchText));
  }
  else
    ui->actionRouteDeleteWaypoint->setText(ui->actionRouteDeleteWaypoint->text().arg(QString()));

  if(airport != nullptr)
  {
    ui->actionRouteAirportStart->setText(ui->actionRouteAirportStart->text().arg(QString()));
    ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(QString()));
  }

  // Update "show range rings for Navaid"
  if(vor != nullptr)
  {
    ui->actionMapNavaidRange->setEnabled(true);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(searchText));
  }
  else if(ndb != nullptr)
  {
    ui->actionMapNavaidRange->setEnabled(true);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(searchText));
  }
  else
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(QString()));

  if(parking == nullptr && !searchText.isEmpty())
  {
    ui->actionMapMeasureDistance->setText(ui->actionMapMeasureDistance->text().arg(searchText));
    ui->actionMapMeasureRhumbDistance->setText(ui->actionMapMeasureRhumbDistance->text().arg(searchText));
  }
  else
  {
    ui->actionMapMeasureDistance->setText(ui->actionMapMeasureDistance->text().arg("here"));
    ui->actionMapMeasureRhumbDistance->setText(ui->actionMapMeasureRhumbDistance->text().arg("here"));
  }

  // Show the menu ------------------------------------------------
  QAction *action = menu.exec(QCursor::pos());

  if(action == ui->actionMapHideRangeRings)
  {
    rangeMarkers.clear();
    distanceMarkers.clear();
    currentDistanceMarkerIndex = -1;
    update();
  }

  if(visible)
  {
    if(action == ui->actionShowInSearch)
    {
      if(airport != nullptr)
      {
        ui->dockWidgetSearch->raise();
        ui->tabWidgetSearch->setCurrentIndex(0);
        emit objectSelected(maptypes::AIRPORT, airport->ident, QString(), QString());
      }
      else if(vor != nullptr)
      {
        ui->dockWidgetSearch->raise();
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit objectSelected(maptypes::VOR, vor->ident, vor->region, vor->apIdent);
      }
      else if(ndb != nullptr)
      {
        ui->dockWidgetSearch->raise();
        ui->tabWidgetSearch->setCurrentIndex(1);
        // TODO check airport ident (probably not loaded)
        emit objectSelected(maptypes::NDB, ndb->ident, ndb->region, ndb->apIdent);
      }
      else if(waypoint != nullptr)
      {
        ui->dockWidgetSearch->raise();
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit objectSelected(maptypes::WAYPOINT, waypoint->ident, waypoint->region, waypoint->apIdent);
      }
    }
    else if(action == ui->actionMapNavaidRange)
    {
      if(vor != nullptr)
        addNavRangeRing(vor->position, maptypes::VOR, vor->ident, vor->frequency, vor->range);
      else if(ndb != nullptr)
        addNavRangeRing(ndb->position, maptypes::NDB, ndb->ident, ndb->frequency, ndb->range);
    }
    else if(action == ui->actionMapRangeRings)
      addRangeRing(pos);
    else if(action == ui->actionMapSetMark)
      changeMark(pos);
    else if(action == ui->actionMapHideOneRangeRing)
    {
      rangeMarkers.removeAt(rangeMarkerIndex);
      update();
    }
    else if(action == ui->actionMapHideDistanceMarker)
    {
      distanceMarkers.removeAt(distMarkerIndex);
      update();
    }
    else if(action == ui->actionMapMeasureDistance || action == ui->actionMapMeasureRhumbDistance)
    {
      // Distance line
      maptypes::DistanceMarker dm;

      if(vor != nullptr)
      {
        dm.text = vor->ident + " " + formatter::formatDoubleUnit(vor->frequency / 1000., QString(), 2);
        dm.from = vor->position;
        dm.magvar = vor->magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::vorSymbolColor;
        dm.type = maptypes::VOR;
      }
      else if(ndb != nullptr)
      {
        dm.text = ndb->ident + " " + formatter::formatDoubleUnit(ndb->frequency / 100., QString(), 2);
        dm.from = ndb->position;
        dm.magvar = ndb->magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::ndbSymbolColor;
        dm.type = maptypes::NDB;
      }
      else if(waypoint != nullptr)
      {
        dm.text = waypoint->ident;
        dm.from = waypoint->position;
        dm.magvar = waypoint->magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::waypointSymbolColor;
        dm.type = maptypes::WAYPOINT;
      }
      else if(airport != nullptr)
      {
        dm.text = airport->name + " (" + airport->ident + ")";
        dm.from = airport->position;
        dm.magvar = airport->magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::colorForAirport(*airport);
        dm.type = maptypes::AIRPORT;
      }
      else
      {
        dm.hasMagvar = false;
        dm.from = pos;
        dm.color = dm.rhumbLine ? mapcolors::distanceColor : mapcolors::distanceRhumbColor;
      }

      dm.rhumbLine = action == ui->actionMapMeasureRhumbDistance;
      dm.to = pos;
      distanceMarkers.append(dm);

      mouseState = DRAG_DISTANCE;
      setContextMenuPolicy(Qt::NoContextMenu);
      currentDistanceMarkerIndex = distanceMarkers.size() - 1;
    }
    else if(action == ui->actionRouteDeleteWaypoint)
    {
      ui->dockWidgetRoute->raise();
      emit routeDelete(delRouteIndex, delType);
    }
    else if(action == ui->actionRouteAdd || action == ui->actionRouteAirportStart ||
            action == ui->actionRouteAirportDest || action == ui->actionRouteParkingStart)
    {
      ui->dockWidgetRoute->raise();

      atools::geo::Pos position = pos;
      maptypes::MapObjectTypes type;

      int id = -1;
      if(airport != nullptr)
      {
        id = airport->id;
        type = maptypes::AIRPORT;
      }
      else if(parking != nullptr)
      {
        id = parking->id;
        type = maptypes::PARKING;
      }
      else if(vor != nullptr)
      {
        id = vor->id;
        type = maptypes::VOR;
      }
      else if(ndb != nullptr)
      {
        id = ndb->id;
        type = maptypes::NDB;
      }
      else if(waypoint != nullptr)
      {
        id = waypoint->id;
        type = maptypes::WAYPOINT;
      }
      else
      {
        if(userpoint != nullptr)
          id = userpoint->id;
        type = maptypes::USER;
        position = pos;
      }

      if(action == ui->actionRouteParkingStart && parking != nullptr)
        emit routeSetParkingStart(*parking);
      else if(action == ui->actionRouteAdd)
      {
        if(parking != nullptr)
        {
          // Adjust values in case user selected add on a parking position
          type = maptypes::USER;
          id = -1;
        }
        emit routeAdd(id, position, type);
      }
      else if(action == ui->actionRouteAirportStart)
        emit routeSetStart(*airport);
      else if(action == ui->actionRouteAirportDest)
        emit routeSetDest(*airport);
    }
  }
}

void NavMapWidget::addNavRangeRing(const atools::geo::Pos& pos, maptypes::MapObjectTypes type,
                                   const QString& ident, int frequency, int range)
{
  maptypes::RangeMarker ring;
  ring.type = type;
  ring.center = pos;

  if(type == maptypes::VOR || type == maptypes::ILS)
    ring.text = ident + " " + formatter::formatDoubleUnit(frequency / 1000., QString(), 2);
  else if(type == maptypes::NDB)
    ring.text = ident + " " + formatter::formatDoubleUnit(frequency / 100., QString(), 2);

  ring.ranges.append(range);
  rangeMarkers.append(ring);
  qDebug() << "navaid range" << ring.center;
  update();
}

void NavMapWidget::addRangeRing(const atools::geo::Pos& pos)
{
  maptypes::RangeMarker rings;
  rings.type = maptypes::NONE;
  rings.center = pos;
  rings.ranges = {50, 100, 200, 500};
  rangeMarkers.append(rings);

  qDebug() << "range rings" << rings.center;
  update();
}

void NavMapWidget::clearRangeRings()
{
  qDebug() << "range rings hide";
  rangeMarkers.clear();
  update();
}

bool NavMapWidget::eventFilter(QObject *obj, QEvent *e)
{
  if(e->type() == QEvent::MouseButtonDblClick)
  {
    // Catch the double click event
    qDebug() << "eventFilter mouseDoubleClickEvent";
    e->accept();
    event(e);
    return true;
  }
  if(e->type() == QEvent::MouseMove && mouseState != NONE)
  {
    // Do not allow mouse scrolling during drag actions
    e->accept();
    event(e);
    return true;
  }
  Marble::MarbleWidget::eventFilter(obj, e);
  return false;
}

void NavMapWidget::mouseMoveEvent(QMouseEvent *event)
{
  if(!isActiveWindow())
    return;

  if(mouseState & DRAG_DISTANCE || mouseState & DRAG_CHANGE_DISTANCE)
  {
    if(cursor().shape() != Qt::CrossCursor)
      setCursor(Qt::CrossCursor);

    qreal lon, lat;
    bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
    if(visible)
    {
      atools::geo::Pos p(lon, lat);
      if(!distanceMarkers.isEmpty())
        distanceMarkers[currentDistanceMarkerIndex].to = p;
    }
    setViewContext(Marble::Animation);
    update();

    // CoordinateConverter conv(viewport());
    // QPoint origin = conv.wToS(distanceMarkers[currentDistanceMarkerIndex].from);
    // QRect clipRect(origin, QPoint(event->pos().x(), event->pos().y()));
    // clipRect = clipRect.normalized();
    // clipRect = clipRect.marginsAdded(QMargins(100, 100, 100, 100));
    // update(clipRect.normalized());
  }
  else if(mouseState & DRAG_ROUTE_LEG || mouseState & DRAG_ROUTE_POINT)
  {
    if(cursor().shape() != Qt::CrossCursor)
      setCursor(Qt::CrossCursor);

    routeDragCur = QPoint(event->pos().x(), event->pos().y());

    setViewContext(Marble::Animation);
    update();
  }
  else if(mouseState == NONE)
    if(event->buttons() == Qt::NoButton)
    {
      const QList<RouteMapObject>& rmos = parentWindow->getRouteController()->getRouteMapObjects();

      if(getNearestRoutePointIndex(event->pos().x(), event->pos().y(), 10) != -1 && rmos.size() > 1)
      {
        // Change cursor at one route point
        if(cursor().shape() != Qt::CrossCursor)
          setCursor(Qt::CrossCursor);
      }
      else if(getNearestRouteLegIndex(event->pos().x(), event->pos().y(), 10) != -1 && rmos.size() > 1)
      {
        // Change cursor above a route line
        if(cursor().shape() != Qt::CrossCursor)
          setCursor(Qt::CrossCursor);
      }
      else if(getNearestDistanceMarkerIndex(event->pos().x(), event->pos().y(), 10) != -1)
      {
        // Change cursor at the end of an marker
        if(cursor().shape() != Qt::CrossCursor)
          setCursor(Qt::CrossCursor);
      }
      else if(cursor().shape() != Qt::ArrowCursor)
        setCursor(Qt::ArrowCursor);
    }
}

void NavMapWidget::mousePressEvent(QMouseEvent *event)
{
  qDebug() << "mousePressEvent";
  if(mouseState == DRAG_DISTANCE || mouseState == DRAG_CHANGE_DISTANCE ||
     mouseState == DRAG_ROUTE_POINT || mouseState == DRAG_ROUTE_LEG)
  {
    setCursor(Qt::ArrowCursor);
    if(event->button() == Qt::LeftButton)
      mouseState |= DRAG_POST;
    else if(event->button() == Qt::RightButton)
      mouseState |= DRAG_POST_CANCEL;
  }
  else if(mouseState == NONE)
    if(event->button() == Qt::RightButton)
      setContextMenuPolicy(Qt::CustomContextMenu);
}

void NavMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
  Q_UNUSED(event);
  qDebug() << "mouseReleaseEvent";

  if(mouseState & DRAG_ROUTE_POINT || mouseState & DRAG_ROUTE_LEG)
  {
    if(mouseState & DRAG_POST)
      updateRouteFromDrag(routeDragCur, mouseState, routeDragLeg, routeDragPoint);

    // End all other dragging
    mouseState = NONE;
    routeDragCur = QPoint();
    routeDragFrom = atools::geo::EMPTY_POS;
    routeDragTo = atools::geo::EMPTY_POS;
    routeDragPoint = -1;
    routeDragLeg = -1;
    setViewContext(Marble::Still);
    update();
  }

  if(mouseState & DRAG_DISTANCE || mouseState & DRAG_CHANGE_DISTANCE)
  {
    if(!distanceMarkers.isEmpty())
    {
      setCursor(Qt::ArrowCursor);
      if(mouseState & DRAG_POST)
      {
        qreal lon, lat;
        bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
        if(visible)
          distanceMarkers[currentDistanceMarkerIndex].to = atools::geo::Pos(lon, lat);

        // atools::geo::Pos from = distanceMarkers[currentDistanceMarkerIndex].from;
        // atools::geo::Pos to = distanceMarkers[currentDistanceMarkerIndex].to;
        // const ElevationModel *localElevationModel = model()->elevationModel();
        // QList<GeoDataCoordinates> elev = localElevationModel->heightProfile(from.getLonX(), from.getLatY(),
        // to.getLonX(), to.getLatY());
        // for(const GeoDataCoordinates& e : elev)
        // qDebug() << e.altitude();
        // qDebug() << "from height" << localElevationModel->height(from.getLonX(), from.getLatY());
        // qDebug() << "to height" << localElevationModel->height(to.getLonX(), to.getLatY());
      }
      else if(mouseState & DRAG_POST_CANCEL)
      {
        if(mouseState & DRAG_DISTANCE)
          // Remove new one
          distanceMarkers.removeAt(currentDistanceMarkerIndex);
        else if(mouseState & DRAG_CHANGE_DISTANCE)
          // Replace modified one with backup
          distanceMarkers[currentDistanceMarkerIndex] = distanceMarkerBackup;
        currentDistanceMarkerIndex = -1;
      }
    }
    mouseState = NONE;
    setViewContext(Marble::Still);
    update();
  }
  else if(mouseState != NONE)
  {
    // End all other dragging
    setViewContext(Marble::Still);
    mouseState = NONE;
    update();
  }
  else if(event->button() == Qt::LeftButton)
  {
    // Start all dragging
    currentDistanceMarkerIndex = getNearestDistanceMarkerIndex(event->pos().x(), event->pos().y(), 10);
    if(currentDistanceMarkerIndex != -1)
    {
      // Found an end - create a backup and start dragging
      mouseState = DRAG_CHANGE_DISTANCE;
      distanceMarkerBackup = distanceMarkers.at(currentDistanceMarkerIndex);
      setContextMenuPolicy(Qt::NoContextMenu);
    }
    else
    {
      const QList<RouteMapObject>& rmos = parentWindow->getRouteController()->getRouteMapObjects();

      if(rmos.size() > 1)
      {
        int routePoint = getNearestRoutePointIndex(event->pos().x(), event->pos().y(), 10);
        if(routePoint != -1)
        {
          routeDragPoint = routePoint;
          qDebug() << "route point" << routePoint;

          // Found a leg - start dragging
          mouseState = DRAG_ROUTE_POINT;

          routeDragCur = QPoint(event->pos().x(), event->pos().y());

          if(routePoint > 0)
            routeDragFrom = rmos.at(routePoint - 1).getPosition();
          else
            routeDragFrom = atools::geo::EMPTY_POS;

          if(routePoint < rmos.size() - 1)
            routeDragTo = rmos.at(routePoint + 1).getPosition();
          else
            routeDragTo = atools::geo::EMPTY_POS;
        }
        else
        {
          int routeLeg = getNearestRouteLegIndex(event->pos().x(), event->pos().y(), 10);
          if(routeLeg != -1)
          {
            routeDragLeg = routeLeg;
            qDebug() << "route leg" << routeLeg;
            // Found a leg - start dragging
            mouseState = DRAG_ROUTE_LEG;

            routeDragCur = QPoint(event->pos().x(), event->pos().y());

            routeDragFrom = rmos.at(routeLeg).getPosition();
            routeDragTo = rmos.at(routeLeg + 1).getPosition();
          }
        }
      }
    }
  }
}

void NavMapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  qDebug() << "mouseDoubleClickEvent";

  maptypes::MapSearchResult mapSearchResult;
  getAllNearestMapObjects(event->pos().x(), event->pos().y(), 10, mapSearchResult);

  if(!mapSearchResult.airports.isEmpty())
  {
    if(mapSearchResult.airports.at(0).bounding.isPoint())
      showPos(mapSearchResult.airports.at(0).bounding.getTopLeft());
    else
      showRect(mapSearchResult.airports.at(0).bounding);
  }
  else if(!mapSearchResult.vors.isEmpty())
    showPos(mapSearchResult.vors.at(0).position);
  else if(!mapSearchResult.ndbs.isEmpty())
    showPos(mapSearchResult.ndbs.at(0).position);
  else if(!mapSearchResult.waypoints.isEmpty())
    showPos(mapSearchResult.waypoints.at(0).position);
  else if(!mapSearchResult.userPoints.isEmpty())
    showPos(mapSearchResult.userPoints.at(0).position);
}

bool NavMapWidget::event(QEvent *event)
{
  if(event->type() == QEvent::ToolTip)
  {
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);

    maptypes::MapSearchResult mapSearchResult;
    getAllNearestMapObjects(helpEvent->pos().x(), helpEvent->pos().y(), 10, mapSearchResult);
    QString text = mapTooltip->buildTooltip(mapSearchResult,
                                            parentWindow->getRouteController()->getRouteMapObjects(),
                                            paintLayer->getMapLayer()->isAirportDiagram());

    if(!text.isEmpty())
      QToolTip::showText(helpEvent->globalPos(), text.trimmed(), nullptr, QRect(), 3600 * 1000);
    else
      QToolTip::hideText();
    event->accept();

    return true;
  }

  return QWidget::event(event);
}

void NavMapWidget::paintEvent(QPaintEvent *paintEvent)
{
  bool changed = false;
  const GeoDataLatLonAltBox visibleLatLonAltBox = viewport()->viewLatLonAltBox();
  if(viewContext() == Marble::Still && (zoom() != curZoom || visibleLatLonAltBox != curBox))
  {
    curZoom = zoom();
    curBox = visibleLatLonAltBox;

    qDebug() << "paintEvent map view has changed zoom" << curZoom
             << "distance" << distance() << " (" << atools::geo::meterToNm(distance() * 1000.) << " km)";
    qDebug() << "pole" << curBox.containsPole() << curBox.toString(GeoDataCoordinates::Degree);
    history.addEntry(atools::geo::Pos(centerLongitude(), centerLatitude()), zoom());
    parentWindow->clearMessageText();
    changed = true;
  }

  MarbleWidget::paintEvent(paintEvent);

  if(changed)
    updateRouteScreenLines();

  // if(viewContext() == Marble::Still)
  // {
  // model()->setWorkOffline(false);
  // qDebug() << "Offline false";
  // // update();
  // }
  // else
  // model()->setWorkOffline(true);

}

void NavMapWidget::updateRouteScreenLines()
{
  using atools::geo::Pos;

  const QList<RouteMapObject>& routeMapObjects = parentWindow->getRouteController()->getRouteMapObjects();
  CoordinateConverter conv(viewport());

  routeScreenLines.clear();
  routeScreenPoints.clear();

  const MapScale *scale = paintLayer->getMapScale();
  if(scale->isValid())
  {
    Pos p1;

    for(int i = 0; i < routeMapObjects.size(); i++)
    {
      const Pos& p2 = routeMapObjects.at(i).getPosition();
      int x2, y2;
      conv.wToS(p2, x2, y2);
      routeScreenPoints.append(QPoint(x2, y2));

      if(p1.isValid())
      {
        float distanceMeter = p2.distanceMeterTo(p1);
        // Approximate the needed number of line segments
        int pixel = scale->getPixelIntForMeter(distanceMeter);
        int numSegments = std::min(std::max(pixel / 20, 4), 72);

        for(int j = 1; j < numSegments - 1; j++)
        {
          Pos pa = p1.interpolate(p2, distanceMeter, 1.f / numSegments * (j - 1));
          Pos pb = p1.interpolate(p2, distanceMeter, 1.f / numSegments * j);

          int xs1, ys1, xs2, ys2;
          bool va = conv.wToS(pa, xs1, ys1);
          bool vb = conv.wToS(pb, xs2, ys2);

          if(va || vb)
            routeScreenLines.append(std::make_pair(i - 1, QLine(xs1, ys1, xs2, ys2)));
        }
      }
      p1 = p2;
    }
  }
}

void NavMapWidget::getAllNearestMapObjects(int xs, int ys, int screenDistance,
                                           maptypes::MapSearchResult& mapSearchResult)
{
  CoordinateConverter conv(viewport());
  const MapLayer *mapLayer = paintLayer->getMapLayer();
  const MapLayer *mapLayerEffective = paintLayer->getMapLayerEffective();

  // Get copies from route
  getNearestRouteMapObjects(xs, ys, screenDistance, parentWindow->getRouteController()->getRouteMapObjects(),
                            mapSearchResult);

  // Get copies from highlightMapObjects
  getNearestHighlightMapObjects(xs, ys, screenDistance, mapSearchResult);

  // Get objects from cache - alread present objects will be skipped
  mapQuery->getNearestObjects(conv, mapLayer, mapLayerEffective->isAirportDiagram(),
                              paintLayer->getShownMapFeatures() &
                              (maptypes::AIRPORT_ALL | maptypes::VOR | maptypes::NDB | maptypes::WAYPOINT |
                               maptypes::MARKER | maptypes::AIRWAYJ | maptypes::AIRWAYV),
                              xs, ys, screenDistance, mapSearchResult);

  // Update all incomplete objects, especially from search
  for(maptypes::MapAirport& obj : mapSearchResult.airports)
    if(!obj.complete())
      mapQuery->getAirportById(obj, obj.getId());
}

void NavMapWidget::getNearestRouteMapObjects(int xs, int ys, int screenDistance,
                                             const QList<RouteMapObject>& routeMapObjects,
                                             maptypes::MapSearchResult& mapobjects)
{
  using maptools::insertSortedByDistance;

  CoordinateConverter conv(viewport());
  int x, y;

  int i = 0;
  for(const RouteMapObject& obj : routeMapObjects)
  {
    if(conv.wToS(obj.getPosition(), x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
      {
        switch(obj.getMapObjectType())
        {
          case maptypes::VOR :
            {
              maptypes::MapVor vor = obj.getVor();
              vor.routeIndex = i;
              insertSortedByDistance(conv, mapobjects.vors, &mapobjects.vorIds, xs, ys, vor);
            }
            break;
          case maptypes::WAYPOINT:
            {
              maptypes::MapWaypoint wp = obj.getWaypoint();
              wp.routeIndex = i;
              insertSortedByDistance(conv, mapobjects.waypoints, &mapobjects.waypointIds, xs, ys, wp);
            }
            break;
          case maptypes::NDB:
            {
              maptypes::MapNdb ndb = obj.getNdb();
              ndb.routeIndex = i;
              insertSortedByDistance(conv, mapobjects.ndbs, &mapobjects.ndbIds, xs, ys, ndb);
            }
            break;
          case maptypes::AIRPORT:
            {
              maptypes::MapAirport ap = obj.getAirport();
              ap.routeIndex = i;
              insertSortedByDistance(conv, mapobjects.airports, &mapobjects.airportIds, xs, ys, ap);
            }
            break;
          case maptypes::INVALID:
            {
              maptypes::MapUserpoint up;
              up.routeIndex = i;
              up.name = obj.getIdent() + " (not found)";
              up.position = obj.getPosition();
              mapobjects.userPoints.append(up);
            }
            break;
          case maptypes::USER:
            {
              maptypes::MapUserpoint up;
              up.id = i;
              up.routeIndex = i;
              up.name = obj.getIdent();
              up.position = obj.getPosition();
              mapobjects.userPoints.append(up);
            }
            break;
        }
      }
    i++;
  }
}

void NavMapWidget::getNearestHighlightMapObjects(int xs, int ys, int screenDistance,
                                                 maptypes::MapSearchResult& mapobjects)
{
  using namespace maptypes;

  CoordinateConverter conv(viewport());
  int x, y;

  using maptools::insertSortedByDistance;

  for(const maptypes::MapAirport& obj : highlightMapObjects.airports)
    if(conv.wToS(obj.position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
        insertSortedByDistance(conv, mapobjects.airports, &mapobjects.airportIds, xs, ys, obj);

  for(const maptypes::MapVor& obj : highlightMapObjects.vors)
    if(conv.wToS(obj.position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
        insertSortedByDistance(conv, mapobjects.vors, &mapobjects.vorIds, xs, ys, obj);

  for(const maptypes::MapNdb& obj : highlightMapObjects.ndbs)
    if(conv.wToS(obj.position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
        insertSortedByDistance(conv, mapobjects.ndbs, &mapobjects.ndbIds, xs, ys, obj);

  for(const maptypes::MapWaypoint& obj : highlightMapObjects.waypoints)
    if(conv.wToS(obj.position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
        insertSortedByDistance(conv, mapobjects.waypoints, &mapobjects.waypointIds, xs, ys, obj);
}

int NavMapWidget::getNearestDistanceMarkerIndex(int xs, int ys, int screenDistance)
{
  CoordinateConverter conv(viewport());
  int index = 0;
  int x, y;
  for(const maptypes::DistanceMarker& marker : distanceMarkers)
  {
    if(conv.wToS(marker.to, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
        return index;

    index++;
  }
  return -1;
}

int NavMapWidget::getNearestRoutePointIndex(int xs, int ys, int screenDistance)
{
  int minIndex = -1;
  int minDist = std::numeric_limits<int>::max();

  for(int i = 0; i < routeScreenPoints.size(); i++)
  {
    const QPoint& point = routeScreenPoints.at(i);

    int dist = atools::geo::manhattanDistance(point.x(), point.y(), xs, ys);
    if(dist < minDist && dist < screenDistance)
    {
      minDist = dist;
      minIndex = i;
    }
  }
  return minIndex;
}

int NavMapWidget::getNearestRouteLegIndex(int xs, int ys, int screenDistance)
{
  int minIndex = -1;
  float minDist = std::numeric_limits<float>::max();

  for(int i = 0; i < routeScreenLines.size(); i++)
  {
    const std::pair<int, QLine>& line = routeScreenLines.at(i);
    QVector2D vec(line.second.dx(), line.second.dy());
    QVector2D p(xs - line.second.x1(), ys - line.second.y1());

    float dist = vec.distanceToPoint(p);
    if(dist < minDist && dist < screenDistance)
    {
      minDist = dist;
      minIndex = line.first;
    }
  }
  return minIndex;
}

int NavMapWidget::getNearestRangeMarkerIndex(int xs, int ys, int screenDistance)
{
  CoordinateConverter conv(viewport());
  int index = 0;
  int x, y;
  for(const maptypes::RangeMarker& marker : rangeMarkers)
  {
    if(conv.wToS(marker.center, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
        return index;

    index++;
  }
  return -1;
}
