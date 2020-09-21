/***************************************************************************
 *   Copyright (C) 2020 by Stefan Kebekus                                  *
 *   stefan.kebekus@gmail.com                                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#pragma once

#include <QDateTime>
#include <QGeoCoordinate>
#include <QXmlStreamReader>

#include "WeatherReport.h"


/*! \brief This extremely simple class holds a few numbers that describe an
    WeatherReport::METAR */

class WeatherReport::METAR : public QObject {
    Q_OBJECT

public:
    /*! \brief Default constructor
     *
     * This constructor reads the values of the properties listed below via
     * QSettings. The values are set to NaN if no valid numbers can be found
     * in the settings object.
     *
     * @param parent The standard QObject parent pointer
     */
#warning Need to explain auto-delete feature!!!
    explicit METAR(QXmlStreamReader &xml, QObject *parent = nullptr);

    // Standard destructor
    ~METAR() override = default;

    Q_INVOKABLE QString oneLineDescription() const;


#warning Need to make this private!!!
    int _qnh {0};
    QGeoCoordinate _location;
    QString _station_id;
    QDateTime _observationTime;
    QString _raw_text;

    QMultiMap<QString, QVariant> data;
    QStringList dataStrings;

private:
    Q_DISABLE_COPY_MOVE(METAR)
};