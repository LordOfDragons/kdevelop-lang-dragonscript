#ifndef DSPROJECTSETTINGS_H
#define DSPROJECTSETTINGS_H

#include <QString>
#include <QStringList>

namespace DragonScript{

class DSProjectSettings
{
public:
	DSProjectSettings();
	
private:
	QStringList pPathPackages;
};

}
#endif
