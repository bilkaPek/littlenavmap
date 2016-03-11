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

#ifndef MAPPAINTERAIRPORT_H
#define MAPPAINTERAIRPORT_H

#include "mapgui/mappainter.h"

#include "mapquery.h"

class SymbolPainter;

class MapPainterAirport :
  public MapPainter
{
public:
  MapPainterAirport(Marble::MarbleWidget *marbleWidget, MapQuery *mapQuery, MapScale *mapScale);
  virtual ~MapPainterAirport();

  virtual void paint(const MapLayer *mapLayer, Marble::GeoPainter *painter,
                     Marble::ViewportParams *viewport) override;

private:
  void airportSymbol(Marble::GeoPainter *painter, const MapAirport& ap, int x, int y,
                     const MapLayer *mapLayer,
                     bool fast);
  void textBox(Marble::GeoPainter *painter, const MapAirport& ap, const QStringList& texts,
               const QPen& pen, int x, int y, bool transparent);

  void airportDiagram(const MapLayer *mapLayer, Marble::GeoPainter *painter, const MapAirport& airport,
                      bool fast);

  void runwayCoords(const QList<MapRunway> *rw, QList<QPoint> *centers, QList<QRect> *rects,
                    QList<QRect> *innerRects, QList<QRect> *backRects);

  void airportSymbolOverview(Marble::GeoPainter *painter, const MapAirport& ap, const MapLayer *mapLayer,
                             bool fast);

  QString parkingName(const QString& name);

  QStringList airportTexts(const MapLayer *mapLayer, const MapAirport& airport);

  SymbolPainter *symbolPainter;
};

#endif // MAPPAINTERAIRPORT_H