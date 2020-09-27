#ifndef DSSESSIONSETTINGS_H
#define DSSESSIONSETTINGS_H

#include <QString>
#include <QStringList>

namespace DragonScript{

class DSSessionSettings
{
public:
	/** Default DSI path. */
	static QString defaultPathDSI;
	
	/** Default Drag[en]gine DragonScript Module path. */
	static QString defaultPathDragengine;
	
	/** Global session settings. */
	static DSSessionSettings self;
	
	
	
	/** Path to DSI. */
	inline const QString &pathDSI() const{ return pPathDSI; }
	
	/**
	 * Path to Drag[en]gine DragonScript Module. This is the directory containing the
	 * various dragonscript module versions "major.minor".
	 */
	inline const QString &pathDragengine() const{ return pPathDragengine; }
	
	/**
	 * Path to Drag[en]gine DragonScript Module to use. This is the directory directly
	 * underneath pathDragengine() with the highest version number.
	 */
	inline const QString &pathDragengineUse() const{ return pPathDragengineUse; }
	
	/** Update after changing settings. */
	void update();
	
	
	
private:
	DSSessionSettings();
	
	void detectDragengineUse();
	
	QString pPathDSI;
	QString pPathDragengine;
	QString pPathDragengineUse;
};

}
#endif
