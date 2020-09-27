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

class KDEVDSDUCHAIN_EXPORT Helpers {
private:
	static KDevelop::AbstractType::Ptr pTypeVoid;
	static KDevelop::AbstractType::Ptr pTypeNull;
	static KDevelop::AbstractType::Ptr pTypeInvalid;
	
	static KDevelop::AbstractType::Ptr pTypeByte;
	static KDevelop::AbstractType::Ptr pTypeBool;
	static KDevelop::AbstractType::Ptr pTypeInt;
	static KDevelop::AbstractType::Ptr pTypeFloat;
	static KDevelop::AbstractType::Ptr pTypeString;
	static KDevelop::AbstractType::Ptr pTypeObject;
	static KDevelop::AbstractType::Ptr pTypeBlock;
	static KDevelop::AbstractType::Ptr pTypeEnumeration;
	
	static DeclarationPointer pTypeDeclByte;
	static DeclarationPointer pTypeDeclBool;
	static DeclarationPointer pTypeDeclInt;
	static DeclarationPointer pTypeDeclFloat;
	static DeclarationPointer pTypeDeclString;
	static DeclarationPointer pTypeDeclObject;
	static DeclarationPointer pTypeDeclBlock;
	static DeclarationPointer pTypeDeclEnumeration;
	
	static IndexedString documentationFileObject;
	static IndexedString documentationFileEnumeration;
	
	
	
public:
	enum ContextSearchFlags {
		NoFlags,
		PublicOnly
	};
	
	/**
	 * Void type.
	 * \note DUChainReadLocker required.
	 */
	static KDevelop::AbstractType::Ptr getTypeVoid();
	
	/**
	 * Null type.
	 * \note DUChainReadLocker required.
	 */
	static KDevelop::AbstractType::Ptr getTypeNull();
	
	/**
	 * Invalid type.
	 * \note DUChainReadLocker required.
	 */
	static KDevelop::AbstractType::Ptr getTypeInvalid();
	
	/**
	 * Byte type.
	 * \note DUChainReadLocker required.
	 */
	static KDevelop::AbstractType::Ptr getTypeByte();
	
	/**
	 * Bool type.
	 * \note DUChainReadLocker required.
	 */
	static KDevelop::AbstractType::Ptr getTypeBool();
	
	/**
	 * Int type.
	 * \note DUChainReadLocker required.
	 */
	static KDevelop::AbstractType::Ptr getTypeInt();
	
	/**
	 * Float type.
	 * \note DUChainReadLocker required.
	 */
	static KDevelop::AbstractType::Ptr getTypeFloat();
	
	/**
	 * String type.
	 * \note DUChainReadLocker required.
	 */
	static KDevelop::AbstractType::Ptr getTypeString();
	
	/**
	 * Object type.
	 * \note DUChainReadLocker required.
	 */
	static KDevelop::AbstractType::Ptr getTypeObject();
	
	/**
	 * Block type.
	 * \note DUChainReadLocker required.
	 */
	static KDevelop::AbstractType::Ptr getTypeBlock();
	
	/**
	 * Enumeration type.
	 * \note DUChainReadLocker required.
	 */
	static KDevelop::AbstractType::Ptr getTypeEnumeration();
	
	/**
	 * Byte type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeByte( DeclarationPointer &declaration, KDevelop::AbstractType::Ptr &type );
	
	/**
	 * Bool type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeBool( DeclarationPointer &declaration, KDevelop::AbstractType::Ptr &type );
	
	/**
	 * Int type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeInt( DeclarationPointer &declaration, KDevelop::AbstractType::Ptr &type );
	
	/**
	 * Float type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeFloat( DeclarationPointer &declaration, KDevelop::AbstractType::Ptr &type );
	
	/**
	 * String type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeString( DeclarationPointer &declaration, KDevelop::AbstractType::Ptr &type );
	
	/**
	 * Object type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeObject( DeclarationPointer &declaration, KDevelop::AbstractType::Ptr &type );
	
	/**
	 * Block type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeBlock( DeclarationPointer &declaration, KDevelop::AbstractType::Ptr &type );
	
	/**
	 * Enumeration type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeEnumeration( DeclarationPointer &declaration, KDevelop::AbstractType::Ptr &type );
	
	
	
	/** Internal class context for type or null-context if not found. */
	static DUChainPointer<const DUContext> contextForType( const TopDUContext *top,
		const KDevelop::AbstractType::Ptr &type );
	
	/** This class declaration for context or nullptr if not found. */
	static ClassDeclaration *thisClassDeclFor( DUChainPointer<const DUContext> context );
	
	/** Super class declaration for context or nullptr if not found. */
	static ClassDeclaration *superClassDeclFor( DUChainPointer<const DUContext> context );
	
	/** Class declaration for context or nullptr if not found. */
	static ClassDeclaration *classDeclFor( DUChainPointer<const DUContext> context );
	
	/** Class declaration for type or nullptr if not found. */
	static ClassDeclaration *classDeclFor( const TopDUContext *top,
		const KDevelop::AbstractType::Ptr &type );
	
	/** Super class of class declaration or nullptr if not set. */
	static ClassDeclaration *superClassOf( const TopDUContext *top,
		const ClassDeclaration *declaration );
	
	/** Implementation classes of class declaration. */
	static QVector<ClassDeclaration*> implementsOf( const TopDUContext *top,
		const ClassDeclaration *declaration );
	
	/** All base classes of class declaration. */
	static QVector<ClassDeclaration*> baseClassesOf( const TopDUContext *top,
		const ClassDeclaration *declaration );
	
	/** Two type points are the same. */
	static bool equals( const TopDUContext *top, const KDevelop::AbstractType::Ptr &type1,
		const KDevelop::AbstractType::Ptr &type2 );
	
	/** Type is castable to another. */
	static bool castable( const TopDUContext *top, const KDevelop::AbstractType::Ptr &type,
		const KDevelop::AbstractType::Ptr &targetType );
	
	/** Functions have the same signature. */
	static bool sameSignature( const TopDUContext *top, const KDevelop::FunctionType::Ptr &func1,
		const KDevelop::FunctionType::Ptr &func2 );
	
	/** Functions have the same signature but potentially different return type. */
	static bool sameSignatureAnyReturnType( const TopDUContext *top,
		const KDevelop::FunctionType::Ptr &func1, const KDevelop::FunctionType::Ptr &func2 );
	
	/** Function overrides another. */
	static bool overrides( const TopDUContext *top, const ClassFunctionDeclaration *func1,
		const ClassFunctionDeclaration *func2 );
	
	/** Get documentation file for Object class. */
	static IndexedString getDocumentationFileObject();
	
	/** Get documentation file for Enumeration class. */
	static IndexedString getDocumentationFileEnumeration();
	
	/**
	 * Get internal type declaration if loaded.
	 * \note DUChainReadLocker required.
	 */
	static Declaration *getInternalTypeDeclaration( const QString &name );
	
	static KDevelop::AbstractType::Ptr resolveAliasType( const KDevelop::AbstractType::Ptr eventualAlias );
	
// 	static IndexedDeclaration declarationUnderCursor( bool allowUse = true );
	
	/**
	 * Find all internal contexts for this class and its base classes recursively
	 *
	 * \param classType Type object for the class to search contexts
	 * \param context TopContext for finding the declarations for types
	 * \return list of contexts which were found
	 **/
	static QVector<DUContext*> internalContextsForClass(
		const KDevelop::StructureType::Ptr classType, const TopDUContext* context,
		ContextSearchFlags flags = NoFlags, int depth = 0 );
	
	/**
	 * Resolve the given declaration if it is an alias declaration.
	 *
	 * \param decl the declaration to resolve
	 * \return :Declaration* decl if not an alias declaration, decl->aliasedDeclaration().data otherwise
	 * DUChain must be read locked
	 **/
	static Declaration *resolveAliasDeclaration( Declaration *decl );
	
	/**
	 * Find first matching declaration.
	 * \note DUChainReadLocker required.
	 **/
	static Declaration *declarationForName( const QString& name,
		const CursorInRevision& location, DUChainPointer<const DUContext> context );
	
	/**
	 * Find first matching declaration.
	 * \note DUChainReadLocker required.
	 **/
	static Declaration *declarationForName( const QString& name,
		const CursorInRevision& location, DUChainPointer<const DUContext> context,
		const QVector<DUChainPointer<const DUContext>> &pinned );
	
	/**
	 * Find first matching declaration in base classes only.
	 * \note DUChainReadLocker required.
	 **/
	static Declaration *declarationForNameInBase( const QString& name,
		DUChainPointer<const DUContext> context );
	
	/**
	 * Find all matching declarations.
	 * \note DUChainReadLocker required.
	 **/
	static QVector<Declaration*> declarationsForName( const QString& name,
		const CursorInRevision& location, DUChainPointer<const DUContext> context );
	
	/**
	 * Find all matching declarations.
	 * \note DUChainReadLocker required.
	 **/
	static QVector<Declaration*> declarationsForName( const QString& name,
		const CursorInRevision& location, DUChainPointer<const DUContext> context,
		const QVector<DUChainPointer<const DUContext>> &pinned );
	
	/**
	 * Find all matching declarations in base classes only.
	 * \note DUChainReadLocker required.
	 **/
	static QVector<Declaration*> declarationsForNameInBase( const QString& name,
		DUChainPointer<const DUContext> context );
	
	/**
	 * Find all constructor declarations in class.
	 * \note DUChainReadLocker required.
	 **/
	static QVector<Declaration*> constructorsInClass( DUChainPointer<const DUContext> context );
	
	/**
	 * Find best matching function.
	 * 
	 * Best matching means all parameters match the signature without casting.
	 * 
	 * \note DUChainReadLocker required.
	 * 
	 * \param[in] top Top context required for matching types.
	 * \param[in] signature Signature to match.
	 * \param[in] declarations Declarations to match against.
	 */
	static ClassFunctionDeclaration *bestMatchingFunction( const TopDUContext *top,
		const QVector<KDevelop::AbstractType::Ptr> &signature,
		const QVector<Declaration*> &declarations );
	
	/**
	 * Find suitable functions requiring auto-casting.
	 * 
	 * Functions are suitable if one or more of the signature arguments have to be
	 * auto-castable to the function arguments.
	 * 
	 * \note DUChainReadLocker required.
	 * 
	 * \param[in] top Top context required for matching types.
	 * \param[in] signature Signature to match.
	 * \param[in] declarations Declarations to match against.
	 */
	static QVector<ClassFunctionDeclaration*> autoCastableFunctions( const TopDUContext *top,
		const QVector<KDevelop::AbstractType::Ptr> &signature,
		const QVector<Declaration*> &declarations );
};

}

#endif
