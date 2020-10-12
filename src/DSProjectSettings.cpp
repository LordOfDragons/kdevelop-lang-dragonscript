#include <QDirIterator>
#include <QRegularExpression>
#include <QVersionNumber>
#include <QDebug>
#include <interfaces/icore.h>
#include <interfaces/isession.h>
#include <KF5/KConfigCore/kconfiggroup.h>

#include "DSProjectSettings.h"


using namespace KDevelop;

namespace DragonScript{


DSProjectSettings::DSProjectSettings() :
pRequireDragenginePackage( false ){
}


void DSProjectSettings::setRequireDragenginePackage( bool requires ){
	pRequireDragenginePackage = requires;
}

void DSProjectSettings::setPathInclude( const QStringList &list ){
	pPathInclude = list;
}

void DSProjectSettings::load( const IProject &project ){
	const KConfigGroup config( project.projectConfiguration()->group( "dragonscriptsupport" ) );
	pRequireDragenginePackage = config.readEntry( "requireDragenginePackage", QString() ) == "true";
	pPathInclude = config.readEntry( "pathInclude", QStringList() );
}

void DSProjectSettings::save( const IProject &project ){
	KConfigGroup config( project.projectConfiguration()->group( "dragonscriptsupport" ) );
	config.writeEntry( "requireDragenginePackage", pRequireDragenginePackage ? "true" : "false" );
	config.writeEntry( "pathInclude", pPathInclude );
}

}
