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

#include "search/searchcontroller.h"

#include "gui/mainwindow.h"
#include "search/column.h"
#include "search/columnlist.h"
#include "search/airportsearch.h"
#include "search/navsearch.h"
#include "mapgui/mapwidget.h"
#include "gui/helphandler.h"
#include "ui_mainwindow.h"

#include <QTabWidget>
#include <QUrl>

using atools::gui::HelpHandler;

SearchController::SearchController(MainWindow *parent, MapQuery *mQuery,
                                   QTabWidget *tabWidgetSearch)
  : mapQuery(mQuery), mainWindow(parent), tabWidget(tabWidgetSearch)
{
  connect(tabWidget, &QTabWidget::currentChanged, this, &SearchController::tabChanged);
  connect(parent->getUi()->pushButtonAirportHelpSearch, &QPushButton::clicked,
          this, &SearchController::helpPressed);
  connect(parent->getUi()->pushButtonNavHelpSearch, &QPushButton::clicked,
          this, &SearchController::helpPressed);
}

SearchController::~SearchController()
{
  delete airportSearch;
  delete navSearch;
}

void SearchController::getSelectedMapObjects(maptypes::MapSearchResult& result) const
{
  allSearchTabs.at(tabWidget->currentIndex())->getSelectedMapObjects(result);
}

void SearchController::optionsChanged()
{
  for(SearchBase *search : allSearchTabs)
    search->optionsChanged();
}

void SearchController::helpPressed()
{
  HelpHandler::openHelpUrl(mainWindow, HelpHandler::getHelpUrl(mainWindow, "help", "indexsearch.html"));
}

/* Forces an emit of selection changed signal if the active tab changes */
void SearchController::tabChanged(int index)
{
  allSearchTabs.at(index)->updateTableSelection();
}

void SearchController::saveState()
{
  for(SearchBase *s : allSearchTabs)
    s->saveState();
}

void SearchController::restoreState()
{
  for(SearchBase *s : allSearchTabs)
    s->restoreState();
}

void SearchController::createAirportSearch(QTableView *tableView)
{
  airportSearch = new AirportSearch(mainWindow, tableView, mapQuery, 0);
  postCreateSearch(airportSearch);
}

void SearchController::createNavSearch(QTableView *tableView)
{
  navSearch = new NavSearch(mainWindow, tableView, mapQuery, 1);
  postCreateSearch(navSearch);
}

/* Connect signals and append search object to all search tabs list */
void SearchController::postCreateSearch(SearchBase *search)
{
  search->connectSearchSlots();
  search->updateUnits();

  mainWindow->getMapWidget()->connect(mainWindow->getMapWidget(), &MapWidget::searchMarkChanged,
                                      search, &SearchBase::searchMarkChanged);
  allSearchTabs.append(search);
}

void SearchController::preDatabaseLoad()
{
  for(SearchBase *search : allSearchTabs)
    search->preDatabaseLoad();
}

void SearchController::postDatabaseLoad()
{
  for(SearchBase *search : allSearchTabs)
    search->postDatabaseLoad();
}

void SearchController::showInSearch(maptypes::MapObjectTypes type, const QString& ident,
                                    const QString& region, const QString& airportIdent)
{
  qDebug() << "SearchController::objectSelected type" << type << "ident" << ident << "region" << region;

  switch(type)
  {
    case maptypes::AIRPORT:
      // Shown in airport tab
      airportSearch->resetSearch();
      airportSearch->filterByIdent(ident);
      break;
    case maptypes::NDB:
    case maptypes::VOR:
    case maptypes::WAYPOINT:
      // Shown in navaid tab
      navSearch->resetSearch();
      navSearch->filterByIdent(ident, region, airportIdent);
      break;
    case maptypes::ILS:
    case maptypes::MARKER:
    case maptypes::NONE:
    case maptypes::ALL_NAV:
    case maptypes::ALL:
      qWarning() << "showInSearch invalid type" << type;
      break;
  }
}
