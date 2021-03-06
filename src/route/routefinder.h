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

#ifndef LITTLENAVMAP_ROUTEFINDER_H
#define LITTLENAVMAP_ROUTEFINDER_H

#include "common/maptypes.h"
#include "util/heap.h"
#include "route/routenetwork.h"
#include "geo/calculations.h"

namespace rf {
/* Used when fetching the route points after calculation. Adds airway id to node */
struct RouteEntry
{
  maptypes::MapObjectRef ref;
  int airwayId;
};

}

/*
 * Calculates flight plans within a route network which can be an airway or radio navaid network.
 * Use A* algorithm and several cost factor adjustments to get reasonable routes.
 */
class RouteFinder
{
public:
  /* Creates a route finder that uses the given network */
  RouteFinder(RouteNetwork *routeNetwork);
  virtual ~RouteFinder();

  /*
   * Calculates a flight plan between two points. The points are added to the network but will not be returned
   * in extractRoute.
   * @param from departure position
   * @param to destination position
   * @param flownAltitude create a flight plan using airways for the given altitude. Set to 0 to ignore.
   * @return true if a route was found
   */
  bool calculateRoute(const atools::geo::Pos& from, const atools::geo::Pos& to, int flownAltitude);

  /* Extract route points and total distance if calculateRoute was successfull.
   * From and to are not included in the list */
  void extractRoute(QVector<rf::RouteEntry>& route, float& distanceMeter);

  /* Prefer VORs to transition from departure to airway network */
  void setPreferVorToAirway(bool value)
  {
    preferVorToAirway = value;
  }

  /* Prefer NDBs to transition from departure to airway network */
  void setPreferNdbToAirway(bool value)
  {
    preferNdbToAirway = value;
  }

private:
  void expandNode(const nw::Node& node, const nw::Node& destNode);
  float calculateEdgeCost(const nw::Node& node, const nw::Node& successorNode, int lengthMeter);
  float costEstimate(const nw::Node& currentNode, const nw::Node& destNode);
  maptypes::MapObjectTypes toMapObjectType(nw::NodeType type);

  /* Force algortihm to avoid direct route from start to destination */
  static Q_DECL_CONSTEXPR float COST_FACTOR_DIRECT = 2.f;

  /* Force algortihm to use close waypoints near start and destination */
  static Q_DECL_CONSTEXPR float COST_FACTOR_FORCE_CLOSE_NODES = 1.5f;

  /* Force algortihm to use closest radio navaids near start and destination before waypoints */
  /* Has to be smaller than COST_FACTOR_FORCE_CLOSE_NODES */
  static Q_DECL_CONSTEXPR float COST_FACTOR_FORCE_CLOSE_RADIONAV_VOR = 1.1f;
  static Q_DECL_CONSTEXPR float COST_FACTOR_FORCE_CLOSE_RADIONAV_NDB = 1.2f;

  /* Increase costs to force reception of at least one radio navaid along the route */
  static Q_DECL_CONSTEXPR float COST_FACTOR_UNREACHABLE_RADIONAV = 2.f;

  /* Try to avoid NDBs */
  static Q_DECL_CONSTEXPR float COST_FACTOR_NDB = 1.5f;

  /* Try to avoid VORs (no DME) */
  static Q_DECL_CONSTEXPR float COST_FACTOR_VOR = 1.2f;

  /* Avoid DMEs */
  static Q_DECL_CONSTEXPR float COST_FACTOR_DME = 4.f;

  /* Avoid too long airway segments */
  static Q_DECL_CONSTEXPR float COST_FACTOR_LONG_AIRWAY = 1.2f;

  /* Avoid airway changes during routing */
  static Q_DECL_CONSTEXPR float COST_FACTOR_AIRWAY_CHANGE = 1.1f;

  /* Distance to define a long airway segment in meter */
  static Q_DECL_CONSTEXPR float DISTANCE_LONG_AIRWAY_METER = atools::geo::nmToMeter(200.f);

  int altitude = 0;

  RouteNetwork *network;

  /* Heap structure storing open nodes.
   * Sort order is defined by costs from start to node + estimate to destination */
  atools::util::Heap<nw::Node> openNodesHeap;

  /* Nodes that have been processed already and have a known shortest path */
  QSet<int> closedNodes;

  /* Costs from start to this node. Maps node id to costs. Costs are distance in meter
   * adjusted by some factors. */
  QHash<int, float> nodeCosts;

  /* Maps node id to predecessor node id */
  QHash<int, int> nodePredecessor;
  /* Maps node id to predecessor airway id */
  QHash<int, int> nodeAirwayId;
  QHash<int, QString> nodeAirwayName;

  /* For RouteNetwork::getNeighbours to avoid instantiations */
  QVector<nw::Node> successorNodes;
  QVector<nw::Edge> successorEdges;

  bool preferVorToAirway = false, preferNdbToAirway = false;
};

#endif // LITTLENAVMAP_ROUTEFINDER_H
