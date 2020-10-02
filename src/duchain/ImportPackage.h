#ifndef IMPORTPACKAGE_H
#define IMPORTPACKAGE_H

#include <QString>
#include <QSet>
#include <QSharedPointer>

#include <language/duchain/topducontext.h>
#include <serialization/indexedstring.h>


using namespace KDevelop;

namespace DragonScript {

/**
 * Import package tracker.
 */
class ImportPackage{
public:
	/** Typedef shared pointer. */
	typedef QSharedPointer<ImportPackage> Ref;
	
	/** State. */
	struct State{
		/** Package is ready. */
		bool ready;
		
		/** Contexts required to be added as imports. */
		QSet<TopDUContext*> importContexts;
		
		/** If not ready the priority to use for reparsing. */
		int reparsePriority;
		
		/** File to wait for. */
		QSet<IndexedString> waitForFiles;
	};
	
	
	
	/**
	 * Create package tracker. Starts importing as soon as required.
	 */
	ImportPackage( const QString &name, const QSet<IndexedString> &files );
	
	/** Clean up import package. */
	virtual ~ImportPackage();
	
	
	
	/**
	 * Package name.
	 */
	inline const QString &name() const{ return pName; }
	
	/**
	 * Files included in the package.
	 */
	inline const QSet<IndexedString> &files() const{ return pFiles; }
	
	
	
	/**
	 * List of contexts. If package is not fully parsed yet returns empty list. In this case
	 * cancel the parsing of the document and schedule it again with a low priority.
	 * 
	 * \note DUChainReadLocker required.
	 */
	void contexts( State &state );
	
	/**
	 * Packages this package depends on.
	 */
	inline QSet<Ref> dependsOn(){ return pDependsOn; }
	inline const QSet<Ref> &dependsOn() const{ return pDependsOn; }
	
	/**
	 * Recusrive dependency depth.
	 */
	int dependencyDepth() const;
	
	
	/**
	 * Reparse files.
	 */
	void reparse();
	
	
	
protected:
	ImportPackage();
	
	
	
	QString pName;
	QSet<IndexedString> pFiles;
	
	QSet<Ref> pDependsOn;
	
	bool pDebug;
};

}

#endif
