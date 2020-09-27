#ifndef IMPORTPACKAGE_H
#define IMPORTPACKAGE_H

#include <QString>
#include <QVector>
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
	
	
	
	/**
	 * Create package tracker. Starts importing as soon as required.
	 */
	ImportPackage( const QString &name, const QVector<IndexedString> &files );
	
	/** Clean up import package. */
	virtual ~ImportPackage();
	
	
	
	/**
	 * Package name.
	 */
	inline const QString &name() const{ return pName; }
	
	/**
	 * Files included in the package.
	 */
	inline const QVector<IndexedString> &files() const{ return pFiles; }
	
	
	
	/**
	 * List of contexts. If package is not fully parsed yet returns empty list. In this case
	 * cancel the parsing of the document and schedule it again with a low priority.
	 * 
	 * \note DUChainReadLocker required.
	 */
	QVector<ReferencedTopDUContext> getContexts();
	
	/**
	 * Add contexts as imports to a top context.
	 * 
	 * \note DUChainWriteLocker required.
	 * 
	 * \retval true Successfully added contexts.
	 * \retval false Package is not fully parse yet. Cancel parsing of document and schedule
	 *               it again with a low priority.
	 */
	bool addImports( TopDUContext *context );
	
	/**
	 * Drop contexts.
	 */
	void dropContexts();
	
	/**
	 * Packages this package depends on.
	 */
	inline QVector<Ref> dependsOn(){ return pDependsOn; }
	inline const QVector<Ref> &dependsOn() const{ return pDependsOn; }
	
	
	
protected:
	ImportPackage();
	
	
	
	QString pName;
	QVector<IndexedString> pFiles;
	QVector<ReferencedTopDUContext> pContexts;
	bool pReady;
	bool pParsing;
	
	QVector<Ref> pDependsOn;
	
	bool pDebug;
};

}

#endif
