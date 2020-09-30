#include <QDirIterator>
#include <QRegularExpression>
#include <QVersionNumber>
#include <QDebug>
#include <interfaces/icore.h>
#include <interfaces/isession.h>
#include <KF5/KConfigCore/kconfiggroup.h>

#include "DSSessionSettings.h"
#include "duchain/ImportPackageDragengine.h"


using namespace KDevelop;

namespace DragonScript{

QString DSSessionSettings::defaultPathDSI( "/usr/bin/dsi" );
QString DSSessionSettings::defaultPathDragengine( "/usr/share/dragengine/modules/scripting/dragonscript" );

DSSessionSettings DSSessionSettings::self;



void DSSessionSettings::update(){
	KConfigGroup cgroup( ICore::self()->activeSession()->config()->group( "dragonscriptsupport" ) );
	
	pPathDSI = cgroup.readEntry( "pathDSI", "" );
	if( pPathDSI.isEmpty() ){
		pPathDSI = defaultPathDSI;
	}
	
	pPathDragengine = cgroup.readEntry( "pathDragengine", "" );
	if( pPathDragengine.isEmpty() ){
		pPathDragengine = defaultPathDragengine;
	}
	
	pPathDragengineUse = cgroup.readEntry( "pathDragengineUse", "" );
	
	detectDragengineUse();
}



DSSessionSettings::DSSessionSettings(){
}

void DSSessionSettings::detectDragengineUse(){
	qDebug() << "DSSessionSettings.detectDragengineUse" << "pathDragengine" << pPathDragengine;
	
	QDirIterator iter( pathDragengine(), QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot );
	QRegularExpression pattern( "[0-9]+\\.[0-9]+" );
	const QString oldPath( pPathDragengineUse );
	QVersionNumber bestVersion;
	
	pPathDragengineUse.clear();
	
	while( iter.hasNext() ){
		iter.next();
		if( pattern.match( iter.fileName() ).hasMatch() ){
			qDebug() << "DSSessionSettings.detectDragengineUse: found version" << iter.fileName();
			const QVersionNumber version( QVersionNumber::fromString( iter.fileName() ) );
			if( version > bestVersion ){
				bestVersion = version;
				pPathDragengineUse = iter.filePath();
			}
		}
	}
	
	qDebug() << "DSSessionSettings.detectDragengineUse: using path" << pPathDragengineUse;
	
	if( pPathDragengineUse != oldPath ){
		ICore::self()->activeSession()->config()->group( "dragonscriptsupport" )
			.writeEntry( "pathDragengineUse", pathDragengineUse() );
		ImportPackageDragengine::self()->reparse();
	}
}

}
