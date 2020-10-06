#include <QList>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QPair>
#include <QMetaType>
#include <QDirIterator>
#include <QDebug>

#include <language/duchain/aliasdeclaration.h>
#include <language/duchain/classdeclaration.h>
#include <language/duchain/duchain.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/types/containertypes.h>
#include <language/duchain/types/integraltype.h>
#include <language/duchain/types/typeutils.h>
#include <language/duchain/types/unsuretype.h>
#include <language/backgroundparser/backgroundparser.h>
#include <language/duchain/types/functiontype.h>
#include <interfaces/iproject.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/icore.h>
#include <interfaces/ilanguagecontroller.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/ipartcontroller.h>
#include <util/path.h>

#include <shell/partcontroller.h>

#include <KTextEditor/View>
#include <KConfigGroup>

#include "Helpers.h"
#include "ExpressionVisitor.h"
#include "ImportPackageLanguage.h"


using namespace KDevelop;

namespace DragonScript {

IndexedString Helpers::documentationFileObject;
IndexedString Helpers::documentationFileEnumeration;

const AbstractType::Ptr Helpers::pTypeVoid( new IntegralType( IntegralType::TypeVoid ) );
const AbstractType::Ptr Helpers::pTypeNull( new IntegralType( IntegralType::TypeNull ) );
const AbstractType::Ptr Helpers::pTypeInvalid( new IntegralType( IntegralType::TypeMixed ) );

const QualifiedIdentifier Helpers::pTypeByte( "byte" );
const QualifiedIdentifier Helpers::pTypeBool( "bool" );
const QualifiedIdentifier Helpers::pTypeInt( "int" );
const QualifiedIdentifier Helpers::pTypeFloat( "float" );
const QualifiedIdentifier Helpers::pTypeString( "String" );
const QualifiedIdentifier Helpers::pTypeObject( "Object" );
const QualifiedIdentifier Helpers::pTypeBlock( "Block" );
const QualifiedIdentifier Helpers::pTypeEnumeration( "Enumeration" );

const IndexedIdentifier Helpers::pNameConstructor( Identifier( "new" ) );



DUContext *Helpers::contextForType( const TopDUContext &top, const AbstractType::Ptr &type,
const QVector<const TopDUContext*> &reachableContexts ){
	const ClassDeclaration * const classDecl = classDeclFor( top, type, reachableContexts );
	return classDecl ? classDecl->internalContext() : nullptr;
}

ClassDeclaration *Helpers::thisClassDeclFor( const DUContext &context ){
	const DUContext *findContext = &context;
	while( findContext ){
		ClassDeclaration * const decl = classDeclFor( *findContext );
		if( decl ){
			return decl;
		}
		findContext = findContext->parentContext();
	}
	return nullptr;
}

ClassDeclaration *Helpers::superClassDeclFor( const DUContext &context,
const QVector<const TopDUContext*> &reachableContexts ){
	ClassDeclaration * const decl = thisClassDeclFor( context );
	if( ! decl || ! context.parentContext() ){
		return nullptr;
	}
	
	const TopDUContext * const topContext = context.topContext();
	const ClassDeclaration * const classDecl = static_cast<const ClassDeclaration*>( decl );
	if( ! topContext || ! classDecl ){
		return nullptr;
	}
	
	return superClassOf( *topContext, *classDecl, reachableContexts );
}

ClassDeclaration *Helpers::classDeclFor( const DUContext &context ){
	Declaration * const declaration = context.owner();
	if( ! declaration ){
		return nullptr;
	}
	return dynamic_cast<ClassDeclaration*>( declaration );
}

ClassDeclaration *Helpers::classDeclFor( const DUContext *context ){
	return context ? classDeclFor( *context ) : nullptr;
}

ClassDeclaration *Helpers::classDeclFor( const TopDUContext &top, const AbstractType::Ptr &type,
const QVector<const TopDUContext*> &reachableContexts ){
	StructureType::Ptr structType = type.cast<StructureType>();
	if( ! structType ){
		return nullptr;
	}
	
	Declaration *declType = structType->declaration( &top );
	if( ! declType ){
		foreach( const TopDUContext *each, reachableContexts ){
			declType = structType->declaration( each );
			if( declType ){
				break;
			}
		}
	}
	if( ! declType ){
		return nullptr;
	}
	
	return static_cast<ClassDeclaration*>( declType );
}

ClassDeclaration *Helpers::superClassOf( const TopDUContext &top,
const ClassDeclaration &declaration, const QVector<const TopDUContext*> &reachableContexts ){
	if( declaration.baseClassesSize() == 0 ){
		return nullptr;
	}
	
	return classDeclFor( top, declaration.baseClasses()[ 0 ].baseClass.abstractType(), reachableContexts );
}

QVector<ClassDeclaration*> Helpers::implementsOf( const TopDUContext &top, const
ClassDeclaration &declaration, const QVector<const TopDUContext*> &reachableContexts ){
	QVector<ClassDeclaration*> implements;
	uint i;
	
	for( i=1; i<declaration.baseClassesSize(); i++ ){
		ClassDeclaration * const implementDecl = classDeclFor( top,
			declaration.baseClasses()[ i ].baseClass.abstractType(), reachableContexts );
		if( implementDecl ){
			implements.append( implementDecl );
		}
	}
	
	return implements;
}

QVector<ClassDeclaration*> Helpers::baseClassesOf( const TopDUContext &top,
const ClassDeclaration &declaration, const QVector<const TopDUContext*> &reachableContexts ){
	QVector<ClassDeclaration*> baseClasses;
	uint i;
	
	for( i=0; i<declaration.baseClassesSize(); i++ ){
		ClassDeclaration * const baseClassDecl = classDeclFor( top,
			declaration.baseClasses()[ i ].baseClass.abstractType(), reachableContexts );
		if( baseClassDecl ){
			baseClasses.append( baseClassDecl );
		}
	}
	
	return baseClasses;
}

bool Helpers::equals( const AbstractType::Ptr &type1, const AbstractType::Ptr &type2 ){
	// NOTE pTypeInvalid is considered a wildcard to reduce errors shown to meaningful ones
	if( type1 == pTypeInvalid || type2 == pTypeInvalid ){
		return true;
	}
	
	return type1 && type2 && type1->equals( type2.data() );
}

bool Helpers::equalsInternal( const AbstractType::Ptr &type, const QualifiedIdentifier &internal ){
	if( ! type ){
		return false;
	}
	
	const StructureType::Ptr structType( TypePtr<StructureType>::dynamicCast( type ) );
	if( ! structType ){
		return false;
	}
	
	return structType->qualifiedIdentifier() == internal;
}

bool Helpers::castable( const TopDUContext &top, const AbstractType::Ptr &type,
const AbstractType::Ptr &targetType, const QVector<const TopDUContext*> &reachableContexts ){
	// NOTE pTypeInvalid is considered a wildcard to reduce errors shown to meaningful ones
	if( type == pTypeInvalid || targetType == pTypeInvalid ){
		return true;
	}
	
	// NOTE missing types are wildcards too to reduce errors shown to meaningful ones
	if( ! type || ! targetType ){
		return true;
	}
	
	if( type->equals( targetType.data() ) ){
		return true;
	}
	
	// if type is null-type cast is always possible
	if( type->equals( pTypeNull.data() ) ){
		return true;
	}
	
	// test against supper class and interfaces of type1
	ClassDeclaration * const classDecl = classDeclFor( top, type, reachableContexts );
	if( ! classDecl ){
		return false;
	}
	
	ClassDeclaration * const superClassDecl = superClassOf( top, *classDecl, reachableContexts );
	if( superClassDecl ){
		if( castable( top, superClassDecl->abstractType(), targetType, reachableContexts ) ){
			return true;
		}
	}
	
	const QVector<ClassDeclaration*> implements( implementsOf( top, *classDecl, reachableContexts ) );
	foreach( ClassDeclaration* implement, implements ){
		if( castable( top, implement->abstractType(), targetType, reachableContexts ) ){
			return true;
		}
	}
	
	// check for primitive auto-casting
	const StructureType::Ptr structType( TypePtr<StructureType>::dynamicCast( type ) );
	const StructureType::Ptr structTargetType( TypePtr<StructureType>::dynamicCast( targetType ) );
	if( ! structType || ! structTargetType ){
		return false;
	}
	
	if( structType->qualifiedIdentifier() == pTypeByte ){
		return structTargetType->qualifiedIdentifier() == pTypeInt
			|| structTargetType->qualifiedIdentifier() == pTypeFloat;
		
	}else if( structType->qualifiedIdentifier() == pTypeInt ){
		// to byte is possible if constant value
		return structTargetType->qualifiedIdentifier() == pTypeByte
			|| structTargetType->qualifiedIdentifier() == pTypeFloat;
	}
	
	return false;
}

bool Helpers::sameSignature( const FunctionType::Ptr &func1, const FunctionType::Ptr &func2 ){
	return sameSignatureAnyReturnType( func1, func2 )
		&& equals( func1->returnType(), func2->returnType() );
}

bool Helpers::sameSignatureAnyReturnType( const FunctionType::Ptr &func1,
const FunctionType::Ptr &func2 ){
	if( ! func1 || ! func2 ){
		return false;
	}
	
	if( func1->arguments().size() != func2->arguments().size() ){
		return false;
	}
	
	int i;
	for( i=0; i<func1->arguments().size(); i++ ){
		if( ! equals( func1->arguments().at( i ), func2->arguments().at( i ) ) ){
			return false;
		}
	}
	
	return true;
}

bool Helpers::overrides( const TopDUContext &top, const ClassFunctionDeclaration *func1,
const ClassFunctionDeclaration *func2, const QVector<const TopDUContext*> &reachableContexts ){
	return func1 && func1->context() && func1->context()->owner()
		&& func2 && func2->context() && func2->context()->owner()
		&& castable( top, func1->context()->owner()->abstractType(),
			func2->context()->owner()->abstractType(), reachableContexts );
}



IndexedString Helpers::getDocumentationFileObject(){
	if( documentationFileObject.isEmpty() ){
		documentationFileObject = IndexedString(
			QStandardPaths::locate( QStandardPaths::GenericDataLocation,
				QString( "kdevdragonscriptsupport/dslangdoc/Object.ds" ) ) );
	}
	return documentationFileObject;
}

IndexedString Helpers::getDocumentationFileEnumeration(){
	if( documentationFileEnumeration.isEmpty() ){
		documentationFileEnumeration = IndexedString(
			QStandardPaths::locate( QStandardPaths::GenericDataLocation,
				QString( "kdevdragonscriptsupport/dslangdoc/Enumeration.ds" ) ) );
	}
	return documentationFileEnumeration;
}

Declaration *Helpers::getInternalTypeDeclaration( const TopDUContext &top,
const QualifiedIdentifier &identifier, const QVector<const TopDUContext*> &reachableContexts ){
	QList<Declaration*> declarations( top.findDeclarations( identifier ) );
	if( ! declarations.isEmpty() ){
		return declarations.first();
	}
	
	foreach( const TopDUContext *each, reachableContexts ){
		declarations = each->findDeclarations( identifier );
		if( ! declarations.isEmpty() ){
			return declarations.first();
		}
	}
	
	return nullptr;
}



Declaration *Helpers::declarationForName( const IndexedIdentifier &identifier,
const CursorInRevision &location, const DUContext &context, bool useReachable,
const QVector<const TopDUContext*> &reachableContexts ){
	QList<Declaration*> declarations;
	
	// find declaration in local context up to location
// 	declarations = context.findLocalDeclarations( identifier, location );
// 	if( ! declarations.isEmpty() ){
// 		return declarations.last();
// 	}
	
	// find declaration in this context, base class and up the parent chain. this finds
	// also definitions in the local scope beyond the location. both checks are required
	// with the location based local version first to avoid a later definition hiding a
	// local one instead of vice versa
	declarations = context.findDeclarations( identifier, location );
	if( ! declarations.isEmpty() ){
		return declarations.last();
	}
	
	// find declarations in base context. for this to work properly we to first find the
	// closest class declaration up the parent chain
	const ClassDeclaration * const classDecl = thisClassDeclFor( context );
	if( classDecl && classDecl->internalContext() ){
		Declaration * const foundDeclaration = declarationForNameInBase(
			identifier, *classDecl->internalContext(), reachableContexts );
		if( foundDeclaration ){
			return foundDeclaration;
		}
	}
	
	// find declaration in neighbor and pinned contexts
	if( useReachable ){
		for( const DUContext *reachable : reachableContexts ){
			declarations = reachable->findDeclarations( identifier );
			if( ! declarations.isEmpty() ){
				return declarations.last();
			}
		}
	}
	
	// nothing found
	return nullptr;
}

Declaration *Helpers::declarationForNameInBase( const IndexedIdentifier &identifier,
const DUContext &context, const QVector<const TopDUContext*> &reachableContexts ){
	const ClassDeclaration * const classDecl = dynamic_cast<ClassDeclaration*>( context.owner() );
	if( ! classDecl || classDecl->baseClassesSize() == 0 ){
		return nullptr;
	}
	
	TopDUContext * const topContext = context.topContext();
	uint i;
	
	for( i=0; i<classDecl->baseClassesSize(); i++ ){
		DUContext * const baseContext = contextForType( *topContext,
			classDecl->baseClasses()[ i ].baseClass.abstractType(), reachableContexts );
		if( ! baseContext ){
			continue;
		}
		
		const QList<Declaration*> declarations( baseContext->findDeclarations( identifier ) );
		if( ! declarations.isEmpty() ){
			return declarations.last();
		}
		
		Declaration * const foundDeclaration = declarationForNameInBase(
			identifier, *baseContext, reachableContexts );
		if( foundDeclaration ){
			return foundDeclaration;
		}
	}
	
	return nullptr;
}



QVector<Declaration*> Helpers::declarationsForName( const IndexedIdentifier &identifier,
const CursorInRevision &location, const DUContext &context, bool useReachable,
const QVector<const TopDUContext*> &reachableContexts, bool onlyFunctions ){
	DUContext::SearchFlag searchFlags = DUContext::NoSearchFlags;
	QVector<Declaration*> foundDeclarations;
	QList<Declaration*> declarations;
	
	if( onlyFunctions ){
		searchFlags = ( DUContext::SearchFlag )( searchFlags | DUContext::OnlyFunctions );
	}
	
	// find declaration in local context
// 	declarations = context.findLocalDeclarations( identifier, location );
// 	foreach( Declaration *declaration, declarations ){
// 		foundDeclarations.append( declaration );
// 	}
	
	// find declaration in this context and up the parent chain
	declarations = context.findDeclarations( identifier, location, nullptr, searchFlags );
	foreach( Declaration *declaration, declarations ){
		if( ! foundDeclarations.contains( declaration ) ){
			foundDeclarations.append( declaration );
		}
	}
	
	// find declarations in base context. for this to work properly we to first find the
	// closest class declaration up the parent chain
	const ClassDeclaration * const classDecl = thisClassDeclFor( context );
	if( classDecl && classDecl->internalContext() ){
		foundDeclarations.append( declarationsForNameInBase( identifier,
			*classDecl->internalContext(), reachableContexts, onlyFunctions ) );
	}
	
	// find declarations in neighbor and pinned contexts
	if( useReachable ){
		for( const DUContext *reachable : reachableContexts ){
			declarations = reachable->findDeclarations( identifier,
				CursorInRevision::invalid(), nullptr, searchFlags );
			foreach( Declaration* declaration, declarations ){
				foundDeclarations.append( declaration );
			}
		}
	}
	
	return foundDeclarations;
}

QVector<Declaration*> Helpers::declarationsForNameInBase( const IndexedIdentifier &identifier,
const DUContext &context, const QVector<const TopDUContext*> &reachableContexts, bool onlyFunctions ){
	QVector<Declaration*> foundDeclarations;
	
	const ClassDeclaration * const classDecl = dynamic_cast<ClassDeclaration*>( context.owner() );
	if( ! classDecl || classDecl->baseClassesSize() == 0 ){
		return foundDeclarations;
	}
	
	DUContext::SearchFlag searchFlags = DUContext::NoSearchFlags;
	TopDUContext * const top = context.topContext();
	uint i;
	
	if( onlyFunctions ){
		searchFlags = ( DUContext::SearchFlag )( searchFlags | DUContext::OnlyFunctions );
	}
	
	for( i=0; i<classDecl->baseClassesSize(); i++ ){
		const StructureType::Ptr structType( classDecl->baseClasses()[ i ]
			.baseClass.abstractType().cast<StructureType>() );
		if( ! structType ){
			continue;
		}
		
		DUContext *baseContext = structType->internalContext( top );
		if( ! baseContext ){
			foreach( const DUContext *each, reachableContexts ){
				baseContext = structType->internalContext( each->topContext() );
				if( baseContext ){
					break;
				}
			}
			
			if( ! baseContext ){
				continue;
			}
		}
		
		const QList<Declaration*> declarations( baseContext->findDeclarations(
			identifier, CursorInRevision::invalid(), nullptr, searchFlags ) );
		foreach( Declaration *declaration, declarations ){
			if( ! foundDeclarations.contains( declaration ) ){
				foundDeclarations.append( declaration );
			}
		}
		
		foundDeclarations.append( declarationsForNameInBase(
			identifier, *baseContext, reachableContexts, onlyFunctions ) );
	}
	
	return foundDeclarations;
}

QVector<Declaration*> Helpers::constructorsInClass( const DUContext &context ){
	// find declaration in this context without base class or parent chain
	QList<Declaration*> declarations( context.findLocalDeclarations( pNameConstructor,
		CursorInRevision::invalid(), nullptr, {}, DUContext::OnlyFunctions ) );
	return QVector<Declaration*>( declarations.cbegin(), declarations.cend() );
}

ClassFunctionDeclaration *Helpers::bestMatchingFunction(
const QVector<AbstractType::Ptr> &signature, const QVector<Declaration*> &declarations ){
	ClassFunctionDeclaration *bestMatchingDecl = nullptr;
	const int argCount = signature.size();
	int i;
	
	foreach( Declaration* declaration, declarations ){
		ClassFunctionDeclaration * const funcDecl = dynamic_cast<ClassFunctionDeclaration*>( declaration );
		if( ! funcDecl ){
			continue;
		}
		
		const FunctionType::Ptr funcType = funcDecl->type<FunctionType>();
		if( funcType->arguments().size() != argCount ){
			continue;
		}
		
		for( i=0; i<argCount; i++ ){
			if( ! equals( signature.at( i ), funcType->arguments().at( i ) ) ){
				break;
			}
		}
		if( i == argCount ){
			bestMatchingDecl = funcDecl;
			break;
		}
	}
	
	return bestMatchingDecl;
}

QVector<ClassFunctionDeclaration*> Helpers::autoCastableFunctions( const TopDUContext &top,
const QVector<AbstractType::Ptr> &signature, const QVector<Declaration*> &declarations,
const QVector<const TopDUContext*> &reachableContexts ){
	QVector<ClassFunctionDeclaration*> possibleFunctions;
	const int argCount = signature.size();
	int i;
	
	foreach( Declaration *declaration, declarations ){
		ClassFunctionDeclaration * const funcDecl = dynamic_cast<ClassFunctionDeclaration*>( declaration );
		if( ! funcDecl ){
			continue;
		}
		
		const FunctionType::Ptr funcType = funcDecl->type<FunctionType>();
		if( funcType->arguments().size() != argCount ){
			continue;
		}
		
		for( i=0; i<argCount; i++ ){
			if( ! castable( top, signature.at( i ), funcType->arguments().at( i ), reachableContexts ) ){
				break;
			}
		}
		if( i < argCount ){
			continue;
		}
		
		// this function could be identical in signature to another one found so far.
		// if this is the case and the already found function is a reimplmentation
		// of this function then ignore this function
		bool ignoreFunction = false;
		foreach( const ClassFunctionDeclaration *checkFuncDecl, possibleFunctions ){
			// if this is a constructor call do not check the return type
			if( pNameConstructor == declaration->identifier() ){
				if( sameSignatureAnyReturnType( funcType, checkFuncDecl->type<FunctionType>() )
				&& overrides( top, checkFuncDecl, funcDecl, reachableContexts ) ){
					ignoreFunction = true;
					break;
				}
				
			}else{
				if( sameSignature( funcType, checkFuncDecl->type<FunctionType>() )
				&& overrides( top, checkFuncDecl, funcDecl, reachableContexts ) ){
					ignoreFunction = true;
					break;
				}
			}
		}
		if( ignoreFunction ){
			continue;
		}
		
		// function matches and is a suitable pick
		possibleFunctions.append( funcDecl );
	}
	
	return possibleFunctions;
}

}
