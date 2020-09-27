#include <QDirIterator>
#include <QStandardPaths>
#include <QDebug>

#include "ImportPackages.h"
#include "ImportPackageLanguage.h"
#include "../DSSessionSettings.h"
#include "../DSLanguageSupport.h"


namespace DragonScript {

const QString ImportPackageLanguage::packageName( "#internal#language#files#" );

ImportPackageLanguage::ImportPackageLanguage(){
	pName = packageName;
	
	const QStringList dirs( QStandardPaths::locateAll( QStandardPaths::GenericDataLocation,
		"kdevdragonscriptsupport/dslangdoc", QStandardPaths::LocateDirectory ) );
	
	foreach( const QString &dir, dirs ){
		qDebug() << "KDevDScript ImportPackageLanguage data dir" << dir;
		QDirIterator iter( dir, QStringList() << "*.ds", QDir::Files, QDirIterator::Subdirectories );
		while( iter.hasNext() ){
			pFiles << IndexedString( iter.next() );
		}
	}
	
	foreach( const IndexedString &each, pFiles ){
		qDebug() << "KDevDScript ImportPackageLanguage: add file" << each;
	}
}

ImportPackage::Ref ImportPackageLanguage::self(){
	ImportPackages &importPackages = DSLanguageSupport::self()->importPackages();
	ImportPackage::Ref package( importPackages.packageNamed( packageName ) );
	if( ! package ){
		package = ImportPackage::Ref( new ImportPackageLanguage );
		importPackages.addPackage( package );
	}
	return package;
}

}
