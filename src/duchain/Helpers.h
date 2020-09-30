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
	static AbstractType::Ptr pTypeVoid;
	static AbstractType::Ptr pTypeNull;
	static AbstractType::Ptr pTypeInvalid;
	
	static AbstractType::Ptr pTypeByte;
	static AbstractType::Ptr pTypeBool;
	static AbstractType::Ptr pTypeInt;
	static AbstractType::Ptr pTypeFloat;
	static AbstractType::Ptr pTypeString;
	static AbstractType::Ptr pTypeObject;
	static AbstractType::Ptr pTypeBlock;
	static AbstractType::Ptr pTypeEnumeration;
	
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
	
	/** Void type. */
	static AbstractType::Ptr getTypeVoid();
	
	/** Null type. */
	static AbstractType::Ptr getTypeNull();
	
	/** Invalid type. */
	static AbstractType::Ptr getTypeInvalid();
	
	/**
	 * Byte type.
	 * \note DUChainReadLocker required.
	 */
	static AbstractType::Ptr getTypeByte();
	
	/**
	 * Bool type.
	 * \note DUChainReadLocker required.
	 */
	static AbstractType::Ptr getTypeBool();
	
	/**
	 * Int type.
	 * \note DUChainReadLocker required.
	 */
	static AbstractType::Ptr getTypeInt();
	
	/**
	 * Float type.
	 * \note DUChainReadLocker required.
	 */
	static AbstractType::Ptr getTypeFloat();
	
	/**
	 * String type.
	 * \note DUChainReadLocker required.
	 */
	static AbstractType::Ptr getTypeString();
	
	/**
	 * Object type.
	 * \note DUChainReadLocker required.
	 */
	static AbstractType::Ptr getTypeObject();
	
	/**
	 * Block type.
	 * \note DUChainReadLocker required.
	 */
	static AbstractType::Ptr getTypeBlock();
	
	/**
	 * Enumeration type.
	 * \note DUChainReadLocker required.
	 */
	static AbstractType::Ptr getTypeEnumeration();
	
	/**
	 * Byte type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeByte( DeclarationPointer &declaration, AbstractType::Ptr &type );
	
	/**
	 * Bool type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeBool( DeclarationPointer &declaration, AbstractType::Ptr &type );
	
	/**
	 * Int type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeInt( DeclarationPointer &declaration, AbstractType::Ptr &type );
	
	/**
	 * Float type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeFloat( DeclarationPointer &declaration, AbstractType::Ptr &type );
	
	/**
	 * String type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeString( DeclarationPointer &declaration, AbstractType::Ptr &type );
	
	/**
	 * Object type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeObject( DeclarationPointer &declaration, AbstractType::Ptr &type );
	
	/**
	 * Block type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeBlock( DeclarationPointer &declaration, AbstractType::Ptr &type );
	
	/**
	 * Enumeration type.
	 * \note DUChainReadLocker required.
	 */
	static void getTypeEnumeration( DeclarationPointer &declaration, AbstractType::Ptr &type );
	
	
	
	/**
	 * Internal class context for type or null-context if not found.
	 * \note DUChainReadLocker required.
	 */
	static DUChainPointer<const DUContext> contextForType( const TopDUContext *top,
		const AbstractType::Ptr &type );
	
	/**
	 * This class declaration for context or nullptr if not found.
	 * \note DUChainReadLocker required.
	 */
	static ClassDeclaration *thisClassDeclFor( DUChainPointer<const DUContext> context );
	
	/**
	 * Super class declaration for context or nullptr if not found.
	 * \note DUChainReadLocker required.
	 */
	static ClassDeclaration *superClassDeclFor( DUChainPointer<const DUContext> context );
	
	/**
	 * Class declaration for context or nullptr if not found.
	 * \note DUChainReadLocker required.
	 */
	static ClassDeclaration *classDeclFor( DUChainPointer<const DUContext> context );
	
	/**
	 * Class declaration for type or nullptr if not found.
	 * \note DUChainReadLocker required.
	 */
	static ClassDeclaration *classDeclFor( const TopDUContext *top, const AbstractType::Ptr &type );
	
	/**
	 * Super class of class declaration or nullptr if not set.
	 * \note DUChainReadLocker required.
	 */
	static ClassDeclaration *superClassOf( const TopDUContext *top,
		const ClassDeclaration *declaration );
	
	/**
	 * Implementation classes of class declaration.
	 * \note DUChainReadLocker required.
	 */
	static QVector<ClassDeclaration*> implementsOf( const TopDUContext *top,
		const ClassDeclaration *declaration );
	
	/**
	 * All base classes of class declaration.
	 * \note DUChainReadLocker required.
	 */
	static QVector<ClassDeclaration*> baseClassesOf( const TopDUContext *top,
		const ClassDeclaration *declaration );
	
	/**
	 * Two type points are the same.
	 * \note DUChainReadLocker required.
	 */
	static bool equals( const TopDUContext *top, const AbstractType::Ptr &type1,
		const AbstractType::Ptr &type2 );
	
	/**
	 * Type is castable to another.
	 * \note DUChainReadLocker required.
	 */
	static bool castable( const TopDUContext *top, const AbstractType::Ptr &type,
		const AbstractType::Ptr &targetType );
	
	/**
	 * Functions have the same signature.
	 * \note DUChainReadLocker required.
	 */
	static bool sameSignature( const TopDUContext *top, const FunctionType::Ptr &func1,
		const FunctionType::Ptr &func2 );
	
	/**
	 * Functions have the same signature but potentially different return type.
	 * \note DUChainReadLocker required.
	 */
	static bool sameSignatureAnyReturnType( const TopDUContext *top,
		const FunctionType::Ptr &func1, const FunctionType::Ptr &func2 );
	
	/**
	 * Function overrides another.
	 * \note DUChainReadLocker required.
	 */
	static bool overrides( const TopDUContext *top, const ClassFunctionDeclaration *func1,
		const ClassFunctionDeclaration *func2 );
	
	/** Get documentation file for Object class. */
	static IndexedString getDocumentationFileObject();
	
	/**
	 * Get internal type declaration if loaded.
	 * \note DUChainReadLocker required.
	 */
	static Declaration *getInternalTypeDeclaration( const QString &name );
	
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
		const QVector<AbstractType::Ptr> &signature,
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
		const QVector<AbstractType::Ptr> &signature,
		const QVector<Declaration*> &declarations );
};

}

#endif
