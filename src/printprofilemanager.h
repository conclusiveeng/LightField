#ifndef __PRINTPROFILEMANAGER_H__
#define __PRINTPROFILEMANAGER_H__

#include <QtCore>
#include "printprofile.h"

using PrintProfileCollection = QMap<QString, QSharedPointer<PrintProfile>>;

class PrintProfileManager: public QObject
{
    Q_OBJECT

public:
    PrintProfileManager(QObject* parent = nullptr);

    virtual ~PrintProfileManager() override
    {
        /*empty*/
    }

    PrintProfileCollection& profiles()
    {
        return _profiles;
    }

    QSharedPointer<PrintProfile> activeProfile()
    {
        return _activeProfile;
    }

    QSharedPointer<PrintProfile> getProfile(const QString& name)
    {
        auto iter = _profiles.find(name);
        return iter == _profiles.end() ? QSharedPointer<PrintProfile>() : *iter;
    }

    bool addProfile(QSharedPointer<PrintProfile> newProfile);
    void removeProfile(QString const& name);
    void setActiveProfile(QString const& profileName);
    bool importProfiles(QString const& fileName);
    bool exportProfiles(QString const& fileName);
    void saveProfiles();
    void reload();

private:
    PrintProfileCollection _profiles;
    QSharedPointer<PrintProfile> _activeProfile;

signals:
    void activeProfileChanged(QSharedPointer<PrintProfile> newProfile);
    void reloadProfiles(PrintProfileCollection& profiles);
};

#endif //!__PRINTPROFILEMANAGER_H__
