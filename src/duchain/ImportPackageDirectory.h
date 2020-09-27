#ifndef IMPORTPACKAGEDIRECTORY_H
#define IMPORTPACKAGEDIRECTORY_H

#include "ImportPackage.h"


namespace DragonScript {

/**
 * Import package for directory containing files.
 */
class ImportPackageDirectory : public ImportPackage{
public:
	/**
	 * Create package. Starts importing as soon as required.
	 */
	ImportPackageDirectory( const QString &name, const QString &directory );
};

}

#endif
