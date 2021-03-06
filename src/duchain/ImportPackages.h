#ifndef IMPORTPACKAGES_H
#define IMPORTPACKAGES_H

#include <QHash>
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
	QHash<QString, ImportPackage::Ref> pPackages;
	
	
	
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
	 * Reparse files of named import package if present.
	 */
	void reparseNamed( const QString &name );
	
	/**
	 * Reparse all import packages.
	 */
	void reparseAll();
};

}

#endif
