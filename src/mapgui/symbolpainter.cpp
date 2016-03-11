#include "mapquery.h"
#include "symbolpainter.h"
#include "common/mapcolors.h"

#include <QPainter>

SymbolPainter::SymbolPainter()
{
}

void SymbolPainter::drawAirportSymbol(QPainter *painter, const MapAirport& ap, int x, int y, int size,
                                      bool isAirportDiagram, bool fast)
{
  painter->save();
  QColor apColor = mapcolors::colorForAirport(ap);

  int radius = size / 2;
  painter->setBackgroundMode(Qt::OpaqueMode);

  if(ap.isSet(HARD) && !ap.isSet(MIL) && !ap.isSet(CLOSED))
    // Use filled circle
    painter->setBrush(QBrush(apColor));
  else
    // Use white filled circle
    painter->setBrush(QBrush(mapcolors::airportSymbolFillColor));

  if(!fast || isAirportDiagram)
    if(ap.isSet(FUEL) && !ap.isSet(MIL) && !ap.isSet(CLOSED) && size > 6)
    {
      // Draw fuel spikes
      int fuelRadius = static_cast<int>(std::round(static_cast<double>(radius) * 1.4));
      if(fuelRadius < radius + 2)
        fuelRadius = radius + 2;
      painter->setPen(QPen(QBrush(apColor), size / 4, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x, y - fuelRadius, x, y + fuelRadius);
      painter->drawLine(x - fuelRadius, y, x + fuelRadius, y);
    }

  painter->setPen(QPen(QBrush(apColor), size / 5, Qt::SolidLine, Qt::FlatCap));
  painter->drawEllipse(QPoint(x, y), radius, radius);

  if(!fast || isAirportDiagram)
  {
    if(ap.isSet(MIL))
      painter->drawEllipse(QPoint(x, y), radius / 2, radius / 2);

    if(ap.waterOnly() && size > 6)
    {
      int lineWidth = size / 7;
      painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x - lineWidth / 4, y - radius / 2, x - lineWidth / 4, y + radius / 2);
      painter->drawArc(x - radius / 2, y - radius / 2, radius, radius, 0 * 16, -180 * 16);
    }

    if(ap.isHeliport() && size > 6)
    {
      int lineWidth = size / 7;
      painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x - lineWidth / 4 - size / 5, y - radius / 2,
                        x - lineWidth / 4 - size / 5, y + radius / 2);

      painter->drawLine(x - lineWidth / 4 - size / 5, y,
                        x - lineWidth / 4 + size / 5, y);

      painter->drawLine(x - lineWidth / 4 + size / 5, y - radius / 2,
                        x - lineWidth / 4 + size / 5, y + radius / 2);
    }

    if(ap.isSet(CLOSED) && size > 6)
    {
      // Cross out whatever was painted before
      painter->setPen(QPen(QBrush(apColor), size / 7, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x - radius, y - radius, x + radius, y + radius);
      painter->drawLine(x - radius, y + radius, x + radius, y - radius);
    }
  }

  if(!fast || isAirportDiagram)
    if(ap.isSet(HARD) && !ap.isSet(MIL) && !ap.isSet(CLOSED) && size > 6)
    {
      painter->translate(x, y);
      painter->rotate(ap.longestRunwayHeading);
      painter->setPen(QPen(QBrush(mapcolors::airportSymbolFillColor), size / 5, Qt::SolidLine, Qt::RoundCap));
      painter->drawLine(0, -radius + 2, 0, radius - 2);
      painter->resetTransform();
    }

  painter->restore();
}

void SymbolPainter::drawWaypointSymbol(QPainter *painter, const MapWaypoint& ap, int x, int y, int size,
                                       bool fast)
{
  painter->save();
  painter->setBrush(QColor(Qt::green));
  painter->setPen(QPen(QColor(Qt::green), size, Qt::SolidLine, Qt::RoundCap));
  painter->drawPoint(x, y);
  painter->restore();
}

void SymbolPainter::drawVorSymbol(QPainter *painter, const MapVor& ap, int x, int y, int size, bool fast)
{
  painter->save();
  painter->setBrush(QColor(Qt::blue));
  painter->setPen(QPen(QColor(Qt::blue), size, Qt::SolidLine, Qt::RoundCap));
  painter->drawPoint(x, y);
  painter->restore();
}

void SymbolPainter::drawNdbSymbol(QPainter *painter, const MapNdb& ap, int x, int y, int size, bool fast)
{
  painter->save();
  painter->setBrush(QColor(Qt::darkRed));
  painter->setPen(QPen(QColor(Qt::darkRed), size, Qt::SolidLine, Qt::RoundCap));
  painter->drawPoint(x, y);
  painter->restore();
}