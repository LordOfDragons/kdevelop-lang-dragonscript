#include <QMutexLocker>
#include <QDebug>

#include "ImportPackage.h"
#include "ImportPackages.h"


namespace DragonScript {

QVector<ImportPackage::Ref> ImportPackages::packages(){
	QMutexLocker lock( &pMutex );
	QVector<ImportPackage::Ref> list;
	foreach( const ImportPackage::Ref &each, pPackages ){
		list << each;
	}
	return list;
}

ImportPackage::Ref ImportPackages::packageNamed( const QString &name ){
	QMutexLocker lock( &pMutex );
	QMap<QString, ImportPackage::Ref>::iterator iter( pPackages.find( name ) );
	if( iter == pPackages.end() ){
		return {};
	}
	return *iter;
}

ImportPackage::Ref ImportPackages::packageContaining( const IndexedString &file ){
	QMutexLocker lock( &pMutex );
	foreach( const ImportPackage::Ref &each, pPackages ){
		if( each->files().contains( file ) ){
			return each;
		}
	}
	return nullptr;
}

void ImportPackages::addPackage( const ImportPackage::Ref &package ){
	qDebug() << "KDevDScript ImportPackages.addPackage:" << package->name();
	QMutexLocker lock( &pMutex );
	pPackages[ package->name() ] = package;
}

void ImportPackages::dropContextsNamed( const QString &name ){
	QMutexLocker lock( &pMutex );
	QMap<QString, ImportPackage::Ref>::iterator iter( pPackages.find( name ) );
	if( iter != pPackages.end() ){
		(*iter)->dropContexts();
	}
}

void ImportPackages::dropAllContexts(){
	QMutexLocker lock( &pMutex );
	foreach( const ImportPackage::Ref &each, pPackages ){
		each->dropContexts();
	}
}

}
