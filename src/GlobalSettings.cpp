/***************************************************************************
 *   Copyright (C) 2019-2020 by Stefan Kebekus                             *
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

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>

#include "GlobalSettings.h"


// Static instance of this class
Q_GLOBAL_STATIC(GlobalSettings, globalSettingsStatic);


GlobalSettings::GlobalSettings(QObject *parent)
    : QObject(parent)
{
    installTranslators();
}


GlobalSettings::~GlobalSettings()
{
    // Save some values
    settings.setValue("lastVersion", PROJECT_VERSION);
}


auto GlobalSettings::acceptedWeatherTermsStatic() -> bool
{
    // Find out that unit system we should use
    auto globalSettings = GlobalSettings::globalInstance();
    if (globalSettings)
        return globalSettings->acceptedWeatherTerms();
    // Fallback in the very unlikely case that no global object exists
    return false;
}


auto GlobalSettings::globalInstance() -> GlobalSettings *
{
    return globalSettingsStatic;
}


auto GlobalSettings::hideUpperAirspacesStatic() -> bool
{
    // Find out that unit system we should use
    auto globalSettings = GlobalSettings::globalInstance();
    if (globalSettings)
        return globalSettings->hideUpperAirspaces();
    // Fallback in the very unlikely case that no global object exists
    return false;
}


void GlobalSettings::setAcceptedTerms(int terms)
{
    if (terms == acceptedTerms())
        return;
    settings.setValue("acceptedTerms", terms);
    emit acceptedTermsChanged();
}


void GlobalSettings::setAcceptedWeatherTerms(bool terms)
{
    if (terms == acceptedWeatherTerms())
        return;
    settings.setValue("acceptedWeatherTerms", terms);
    emit acceptedWeatherTermsChanged();
}


void GlobalSettings::setAutoFlightDetection(bool autoDetect)
{
    if (autoDetect == autoFlightDetection())
        return;
    settings.setValue("Map/autoFlightDetection", autoDetect);
    emit autoFlightDetectionChanged();
}


void GlobalSettings::setHideUpperAirspaces(bool hide)
{
    if (hide == hideUpperAirspaces())
        return;
    settings.setValue("Map/hideUpperAirspaces", hide);
    emit hideUpperAirspacesChanged();
}


void GlobalSettings::setLastWhatsNewHash(uint lwnh)
{
    if (lwnh == lastWhatsNewHash())
        return;
    settings.setValue("lastWhatsNewHash", lwnh);
    emit lastWhatsNewHashChanged();
}


auto GlobalSettings::mapBearingPolicy() const -> GlobalSettings::MapBearingPolicyValues
{
    auto intVal = settings.value("Map/bearingPolicy", 0).toInt();
    if (intVal == 0)
        return NUp;
    if (intVal == 1)
        return TTUp;
    return UserDefinedBearingUp;
}


void GlobalSettings::setMapBearingPolicy(MapBearingPolicyValues policy)
{
    if (policy == mapBearingPolicy())
        return;

    switch(policy){
    case NUp:
        settings.setValue("Map/bearingPolicy", 0);
        break;
    case TTUp:
        settings.setValue("Map/bearingPolicy", 1);
        break;
    default:
        settings.setValue("Map/bearingPolicy", 2);
        break;
    }
    emit mapBearingPolicyChanged();
}


void GlobalSettings::setUseMetricUnits(bool unitHorrizKmh)
{
    if (unitHorrizKmh == useMetricUnits())
        return;

    settings.setValue("System/useMetricUnits", unitHorrizKmh);
    emit useMetricUnitsChanged();
}


void GlobalSettings::setPreferEnglish(bool preferEng)
{
    if (preferEng == preferEnglish())
        return;

    settings.setValue("System/preferEnglish", preferEng);
    installTranslators();
    emit preferEnglishChanged();
}


auto GlobalSettings::useMetricUnitsStatic() -> bool
{
    // Find out that unit system we should use
    auto globalSettings = GlobalSettings::globalInstance();
    if (globalSettings)
        return globalSettings->useMetricUnits();
    // Fallback in the very unlikely case that no global object exists
    return false;
}


void GlobalSettings::installTranslators()
{
    // Remove existing translators
    if (enrouteTranslator) {
        QCoreApplication::removeTranslator(enrouteTranslator);
        delete enrouteTranslator;
    }

    // If desired, install new translators
    if (preferEnglish()) {
        QLocale::setDefault(QLocale::English);
    } else {
        QLocale::setDefault(QLocale::system());

        enrouteTranslator = new QTranslator(this);
        enrouteTranslator->load(QString(":enroute_%1.qm").arg(QLocale::system().name().left(2)));
        QCoreApplication::installTranslator(enrouteTranslator);
    }

}
