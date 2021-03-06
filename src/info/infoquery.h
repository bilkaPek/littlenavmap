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

#ifndef LITTLENAVMAP_INFOQUERY_H
#define LITTLENAVMAP_INFOQUERY_H

#include <QCache>
#include <QObject>

namespace atools {
namespace sql {
class SqlDatabase;
class SqlQuery;
class SqlRecord;
class SqlRecordVector;
}
}

/*
 * Database queries for the info controller. Does not return objects but sql records. Records are cached.
 */
class InfoQuery :
  public QObject
{
  Q_OBJECT

public:
  InfoQuery(QObject *parent, atools::sql::SqlDatabase *sqlDb);
  virtual ~InfoQuery();

  /* Get record for joined tables airport, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getAirportInformation(int airportId);

  /* Get record for table com */
  const atools::sql::SqlRecordVector *getComInformation(int airportId);

  /* Get record for joined tables vor, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getVorInformation(int vorId);

  /* Get record for joined tables ndb, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getNdbInformation(int ndbId);

  /* Get record for joined tables waypoint, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getWaypointInformation(int waypointId);

  /* Get record for table airway */
  const atools::sql::SqlRecord *getAirwayInformation(int airwayId);

  /* Get records with pairs of from/to waypoints (ident and region) for an airway.
   * The records are ordered as they appear in the airway. */
  atools::sql::SqlRecordVector getAirwayWaypointInformation(const QString& name, int fragment);

  /* Get record list for table runway of an airport */
  const atools::sql::SqlRecordVector *getRunwayInformation(int airportId);

  /* Get record for table runway_end */
  const atools::sql::SqlRecord *getRunwayEndInformation(int runwayEndId);

  const atools::sql::SqlRecordVector *getHelipadInformation(int airportId);
  const atools::sql::SqlRecordVector *getStartInformation(int airportId);

  /* Get record for table ils for an runway end */
  const atools::sql::SqlRecord *getIlsInformation(int runwayEndId);

  /* Get runway name and all columns from table approach */
  const atools::sql::SqlRecordVector *getApproachInformation(int airportId);

  /* Get record for table transition */
  const atools::sql::SqlRecordVector *getTransitionInformation(int approachId);

  /* Create all queries */
  void initQueries();

  /* Delete all queries */
  void deInitQueries();

private:
  const atools::sql::SqlRecord *cachedRecord(QCache<int, atools::sql::SqlRecord>& cache,
                                             atools::sql::SqlQuery *query, int id);

  const atools::sql::SqlRecordVector *cachedRecordVector(QCache<int, atools::sql::SqlRecordVector>& cache,
                                                         atools::sql::SqlQuery *query, int id);

  /* Caches */
  QCache<int, atools::sql::SqlRecord> airportCache, vorCache, ndbCache, waypointCache, airwayCache,
                                      runwayEndCache, ilsCache;
  QCache<int,
         atools::sql::SqlRecordVector> comCache, runwayCache, helipadCache, startCache, approachCache,
                                       transitionCache;

  atools::sql::SqlDatabase *db;

  /* Prepared database queries */
  atools::sql::SqlQuery *airportQuery = nullptr, *vorQuery = nullptr, *ndbQuery = nullptr,
  *waypointQuery = nullptr, *airwayQuery = nullptr, *comQuery = nullptr,
  *runwayQuery = nullptr, *runwayEndQuery = nullptr, *helipadQuery = nullptr, *startQuery = nullptr,
  *ilsQuery = nullptr, *airwayWaypointQuery = nullptr, *approachQuery = nullptr, *transitionQuery = nullptr;

};

#endif // LITTLENAVMAP_INFOQUERY_H
