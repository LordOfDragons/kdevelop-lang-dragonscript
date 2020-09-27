#ifndef IMPORTPACKAGEDRAGENGINE_H
#define IMPORTPACKAGEDRAGENGINE_H

#include "ImportPackage.h"


namespace DragonScript {

/**
 * Import package for Drag[en]gine DragonScript Module.
 */
class ImportPackageDragengine : public ImportPackage{
public:
	/**
	 * Create package. Starts importing as soon as required.
	 */
	ImportPackageDragengine();
	
	
	
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
