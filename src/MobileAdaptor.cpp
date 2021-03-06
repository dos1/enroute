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


#include "MobileAdaptor.h"

#include <QDir>
#include <QStandardPaths>

#if defined(Q_OS_ANDROID)
#include <QtAndroid>
#include <QtAndroidExtras/QAndroidJniObject>
#include <QAndroidJniEnvironment>

const QStringList permissions({"android.permission.ACCESS_COARSE_LOCATION",
                               "android.permission.ACCESS_FINE_LOCATION",
                               "android.permission.WRITE_EXTERNAL_STORAGE",
                               "android.permission.READ_EXTERNAL_STORAGE"});

MobileAdaptor* MobileAdaptor::mInstance = nullptr;
#endif


MobileAdaptor::MobileAdaptor(QObject *parent)
    : QObject(parent)
{
    // Do all the set-up required for sharing files
    // Android requires you to use a subdirectory within the AppDataLocation for
    // sending and receiving files. We create this and clear this directory on creation of the Share object -- even if the
    // app didn't exit gracefully, the directory is still cleared when starting
    // the app next time.
    fileExchangeDirectoryName = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/exchange/";
    QDir exchangeDir(fileExchangeDirectoryName);
    exchangeDir.removeRecursively();
    exchangeDir.mkpath(fileExchangeDirectoryName);

#if defined (Q_OS_ANDROID)
    // we need the instance for the JNI Call
    mInstance = this;

    // Ask for permissions
    QtAndroid::requestPermissionsSync(permissions);
#endif

    // Disable the screen saver so that the screen does not switch off automatically
#if defined(Q_OS_ANDROID)
    bool on=true;
    // Implementation follows a suggestion found in https://stackoverflow.com/questions/27758499/how-to-keep-the-screen-on-in-qt-for-android
    QtAndroid::runOnAndroidThread([on]{
        QAndroidJniObject activity = QtAndroid::androidActivity();
        if (activity.isValid()) {
            QAndroidJniObject window =
                    activity.callObjectMethod("getWindow", "()Landroid/view/Window;");

            if (window.isValid()) {
                const int FLAG_KEEP_SCREEN_ON = 128;
                if (on) {
                    window.callMethod<void>("addFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
                } else {
                    window.callMethod<void>("clearFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
                }
            }
        }
        QAndroidJniEnvironment env;
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    });
#endif
}


MobileAdaptor::~MobileAdaptor()
{
  // Close all pending notifications
  showDownloadNotification(false);
}


void MobileAdaptor::hideSplashScreen()
{
    if (splashScreenHidden)
        return;
    splashScreenHidden = true;
#if defined(Q_OS_ANDROID)
    QtAndroid::hideSplashScreen(200);
#endif
}


Q_INVOKABLE auto MobileAdaptor::missingPermissionsExist() -> bool
{
#if defined (Q_OS_ANDROID)
    // Check is required permissions have been granted
    for(const QString &permission : permissions)
        if (QtAndroid::checkPermission(permission) == QtAndroid::PermissionResult::Denied)
            return true;
#endif
    return false;
}


void MobileAdaptor::vibrateBrief()
{
#if defined(Q_OS_ANDROID)
    QAndroidJniObject::callStaticMethod<void>("de/akaflieg_freiburg/enroute/MobileAdaptor", "vibrateBrief");
#endif
}


void MobileAdaptor::showDownloadNotification(bool show)
{
  Q_UNUSED(show)
    
#if defined(Q_OS_ANDROID)
    QString text;
  if (show)
    text = tr("Downloading map data…");
  QAndroidJniObject jni_title   = QAndroidJniObject::fromString(text);
  QAndroidJniObject::callStaticMethod<void>("de/akaflieg_freiburg/enroute/MobileAdaptor", "notifyDownload", "(Ljava/lang/String;)V", jni_title.object<jstring>());
#endif
}
