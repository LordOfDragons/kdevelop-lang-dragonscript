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
#include "TypeFinder.h"


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

ClassDeclaration *Helpers::superClassDeclFor( const DUContext &context, TypeFinder &typeFinder ){
	ClassDeclaration * const decl = thisClassDeclFor( context );
	if( ! decl || ! context.parentContext() ){
		return nullptr;
	}
	
	const ClassDeclaration * const classDecl = dynamic_cast<const ClassDeclaration*>( decl );
	if( ! classDecl ){
		return nullptr;
	}
	
	return superClassOf( *classDecl, typeFinder );
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

ClassDeclaration *Helpers::superClassOf( const ClassDeclaration &declaration, TypeFinder &typeFinder ){
	if( declaration.baseClassesSize() == 0 ){
		return nullptr;
	}
	
	return typeFinder.declarationFor( declaration.baseClasses()[ 0 ].baseClass.abstractType() );
}

QVector<ClassDeclaration*> Helpers::implementsOf( const ClassDeclaration &declaration, TypeFinder &typeFinder ){
	QVector<ClassDeclaration*> implements;
	uint i;
	
	for( i=1; i<declaration.baseClassesSize(); i++ ){
		ClassDeclaration * const implementDecl = typeFinder.declarationFor(
			declaration.baseClasses()[ i ].baseClass.abstractType() );
		if( implementDecl ){
			implements.append( implementDecl );
		}
	}
	
	return implements;
}

QVector<ClassDeclaration*> Helpers::baseClassesOf( const ClassDeclaration &declaration, TypeFinder &typeFinder ){
	QVector<ClassDeclaration*> baseClasses;
	uint i;
	
	for( i=0; i<declaration.baseClassesSize(); i++ ){
		ClassDeclaration * const baseClassDecl = typeFinder.declarationFor(
			declaration.baseClasses()[ i ].baseClass.abstractType() );
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

bool Helpers::castable( const AbstractType::Ptr &type, const AbstractType::Ptr &targetType,
TypeFinder &typeFinder ){
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
	ClassDeclaration * const classDecl = typeFinder.declarationFor( type );
	if( ! classDecl ){
		return false;
	}
	
	ClassDeclaration * const superClassDecl = superClassOf( *classDecl, typeFinder );
	if( superClassDecl ){
		if( castable( superClassDecl->abstractType(), targetType, typeFinder ) ){
			return true;
		}
	}
	
	const QVector<ClassDeclaration*> implements( implementsOf( *classDecl, typeFinder ) );
	foreach( ClassDeclaration* implement, implements ){
		if( castable( implement->abstractType(), targetType, typeFinder ) ){
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

bool Helpers::overrides( const ClassFunctionDeclaration *func1,
const ClassFunctionDeclaration *func2, TypeFinder &typeFinder ){
	return func1 && func1->context() && func1->context()->owner()
		&& func2 && func2->context() && func2->context()->owner()
		&& castable( func1->context()->owner()->abstractType(),
			func2->context()->owner()->abstractType(), typeFinder );
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



Declaration *Helpers::declarationForName( const IndexedIdentifier &identifier,
const CursorInRevision &location, const DUContext &context,
const QVector<const TopDUContext*> &pinnedNamespaces, TypeFinder &typeFinder ){
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
			identifier, *classDecl->internalContext(), typeFinder );
		if( foundDeclaration ){
			return foundDeclaration;
		}
	}
	
	// find declaration in pinned contexts
	foreach( const DUContext *pinned, pinnedNamespaces ){
		declarations = pinned->findDeclarations( identifier );
		if( ! declarations.isEmpty() ){
			return declarations.last();
		}
	}
	
	// find declaration in global scope
	return typeFinder.declarationFor( identifier );
}

Declaration *Helpers::declarationForNameInBase( const IndexedIdentifier &identifier,
const DUContext &context, TypeFinder &typeFinder ){
	const ClassDeclaration * const classDecl = dynamic_cast<ClassDeclaration*>( context.owner() );
	if( ! classDecl || classDecl->baseClassesSize() == 0 ){
		return nullptr;
	}
	
	uint i;
	for( i=0; i<classDecl->baseClassesSize(); i++ ){
		DUContext * const baseContext = typeFinder.contextFor(
			classDecl->baseClasses()[ i ].baseClass.abstractType() );
// 		qDebug() << "DEBUG" << classDecl->toString() << i
// 			<< classDecl->baseClasses()[ i ].baseClass.abstractType()->toString()
// 			<< (baseContext ? baseContext->scopeIdentifier(true).toString() : "-");
		if( ! baseContext ){
			continue;
		}
		
		const QList<Declaration*> declarations( baseContext->findDeclarations( identifier ) );
		if( ! declarations.isEmpty() ){
			return declarations.last();
		}
		
		Declaration * const foundDeclaration = declarationForNameInBase( identifier, *baseContext, typeFinder );
		if( foundDeclaration ){
			return foundDeclaration;
		}
	}
	
	return nullptr;
}



QVector<Declaration*> Helpers::declarationsForName( const IndexedIdentifier &identifier,
const CursorInRevision &location, const DUContext &context,
const QVector<const TopDUContext*> &pinnedNamespaces, TypeFinder &typeFinder, bool onlyFunctions ){
	DUContext::SearchFlag searchFlags = DUContext::NoSearchFlags;
	QVector<Declaration*> foundDeclarations;
	QList<Declaration*> declarations;
	
	if( onlyFunctions ){
		searchFlags = ( DUContext::SearchFlag )( searchFlags | DUContext::OnlyFunctions );
	}
	
	// find declaration in local context
// 	declarations = context.findLocalDeclarations( identifier, location, nullptr, {}, searchFlags );
// 	foreach( Declaration *declaration, declarations ){
// 		foundDeclarations.append( declaration );
// 	}
	
	// find declaration in this context and up the parent chain
	declarations = context.findDeclarations( identifier, location, nullptr, searchFlags );
	foreach( Declaration *declaration, declarations ){
		if( ! foundDeclarations.contains( declaration ) ){
			foundDeclarations << declaration;
		}
	}
	
	// find declarations in base context. for this to work properly we to first find the
	// closest class declaration up the parent chain
	const ClassDeclaration * const classDecl = thisClassDeclFor( context );
	if( classDecl && classDecl->internalContext() ){
		foundDeclarations << declarationsForNameInBase( identifier,
			*classDecl->internalContext(), typeFinder, onlyFunctions );
	}
	
	// find declarations in pinned contexts
	foreach( const DUContext *pinned, pinnedNamespaces ){
		declarations = pinned->findDeclarations( identifier, CursorInRevision::invalid(), nullptr, searchFlags );
		foreach( Declaration* declaration, declarations ){
			foundDeclarations << declaration;
		}
	}
	
	// find declaration in global scope
	if( ! onlyFunctions ){
		ClassDeclaration * const globalDecl = typeFinder.declarationFor( identifier );
		if( globalDecl ){
			foundDeclarations << globalDecl;
		}
	}
	
	return foundDeclarations;
}

QVector<Declaration*> Helpers::declarationsForNameInBase( const IndexedIdentifier &identifier,
const DUContext &context, TypeFinder &typeFinder, bool onlyFunctions ){
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
		DUContext * const baseContext = typeFinder.contextFor(
			classDecl->baseClasses()[ i ].baseClass.abstractType() );
		if( ! baseContext ){
			continue;
		}
		
		const QList<Declaration*> declarations( baseContext->findDeclarations(
			identifier, CursorInRevision::invalid(), nullptr, searchFlags ) );
		foreach( Declaration *declaration, declarations ){
			if( ! foundDeclarations.contains( declaration ) ){
				foundDeclarations.append( declaration );
			}
		}
		
		foundDeclarations.append( declarationsForNameInBase(
			identifier, *baseContext, typeFinder, onlyFunctions ) );
	}
	
	return foundDeclarations;
}



QVector<QPair<Declaration*, int>> Helpers::allDeclarations( const CursorInRevision& location,
const DUContext &context, const QVector<const TopDUContext*> &pinnedNamespaces, TypeFinder &typeFinder ){
	// NOTE allDeclarations() is not used since it returns tons of duplicates and
	//      does not play well with neighbor context searching
	QVector<QPair<Declaration*, int>> declarations;
	
	// find declaration in this context and up the parent chain
	const DUContext *searchContext = &context;
	while( searchContext ){
		const QVector<Declaration*> foundDeclarations( searchContext->localDeclarations() );
		foreach( Declaration *each, foundDeclarations ){
			declarations << QPair<Declaration*, int>{ each, 0 };
		}
		searchContext = searchContext->parentContext();
	}
	
	// find declarations in base context. for this to work properly we first find the
	// closest class declaration up the parent chain
	const ClassDeclaration * const classDecl = thisClassDeclFor( context );
	if( classDecl && classDecl->internalContext() ){
		const QVector<QPair<Declaration*, int>> declList(
			allDeclarationsInBase( *classDecl->internalContext(), typeFinder ) );
		foreach( auto each, declList ){
			declarations << QPair<Declaration*, int>{ each.first, each.second + 1 };
		}
	}
	
	// find declarations in pinned contexts
	foreach( const DUContext *pinned, pinnedNamespaces ){
		const QVector<Declaration*> foundDeclarations( pinned->localDeclarations() );
		foreach( Declaration *each, foundDeclarations){
			declarations << QPair<Declaration*, int>{ each, 1000 };
		}
	}
	
	return declarations;
}

QVector<QPair<Declaration*, int>> Helpers::allDeclarationsInBase( const DUContext &context, TypeFinder &typeFinder ){
	// NOTE allDeclarations() is not used since it returns tons of duplicates and
	//      does not play well with neighbor context searching
	QVector<QPair<Declaration*, int>> declarations;
	
	const ClassDeclaration * const classDecl = dynamic_cast<ClassDeclaration*>( context.owner() );
	if( ! classDecl || classDecl->baseClassesSize() == 0 ){
		return declarations;
	}
	
	uint i;
	for( i=0; i<classDecl->baseClassesSize(); i++ ){
		DUContext * const baseContext = typeFinder.contextFor(
			classDecl->baseClasses()[ i ].baseClass.abstractType() );
		if( ! baseContext ){
			continue;
		}
		
		const QVector<Declaration*> foundDeclarations( baseContext->localDeclarations() );
		foreach( Declaration *each, foundDeclarations ){
			declarations << QPair<Declaration*, int>{ each, 0 };
		}
		
		const QVector<QPair<Declaration*, int>> declList( allDeclarationsInBase( *baseContext, typeFinder ) );
		foreach( auto each, declList ){
			declarations << QPair<Declaration*, int>{ each.first, each.second + 1 };
		}
	}
	
	return declarations;
}

QVector<QPair<Declaration*, int>> Helpers::consolidate( const QVector<QPair<Declaration*, int>> &list ){
	QVector<QPair<Declaration*, int>> result;
	
	foreach( auto foundDecl, list ){
		const TypePtr<FunctionType> foundFunc = foundDecl.first->type<FunctionType>();
		bool include = true;
		
		QVector<QPair<Declaration*, int>>::iterator iter;
		for( iter = result.begin(); iter != result.end(); iter++ ){
			// functions can be duplicated because interfaces are base class Object too
			if( foundDecl.first == iter->first ){
				include = false;
				break;
			}
			
			// filter out same signature functions keeping those with smaller depth
			if( foundDecl.first->indexedIdentifier() != iter->first->indexedIdentifier() ){
				continue;
			}
			
			if( foundFunc ){
				const TypePtr<FunctionType> checkFunc = foundDecl.first->type<FunctionType>();
				if( ! foundFunc->equals( checkFunc.data() ) ){
					continue;
				}
			}
			
			if( foundDecl.second >= iter->second ){
				include = false;
				break;
				
			}else{
				result.erase( iter );
				break;
			}
		}
		
		if( include ){
			result << foundDecl;
		}
	}
	
	return result;
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

QVector<ClassFunctionDeclaration*> Helpers::autoCastableFunctions(
const QVector<AbstractType::Ptr> &signature, const QVector<Declaration*> &declarations,
TypeFinder &typeFinder ){
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
			if( ! castable( signature.at( i ), funcType->arguments().at( i ), typeFinder ) ){
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
				&& overrides( checkFuncDecl, funcDecl, typeFinder ) ){
					ignoreFunction = true;
					break;
				}
				
			}else{
				if( sameSignature( funcType, checkFuncDecl->type<FunctionType>() )
				&& overrides( checkFuncDecl, funcDecl, typeFinder ) ){
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
