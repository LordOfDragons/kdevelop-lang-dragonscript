#ifndef TYPEFINDER_H
#define TYPEFINDER_H

#include <language/duchain/declaration.h>
#include <language/duchain/classdeclaration.h>
#include <language/duchain/topducontext.h>
#include <language/duchain/types/abstracttype.h>

#include <QSet>
#include <QHash>

#include <functional>


using namespace KDevelop;

namespace DragonScript{

/**
 * Finds ClassDeclaration for AbstractType.
 * 
 * DUChain does not store a link from AbstractType to ClassDeclaration (or Declaration).
 * This makes it impossible to find a ClassDeclaration for an AbstractType without knowing
 * the TopDUContext the ClassDeclaration is located in. In DragonScript there exists no
 * acyclic dependency graphic so the DUChain provide mechanism can not be used.
 * 
 * This class provides this functionality. Since searching over and over again for these
 * links is very expensive this class builds a TypeMap as it encounters new types. This
 * type map is only valid for the duration of the parser job.
 * 
 * Resolving types using the type map has also the advantage or required little amount of
 * DUChainReadLocker locking, namely only the first time a type is encounted and needs to
 * be looked up.
 * 
 * \note This class locks DUChainReadLocker internally.
 */
class TypeFinder{
public:
	using ClassDeclarationPointer = DUChainPointer<ClassDeclaration>;
	
	// WARNING do NOT use QMap in your live. Never Ever DO IT!!
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
	typedef QHash<AbstractType::Ptr, ClassDeclarationPointer> TypeMap;
	typedef QHash<IndexedIdentifier, ClassDeclarationPointer> IdentifierMap;
	typedef QSet<ReferencedTopDUContext> TypeTopContextList;
	
	
	
private:
	TypeTopContextList pSearchContexts;
	
	TypeMap pTypeMap;
	IdentifierMap pIdentifierMap;
	
	ClassDeclarationPointer pDeclByte;
	ClassDeclarationPointer pDeclBool;
	ClassDeclarationPointer pDeclInt;
	ClassDeclarationPointer pDeclFloat;
	ClassDeclarationPointer pDeclString;
	ClassDeclarationPointer pDeclObject;
	ClassDeclarationPointer pDeclBlock;
	ClassDeclarationPointer pDeclEnumeration;
	
	QSet<TopDUContextPointer> pLangClasses;
	
	
	
public:
	TypeFinder() = default;
	~TypeFinder();
	
	
	/** Search contexts. */
	inline const TypeTopContextList &searchContexts() const{ return pSearchContexts; }
	
	/**
	 * Set search contexts.
	 * \note Requires DUChainWriteLocker.
	 */
	inline TypeTopContextList &searchContexts(){ return pSearchContexts; }
	
	
	
	/**
	 * Integral types.
	 * \note Internally locks DUChainReadLocker.
	 */
	ClassDeclaration *typeByte();
	ClassDeclaration *typeBool();
	ClassDeclaration *typeInt();
	ClassDeclaration *typeFloat();
	ClassDeclaration *typeString();
	ClassDeclaration *typeObject();
	ClassDeclaration *typeBlock();
	ClassDeclaration *typeEnumeration();
	
	/**
	 * Declaration for abstract type.
	 * \note Internally locks DUChainReadLocker.
	 */
	ClassDeclaration *declarationFor( const AbstractType::Ptr &type );
	
	/**
	 * Context for abstract type.
	 * \note Internally locks DUChainReadLocker.
	 */
	DUContext *contextFor( const AbstractType::Ptr &type );
	
	/**
	 * Declaration for global type.
	 * \note Internally locks DUChainReadLocker.
	 */
	ClassDeclaration *declarationFor( const IndexedIdentifier &identifier );
	
	
	
protected:
	ClassDeclaration *declarationForIntegral( const IndexedIdentifier &identifier );
};

}

#endif
