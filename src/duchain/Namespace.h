#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <language/duchain/declaration.h>
#include <language/duchain/classdeclaration.h>
#include <language/duchain/ducontext.h>

#include <QSet>
#include <QHash>
#include <QSharedPointer>


using namespace KDevelop;

namespace DragonScript{

class TypeFinder;

/**
 * Namespace.
 * 
 * Stores encountered namespaces and their contained classes in a lazy way. Upon encountering
 * an unknown namespace the namespace is looked up in the type finder search contexts.
 * If found the namespace is stored. If namespaces or classes inside a found namespace are
 * searched the namespaces respective classes are looked up when needed. This way only the
 * encountered part of the namespace space is looked up while storing the result for upcoming
 * queries.
 * 
 * \note Locking DUChainReadLocker is required.
 * 
 * \warning This class internally uses QSharedPointer to hold on to children. Do not use
 *          Ref type on your own except for the root namespace. Not doing so can cause
 *          the same pointer to be stored in multiple QSharedPointer data blocks causing
 *          the object incorrectly to be destroyed if pointers go out of scope.
 *          in particular this means never store a regular Namespace* into Ref. If you
 *          need to store a Ref for some reason only assign Ref to Ref.
 * 
 * \note Namespaces are never deleted. You only need to store Ref for the root namespace.
 */
class Namespace{
public:
	typedef DUChainPointer<ClassDeclaration> ClassDeclarationPointer;
	
	// WARNING do NOT use QHash in your live. Never Ever DO IT!!
	//         this class sorts keys and uses operator<() instead of operator==().
	//         TypePtr subclasses QExplicitlySharedDataPointer which does have operator==()
	//         but not operator<(). compile will find "operator bool" and auto-cast.
	//         you are in for the hell of your life-time then because all non-null
	//         pointers are then equal. see:
	//         https://forum.qt.io/topic/63847/using-qexplicitlysharedpointer-as-a-key-to-qmap-causes-reference-issues/6
	//         
	//         QHash on the other hand does not sort and thus is not affected by this
	//         problem. using QHash is faster anyways so that's it
	//         
	typedef QSharedPointer<Namespace> Ref;
	typedef QHash<IndexedIdentifier, Ref> TypeNamespaceMap;
	typedef QHash<IndexedIdentifier, ClassDeclarationPointer> TypeClassMap;
	typedef QSet<DUContextPointer> TypeContextList;
	
	
	
private:
	Namespace *pParent;
	IndexedIdentifier pIdentifier;
	IndexedQualifiedIdentifier pQualifiedIdentifier;
	TypeNamespaceMap pNamespaces;
	TypeClassMap pClasses;
	TypeContextList pContexts;
	bool pDirtyContent;
	ClassDeclarationPointer pDeclaration;
	
	
	
public:
	Namespace( TypeFinder &typeFinder );
	Namespace( Namespace &parent, const IndexedIdentifier &identifier );
	
	inline Namespace *parent() const{ return pParent; }
	inline const IndexedIdentifier &identifier() const{ return pIdentifier; }
	inline const IndexedQualifiedIdentifier &qualifiedIdentifier() const{ return pQualifiedIdentifier; }
	inline const TypeNamespaceMap &namespaces() const{ return pNamespaces; }
	inline const TypeClassMap &classes() const{ return pClasses; }
	
	/** Namespace matching identifier or nullptr. */
	Namespace *getNamespace( const IndexedIdentifier &iid );
	
	/** Namespace matching qualified identifier or nullptr. */
	Namespace *getNamespace( const QualifiedIdentifier &qid );
	
	/** Namespace matching identifier adding it if absent. */
	Namespace &getOrAddNamespace( const IndexedIdentifier &iid );
	Namespace &getOrAddNamespace( const QString &name );
	
	/** Namespace matching qualified identifier adding it and parents if absent. */
	Namespace *getOrAddNamespace( const QualifiedIdentifier &qid );
	
	/** Class declaration matching identifier or nullptr. */
	ClassDeclaration *getClass( const IndexedIdentifier &iid );
	
	/** First namespace declaration or nullptr. */
	inline ClassDeclaration *declaration() const{ return pDeclaration.data(); }
	
	
	
private:
	void findContent();
};

}

#endif
