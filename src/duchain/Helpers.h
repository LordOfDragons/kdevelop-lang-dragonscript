#ifndef _HELPERS_H
#define _HELPERS_H

#include <interfaces/iproject.h>
#include <language/duchain/classdeclaration.h>
#include <language/duchain/classfunctiondeclaration.h>
#include <language/duchain/declaration.h>
#include <language/duchain/functiondeclaration.h>
#include <language/duchain/topducontext.h>
#include <language/duchain/types/integraltype.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/types/structuretype.h>
#include <language/duchain/types/unsuretype.h>

#include <QList>
#include <QSharedPointer>

#include <functional>

#include "duchainexport.h"
#include "dsp_ast.h"
#include "ImportPackage.h"


using namespace KDevelop;

namespace DragonScript {

class TypeFinder;

class KDEVDSDUCHAIN_EXPORT Helpers {
private:
	static const AbstractType::Ptr pTypeVoid;
	static const AbstractType::Ptr pTypeNull;
	static const AbstractType::Ptr pTypeInvalid;
	
	static const QualifiedIdentifier pTypeByte;
	static const QualifiedIdentifier pTypeBool;
	static const QualifiedIdentifier pTypeInt;
	static const QualifiedIdentifier pTypeFloat;
	static const QualifiedIdentifier pTypeString;
	static const QualifiedIdentifier pTypeObject;
	static const QualifiedIdentifier pTypeBlock;
	static const QualifiedIdentifier pTypeEnumeration;
	
	static const IndexedIdentifier pNameConstructor;
	
	static IndexedString documentationFileObject;
	static IndexedString documentationFileEnumeration;
	
	
	
public:
	enum ContextSearchFlags {
		NoFlags,
		PublicOnly
	};
	
	/** Void type. */
	static const AbstractType::Ptr &getTypeVoid(){ return pTypeVoid; }
	
	/** Null type. */
	static const AbstractType::Ptr &getTypeNull(){ return pTypeNull; }
	
	/** Invalid type. */
	static const AbstractType::Ptr &getTypeInvalid(){ return pTypeInvalid; }
	
	/** Byte type. */
	static const QualifiedIdentifier &getTypeByte(){ return pTypeByte; }
	
	/** Bool type. */
	static const QualifiedIdentifier &getTypeBool(){ return pTypeBool; }
	
	/** Int type. */
	static const QualifiedIdentifier &getTypeInt(){ return pTypeInt; }
	
	/** Float type. */
	static const QualifiedIdentifier &getTypeFloat(){ return pTypeFloat; }
	
	/** String type. */
	static const QualifiedIdentifier &getTypeString(){ return pTypeString; }
	
	/** Object type. */
	static const QualifiedIdentifier &getTypeObject(){ return pTypeObject; }
	
	/** Block type. */
	static const QualifiedIdentifier &getTypeBlock(){ return pTypeBlock; }
	
	/** Enumeration type. */
	static const QualifiedIdentifier &getTypeEnumeration(){ return pTypeEnumeration; }
	
	/** Identifier of constructors. */
	static const IndexedIdentifier &nameConstructor(){ return pNameConstructor; }

	
	
	/**
	 * This class declaration for context or nullptr if not found.
	 * \note DUChainReadLocker required.
	 */
	static ClassDeclaration *thisClassDeclFor( const DUContext &context );
	
	/**
	 * Super class declaration for context or nullptr if not found.
	 * \note DUChainReadLocker required.
	 */
	static ClassDeclaration *superClassDeclFor( const DUContext &context, TypeFinder &typeFinder );
	
	/**
	 * Class declaration for context or nullptr if not found.
	 * \note DUChainReadLocker required.
	 */
	static ClassDeclaration *classDeclFor( const DUContext &context );
	static ClassDeclaration *classDeclFor( const DUContext *context );
	
	/**
	 * Super class of class declaration or nullptr if not set.
	 * \note DUChainReadLocker required.
	 */
	static ClassDeclaration *superClassOf( const ClassDeclaration &declaration, TypeFinder &typeFinder );
	
	/**
	 * Implementation classes of class declaration.
	 * \note DUChainReadLocker required.
	 */
	static QVector<ClassDeclaration*> implementsOf( const ClassDeclaration &declaration, TypeFinder &typeFinder );
	
	/**
	 * All base classes of class declaration.
	 * \note DUChainReadLocker required.
	 */
	static QVector<ClassDeclaration*> baseClassesOf( const ClassDeclaration &declaration, TypeFinder &typeFinder );
	
	/** Two type points are the same. */
	static bool equals( const AbstractType::Ptr &type1, const AbstractType::Ptr &type2 );
	
	/** Type equals internal type. */
	static bool equalsInternal( const AbstractType::Ptr &type, const QualifiedIdentifier &internal );
	
	/**
	 * Type is castable to another.
	 * \note DUChainReadLocker required.
	 */
	static bool castable( const AbstractType::Ptr &type, const AbstractType::Ptr &targetType,
		TypeFinder &typeFinder );
	
	/** Functions have the same signature. */
	static bool sameSignature( const FunctionType::Ptr &func1, const FunctionType::Ptr &func2 );
	
	/** Functions have the same signature but potentially different return type. */
	static bool sameSignatureAnyReturnType( const FunctionType::Ptr &func1,
		const FunctionType::Ptr &func2 );
	
	/**
	 * Function overrides another.
	 * \note DUChainReadLocker required.
	 */
	static bool overrides( const ClassFunctionDeclaration *func1,
		const ClassFunctionDeclaration *func2, TypeFinder &typeFinder );
	
	/** Get documentation file for Object class. */
	static IndexedString getDocumentationFileObject();
	
	/** Get enumeration file for Object class. */
	static IndexedString getDocumentationFileEnumeration();
	
	/**
	 * Find first matching declaration.
	 * \note DUChainReadLocker required.
	 **/
	static Declaration *declarationForName( const IndexedIdentifier &identifier,
		const CursorInRevision& location, const DUContext &context, 
		const QVector<const TopDUContext*> &namespaces, TypeFinder &typeFinder );
	
	/**
	 * Find first matching declaration in base classes only.
	 * \note DUChainReadLocker required.
	 **/
	static Declaration *declarationForNameInBase( const IndexedIdentifier &identifier,
		const DUContext &context, TypeFinder &typeFinder );
	
	/**
	 * Find all matching declarations.
	 * \note DUChainReadLocker required.
	 **/
	static QVector<Declaration*> declarationsForName( const IndexedIdentifier &identifier,
		const CursorInRevision& location, const DUContext &context,
		const QVector<const TopDUContext*> &namespaces, TypeFinder &typeFinder,
		bool onlyFunctions = false );
	
	/**
	 * Find all matching declarations in base classes only.
	 * \note DUChainReadLocker required.
	 **/
	static QVector<Declaration*> declarationsForNameInBase( const IndexedIdentifier &identifier,
		const DUContext &context, TypeFinder &typeFinder, bool onlyFunctions = false );
	
	/**
	 * Find all declarations.
	 * \note DUChainReadLocker required.
	 **/
	static QVector<QPair<Declaration*, int>> allDeclarations( const CursorInRevision& location,
		const DUContext &context, const QVector<const TopDUContext*> &namespaces,
		TypeFinder &typeFinder );
	
	/**
	 * Find all declarations in base classes only.
	 * \note DUChainReadLocker required.
	 **/
	static QVector<QPair<Declaration*, int>> allDeclarationsInBase( const DUContext &context,
		TypeFinder &typeFinder );
	
	/**
	 * Consolidate found declarations removing overridden members.
	 */
	static QVector<QPair<Declaration*, int>> consolidate( const QVector<QPair<Declaration*, int>> &list );
	
	/**
	 * Find all constructor declarations in class.
	 * \note DUChainReadLocker required.
	 **/
	static QVector<Declaration*> constructorsInClass( const DUContext &context );
	
	/**
	 * Find best matching function.
	 * 
	 * Best matching means all parameters match the signature without casting.
	 */
	static ClassFunctionDeclaration *bestMatchingFunction(
		const QVector<AbstractType::Ptr> &signature, const QVector<Declaration*> &declarations );
	
	/**
	 * Find suitable functions requiring auto-casting.
	 * 
	 * Functions are suitable if one or more of the signature arguments have to be
	 * auto-castable to the function arguments.
	 * 
	 * \note DUChainReadLocker required.
	 */
	static QVector<ClassFunctionDeclaration*> autoCastableFunctions(
		const QVector<AbstractType::Ptr> &signature, const QVector<Declaration*> &declarations,
		TypeFinder &typeFinder );
};

}

#endif
