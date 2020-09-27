#ifndef IMPORTPACKAGES_H
#define IMPORTPACKAGES_H

#include <QMap>
#include <QMutex>
#include <QString>
#include <QVector>

#include <serialization/indexedstring.h>

#include "ImportPackage.h"


using namespace KDevelop;

namespace DragonScript {

/**
 * Stores import packages.
 */
class ImportPackages{
private:
	QMutex pMutex;
	QMap<QString, ImportPackage::Ref> pPackages;
	
	
	
public:
	/**
	 * List of all packages.
	 */
	QVector<ImportPackage::Ref> packages();
	
	/**
	 * Named import package or nullptr if not found.
	 */
	ImportPackage::Ref packageNamed( const QString &name );
	
	/**
	 * Import package containing file or nullptr if not found.
	 */
	ImportPackage::Ref packageContaining( const IndexedString &file );
	
	/**
	 * Add import package replacing existing one if present.
	 */
	void addPackage( const ImportPackage::Ref &package );
	
	/**
	 * Drop contexts of named import package if present.
	 */
	void dropContextsNamed( const QString &name );
	
	/**
	 * Drop contexts of all import packages.
	 */
	void dropAllContexts();
};

}

#endif
