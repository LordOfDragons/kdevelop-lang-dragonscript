#include <QDirIterator>
#include <QDebug>

#include "ImportPackages.h"
#include "ImportPackageDragengine.h"
#include "ImportPackageLanguage.h"
#include "Helpers.h"
#include "../DSSessionSettings.h"
#include "../DSLanguageSupport.h"


namespace DragonScript {

const QString ImportPackageDragengine::packageName( "#internal#dragengine#dragonscript#" );

ImportPackageDragengine::ImportPackageDragengine(){
	pName = packageName;
	
	pDependsOn << ImportPackageLanguage::self();
	
	const QString dir( DSSessionSettings::self.pathDragengineUse() );
	if( ! dir.isEmpty() ){
		qDebug() << "KDevDScript ImportPackageDragengine dir" << dir;
		
		QDirIterator iter( dir + "/scripts", QStringList() << "*.ds", QDir::Files, QDirIterator::Subdirectories );
		while( iter.hasNext() ){
			pFiles << IndexedString( iter.next() );
		}
		
		QDirIterator iter2( dir + "/native", QStringList() << "*.ds", QDir::Files, QDirIterator::Subdirectories );
		while( iter2.hasNext() ){
			pFiles << IndexedString( iter2.next() );
		}
	}
	
	foreach( const IndexedString &each, pFiles ){
		qDebug() << "KDevDScript ImportPackageDragengine: add file" << each;
	}
}

ImportPackage::Ref ImportPackageDragengine::self(){
	ImportPackages &importPackages = DSLanguageSupport::self()->importPackages();
	ImportPackage::Ref package( importPackages.packageNamed( packageName ) );
	if( ! package ){
		package = ImportPackage::Ref( new ImportPackageDragengine );
		importPackages.addPackage( package );
	}
	return package;
}

}
