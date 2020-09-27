#ifndef IMPORTPACKAGELANGUAGE_H
#define IMPORTPACKAGELANGUAGE_H

#include "ImportPackage.h"


namespace DragonScript {

/**
 * Import package for Language files.
 */
class ImportPackageLanguage : public ImportPackage{
public:
	/**
	 * Create package. Starts importing as soon as required.
	 */
	ImportPackageLanguage();
	
	
	
	/**
	 * Package name.
	 */
	static const QString packageName;
	
	/**
	 * Get package from global import package list. Adds package if not present yet.
	 */
	static ImportPackage::Ref self();
};

}

#endif
