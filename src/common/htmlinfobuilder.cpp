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

#include "common/htmlinfobuilder.h"

#include "options/optiondata.h"
#include "atools.h"
#include "common/formatter.h"
#include "common/maptypes.h"
#include "common/weatherreporter.h"
#include "fs/bgl/ap/rw/runway.h"
#include "fs/sc/simconnectdata.h"
#include "geo/calculations.h"
#include "info/infoquery.h"
#include "mapgui/mapquery.h"
#include "route/routemapobjectlist.h"
#include "sql/sqlrecord.h"
#include "common/symbolpainter.h"
#include "util/htmlbuilder.h"
#include "util/morsecode.h"
#include "common/unit.h"

#include <QSize>

using namespace maptypes;
using atools::sql::SqlRecord;
using atools::sql::SqlRecordVector;
using formatter::capNavString;
using atools::geo::normalizeCourse;
using atools::geo::opposedCourseDeg;
using atools::capString;
using atools::util::MorseCode;
using atools::util::HtmlBuilder;
using atools::util::html::Flags;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectUserAircraft;

const int SYMBOL_SIZE = 20;
const float HELIPAD_ZOOM_METER = 200.f;

Q_DECLARE_FLAGS(RunwayMarkingFlags, atools::fs::bgl::rw::RunwayMarkings);
Q_DECLARE_OPERATORS_FOR_FLAGS(RunwayMarkingFlags);

HtmlInfoBuilder::HtmlInfoBuilder(MapQuery *mapDbQuery, InfoQuery *infoDbQuery, bool formatInfo,
                                 bool formatPrint)
  : mapQuery(mapDbQuery), infoQuery(infoDbQuery), info(formatInfo), print(formatPrint)
{
  morse = new MorseCode("&nbsp;", "&nbsp;&nbsp;&nbsp;");

  aircraftEncodedIcon = HtmlBuilder::getEncodedImageHref(
    QIcon(":/littlenavmap/resources/icons/aircraft.svg"), QSize(24, 24));
  aircraftGroundEncodedIcon = HtmlBuilder::getEncodedImageHref(
    QIcon(":/littlenavmap/resources/icons/aircraftground.svg"), QSize(24, 24));

  aircraftAiEncodedIcon = HtmlBuilder::getEncodedImageHref(
    QIcon(":/littlenavmap/resources/icons/aircraftai.svg"), QSize(24, 24));
  aircraftAiGroundEncodedIcon = HtmlBuilder::getEncodedImageHref(
    QIcon(":/littlenavmap/resources/icons/aircraftaiground.svg"), QSize(24, 24));
}

HtmlInfoBuilder::~HtmlInfoBuilder()
{
  delete morse;
}

void HtmlInfoBuilder::airportTitle(const MapAirport& airport, HtmlBuilder& html, QColor background) const
{
  html.img(SymbolPainter(background).createAirportIcon(airport, SYMBOL_SIZE),
           QString(), QString(), QSize(SYMBOL_SIZE, SYMBOL_SIZE));
  html.nbsp().nbsp();

  // Adapt title to airport status
  Flags titleFlags = atools::util::html::BOLD;
  if(airport.closed())
    titleFlags |= atools::util::html::STRIKEOUT;
  if(airport.addon())
    titleFlags |= atools::util::html::ITALIC;

  if(info)
  {
    html.text(tr("%1 (%2)").arg(airport.name).arg(airport.ident), titleFlags | atools::util::html::BIG);
    html.nbsp().nbsp();

    if(!print)
      // Add link to map
      html.a(tr("Map"),
             QString("lnm://show?id=%1&type=%2").arg(airport.id).arg(maptypes::AIRPORT));
  }
  else
    html.text(tr("%1 (%2)").arg(airport.name).arg(airport.ident), titleFlags);
}

void HtmlInfoBuilder::airportText(const MapAirport& airport, HtmlBuilder& html,
                                  const RouteMapObjectList *routeMapObjects, WeatherReporter *weather,
                                  QColor background) const
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getAirportInformation(airport.id);

  airportTitle(airport, html, background);
  html.br();

  QString city, state, country;
  mapQuery->getAirportAdminNamesById(airport.id, city, state, country);

  html.table();
  if(routeMapObjects != nullptr && !routeMapObjects->isEmpty() && airport.routeIndex != -1)
  {
    // Add flight plan information if airport is a part of it
    if(airport.routeIndex == 0)
      html.row2(tr("Departure Airport"), QString());
    else if(airport.routeIndex == routeMapObjects->size() - 1)
      html.row2(tr("Destination Airport"), QString());
    else
      html.row2(tr("Flight Plan position:"), locale.toString(airport.routeIndex + 1));
  }

  // Administrative information
  html.row2(tr("City:"), city);
  if(!state.isEmpty())
    html.row2(tr("State or Province:"), state);
  html.row2(tr("Country:"), country);
  html.row2(tr("Elevation:"), Unit::altFeet(airport.getPosition().getAltitude()));
  html.row2(tr("Magvar:"), maptypes::magvarText(airport.magvar));
  if(rec != nullptr)
  {
    // Add rating and coordinates for info panel
    html.row2(tr("Rating:"), atools::ratingString(rec->valueInt("rating"), 5));
    addCoordinates(rec, html);
  }
  html.tableEnd();

  // Create a list of facilities
  if(info)
    head(html, tr("Facilities"));
  html.table();
  QStringList facilities;

  if(airport.closed())
    facilities.append(tr("Closed"));

  if(airport.addon())
    facilities.append(tr("Add-on"));
  if(airport.flags.testFlag(AP_MIL))
    facilities.append(tr("Military"));

  if(airport.apron())
    facilities.append(tr("Aprons"));
  if(airport.taxiway())
    facilities.append(tr("Taxiways"));
  if(airport.towerObject())
    facilities.append(tr("Tower Object"));
  if(airport.parking())
    facilities.append(tr("Parking"));

  if(airport.helipad())
    facilities.append(tr("Helipads"));
  if(airport.flags.testFlag(AP_AVGAS))
    facilities.append(tr("Avgas"));
  if(airport.flags.testFlag(AP_JETFUEL))
    facilities.append(tr("Jetfuel"));

  if(airport.flags.testFlag(AP_APPR))
    facilities.append(tr("Approaches"));

  if(airport.flags.testFlag(AP_ILS))
    facilities.append(tr("ILS"));

  if(airport.vasi())
    facilities.append(tr("VASI"));
  if(airport.als())
    facilities.append(tr("ALS"));
  if(airport.fence())
    facilities.append(tr("Boundary Fence"));

  if(facilities.isEmpty())
    facilities.append(tr("None"));

  html.row2(info ? QString() : tr("Facilities:"), facilities.join(tr(", ")));

  html.tableEnd();

  // Create a list of runway attributes
  if(info)
    head(html, tr("Runways"));
  html.table();
  QStringList runways;

  if(!airport.noRunways())
  {
    if(airport.hard())
      runways.append(tr("Hard"));
    if(airport.soft())
      runways.append(tr("Soft"));
    if(airport.water())
      runways.append(tr("Water"));
    if(airport.closedRunways())
      runways.append(tr("Closed"));
    if(airport.flags.testFlag(AP_LIGHT))
      runways.append(tr("Lighted"));
  }
  else
    runways.append(tr("None"));

  html.row2(info ? QString() : tr("Runways:"), runways.join(tr(", ")));
  html.tableEnd();

  if(!info && !airport.noRunways())
  {
    // Add longest for tooltip
    html.table();
    html.row2(tr("Longest Runway Length:"), Unit::distShortFeet(airport.longestRunwayLength));
    html.tableEnd();
  }

  if(weather != nullptr)
  {
    // Add weather information
    opts::Flags flags = OptionData::instance().getFlags();
    bool showActiveSky = info ? flags & opts::WEATHER_INFO_ACTIVESKY : flags &
                         opts::WEATHER_TOOLTIP_ACTIVESKY;
    bool showNoaa = info ? flags & opts::WEATHER_INFO_NOAA : flags & opts::WEATHER_TOOLTIP_NOAA;
    bool showVatsim = info ? flags & opts::WEATHER_INFO_VATSIM : flags & opts::WEATHER_TOOLTIP_VATSIM;

    QString activeSkyMetar, noaaMetar, vatsimMetar;
    if(showActiveSky)
      activeSkyMetar = weather->getActiveSkyMetar(airport.ident);

    if(showNoaa)
      noaaMetar = weather->getNoaaMetar(airport.ident);

    if(showVatsim)
      vatsimMetar = weather->getVatsimMetar(airport.ident);

    if(!activeSkyMetar.isEmpty() || !noaaMetar.isEmpty() || !vatsimMetar.isEmpty())
    {
      if(info)
        head(html, tr("Weather"));
      html.table();
      if(!activeSkyMetar.isEmpty())
      {
        QString asText(tr("Active Sky"));
        if(weather->getCurrentActiveSkyType() == WeatherReporter::AS16)
          asText = tr("AS16");
        else if(weather->getCurrentActiveSkyType() == WeatherReporter::ASN)
          asText = tr("ASN");

        html.row2(asText + (info ? tr(":") : tr(" METAR:")), activeSkyMetar);
      }
      if(!noaaMetar.isEmpty())
        html.row2(QString(tr("NOAA")) + (info ? tr(":") : tr(" METAR:")), noaaMetar);
      if(!vatsimMetar.isEmpty())
        html.row2(QString(tr("VATSIM")) + (info ? tr(":") : tr(" METAR:")), vatsimMetar);
      html.tableEnd();
    }
  }

  if(info && !airport.noRunways())
  {
    head(html, tr("Longest Runway"));
    html.table();
    html.row2(tr("Length:"), Unit::distShortFeet(airport.longestRunwayLength));
    if(rec != nullptr)
    {
      html.row2(tr("Width:"),
                Unit::distShortFeet(rec->valueInt("longest_runway_width")));

      float hdg = rec->valueFloat("longest_runway_heading") - airport.magvar;
      hdg = normalizeCourse(hdg);
      float otherHdg = normalizeCourse(opposedCourseDeg(hdg));

      html.row2(tr("Heading:"), locale.toString(hdg, 'f', 0) + tr("°M, ") +
                locale.toString(otherHdg, 'f', 0) + tr("°M"));
      html.row2(tr("Surface:"),
                maptypes::surfaceName(rec->valueStr("longest_runway_surface")));
    }
    html.tableEnd();
  }

  // Add most important COM frequencies
  if(airport.towerFrequency > 0 || airport.atisFrequency > 0 || airport.awosFrequency > 0 ||
     airport.asosFrequency > 0 || airport.unicomFrequency > 0)
  {
    if(info)
      head(html, tr("COM Frequencies"));
    html.table();
    if(airport.towerFrequency > 0)
      html.row2(tr("Tower:"), locale.toString(airport.towerFrequency / 1000., 'f', 3));
    if(airport.atisFrequency > 0)
      html.row2(tr("ATIS:"), locale.toString(airport.atisFrequency / 1000., 'f', 3));
    if(airport.awosFrequency > 0)
      html.row2(tr("AWOS:"), locale.toString(airport.awosFrequency / 1000., 'f', 3));
    if(airport.asosFrequency > 0)
      html.row2(tr("ASOS:"), locale.toString(airport.asosFrequency / 1000., 'f', 3));
    if(airport.unicomFrequency > 0)
      html.row2(tr("Unicom:"), locale.toString(airport.unicomFrequency / 1000., 'f', 3));
    html.tableEnd();
  }

  if(rec != nullptr)
  {
    // Parking overview
    int numParkingGate = rec->valueInt("num_parking_gate");
    int numJetway = rec->valueInt("num_jetway");
    int numParkingGaRamp = rec->valueInt("num_parking_ga_ramp");
    int numParkingCargo = rec->valueInt("num_parking_cargo");
    int numParkingMilCargo = rec->valueInt("num_parking_mil_cargo");
    int numParkingMilCombat = rec->valueInt("num_parking_mil_combat");
    int numHelipad = rec->valueInt("num_helipad");

    if(info)
      head(html, tr("Parking"));
    html.table();
    if(numParkingGate > 0 || numJetway > 0 || numParkingGaRamp > 0 || numParkingCargo > 0 ||
       numParkingMilCargo > 0 || numParkingMilCombat > 0 ||
       !rec->isNull("largest_parking_ramp") || !rec->isNull("largest_parking_gate"))
    {
      if(numParkingGate > 0)
        html.row2(tr("Gates:"), numParkingGate);
      if(numJetway > 0)
        html.row2(tr("Jetways:"), numJetway);
      if(numParkingGaRamp > 0)
        html.row2(tr("GA Ramp:"), numParkingGaRamp);
      if(numParkingCargo > 0)
        html.row2(tr("Cargo:"), numParkingCargo);
      if(numParkingMilCargo > 0)
        html.row2(tr("Military Cargo:"), numParkingMilCargo);
      if(numParkingMilCombat > 0)
        html.row2(tr("Military Combat:"), numParkingMilCombat);

      if(!rec->isNull("largest_parking_ramp"))
        html.row2(tr("Largest Ramp:"), maptypes::parkingRampName(rec->valueStr("largest_parking_ramp")));
      if(!rec->isNull("largest_parking_gate"))
        html.row2(tr("Largest Gate:"), maptypes::parkingRampName(rec->valueStr("largest_parking_gate")));

      if(numHelipad > 0)
        html.row2(tr("Helipads:"), numHelipad);
    }
    else
      html.row2(QString(), tr("None"));
    html.tableEnd();
  }

  if(rec != nullptr && !print)
    addScenery(rec, html);
}

void HtmlInfoBuilder::comText(const MapAirport& airport, HtmlBuilder& html, QColor background) const
{
  if(info && infoQuery != nullptr)
  {
    if(!print)
      airportTitle(airport, html, background);

    const SqlRecordVector *recVector = infoQuery->getComInformation(airport.id);
    if(recVector != nullptr)
    {
      html.h3(tr("COM Frequencies"));
      html.table();
      html.tr(QColor(Qt::lightGray));
      html.td(tr("Type"), atools::util::html::BOLD).
      td(tr("Frequency"), atools::util::html::BOLD).
      td(tr("Name"), atools::util::html::BOLD);
      html.trEnd();

      for(const SqlRecord& rec : *recVector)
      {
        html.tr();
        html.td(maptypes::comTypeName(rec.valueStr("type")));
        html.td(locale.toString(rec.valueInt("frequency") / 1000., 'f', 3) + tr(" MHz"));
        if(rec.valueStr("type") != tr("ATIS"))
          html.td(capString(rec.valueStr("name")));
        else
          // ATIS contains the airport code - do not capitalize this
          html.td(rec.valueStr("name"));
        html.trEnd();
      }
      html.tableEnd();
    }
    else
      html.p(tr("Airport has no COM Frequencies."));
  }
}

void HtmlInfoBuilder::runwayText(const MapAirport& airport, HtmlBuilder& html, QColor background,
                                 bool details, bool soft) const
{
  if(info && infoQuery != nullptr)
  {
    if(!print)
      airportTitle(airport, html, background);

    const SqlRecordVector *recVector = infoQuery->getRunwayInformation(airport.id);
    if(recVector != nullptr)
    {
      for(const SqlRecord& rec : *recVector)
      {
        if(!soft && !maptypes::isHardSurface(rec.valueStr("surface")))
          continue;

        const SqlRecord *recPrim = infoQuery->getRunwayEndInformation(rec.valueInt("primary_end_id"));
        const SqlRecord *recSec = infoQuery->getRunwayEndInformation(rec.valueInt("secondary_end_id"));
        float hdgPrim = normalizeCourse(rec.valueFloat("heading") - airport.magvar);
        float hdgSec = normalizeCourse(opposedCourseDeg(hdgPrim));
        bool closedPrim = recPrim->valueBool("has_closed_markings");
        bool closedSec = recSec->valueBool("has_closed_markings");

        html.h3(tr("Runway ") + recPrim->valueStr("name") + ", " + recSec->valueStr("name"),
                (closedPrim & closedSec ? atools::util::html::STRIKEOUT : atools::util::html::NONE));
        html.table();

        html.row2(tr("Size:"), Unit::distShortFeet(rec.valueFloat("length"), false) +
                  tr(" x ") +
                  Unit::distShortFeet(rec.valueFloat("width")));

        html.row2(tr("Surface:"), maptypes::surfaceName(rec.valueStr("surface")));
        html.row2(tr("Pattern Altitude:"), Unit::altFeet(rec.valueFloat("pattern_altitude")));

        // Lights information
        rowForStrCap(html, &rec, "edge_light", tr("Edge Lights:"), tr("%1"));
        rowForStrCap(html, &rec, "center_light", tr("Center Lights:"), tr("%1"));
        rowForBool(html, &rec, "has_center_red", tr("Has red Center Lights"), false);

        // Add a list of runway markings
        RunwayMarkingFlags flags(rec.valueInt("marking_flags"));
        QStringList markings;
        if(flags & atools::fs::bgl::rw::EDGES)
          markings.append(tr("Edges"));
        if(flags & atools::fs::bgl::rw::THRESHOLD)
          markings.append(tr("Threshold"));
        if(flags & atools::fs::bgl::rw::FIXED_DISTANCE)
          markings.append(tr("Fixed Distance"));
        if(flags & atools::fs::bgl::rw::TOUCHDOWN)
          markings.append(tr("Touchdown"));
        if(flags & atools::fs::bgl::rw::DASHES)
          markings.append(tr("Dashes"));
        if(flags & atools::fs::bgl::rw::IDENT)
          markings.append(tr("Ident"));
        if(flags & atools::fs::bgl::rw::PRECISION)
          markings.append(tr("Precision"));
        if(flags & atools::fs::bgl::rw::EDGE_PAVEMENT)
          markings.append(tr("Edge Pavement"));
        if(flags & atools::fs::bgl::rw::SINGLE_END)
          markings.append(tr("Single End"));

        if(flags & atools::fs::bgl::rw::ALTERNATE_THRESHOLD)
          markings.append(tr("Alternate Threshold"));
        if(flags & atools::fs::bgl::rw::ALTERNATE_FIXEDDISTANCE)
          markings.append(tr("Alternate Fixed Distance"));
        if(flags & atools::fs::bgl::rw::ALTERNATE_TOUCHDOWN)
          markings.append(tr("Alternate Touchdown"));
        if(flags & atools::fs::bgl::rw::ALTERNATE_PRECISION)
          markings.append(tr("Alternate Precision"));

        if(flags & atools::fs::bgl::rw::LEADING_ZERO_IDENT)
          markings.append(tr("Leading Zero Ident"));

        if(flags & atools::fs::bgl::rw::NO_THRESHOLD_END_ARROWS)
          markings.append(tr("No Threshold End Arrows"));

        if(markings.isEmpty())
          markings.append(tr("None"));

        html.row2(tr("Runway Markings:"), markings.join(tr(", ")));

        html.tableEnd();

        if(details)
        {
          runwayEndText(html, recPrim, hdgPrim, rec.valueFloat("length"));
          runwayEndText(html, recSec, hdgSec, rec.valueFloat("length"));
        }
      }
    }
    else
      html.p(tr("Airport has no runways."));

    if(details)
    {
      const SqlRecordVector *heliVector = infoQuery->getHelipadInformation(airport.id);

      if(heliVector != nullptr)
      {
        for(const SqlRecord& heliRec : *heliVector)
        {
          bool closed = heliRec.valueBool("is_closed");
          bool hasStart = !heliRec.isNull("start_number");
          QString num = hasStart ? " " + QString::number(heliRec.valueInt("start_number")) : tr(
            " (No Start Position)");

          html.h3(tr("Helipad%1").arg(num), closed ? atools::util::html::STRIKEOUT : atools::util::html::NONE);
          html.nbsp().nbsp();

          atools::geo::Pos pos(heliRec.valueFloat("lonx"), heliRec.valueFloat("laty"));

          html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2&zoom=%3").
                 arg(pos.getLonX()).arg(pos.getLatY()).arg(Unit::distMeterF(HELIPAD_ZOOM_METER))).br();

          if(closed)
            html.text(tr("Is Closed"));
          html.table();

          html.row2(tr("Size:"), Unit::distShortFeet(heliRec.valueFloat("width"), false) +
                    tr(" x ") +
                    Unit::distShortFeet(heliRec.valueFloat("length")));
          html.row2(tr("Surface:"), maptypes::surfaceName(heliRec.valueStr("surface")) +
                    (heliRec.valueBool("is_transparent") ? tr(" (Transparent)") : QString()));
          html.row2(tr("Type:"), atools::capString(heliRec.valueStr("type")));
          html.row2(tr("Heading:"), tr("%1°M").arg(locale.toString(heliRec.valueFloat("heading"), 'f', 0)));
          html.row2(tr("Elevation:"), Unit::altFeet(heliRec.valueFloat("altitude")));

          addCoordinates(&heliRec, html);
          html.tableEnd();
        }
      }
      else
        html.p(tr("Airport has no helipads."));

    }
  }
}

void HtmlInfoBuilder::runwayEndText(HtmlBuilder& html, const SqlRecord *rec, float hdgPrim,
                                    float length) const
{
  bool closed = rec->valueBool("has_closed_markings");

  html.h3(rec->valueStr("name"), closed ? (atools::util::html::STRIKEOUT) : atools::util::html::NONE);
  html.table();
  if(closed)
    html.row2(tr("Closed"), QString());
  html.row2(tr("Heading:"), tr("%1°M").arg(locale.toString(hdgPrim, 'f', 0)));

  float threshold = rec->valueFloat("offset_threshold");
  if(threshold > 1.f)
  {
    html.row2(tr("Offset Threshold:"), Unit::distShortFeet(threshold));
    html.row2(tr("Effective Landing Distance:"), Unit::distShortFeet(length - threshold));
  }

  float blastpad = rec->valueFloat("blast_pad");
  if(blastpad > 1.f)
    html.row2(tr("Blast Pad:"), Unit::distShortFeet(blastpad));

  float overrrun = rec->valueFloat("overrun");
  if(overrrun > 1.f)
    html.row2(tr("Overrun:"), Unit::distShortFeet(overrrun));

  rowForBool(html, rec, "has_stol_markings", tr("Has STOL Markings"), false);
  // Two flags below only are probably only for AI
  // rowForBool(html, recPrim, "is_takeoff", tr("Closed for Takeoff"), true);
  // rowForBool(html, recPrim, "is_landing", tr("Closed for Landing"), true);
  rowForStrCap(html, rec, "is_pattern", tr("Pattern:"), tr("%1"));

  // Approach indicators
  rowForStr(html, rec, "left_vasi_type", tr("Left VASI Type:"), tr("%1"));
  rowForFloat(html, rec, "left_vasi_pitch", tr("Left VASI Pitch:"), tr("%1°"), 1);
  rowForStr(html, rec, "right_vasi_type", tr("Right VASI Type:"), tr("%1"));
  rowForFloat(html, rec, "right_vasi_pitch", tr("Right VASI Pitch:"), tr("%1°"), 1);

  rowForStr(html, rec, "app_light_system_type", tr("ALS Type:"), tr("%1"));

  // End lights
  QStringList lights;
  if(rec->valueBool("has_end_lights"))
    lights.append(tr("Lights"));
  if(rec->valueBool("has_reils"))
    lights.append(tr("Strobes"));
  if(rec->valueBool("has_touchdown_lights"))
    lights.append(tr("Touchdown"));
  if(!lights.isEmpty())
    html.row2(tr("Runway End Lights:"), lights.join(tr(", ")));
  html.tableEnd();

  const atools::sql::SqlRecord *ilsRec = infoQuery->getIlsInformation(rec->valueInt("runway_end_id"));
  if(ilsRec != nullptr)
  {
    // Add ILS information
    bool dme = !ilsRec->isNull("dme_altitude");
    bool gs = !ilsRec->isNull("gs_altitude");

    html.br().h4(tr("%1 (%2) - ILS %3%4").arg(ilsRec->valueStr("name")).arg(ilsRec->valueStr("ident")).
                 arg(gs ? tr(", GS") : "").arg(dme ? tr(", DME") : ""));

    html.table();
    html.row2(tr("Frequency:"), locale.toString(ilsRec->valueFloat("frequency") / 1000., 'f', 2) + tr(" MHz"));
    html.row2(tr("Range:"), Unit::distNm(ilsRec->valueFloat("range")));
    float magvar = ilsRec->valueFloat("mag_var");
    html.row2(tr("Magvar:"), maptypes::magvarText(magvar));
    rowForBool(html, ilsRec, "has_backcourse", tr("Has Backcourse"), false);

    float hdg = ilsRec->valueFloat("loc_heading") - magvar;
    hdg = normalizeCourse(hdg);

    html.row2(tr("Localizer Heading:"), locale.toString(hdg, 'f', 0) + tr("°M"));
    html.row2(tr("Localizer Width:"), locale.toString(ilsRec->valueFloat("loc_width"), 'f', 0) + tr("°"));
    if(gs)
      html.row2(tr("Glideslope Pitch:"), locale.toString(ilsRec->valueFloat("gs_pitch"), 'f', 1) + tr("°"));

    html.tableEnd();
  }
}

void HtmlInfoBuilder::helipadText(const MapHelipad& helipad, HtmlBuilder& html) const
{
  QString num = helipad.start != -1 ? " " + QString::number(helipad.start) :
                tr(" (No Start Position)");

  head(html, tr("Helipad%1:").arg(num));
  html.brText(tr("Surface: ") + maptypes::surfaceName(helipad.surface));
  html.brText(tr("Type: ") + helipad.type);
  html.brText(Unit::distShortFeet(std::max(helipad.width, helipad.length)));
  if(helipad.closed)
    html.brText(tr("Is Closed"));
}

void HtmlInfoBuilder::approachText(const MapAirport& airport, HtmlBuilder& html, QColor background) const
{
  if(info && infoQuery != nullptr)
  {
    if(!print)
      airportTitle(airport, html, background);

    const SqlRecordVector *recAppVector = infoQuery->getApproachInformation(airport.id);
    if(recAppVector != nullptr)
    {
      for(const SqlRecord& recApp : *recAppVector)
      {
        // Approach information
        QString runway;
        if(!recApp.isNull("runway_name"))
          runway = tr(" - Runway ") + recApp.valueStr("runway_name");

        html.h4(tr("Approach ") + recApp.valueStr("type") + runway);
        html.table();
        rowForBool(html, &recApp, "has_gps_overlay", tr("Has GPS Overlay"), false);
        html.row2(tr("Fix Ident and Region:"), recApp.valueStr("fix_ident") + tr(", ") +
                  recApp.valueStr("fix_region"));
        html.row2(tr("Fix Type:"), capNavString(recApp.valueStr("fix_type")));

        float hdg = recApp.valueFloat("heading") - airport.magvar;
        hdg = normalizeCourse(hdg);
        html.row2(tr("Heading:"), tr("%1°M, %2°T").arg(locale.toString(hdg, 'f', 0)).
                  arg(locale.toString(recApp.valueFloat("heading"), 'f', 0)));

        html.row2(tr("Altitude:"), Unit::altFeet(recApp.valueFloat("altitude")));
        html.row2(tr("Missed Altitude:"), Unit::altFeet(recApp.valueFloat("missed_altitude")));
        html.tableEnd();

        const SqlRecordVector *recTransVector =
          infoQuery->getTransitionInformation(recApp.valueInt("approach_id"));
        if(recTransVector != nullptr)
        {
          // Transitions for this approach
          for(const SqlRecord& recTrans : *recTransVector)
          {
            html.h4(tr("Transition ") + recTrans.valueStr("fix_ident") + runway);
            html.table();
            html.row2(tr("Type:"), capNavString(recTrans.valueStr("type")));
            html.row2(tr("Fix Ident and Region:"), recTrans.valueStr("fix_ident") + tr(", ") +
                      recTrans.valueStr("fix_region"));
            html.row2(tr("Fix Type:"), capNavString(recTrans.valueStr("fix_type")));
            html.row2(tr("Altitude:"), Unit::altFeet(recTrans.valueFloat("altitude")));

            if(!recTrans.isNull("dme_ident"))
              html.row2(tr("DME Ident and Region:"), recTrans.valueStr("dme_ident") + tr(", ") +
                        recTrans.valueStr("dme_region"));

            rowForFloat(html, &recTrans, "dme_radial", tr("DME Radial:"), tr("%1"), 0);
            float dist = recTrans.valueFloat("dme_distance");
            if(dist > 1.f)
              html.row2(tr("DME Distance:"), Unit::altFeet(dist));
            html.tableEnd();
          }
        }
      }
    }
    else
      html.p(tr("Airport has no Approaches."));
  }
}

void HtmlInfoBuilder::vorText(const MapVor& vor, HtmlBuilder& html, QColor background) const
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getVorInformation(vor.id);

  QIcon icon = SymbolPainter(background).createVorIcon(vor, SYMBOL_SIZE);
  html.img(icon, QString(), QString(), QSize(SYMBOL_SIZE, SYMBOL_SIZE));
  html.nbsp().nbsp();

  QString type = maptypes::vorType(vor);
  title(html, type + ": " + capString(vor.name) + " (" + vor.ident + ")");

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(vor.position.getLonX()).arg(vor.position.getLatY()));
    html.br();
  }

  html.table();
  if(vor.routeIndex >= 0)
    html.row2(tr("Flight Plan position:"), locale.toString(vor.routeIndex + 1));

  html.row2(tr("Type:"), maptypes::navTypeName(vor.type));
  html.row2(tr("Region:"), vor.region);
  html.row2(tr("Frequency:"), locale.toString(vor.frequency / 1000., 'f', 2) + tr(" MHz"));
  if(!vor.dmeOnly)
    html.row2(tr("Magvar:"), maptypes::magvarText(vor.magvar));
  html.row2(tr("Elevation:"), Unit::altFeet(vor.getPosition().getAltitude()));
  html.row2(tr("Range:"), Unit::distNm(vor.range));
  html.row2(tr("Morse:"), tr("<b>") + morse->getCode(vor.ident) + tr("</b>"));
  addCoordinates(rec, html);
  html.tableEnd();

  if(rec != nullptr)
    addScenery(rec, html);
}

void HtmlInfoBuilder::ndbText(const MapNdb& ndb, HtmlBuilder& html, QColor background) const
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getNdbInformation(ndb.id);

  QIcon icon = SymbolPainter(background).createNdbIcon(SYMBOL_SIZE);
  html.img(icon, QString(), QString(), QSize(SYMBOL_SIZE, SYMBOL_SIZE));
  html.nbsp().nbsp();

  title(html, tr("NDB: ") + capString(ndb.name) + " (" + ndb.ident + ")");

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(ndb.position.getLonX()).arg(ndb.position.getLatY()));
    html.br();
  }

  html.table();
  if(ndb.routeIndex >= 0)
    html.row2(tr("Flight Plan position "), locale.toString(ndb.routeIndex + 1));
  html.row2(tr("Type:"), maptypes::navTypeName(ndb.type));
  html.row2(tr("Region:"), ndb.region);
  html.row2(tr("Frequency:"), locale.toString(ndb.frequency / 100., 'f', 1) + tr(" kHz"));
  html.row2(tr("Magvar:"), maptypes::magvarText(ndb.magvar));
  html.row2(tr("Elevation:"), Unit::altFeet(ndb.getPosition().getAltitude()));
  html.row2(tr("Range:"), Unit::distNm(ndb.range));
  html.row2(tr("Morse:"), "<b>" + morse->getCode(ndb.ident) + "</b>");
  addCoordinates(rec, html);
  html.tableEnd();

  if(rec != nullptr)
    addScenery(rec, html);
}

void HtmlInfoBuilder::waypointText(const MapWaypoint& waypoint, HtmlBuilder& html, QColor background) const
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getWaypointInformation(waypoint.id);

  QIcon icon = SymbolPainter(background).createWaypointIcon(SYMBOL_SIZE);
  html.img(icon, QString(), QString(), QSize(SYMBOL_SIZE, SYMBOL_SIZE));
  html.nbsp().nbsp();

  title(html, tr("Waypoint: ") + waypoint.ident);

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(waypoint.position.getLonX()).arg(waypoint.position.getLatY()));
    html.br();
  }

  html.table();
  if(waypoint.routeIndex >= 0)
    html.row2(tr("Flight Plan position:"), locale.toString(waypoint.routeIndex + 1));
  html.row2(tr("Type:"), maptypes::navTypeName(waypoint.type));
  html.row2(tr("Region:"), waypoint.region);
  html.row2(tr("Magvar:"), maptypes::magvarText(waypoint.magvar));
  addCoordinates(rec, html);

  html.tableEnd();

  QList<MapAirway> airways;
  mapQuery->getAirwaysForWaypoint(airways, waypoint.id);

  if(!airways.isEmpty())
  {
    // Add airway information

    // Add airway name/text to vector
    QVector<std::pair<QString, QString> > airwayTexts;
    for(const MapAirway& aw : airways)
    {
      QString txt(maptypes::airwayTypeToString(aw.type));
      if(aw.minAltitude > 0)
        txt += tr(", ") + Unit::altFeet(aw.minAltitude);
      airwayTexts.append(std::make_pair(aw.name, txt));
    }

    if(!airwayTexts.isEmpty())
    {
      // Sort airways by name
      std::sort(airwayTexts.begin(), airwayTexts.end(),
                [ = ](const std::pair<QString, QString> &item1, const std::pair<QString, QString> &item2)
                {
                  return item1.first < item2.first;
                });

      // Remove duplicates
      airwayTexts.erase(std::unique(airwayTexts.begin(), airwayTexts.end()), airwayTexts.end());

      if(info)
        head(html, tr("Airways:"));
      else
        html.br().b(tr("Airways: "));

      html.table();
      for(const std::pair<QString, QString>& aw : airwayTexts)
        html.row2(aw.first, aw.second);
      html.tableEnd();
    }
  }

  if(rec != nullptr)
    addScenery(rec, html);
}

void HtmlInfoBuilder::airwayText(const MapAirway& airway, HtmlBuilder& html) const
{
  title(html, tr("Airway: ") + airway.name);
  html.table();
  html.row2(tr("Type:"), maptypes::airwayTypeToString(airway.type));

  if(airway.minAltitude > 0)
    html.row2(tr("Min altitude for this segment:"), Unit::altFeet(airway.minAltitude));

  html.row2(tr("Segment length:"), Unit::distMeter(airway.from.distanceMeterTo(airway.to)));

  if(infoQuery != nullptr && info)
  {
    atools::sql::SqlRecordVector waypoints =
      infoQuery->getAirwayWaypointInformation(airway.name, airway.fragment);

    if(!waypoints.isEmpty())
    {
      QStringList waypointTexts;
      for(const SqlRecord& wprec : waypoints)
        waypointTexts.append(wprec.valueStr("from_ident") + tr("/") + wprec.valueStr("from_region"));
      waypointTexts.append(waypoints.last().valueStr("to_ident") + "/" +
                           waypoints.last().valueStr("to_region"));

      html.row2(tr("Waypoints Ident/Region:"), waypointTexts.join(", "));
    }
  }
  html.tableEnd();
}

void HtmlInfoBuilder::markerText(const MapMarker& marker, HtmlBuilder& html) const
{
  head(html, tr("Marker: ") + marker.type);
}

void HtmlInfoBuilder::towerText(const MapAirport& airport, HtmlBuilder& html) const
{
  if(airport.towerFrequency > 0)
    head(html, tr("Tower: ") + locale.toString(airport.towerFrequency / 1000., 'f', 3));
  else
    head(html, tr("Tower"));
}

void HtmlInfoBuilder::parkingText(const MapParking& parking, HtmlBuilder& html) const
{
  head(html, maptypes::parkingName(parking.name) + " " + locale.toString(parking.number));
  html.brText(maptypes::parkingTypeName(parking.type));
  html.brText(Unit::distShortFeet(parking.radius * 2.f));
  if(parking.jetway)
    html.brText(tr("Has Jetway"));
  if(!parking.airlineCodes.isEmpty())
    html.brText(tr("Airline Codes: ") + parking.airlineCodes);
}

void HtmlInfoBuilder::userpointText(const MapUserpoint& userpoint, HtmlBuilder& html) const
{
  head(html, tr("Flight Plan Point:"));
  html.brText(userpoint.name);
}

void HtmlInfoBuilder::aircraftText(const atools::fs::sc::SimConnectAircraft& aircraft,
                                   HtmlBuilder& html, int num, int total) const
{
  aircraftTitle(aircraft, html);

  html.nbsp().nbsp();
  QString aircraftText;
  if(aircraft.isUser())
    aircraftText = tr("User Aircraft");
  else
  {
    if(num != -1 && total != -1)
      aircraftText = tr("AI / Multiplayer Aircraft %1 of %2").arg(num).arg(total);
    else
      aircraftText = tr("AI / Multiplayer Aircraft");
  }

  head(html, aircraftText);

  html.table();
  if(!aircraft.getAirplaneTitle().isEmpty())
    html.row2(tr("Title:"), aircraft.getAirplaneTitle());

  if(!aircraft.getAirplaneAirline().isEmpty())
    html.row2(tr("Airline:"), aircraft.getAirplaneAirline());

  if(!aircraft.getAirplaneFlightnumber().isEmpty())
    html.row2(tr("Flight Number:"), aircraft.getAirplaneFlightnumber());

  if(!aircraft.getAirplaneModel().isEmpty())
    html.row2(tr("Model:"), aircraft.getAirplaneModel());

  if(!aircraft.getAirplaneRegistration().isEmpty())
    html.row2(tr("Registration:"), aircraft.getAirplaneRegistration());

  if(!aircraft.getAirplaneType().isEmpty())
    html.row2(tr("Type:"), aircraft.getAirplaneType());

  if(info && aircraft.getWingSpan() > 0)
    html.row2(tr("Wingspan:"), Unit::distShortFeet(aircraft.getWingSpan()));
  html.tableEnd();
}

void HtmlInfoBuilder::aircraftTextWeightAndFuel(const atools::fs::sc::SimConnectUserAircraft& userAircraft,
                                                HtmlBuilder& html) const
{
  if(info)
  {
    head(html, tr("Weight and Fuel"));
    html.table();
    html.row2(tr("Max Gross Weight:"), Unit::weightLbs(userAircraft.getAirplaneMaxGrossWeightLbs()));
    html.row2(tr("Gross Weight:"), Unit::weightLbs(userAircraft.getAirplaneTotalWeightLbs()));
    html.row2(tr("Empty Weight:"), Unit::weightLbs(userAircraft.getAirplaneEmptyWeightLbs()));

    html.row2(tr("Fuel:"), Unit::weightLbs(userAircraft.getFuelTotalWeightLbs()) + ", " +
              Unit::volGallon(userAircraft.getFuelTotalQuantityGallons()));
    html.tableEnd();
  }
}

void HtmlInfoBuilder::timeAndDate(const SimConnectUserAircraft *userAircaft, HtmlBuilder& html) const
{
  html.row2(tr("Time and Date:"), locale.toString(userAircaft->getLocalTime(), QLocale::ShortFormat) +
            tr(" ") + userAircaft->getLocalTime().timeZoneAbbreviation() + tr(", ") +
            locale.toString(userAircaft->getZuluTime().time(), QLocale::ShortFormat) +
            " " + userAircaft->getZuluTime().timeZoneAbbreviation());
}

void HtmlInfoBuilder::aircraftProgressText(const atools::fs::sc::SimConnectAircraft& aircraft,
                                           HtmlBuilder& html,
                                           const RouteMapObjectList& rmoList) const
{
  const SimConnectUserAircraft *userAircaft = dynamic_cast<const SimConnectUserAircraft *>(&aircraft);

  if(info && userAircaft != nullptr)
    aircraftTitle(aircraft, html);

  float distFromStartNm = 0.f, distToDestNm = 0.f, nearestLegDistance = 0.f, crossTrackDistance = 0.f;
  int nearestLegIndex;

  if(!rmoList.isEmpty() && userAircaft != nullptr && info)
  {
    if(rmoList.getRouteDistances(aircraft.getPosition(),
                                 &distFromStartNm, &distToDestNm, &nearestLegDistance, &crossTrackDistance,
                                 &nearestLegIndex))
    {
      head(html, tr("Flight Plan Progress"));
      html.table();
      // html.row2("Distance from Start:", locale.toString(distFromStartNm, 'f', 0) + tr(" nm"));
      html.row2(tr("To Destination:"), Unit::distNm(distToDestNm));

      timeAndDate(userAircaft, html);

      if(aircraft.getGroundSpeedKts() > 20.f)
      {
        float timeToDestination = distToDestNm / aircraft.getGroundSpeedKts();
        QDateTime arrival = userAircaft->getZuluTime().addSecs(static_cast<int>(timeToDestination * 3600.f));
        html.row2(tr("Arrival Time:"), locale.toString(arrival.time(), QLocale::ShortFormat) + " " +
                  arrival.timeZoneAbbreviation());
        html.row2(tr("En route Time:"), formatter::formatMinutesHoursLong(timeToDestination));
      }
      html.tableEnd();

      head(html, tr("Next Waypoint"));
      html.table();

      if(nearestLegIndex >= 0 && nearestLegIndex < rmoList.size())
      {
        const RouteMapObject& rmo = rmoList.at(nearestLegIndex);
        float crs = normalizeCourse(aircraft.getPosition().angleDegToRhumb(rmo.getPosition()) - rmo.getMagvar());
        html.row2(tr("Name and Type:"), rmo.getIdent() +
                  (rmo.getMapObjectTypeName().isEmpty() ? QString() : tr(", ") + rmo.getMapObjectTypeName()));

        QString timeStr;
        if(aircraft.getGroundSpeedKts() > 20.f)
          timeStr = tr(", ") + formatter::formatMinutesHoursLong(
            nearestLegDistance / aircraft.getGroundSpeedKts());

        html.row2(tr("Distance, Course and Time:"), Unit::distNm(nearestLegDistance) + ", " +
                  locale.toString(crs, 'f', 0) +
                  tr("°M, ") + timeStr);
        html.row2(tr("Leg Course:"), locale.toString(rmo.getCourseToRhumb(), 'f', 0) + tr("°M"));
      }
      else
        qWarning() << "Invalid route leg index" << nearestLegIndex;

      if(crossTrackDistance != RouteMapObjectList::INVALID_DISTANCE_VALUE)
      {
        QString crossDirection;
        if(crossTrackDistance >= 10.f)
          crossDirection = tr("<b>►</b>");
        else if(crossTrackDistance <= -10.f)
          crossDirection = tr("<b>◄</b>");

        html.row2(tr("Cross Track Distance:"),
                  Unit::distNm(std::abs(crossTrackDistance)) + " " + crossDirection);
      }
      else
        html.row2(tr("Cross Track Distance:"), tr("Not along Track"));
      html.tableEnd();
    }
    else
      html.h4(tr("No Active Flight Plan Leg found."), atools::util::html::BOLD);
  }
  else if(info && userAircaft != nullptr)
  {
    head(html, tr("No Flight Plan loaded."));
    html.table();
    timeAndDate(userAircaft, html);
    html.tableEnd();
  }

  if(userAircaft == nullptr && (!aircraft.getFromIdent().isEmpty() || !aircraft.getToIdent().isEmpty()))
  {
    // Add departure and destination for AI
    if(info && userAircaft != nullptr)
      head(html, tr("Flight Plan"));

    if(!aircraft.getFromIdent().isEmpty())
    {
      html.p();
      html.b(tr("Departure: "));
      if(info)
        html.a(aircraft.getFromIdent(), QString("lnm://show?airport=%1").arg(aircraft.getFromIdent()));
      else
        html.text(aircraft.getFromIdent());
      html.text(tr(". "));
    }

    if(!aircraft.getToIdent().isEmpty())
    {
      html.b(tr("Destination: "));
      if(info)
        html.a(aircraft.getToIdent(), QString("lnm://show?airport=%1").arg(aircraft.getToIdent()));
      else
        html.text(aircraft.getToIdent());
      html.text(tr("."));
      html.pEnd();
    }
  }

  head(html, tr("Aircraft"));
  html.table();
  html.row2(tr("Heading:"), locale.toString(aircraft.getHeadingDegMag(), 'f', 0) + tr("°M, ") +
            locale.toString(aircraft.getHeadingDegTrue(), 'f', 0) + tr("°T"));

  if(userAircaft != nullptr && info)
  {
    if(userAircaft != nullptr)
      html.row2(tr("Track:"), locale.toString(userAircaft->getTrackDegMag(), 'f', 0) + tr("°M, ") +
                locale.toString(userAircaft->getTrackDegTrue(), 'f', 0) + tr("°T"));

    html.row2(tr("Fuel Flow:"), Unit::ffLbs(userAircaft->getFuelFlowPPH()) + ", " +
              Unit::ffGallon(userAircaft->getFuelFlowGPH()));

    if(userAircaft->getFuelFlowPPH() > 1.0f && aircraft.getGroundSpeedKts() > 20.f)
    {
      float hoursRemaining = userAircaft->getFuelTotalWeightLbs() / userAircaft->getFuelFlowPPH();
      float distanceRemaining = hoursRemaining * aircraft.getGroundSpeedKts();
      html.row2(tr("Endurance:"), formatter::formatMinutesHoursLong(hoursRemaining) + tr(", ") +
                Unit::distNm(distanceRemaining));
    }

    if(distToDestNm > 1.f && userAircaft->getFuelFlowPPH() > 1.f && userAircaft->getGroundSpeedKts() > 20.f)
    {
      float neededFuelWeight = distToDestNm / aircraft.getGroundSpeedKts() * userAircaft->getFuelFlowPPH();
      float neededFuelVol = distToDestNm / aircraft.getGroundSpeedKts() * userAircaft->getFuelFlowGPH();
      html.row2(tr("Fuel at Destination:"),
                Unit::weightLbs(userAircaft->getFuelTotalWeightLbs() - neededFuelWeight) + ", " +
                Unit::volGallon(userAircaft->getFuelTotalQuantityGallons() - neededFuelVol));
    }

    QString ice;

    if(userAircaft->getPitotIcePercent() >= 1.f)
      ice += tr("Pitot ") + locale.toString(userAircaft->getPitotIcePercent(), 'f', 0) + tr(" %");
    if(userAircaft->getStructuralIcePercent() >= 1.f)
    {
      if(!ice.isEmpty())
        ice += tr(", ");
      ice += tr("Structure ") + locale.toString(userAircaft->getStructuralIcePercent(), 'f', 0) + tr(" %");
    }
    if(ice.isEmpty())
      ice = tr("None");

    html.row2(tr("Ice:"), ice);
  }
  html.tableEnd();

  if(info)
    head(html, tr("Altitude"));
  html.table();
  if(info)
    html.row2(tr("Indicated:"), Unit::altFeet(aircraft.getIndicatedAltitudeFt()));
  html.row2(info ? tr("Actual:") : tr("Altitude:"), Unit::altFeet(aircraft.getPosition().getAltitude()));

  if(userAircaft != nullptr && info)
  {
    html.row2(tr("Above Ground:"), Unit::altFeet(userAircaft->getAltitudeAboveGroundFt()));
    html.row2(tr("Ground Elevation:"), Unit::altFeet(userAircaft->getGroundAltitudeFt()));
  }
  html.tableEnd();

  if(info)
    head(html, tr("Speed"));
  html.table();
  if(info)
    html.row2(tr("Indicated:"), Unit::speedKts(aircraft.getIndicatedSpeedKts()));

  html.row2(info ? tr("Ground:") : tr("Groundspeed:"), Unit::speedKts(aircraft.getGroundSpeedKts()));
  if(info)
    html.row2(tr("True Airspeed:"), Unit::speedKts(aircraft.getTrueSpeedKts()));

  if(info)
  {
    float mach = aircraft.getMachSpeed();
    if(mach > 0.4f)
      html.row2(tr("Mach:"), locale.toString(mach, 'f', 2));
    else
      html.row2(tr("Mach:"), tr("-"));
  }

  int vspeed = atools::roundToInt(aircraft.getVerticalSpeedFeetPerMin());
  QString upDown;
  if(vspeed >= 100)
    upDown = tr(" <b>▲</b>");
  else if(vspeed <= -100)
    upDown = tr(" <b>▼</b>");

  if(vspeed < 10.f && vspeed > -10.f)
    vspeed = 0.f;

  html.row2(info ? tr("Vertical:") : tr("Vertical Speed:"), Unit::speedVertFpm(vspeed) + upDown);
  html.tableEnd();

  if(userAircaft != nullptr && info)
  {
    head(html, tr("Environment"));
    html.table();
    float windSpeed = userAircaft->getWindSpeedKts();
    float windDir = normalizeCourse(userAircaft->getWindDirectionDegT() - userAircaft->getMagVarDeg());
    if(windSpeed >= 1.f)
      html.row2(tr("Wind Direction and Speed:"), locale.toString(windDir, 'f', 0) + tr("°M, ") +
                Unit::speedKts(windSpeed));
    else
      html.row2(tr("Wind Direction and Speed:"), tr("None"));

    float diffRad = atools::geo::toRadians(windDir - userAircaft->getHeadingDegMag());
    float headWind = windSpeed * std::cos(diffRad);
    float crossWind = windSpeed * std::sin(diffRad);

    QString value;
    if(std::abs(headWind) >= 1.0f)
    {
      value += Unit::speedKts(std::abs(headWind));

      if(headWind <= -1.f)
        value += tr("<b>▲</b>");  // Tailwind
      else
        value += tr("<b>▼</b>");  // Headwind
    }

    if(std::abs(crossWind) >= 1.0f)
    {
      if(!value.isEmpty())
        value += tr(", ");

      value += Unit::speedKts(std::abs(crossWind));

      if(crossWind >= 1.f)
        value += tr("<b>◄</b>");
      else if(crossWind <= -1.f)
        value += tr("<b>►</b>");

    }

    html.row2(QString(), value);

    float temp = userAircaft->getTotalAirTemperatureCelsius();
    html.row2(tr("Total Air Temperature:"), locale.toString(temp, 'f', 0) + tr("°C, ") +
              locale.toString(atools::geo::degCToDegF(temp), 'f', 0) + tr("°F"));

    temp = userAircaft->getAmbientTemperatureCelsius();
    html.row2(tr("Static Air Temperature:"), locale.toString(temp, 'f', 0) + tr("°C, ") +
              locale.toString(atools::geo::degCToDegF(temp), 'f', 0) + tr("°F"));

    float slp = userAircaft->getSeaLevelPressureMbar();
    html.row2(tr("Sea Level Pressure:"), locale.toString(slp, 'f', 0) + tr(" hPa, ") +
              locale.toString(atools::geo::mbarToInHg(slp), 'f', 2) + tr(" inHg"));

    QStringList precip;
    // if(data.getFlags() & atools::fs::sc::IN_CLOUD) // too unreliable
    // precip.append(tr("Cloud"));
    if(userAircaft->inRain())
      precip.append(tr("Rain"));
    if(userAircaft->inSnow())
      precip.append(tr("Snow"));
    if(precip.isEmpty())
      precip.append(tr("None"));
    html.row2(tr("Conditions:"), precip.join(tr(", ")));

    float visibility = Unit::distMeterF(userAircaft->getAmbientVisibilityMeter());

    if(visibility > 20.f)
      html.row2(tr("Visibility:"), tr("> 20 ") + Unit::getUnitDistStr());
    else
      html.row2(tr("Visibility:"), Unit::distMeter(visibility, true /*unit*/, 5));

    html.tableEnd();
  }

  if(info)
  {
    head(html, tr("Position"));
    html.row2(tr("Coordinates:"), Unit::coords(aircraft.getPosition()));
    html.tableEnd();
  }
}

void HtmlInfoBuilder::aircraftTitle(const atools::fs::sc::SimConnectAircraft& aircraft,
                                    HtmlBuilder& html) const
{
  const QString *icon;

  if(aircraft.isUser())
  {
    if(aircraft.isOnGround())
      icon = &aircraftGroundEncodedIcon;
    else
      icon = &aircraftEncodedIcon;
  }
  else
  {
    if(aircraft.isOnGround())
      icon = &aircraftAiGroundEncodedIcon;
    else
      icon = &aircraftAiEncodedIcon;
  }

  if(aircraft.isUser())
    html.img(*icon, tr("User Aircraft"), QString(), QSize(24, 24));
  else
    html.img(*icon, tr("AI / Multiplayer Aircraft"), QString(), QSize(24, 24));
  html.nbsp().nbsp();

  QString title(aircraft.getAirplaneRegistration());

  QString title2;
  if(!aircraft.getAirplaneType().isEmpty())
    title2 += aircraft.getAirplaneType();

  if(!aircraft.getAirplaneModel().isEmpty())
    title2 += (title2.isEmpty() ? "" : ", ") + aircraft.getAirplaneModel();

  if(!title2.isEmpty())
    title += " (" + title2 + ")";

  html.text(title, atools::util::html::BOLD | atools::util::html::BIG);

  if(info)
  {
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(aircraft.getPosition().getLonX()).arg(aircraft.getPosition().getLatY()));
  }
}

void HtmlInfoBuilder::addScenery(const atools::sql::SqlRecord *rec, HtmlBuilder& html) const
{
  head(html, tr("Scenery"));
  html.table();
  html.row2(tr("Title:"), rec->valueStr(tr("title")));
  html.row2(tr("BGL Filepath:"), rec->valueStr(tr("filepath")));
  html.tableEnd();
}

void HtmlInfoBuilder::addCoordinates(const atools::sql::SqlRecord *rec, HtmlBuilder& html) const
{
  if(rec != nullptr)
  {
    float alt = 0;
    if(rec->contains("altitude"))
      alt = rec->valueFloat("altitude");
    atools::geo::Pos pos(rec->valueFloat("lonx"), rec->valueFloat("laty"), alt);
    html.row2(tr("Coordinates:"), Unit::coords(pos));
  }
}

void HtmlInfoBuilder::head(HtmlBuilder& html, const QString& text) const
{
  if(info)
    html.h4(text);
  else
    html.b(text);
}

void HtmlInfoBuilder::title(HtmlBuilder& html, const QString& text) const
{
  if(info)
    html.text(text, atools::util::html::BOLD | atools::util::html::BIG);
  else
    html.b(text);
}

void HtmlInfoBuilder::rowForFloat(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                  const QString& msg, const QString& val, int precision) const
{
  if(!rec->isNull(colName))
  {
    float i = rec->valueFloat(colName);
    if(i > 0)
      html.row2(msg, val.arg(locale.toString(i, 'f', precision)));
  }
}

void HtmlInfoBuilder::rowForInt(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                const QString& msg, const QString& val) const
{
  if(!rec->isNull(colName))
  {
    int i = rec->valueInt(colName);
    if(i > 0)
      html.row2(msg, val.arg(locale.toString(i)));
  }
}

void HtmlInfoBuilder::rowForBool(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                 const QString& msg, bool expected) const
{
  if(!rec->isNull(colName))
  {
    bool i = rec->valueBool(colName);
    if(i != expected)
      html.row2(msg, QString());
  }
}

void HtmlInfoBuilder::rowForStr(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                const QString& msg, const QString& val) const
{
  if(!rec->isNull(colName))
  {
    QString i = rec->valueStr(colName);
    if(!i.isEmpty())
      html.row2(msg, val.arg(i));
  }
}

void HtmlInfoBuilder::rowForStrCap(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                   const QString& msg, const QString& val) const
{
  if(!rec->isNull(colName))
  {
    QString i = rec->valueStr(colName);
    if(!i.isEmpty())
      html.row2(msg, val.arg(capString(i)));
  }
}
