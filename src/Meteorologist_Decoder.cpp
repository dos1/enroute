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

#include <QDataStream>
#include <QTimeZone>
#include <QXmlStreamAttribute>

#include <QDebug>
#include "../3rdParty/metaf/include/metaf.hpp"
#include <vector>
#include <string>
#include <string_view>
#include <sstream>
#include <cmath>

#include "Clock.h"
#include "Meteorologist_Decoder.h"
#include "Meteorologist_WeatherStation.h"


Meteorologist::Decoder::Decoder(QString rawText, QObject *parent)
    : QObject(parent)
{

    const auto result = metaf::Parser::parse(rawText.toStdString());

    // Error handling
    /*
    if (result.reportMetadata.error == metaf::ReportError::NONE)
        qDebug() << "parsed successfully";
    */

    QStringList decodedStrings;
    for (const auto &groupInfo : result.groups) {
        auto description = visit(groupInfo);
        decodedStrings.append(description);
/*
           if (std::holds_alternative<metaf::WeatherGroup>(groupInfo.group))
            _weather = description;
 */
    }

    _decodedText = decodedStrings.join("<br>");
}


// explanation Methods


QString Meteorologist::Decoder::explainDirection(const metaf::Direction & direction, bool trueCardinalDirections)
{
    std::ostringstream result;
    switch (direction.type()) {
    case metaf::Direction::Type::NOT_REPORTED:
        return tr("not reported");

    case metaf::Direction::Type::VARIABLE:
        return tr("variable");

    case metaf::Direction::Type::NDV:
        return tr("no directional variation");

    case metaf::Direction::Type::VALUE_DEGREES:
        if (const auto d = direction.degrees(); d.has_value())
            return QString("%1°").arg(*d);
        return tr("[unable to produce value in °]");

    case metaf::Direction::Type::VALUE_CARDINAL:
        if (const auto c = cardinalDirectionToString(direction.cardinal(trueCardinalDirections)); !c.isEmpty()) {
            if (direction.type() == metaf::Direction::Type::VALUE_DEGREES)
                return QString("(%1)").arg(c);
            return c;
        }
        break;

    case metaf::Direction::Type::OVERHEAD:
        return tr("overhead");

    case metaf::Direction::Type::ALQDS:
        return tr("all quadrants (all directions)");

    case metaf::Direction::Type::UNKNOWN:
        return tr("unknown direction");
    }

    return QString();
}

QString Meteorologist::Decoder::explainDistance_FT(const metaf::Distance & distance) {

    if (!distance.isReported())
        return tr("not reported");

    QString modifier;
    switch (distance.modifier()) {
    case metaf::Distance::Modifier::LESS_THAN:
        modifier = "≤ ";
        break;

    case metaf::Distance::Modifier::MORE_THAN:
        modifier = "≥ ";
        break;

    default:
        break;
    }

    if (!distance.isValue())
        return tr("no value");

    const auto d = distance.toUnit(metaf::Distance::Unit::FEET);
    if (d.has_value())
        return modifier + QString("%1 ft").arg(qRound(*d));

    return "[unable to convert distance to feet]";
}

QString Meteorologist::Decoder::explainMetafTime(const metaf::MetafTime & metafTime)
{
    // QTime for result
    auto metafQTime = QTime(metafTime.hour(), metafTime.minute());

#warning We should use the reporting time of the METAR/TAF here, but for now this is a good approximation
    // Compute QDime for result. This is complicated because METARs/TAFs contain only the day of the month, but
    // not the day.
    auto currentQDate = QDate::currentDate();
    auto currentDate = metaf::MetafTime::Date(currentQDate.year(), currentQDate.month(), currentQDate.day());
    auto metafDate = metafTime.dateBeforeRef(currentDate);
    auto metafQDate = QDate(metafDate.year, metafDate.month, metafDate.day);

    auto metafQDateTime = QDateTime(metafQDate, metafQTime, QTimeZone::utc());
    return Clock::describePointInTime(metafQDateTime);


    auto currentDateTime = QDateTime::currentDateTimeUtc();
    auto metarDateTime = currentDateTime;

    const auto day = metafTime.day();
    if (day.has_value()) {
        if (currentDateTime.date().day() > 25 && *day < 5)
            metarDateTime = currentDateTime.addMonths(1);
        if (currentDateTime.date().day() < 5 && *day > 25)
            metarDateTime = currentDateTime.addMonths(-1);
    }
    metarDateTime.setTime(QTime(metafTime.hour(), metafTime.minute()));

    return Clock::describePointInTime(metarDateTime);
}

QString Meteorologist::Decoder::explainPressure(const metaf::Pressure & pressure) {

    if (!pressure.pressure().has_value())
        return tr("not reported");

    const auto phpa = pressure.toUnit(metaf::Pressure::Unit::HECTOPASCAL);
    if (phpa.has_value())
        return QString("%1 hPa").arg(qRound(*phpa));
    return tr("[unable to convert pressure to hPa]");
}

QString Meteorologist::Decoder::explainRunway(const metaf::Runway & runway) {
    if (runway.isAllRunways())
        return tr("all runways");
    if (runway.isMessageRepetition())
        return tr("same runway (repetition of last message)");

    switch(runway.designator()) {
    case metaf::Runway::Designator::NONE:
        return tr("runway %1").arg(runway.number(), 2, 10, QChar('0'));

    case metaf::Runway::Designator::LEFT:
        return tr("runway %1 LEFT").arg(runway.number(), 2, 10, QChar('0'));

    case metaf::Runway::Designator::CENTER:
        return tr("runway %1 CENTER").arg(runway.number(), 2, 10, QChar('0'));

    case metaf::Runway::Designator::RIGHT:
        return tr("runway %1 RIGHT").arg(runway.number(), 2, 10, QChar('0'));

    }
    return QString();
}

QString Meteorologist::Decoder::explainSpeed(const metaf::Speed & speed) {

    if (const auto s = speed.speed(); !s.has_value())
        return tr("not reported");

    bool useMetric = false;
    auto globalSettings = GlobalSettings::globalInstance();
    if (globalSettings)
        useMetric = globalSettings->useMetricUnits();

    if (useMetric) {
        const auto s = speed.toUnit(metaf::Speed::Unit::KILOMETERS_PER_HOUR);
        if (s.has_value())
            return QString("%1 km/h").arg(qRound(*s));
        return tr("[unable to convert speed to km/h]");
    }

    const auto s = speed.toUnit(metaf::Speed::Unit::KNOTS);
    if (s.has_value())
        return QString("%1 kt").arg(qRound(*s));
    return tr("[unable to convert speed to knots]");
}

QString Meteorologist::Decoder::explainTemperature(const metaf::Temperature & temperature)
{
    if (!temperature.temperature().has_value())
        return tr("not reported");

    QString temperatureString = tr("[unable to convert temperature to °C]");
    const auto t = temperature.toUnit(metaf::Temperature::Unit::C);
    if (t.has_value())
        temperatureString = QString("%1 °C").arg(qRound(*t));

    if (!(*temperature.temperature()) && !temperature.isPrecise()) {
        if (temperature.isFreezing())
            return tr("slightly less than %1").arg(temperatureString);
        if (!temperature.isFreezing())
            return tr("slightly more than %1").arg(temperatureString);
    }

    return temperatureString;
}

QString Meteorologist::Decoder::explainWeatherPhenomena(const metaf::WeatherPhenomena & wp)
{
    /* Handle special cases */
    const auto weatherStr = Meteorologist::Decoder::specialWeatherPhenomenaToString(wp);
    if (!weatherStr.isEmpty())
        return weatherStr;

    // Obtain strings for qualifier & descriptor
    auto qualifier = Meteorologist::Decoder::weatherPhenomenaQualifierToString(wp.qualifier()); // Qualifier, such as "light" or "moderate"
    auto descriptor = Meteorologist::Decoder::weatherPhenomenaDescriptorToString(wp.descriptor()); // Descriptor, such as "freezing" or "blowing"

    // String that will hold the result
    QString result;

    QStringList weatherPhenomena;
    for (const auto w : wp.weather()) {
        // This is a string such as "hail" or "rain"
        auto wpString = Meteorologist::Decoder::weatherPhenomenaWeatherToString(w);
        if (!wpString.isEmpty())
            weatherPhenomena << Meteorologist::Decoder::weatherPhenomenaWeatherToString(w);
    }
    // Special case: "shower" is used as a phenomenom
    if (weatherPhenomena.isEmpty() && wp.descriptor() == metaf::WeatherPhenomena::Descriptor::SHOWERS) {
        weatherPhenomena << tr("shower");
        descriptor = QString();
    }
    if (weatherPhenomena.isEmpty() && wp.descriptor() == metaf::WeatherPhenomena::Descriptor::THUNDERSTORM) {
        weatherPhenomena << tr("thunderstorm");
        descriptor = QString();
    }
    result += weatherPhenomena.join(", ");

    // Handle special qualifiers

    if (wp.qualifier() == metaf::WeatherPhenomena::Qualifier::RECENT) {
        result = tr("recent %1").arg(result);
        qualifier = QString();
    }
    if (wp.qualifier() == metaf::WeatherPhenomena::Qualifier::VICINITY) {
        result = tr("%1 in the vicinity").arg(result);
        qualifier = QString();
    }

    // The remaining descriptors and qualifiers go into a parenthesis text
    QStringList parenthesisTexts;
    if (!qualifier.isEmpty())
        parenthesisTexts << qualifier;
    if (!descriptor.isEmpty())
        parenthesisTexts << descriptor;
    auto parenthesisText = parenthesisTexts.join(", ");
    if (!parenthesisText.isEmpty())
        result += QString(" (%1)").arg(parenthesisText);


    const auto time = wp.time();
    switch (wp.event()){
    case metaf::WeatherPhenomena::Event::BEGINNING:
        if (!time.has_value())
            break;
        result += " " + tr("began:") + " " + Meteorologist::Decoder::explainMetafTime(*wp.time());
        break;

    case metaf::WeatherPhenomena::Event::ENDING:
        if (!time.has_value())
            break;
        result += " " + tr("ended:") + " " + Meteorologist::Decoder::explainMetafTime(*wp.time());
        break;

    case metaf::WeatherPhenomena::Event::NONE:
        break;
    }

    if (!parenthesisText.isEmpty())
        qWarning() << "Weather phenomena w/o special handling code" << result;

    return result;
}


// …toString Methods


QString Meteorologist::Decoder::cardinalDirectionToString(metaf::Direction::Cardinal cardinal)
{
    switch(cardinal) {
    case metaf::Direction::Cardinal::NOT_REPORTED:
        return tr("not reported");

    case metaf::Direction::Cardinal::N:
        return tr("north");

    case metaf::Direction::Cardinal::S:
        return tr("south");

    case metaf::Direction::Cardinal::W:
        return tr("west");

    case metaf::Direction::Cardinal::E:
        return tr("east");

    case metaf::Direction::Cardinal::NW:
        return tr("northwest");

    case metaf::Direction::Cardinal::NE:
        return tr("northeast");

    case metaf::Direction::Cardinal::SW:
        return tr("southwest");

    case metaf::Direction::Cardinal::SE:
        return tr("southeast");

    case metaf::Direction::Cardinal::TRUE_N:
        return tr("true north");

    case metaf::Direction::Cardinal::TRUE_W:
        return tr("true west");

    case metaf::Direction::Cardinal::TRUE_S:
        return tr("true south");

    case metaf::Direction::Cardinal::TRUE_E:
        return tr("true east");

    case metaf::Direction::Cardinal::NDV:
        return tr("no directional variations");

    case metaf::Direction::Cardinal::VRB:
        return "variable";

    case metaf::Direction::Cardinal::OHD:
        return "overhead";

    case metaf::Direction::Cardinal::ALQDS:
        return "all quadrants (in all directions)";

    case metaf::Direction::Cardinal::UNKNOWN:
        return "unknown direction";
    }
}

QString Meteorologist::Decoder::weatherPhenomenaDescriptorToString(metaf::WeatherPhenomena::Descriptor descriptor)
{
    switch(descriptor) {
    case metaf::WeatherPhenomena::Descriptor::NONE:
        return QString();

    case metaf::WeatherPhenomena::Descriptor::SHALLOW:
        return tr("shallow");

    case metaf::WeatherPhenomena::Descriptor::PARTIAL:
        return tr("partial");

    case metaf::WeatherPhenomena::Descriptor::PATCHES:
        return tr("patches");

    case metaf::WeatherPhenomena::Descriptor::LOW_DRIFTING:
        return tr("low drifting");

    case metaf::WeatherPhenomena::Descriptor::BLOWING:
        return tr("blowing");

    case metaf::WeatherPhenomena::Descriptor::SHOWERS:
        return tr("showers");

    case metaf::WeatherPhenomena::Descriptor::THUNDERSTORM:
        return tr("thunderstorm");

    case metaf::WeatherPhenomena::Descriptor::FREEZING:
        return tr("freezing");
    }
}

QString Meteorologist::Decoder::weatherPhenomenaQualifierToString(metaf::WeatherPhenomena::Qualifier qualifier)
{
    switch (qualifier) {
    case metaf::WeatherPhenomena::Qualifier::NONE:
        return QString();

    case metaf::WeatherPhenomena::Qualifier::RECENT:
        return QString("recent");

    case metaf::WeatherPhenomena::Qualifier::VICINITY:
        return QString("in vicinity");

    case metaf::WeatherPhenomena::Qualifier::LIGHT:
        return tr("light");

    case metaf::WeatherPhenomena::Qualifier::MODERATE:
        return tr("moderate");

    case metaf::WeatherPhenomena::Qualifier::HEAVY:
        return tr("heavy");
    }
}

QString Meteorologist::Decoder::weatherPhenomenaWeatherToString(metaf::WeatherPhenomena::Weather weather)
{
    switch (weather) {
    case metaf::WeatherPhenomena::Weather::NOT_REPORTED:
        return QString();

    case metaf::WeatherPhenomena::Weather::DRIZZLE:
        return tr("drizzle");

    case metaf::WeatherPhenomena::Weather::RAIN:
        return tr("rain");

    case metaf::WeatherPhenomena::Weather::SNOW:
        return tr("snow");

    case metaf::WeatherPhenomena::Weather::SNOW_GRAINS:
        return tr("snow grains");

    case metaf::WeatherPhenomena::Weather::ICE_CRYSTALS:
        return tr("ice crystals");

    case metaf::WeatherPhenomena::Weather::ICE_PELLETS:
        return tr("ice pellets");

    case metaf::WeatherPhenomena::Weather::HAIL:
        return tr("hail");

    case metaf::WeatherPhenomena::Weather::SMALL_HAIL:
        return tr("small hail");

    case metaf::WeatherPhenomena::Weather::UNDETERMINED:
        return tr("undetermined precipitation");

    case metaf::WeatherPhenomena::Weather::MIST:
        return tr("mist");

    case metaf::WeatherPhenomena::Weather::FOG:
        return tr("fog");

    case metaf::WeatherPhenomena::Weather::SMOKE:
        return tr("smoke");

    case metaf::WeatherPhenomena::Weather::VOLCANIC_ASH:
        return tr("volcanic ash");

    case metaf::WeatherPhenomena::Weather::DUST:
        return tr("dust");

    case metaf::WeatherPhenomena::Weather::SAND:
        return tr("sand");

    case metaf::WeatherPhenomena::Weather::HAZE:
        return tr("haze");

    case metaf::WeatherPhenomena::Weather::SPRAY:
        return tr("spray");

    case metaf::WeatherPhenomena::Weather::DUST_WHIRLS:
        return tr("dust or sand whirls");

    case metaf::WeatherPhenomena::Weather::SQUALLS:
        return tr("squalls");

    case metaf::WeatherPhenomena::Weather::FUNNEL_CLOUD:
        return tr("funnel cloud");

    case metaf::WeatherPhenomena::Weather::SANDSTORM:
        return tr("sand storm");

    case metaf::WeatherPhenomena::Weather::DUSTSTORM:
        return tr("dust storm");
    }
}

QString Meteorologist::Decoder::specialWeatherPhenomenaToString(const metaf::WeatherPhenomena & wp)
{
    QStringList results;
    for (const auto &weather : wp.weather()) {

        // PRECIPITATION, undetermined
        if (wp.descriptor() == metaf::WeatherPhenomena::Descriptor::NONE && weather == metaf::WeatherPhenomena::Weather::UNDETERMINED) {
            switch(wp.qualifier()) {
            case metaf::WeatherPhenomena::Qualifier::NONE:
                results << tr("precipitation");
                break;
            case metaf::WeatherPhenomena::Qualifier::LIGHT:
                results << tr("light precipitation");
                break;
            case metaf::WeatherPhenomena::Qualifier::MODERATE:
                results << tr("moderate precipitation");
                break;
            case metaf::WeatherPhenomena::Qualifier::HEAVY:
                results << tr("heavy precipitation");
                break;
            default:
                return QString();
            }
            continue;
        }

        // DRIZZLE
        if (wp.descriptor() == metaf::WeatherPhenomena::Descriptor::NONE && weather == metaf::WeatherPhenomena::Weather::DRIZZLE) {
            switch(wp.qualifier()) {
            case metaf::WeatherPhenomena::Qualifier::LIGHT:
                results << tr("light drizzle");
                break;
            case metaf::WeatherPhenomena::Qualifier::MODERATE:
                results << tr("moderate drizzle");
                break;
            case metaf::WeatherPhenomena::Qualifier::HEAVY:
                results << tr("heavy drizzle");
                break;
            default:
                return QString();
            }
            continue;
        }

        // DUST, BLOWING
        if (wp.descriptor() == metaf::WeatherPhenomena::Descriptor::BLOWING && weather == metaf::WeatherPhenomena::Weather::DUST) {
            switch(wp.qualifier()) {
            case metaf::WeatherPhenomena::Qualifier::NONE:
                results << tr("blowing dust");
                break;
            case metaf::WeatherPhenomena::Qualifier::LIGHT:
                results << tr("light blowing dust");
                break;
            case metaf::WeatherPhenomena::Qualifier::MODERATE:
                results << tr("moderate blowing dust");
                break;
            case metaf::WeatherPhenomena::Qualifier::HEAVY:
                results << tr("heavy blowing dust");
                break;
            default:
                return QString();
            }
            continue;
        }

        // FOG
        if (wp.qualifier() == metaf::WeatherPhenomena::Qualifier::NONE && weather == metaf::WeatherPhenomena::Weather::FOG) {
            switch(wp.descriptor()) {
            case metaf::WeatherPhenomena::Descriptor::FREEZING:
                results << tr("freezing fog");
                break;
            case metaf::WeatherPhenomena::Descriptor::PARTIAL:
                results << tr("partial fog");
                break;
            case metaf::WeatherPhenomena::Descriptor::PATCHES:
                results << tr("patches of fog");
                break;
            case metaf::WeatherPhenomena::Descriptor::SHALLOW:
                results << tr("shallow fog");
                break;
            default:
                return QString();
            }
            continue;
        }


        // RAIN
        if (wp.descriptor() == metaf::WeatherPhenomena::Descriptor::NONE && weather == metaf::WeatherPhenomena::Weather::RAIN) {
            switch(wp.qualifier()) {
            case metaf::WeatherPhenomena::Qualifier::LIGHT:
                results << tr("light rain");
                break;
            case metaf::WeatherPhenomena::Qualifier::MODERATE:
                results << tr("moderate rain");
                break;
            case metaf::WeatherPhenomena::Qualifier::HEAVY:
                results << tr("heavy rain");
                break;
            default:
                return QString();
            }
            continue;
        }

        // RAIN SHOWERS
        if (wp.descriptor() == metaf::WeatherPhenomena::Descriptor::SHOWERS && weather == metaf::WeatherPhenomena::Weather::RAIN) {
            switch(wp.qualifier()) {
            case metaf::WeatherPhenomena::Qualifier::LIGHT:
                results << tr("light rain showers");
                break;
            case metaf::WeatherPhenomena::Qualifier::MODERATE:
                results << tr("moderate rain showers");
                break;
            case metaf::WeatherPhenomena::Qualifier::HEAVY:
                results << tr("heavy rain showers");
                break;
            case metaf::WeatherPhenomena::Qualifier::RECENT:
                results << tr("recent rain showers");
                break;
            default:
                return QString();
            }
            continue;
        }

        // SNOW
        if (wp.descriptor() == metaf::WeatherPhenomena::Descriptor::NONE && weather == metaf::WeatherPhenomena::Weather::SNOW) {
            switch(wp.qualifier()) {
            case metaf::WeatherPhenomena::Qualifier::LIGHT:
                results << tr("light snowfall");
                break;
            case metaf::WeatherPhenomena::Qualifier::MODERATE:
                results << tr("moderate snowfall");
                break;
            case metaf::WeatherPhenomena::Qualifier::HEAVY:
                results << tr("heavy snowfall");
                break;
            default:
                return QString();
            }
            continue;
        }

        // SNOW, BLOWING
        if (wp.descriptor() == metaf::WeatherPhenomena::Descriptor::BLOWING && weather == metaf::WeatherPhenomena::Weather::SNOW) {
            switch(wp.qualifier()) {
            case metaf::WeatherPhenomena::Qualifier::NONE:
                results << tr("blowing snow");
                break;
            case metaf::WeatherPhenomena::Qualifier::LIGHT:
                results << tr("light blowing snow");
                break;
            case metaf::WeatherPhenomena::Qualifier::MODERATE:
                results << tr("moderate blowing snow");
                break;
            case metaf::WeatherPhenomena::Qualifier::HEAVY:
                results << tr("heavy blowing snow");
                break;
            default:
                return QString();
            }
            continue;
        }

        // SNOW, LOW_DRIFTING
        if (wp.descriptor() == metaf::WeatherPhenomena::Descriptor::LOW_DRIFTING && weather == metaf::WeatherPhenomena::Weather::SNOW) {
            switch(wp.qualifier()) {
            case metaf::WeatherPhenomena::Qualifier::NONE:
                results << tr("drifting snow");
                break;
            case metaf::WeatherPhenomena::Qualifier::LIGHT:
                results << tr("light drifting snow");
                break;
            case metaf::WeatherPhenomena::Qualifier::MODERATE:
                results << tr("moderate drifting snow");
                break;
            case metaf::WeatherPhenomena::Qualifier::HEAVY:
                results << tr("heavy drifting snow");
                break;
            default:
                return QString();
            }
            continue;
        }

        // SNOW SHOWERS
        if (wp.descriptor() == metaf::WeatherPhenomena::Descriptor::SHOWERS && weather == metaf::WeatherPhenomena::Weather::SNOW) {
            switch(wp.qualifier()) {
            case metaf::WeatherPhenomena::Qualifier::LIGHT:
                results << tr("light snow showers");
                break;
            case metaf::WeatherPhenomena::Qualifier::MODERATE:
                results << tr("moderate snow showers");
                break;
            case metaf::WeatherPhenomena::Qualifier::HEAVY:
                results << tr("heavy snow showers");
                break;
            default:
                return QString();
            }
            continue;
        }

        return QString();
    }

    return results.join(" • ");
}


// Visitor methods

QString Meteorologist::Decoder::visitPressureGroup(const PressureGroup & group, ReportPart, const std::string &)
{
    if (!group.isValid())
        return tr("Invalid data");

    switch(group.type()) {
    case metaf::PressureGroup::Type::OBSERVED_QNH:
        return tr("QNH: %1").arg(explainPressure(group.atmosphericPressure()));

    case metaf::PressureGroup::Type::FORECAST_LOWEST_QNH:
        return tr("Forecast lowest QNH: %1").arg(explainPressure(group.atmosphericPressure()));
        break;

    case metaf::PressureGroup::Type::OBSERVED_QFE:
        return tr("QFE: %1").arg(explainPressure(group.atmosphericPressure()));
        break;

    case metaf::PressureGroup::Type::SLPNO:
        return tr("QNH is not available");
        break;

    case metaf::PressureGroup::Type::PRES_MISG:
        return tr("Atmospheric pressure data is missing");
    }
    return QString();
}

QString Meteorologist::Decoder::visitTemperatureGroup(const TemperatureGroup & group, ReportPart, const std::string &)
{
    if (!group.isValid())
        return tr("Invalid data");

    const auto rh = group.relativeHumidity();

    switch (group.type()) {
    case metaf::TemperatureGroup::Type::TEMPERATURE_AND_DEW_POINT:
        if (rh.has_value())
            return tr("Temperature %1, Dew point %2, Humidity %3%").arg(explainTemperature(group.airTemperature())).arg(explainTemperature(group.dewPoint())).arg(qRound(*rh));
        return tr("Temperature %1, Dew point %2").arg(explainTemperature(group.airTemperature())).arg(explainTemperature(group.dewPoint()));

    case metaf::TemperatureGroup::Type::T_MISG:
        return tr("Temperature data is missing");

    case metaf::TemperatureGroup::Type::TD_MISG:
        return tr("Dew point data is missing");
    }
    return QString();
}

QString Meteorologist::Decoder::visitWindGroup(const WindGroup & group, ReportPart, const std::string &)
{
    if (!group.isValid())
        return tr("Invalid data");

    switch (group.type()) {
    case metaf::WindGroup::Type::SURFACE_WIND_CALM:
        return tr("No wind");

    case metaf::WindGroup::Type::SURFACE_WIND:
        if (group.gustSpeed().isReported())
            return tr("Wind direction %1, wind speed %2, gusts at %3")
                    .arg(explainDirection(group.direction(), true))
                    .arg(explainSpeed(group.windSpeed()))
                    .arg(explainSpeed(group.gustSpeed()));
        return tr("Wind direction %1, wind speed %2")
                .arg(explainDirection(group.direction(), true))
                .arg(explainSpeed(group.windSpeed()));

    case metaf::WindGroup::Type::VARIABLE_WIND_SECTOR:
        return tr("Variable wind direction %1 -- %2")
                .arg(explainDirection(group.varSectorBegin()))
                .arg(explainDirection(group.varSectorEnd()));

    case metaf::WindGroup::Type::SURFACE_WIND_WITH_VARIABLE_SECTOR:
        if (group.gustSpeed().isReported())
            return tr("Wind direction %1 (%2 -- %3), wind speed %4, gusts at %5")
                    .arg(explainDirection(group.direction(), true))
                    .arg(explainDirection(group.varSectorBegin()))
                    .arg(explainDirection(group.varSectorEnd()))
                    .arg(explainSpeed(group.windSpeed()))
                    .arg(explainSpeed(group.gustSpeed()));
        return tr("Wind direction %1 (%2 -- %3), wind speed %4")
                .arg(explainDirection(group.direction(), true))
                .arg(explainDirection(group.varSectorBegin()))
                .arg(explainDirection(group.varSectorEnd()))
                .arg(explainSpeed(group.windSpeed()));

    case metaf::WindGroup::Type::WIND_SHEAR:
        if (group.gustSpeed().isReported())
            return tr("Wind shear at %1 AGL, wind direction %2, wind speed %3, gusts at %4")
                    .arg(explainDistance_FT(group.height()))
                    .arg(explainDirection(group.direction(), true))
                    .arg(explainSpeed(group.windSpeed()))
                    .arg(explainSpeed(group.gustSpeed()));
        return tr("Wind shear at %1 AGL, wind direction %2, wind speed %3")
                .arg(explainDistance_FT(group.height()))
                .arg(explainDirection(group.direction(), true))
                .arg(explainSpeed(group.windSpeed()));


    case metaf::WindGroup::Type::WIND_SHIFT:
        if (group.eventTime())
            return tr("Wind direction changed at %1").arg(explainMetafTime(*group.eventTime()));
        return tr("Wind direction changed recently");

    case metaf::WindGroup::Type::WIND_SHIFT_FROPA:
        if (group.eventTime())
            return tr("Wind direction changed at %1 because of weather front passage").arg(explainMetafTime(*group.eventTime()));
        return tr("Wind directed changed recently because of weather front passage");

    case metaf::WindGroup::Type::PEAK_WIND:
        return tr("Peak wind observed at %1, wind direction %2, wind speed %3")
                .arg(explainMetafTime(group.eventTime().value()))
                .arg(explainDirection(group.direction(), true))
                .arg(explainSpeed(group.windSpeed()));

    case metaf::WindGroup::Type::WIND_SHEAR_IN_LOWER_LAYERS:
        if (const auto rw = group.runway(); rw.has_value())
            return  tr("Wind shear between runway level and 1.600 ft at runway %1").arg(explainRunway(*rw));
        return tr("Wind shear between runway level and 1.600 ft");

    case metaf::WindGroup::Type::WSCONDS:
        return tr("Potential wind shear");

    case metaf::WindGroup::Type::WND_MISG:
        return tr("Wind data is missing");
    }
    return QString();
}