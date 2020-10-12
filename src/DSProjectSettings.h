#ifndef DSPROJECTSETTINGS_H
#define DSPROJECTSETTINGS_H

#include <QString>
#include <QStringList>

#include <interfaces/iproject.h>


using namespace KDevelop;

namespace DragonScript{

class DSProjectSettings
{
public:
	/** Require dragengine dragonscript package. */
	inline bool requireDragenginePackage() const{ return pRequireDragenginePackage; }
	
	/** Set require dragengine dragonscript package. */
	void setRequireDragenginePackage( bool requires );
	
	/** List of package directories to include. */
	inline const QStringList &pathInclude() const{ return pPathInclude; }
	
	/** List of package directories to include. */
	void setPathInclude( const QStringList &list );
	
	
	
public:
	DSProjectSettings();
	
	/** Load configuration. */
	void load( const IProject &project );
	
	/** Save configuration. */
	void save( const IProject &project );
	
	
	
private:
	/** Require dragengine dragonscript package. */
	bool pRequireDragenginePackage;
	
	/** List of package directories to include. */
	QStringList pPathInclude;
	
};

}
#endif
