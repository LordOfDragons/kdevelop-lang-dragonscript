#include <QDirIterator>
#include <QDebug>

#include "ImportPackages.h"
#include "ImportPackageDirectory.h"
#include "../DSSessionSettings.h"
#include "../DSLanguageSupport.h"


namespace DragonScript {

ImportPackageDirectory::ImportPackageDirectory( const QString &name, const QString &directory ){
	pName = name;
	
	QDirIterator iter( directory, QStringList() << "*.ds", QDir::Files, QDirIterator::Subdirectories );
	while( iter.hasNext() ){
		pFiles << IndexedString( iter.next() );
	}
	
	qDebug() << "KDevDScript ImportPackageDirectory: directory" << directory;
	foreach( const IndexedString &each, pFiles ){
		qDebug() << "KDevDScript ImportPackageDirectory: add file" << each;
	}
}

}
