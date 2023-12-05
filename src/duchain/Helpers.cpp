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
	
	const StructureType::Ptr structType( type.dynamicCast<StructureType>() );
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
	const StructureType::Ptr structType( type.dynamicCast<StructureType>() );
	const StructureType::Ptr structTargetType( type.dynamicCast<StructureType>() );
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

QualifiedIdentifier Helpers::shortestQualifiedIdentifier( const AbstractType::Ptr &type,
const QVector<Namespace*> &namespaces, TypeFinder &typeFinder, Namespace &rootNamespace ){
	const ClassDeclaration * const classDecl = typeFinder.declarationFor( type );
	if( ! classDecl ){
		return QualifiedIdentifier();
	}
	
	const QualifiedIdentifier identifier( classDecl->qualifiedIdentifier() );
	const int count = identifier.count();
	if( count == 0 ){
		return QualifiedIdentifier();
	}
	
	// find first the namespace in the qualified identifier. this way we know which
	// identifier part is the top most class identifier.
	Namespace *ns = &rootNamespace;
	int i;
	
	for( i=0; i<count-1; i++ ){
		Namespace * const findNS = ns->getNamespace( IndexedIdentifier( identifier.at( i ) ) );
		if( ! findNS ){
			break;
		}
		ns = findNS;
	}
	
	// try to find the class identifier in the contexts up to the first namespace context.
	// this covers the class being an inner class of one of the contexts underneath namespace
	DUContext *context = classDecl->internalContext();
	while( context ){
		if( context->owner() && context->owner()->kind() == Declaration::Namespace ){
			break;
		}
		const QList<Declaration*> decls( context->findLocalDeclarations(
			identifier.at( i ), CursorInRevision::invalid() ) );
		if( ! decls.isEmpty() ){
			return QualifiedIdentifier( identifier.at( i ) );
		}
		context = context->parentContext();
	}
	
	// if this yields no result walk up the namespace identifier in the qualified identifier.
	// try to find each namespace part in the search namespaces. the first one scoring a
	// match is used as the namespace to resolve against
	for( ; i>=0; i-- ){
		foreach( Namespace *each, namespaces ){
			if( each != ns ){
				continue;
			}
			
			QualifiedIdentifier result;
			for( ; i<count; i++ ){
				result += identifier.at( i );
			}
			return result;
		}
		
		ns = ns->parent();
	}
	
	return identifier;
}

QString Helpers::formatShortestIdentifier( const AbstractType::Ptr &type,
const QVector<Namespace*> &namespaces, TypeFinder &typeFinder, Namespace &rootNamespace ){
	const QualifiedIdentifier identifier( shortestQualifiedIdentifier(
		type, namespaces, typeFinder, rootNamespace ) );
	if( identifier.isEmpty() ){
		return type->toString();
		
	}else{
		return formatIdentifier( identifier );
	}
}

QString Helpers::formatIdentifier( const QualifiedIdentifier &identifier ){
	const int count = identifier.count();
	QString name;
	int i;
	
	for( i=0; i<count; i++ ){
		if( i > 0 ){
			name += ".";
		}
		name += identifier.at( i ).toString();
	}
	
	return name;
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
const QVector<Namespace*> &namespaces, TypeFinder &typeFinder,
Namespace &rootNamespace, bool withGlobal ){
	// findLocalDeclarations() and findDeclarations() are not working for dragonscript
	// since they search only up to a specific location. it is also not possible to skip
	// that location at all otherwise the first context-for-location look up fails.
	// we have thus to search on our own. start searching limited by location until the
	// function definition is hit. then continue with unlimited searching
	const ClassDeclaration * const classDecl = thisClassDeclFor( context );
	const DUContext * const classContext = classDecl ? classDecl->internalContext() : nullptr;
	const DUContext *searchContext = &context;
	
	// up to class scope apply location if present
	if( location.isValid() ){
		while( searchContext && searchContext != classContext ){
			const QList<Declaration*> declarations( searchContext->findLocalDeclarations( identifier, location ) );
			if( ! declarations.isEmpty() ){
				return declarations.last();
			}
			searchContext = searchContext->parentContext();
		}
	}
	
	// up to and including class scope find all declarations
	while( searchContext ){
		const QList<Declaration*> declarations( searchContext->findLocalDeclarations( identifier ) );
		if( ! declarations.isEmpty() ){
			return declarations.last();
		}
		if( searchContext == classContext ){
			break;
		}
		searchContext = searchContext->parentContext();
	}
	
	// find declarations up to but excluding first namespace scope. here only types are allowed
	if( classContext ){
		searchContext = classContext->parentContext();
		while( searchContext ){
			if( searchContext->owner() && searchContext->owner()->kind() == Declaration::Namespace ){
				break;
			}
			const QList<Declaration*> declarations( searchContext->findLocalDeclarations( identifier ) );
			foreach( Declaration *d, declarations ){
				if( d->type<StructureType>() ){
					return d;
				}
			}
			searchContext = searchContext->parentContext();
		}
	}
	
	// find declarations in base contexts
	if( classContext ){
		Declaration * const declaration = declarationForNameInBase( identifier, *classContext, typeFinder );
		if( declaration ){
			return declaration;
		}
	}
	
	// find declaration in namespaces. if context declaration is a namespace include context
	if( context.owner() && context.owner()->kind() == Declaration::Namespace ){
		Namespace *ns = rootNamespace.getNamespace( context.owner()->qualifiedIdentifier() );
		if( ns ){
			ClassDeclaration * const classDecl = ns->getClass( identifier );
			if( classDecl ){
				return classDecl;
			}
			ns = ns->getNamespace( identifier );
			if( ns && ns->declaration() ){
				return ns->declaration();
			}
		}
	}
	
	if( ! namespaces.isEmpty() ){
		Namespace *ns = nullptr;
		foreach( Namespace *each, namespaces ){
			ClassDeclaration * const classDecl = each->getClass( identifier );
			if( classDecl ){
				return classDecl;
			}
			if( ! ns ){
				ns = each->getNamespace( identifier );
			}
		}
		if( ns && ns->declaration() ){
			return ns->declaration();
		}
	}
	
	// find declaration in global scope
	if( withGlobal ){
		ClassDeclaration * const globalClassDecl = rootNamespace.getClass( identifier );
		if( globalClassDecl ){
			return globalClassDecl;
		}
		
		Namespace * const globalNS = rootNamespace.getNamespace( identifier );
		if( globalNS ){
			return globalNS->declaration();
		}
	}
	
	return nullptr;
}

Declaration *Helpers::declarationForNameInBase( const IndexedIdentifier &identifier,
const DUContext &context, TypeFinder &typeFinder ){
	const ClassDeclaration * const classDecl = dynamic_cast<ClassDeclaration*>( context.owner() );
	if( ! classDecl || classDecl->baseClassesSize() == 0 ){
		return nullptr;
	}
	
	uint i;
	for( i=0; i<classDecl->baseClassesSize(); i++ ){
		DUContext * const baseContext = typeFinder.contextFor( classDecl->baseClasses()[ i ].baseClass.abstractType() );
		if( ! baseContext ){
			continue;
		}
		
		DUContext *searchContext = baseContext;
		while( searchContext ){
			if( searchContext->owner() && searchContext->owner()->kind() == Declaration::Namespace ){
				break;
			}
			const QList<Declaration*> declarations( searchContext->findLocalDeclarations( identifier ) );
			if( ! declarations.isEmpty() ){
				return declarations.last();
			}
			searchContext = searchContext->parentContext();
		}
		
		Declaration * const declaration = declarationForNameInBase( identifier, *baseContext, typeFinder );
		if( declaration ){
			return declaration;
		}
	}
	
	return nullptr;
}



QVector<Declaration*> Helpers::declarationsForName( const IndexedIdentifier &identifier,
const CursorInRevision &location, const DUContext &context,
const QVector<Namespace*> &namespaces, TypeFinder &typeFinder,
Namespace &rootNamespace, bool onlyFunctions, bool withGlobal ){
	// findLocalDeclarations() and findDeclarations() are not working for dragonscript
	// since they search only up to a specific location. it is also not possible to skip
	// that location at all otherwise the first context-for-location look up fails.
	// we have thus to search on our own. start searching limited by location until the
	// function definition is hit. then continue with unlimited searching
	const ClassDeclaration * const classDecl = thisClassDeclFor( context );
	const DUContext * const classContext = classDecl ? classDecl->internalContext() : nullptr;
	const DUContext *searchContext = &context;
	QVector<Declaration*> declarations;
	
	DUContext::SearchFlag searchFlags = DUContext::NoSearchFlags;
	if( onlyFunctions ){
		searchFlags = ( DUContext::SearchFlag )( searchFlags | DUContext::OnlyFunctions );
	}
	
	// up to class scope apply location if present
	if( location.isValid() ){
		while( searchContext && searchContext != classContext ){
			const QList<Declaration*> found( searchContext->findLocalDeclarations(
				identifier, location, nullptr, {}, searchFlags ) );
			foreach( Declaration *d, found ){
				declarations << d;
			}
			searchContext = searchContext->parentContext();
		}
	}
	
	// up to and including class scope find all declarations
	while( searchContext ){
		const QList<Declaration*> found( searchContext->findLocalDeclarations(
			identifier, CursorInRevision::invalid(), nullptr, {}, searchFlags ) );
		foreach( Declaration *d, found ){
			declarations << d;
		}
		if( searchContext == classContext ){
			break;
		}
		searchContext = searchContext->parentContext();
	}
	
	// find declarations up to but excluding first namespace scope. here only types are allowed
	if( classContext ){
		searchContext = classContext->parentContext();
		while( searchContext ){
			if( searchContext->owner() && searchContext->owner()->kind() == Declaration::Namespace ){
				break;
			}
			const QList<Declaration*> found( searchContext->findLocalDeclarations(
				identifier, CursorInRevision::invalid(), nullptr, {}, searchFlags ) );
			foreach( Declaration *d, found ){
				if( d->type<StructureType>() ){
					declarations << d;
				}
			}
			searchContext = searchContext->parentContext();
		}
	}
	
	// find declarations in base contexts
	if( classContext ){
		declarations << declarationsForNameInBase( identifier, *classContext,
			typeFinder, rootNamespace, onlyFunctions );
	}
	
	// find declaration in namespaces. add first classes then namespace declarations
	// if context declaration is a namespace include context
	if( context.owner() && context.owner()->kind() == Declaration::Namespace ){
		Namespace *ns = rootNamespace.getNamespace( context.owner()->qualifiedIdentifier() );
		if( ns ){
			ClassDeclaration * const classDecl = ns->getClass( identifier );
			if( classDecl ){
				declarations << classDecl;
			}
			ns = ns->getNamespace( identifier );
			if( ns && ns->declaration() ){
				declarations << ns->declaration();
			}
		}
	}
	
	if( ! onlyFunctions && ! namespaces.isEmpty() ){
		QVector<Declaration*> namespaceDecls;
		foreach( Namespace *each, namespaces ){
			ClassDeclaration * const classDecl = each->getClass( identifier );
			if( classDecl ){
				declarations << classDecl;
			}
			Namespace * const possibleNS = each->getNamespace( identifier );
			if( possibleNS && possibleNS->declaration() ){
				namespaceDecls << possibleNS->declaration();
			}
		}
		declarations << namespaceDecls;
	}
	
	// find declaration in global scope
	if( withGlobal ){
		ClassDeclaration * const classDecl = rootNamespace.getClass( identifier );
		if( classDecl ){
			declarations << classDecl;
		}
		
		Namespace * const possibleNS = rootNamespace.getNamespace( identifier );
		if( possibleNS && possibleNS->declaration() ){
			declarations << possibleNS->declaration();
		}
	}
	
	return declarations;
}

QVector<Declaration*> Helpers::declarationsForNameInBase( const IndexedIdentifier &identifier,
const DUContext &context, TypeFinder &typeFinder, Namespace &rootNamespace, bool onlyFunctions ){
	QVector<Declaration*> foundDeclarations;
	
	const ClassDeclaration * const classDecl = dynamic_cast<ClassDeclaration*>( context.owner() );
	if( ! classDecl || classDecl->baseClassesSize() == 0 ){
		return foundDeclarations;
	}
	
	DUContext::SearchFlag searchFlags = DUContext::NoSearchFlags;
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
		
		DUContext *searchContext = baseContext;
		while( searchContext ){
			if( searchContext->owner() && searchContext->owner()->kind() == Declaration::Namespace ){
				break;
			}
			const QList<Declaration*> declarations( searchContext->findLocalDeclarations( identifier ) );
			foreach( Declaration *decl, declarations ){
				foundDeclarations << decl;
			}
			searchContext = searchContext->parentContext();
		}
		
		foundDeclarations.append( declarationsForNameInBase( identifier, *baseContext,
			typeFinder, rootNamespace, onlyFunctions ) );
	}
	
	return foundDeclarations;
}



QVector<QPair<Declaration*, int>> Helpers::allDeclarations( const CursorInRevision& location,
const DUContext &context, const QVector<Namespace*> &namespaces, TypeFinder &typeFinder,
Namespace &rootNamespace, bool withGlobal ){
	// findLocalDeclarations() and findDeclarations() are not working for dragonscript
	// since they search only up to a specific location. it is also not possible to skip
	// that location at all otherwise the first context-for-location look up fails.
	// we have thus to search on our own. start searching limited by location until the
	// function definition is hit. then continue with unlimited searching
	const ClassDeclaration * const classDecl = thisClassDeclFor( context );
	const DUContext * const classContext = classDecl ? classDecl->internalContext() : nullptr;
	QVector<QPair<Declaration*, int>> declarations;
	const DUContext *searchContext = &context;
	
	// up to class scope apply location if present
	if( location.isValid() ){
		while( searchContext && searchContext != classContext ){
			const QVector<Declaration*> found( searchContext->localDeclarations() );
			foreach( Declaration *d, found ){
				if( d->range().end < location ){
					declarations << QPair<Declaration*, int>{ d, 0 };
				}
			}
			searchContext = searchContext->parentContext();
		}
	}
	
	// up to and including class scope find all declarations
	while( searchContext ){
		const QVector<Declaration*> found( searchContext->localDeclarations() );
		foreach( Declaration *d, found ){
			declarations << QPair<Declaration*, int>{ d, 0 };
		}
		if( searchContext == classContext ){
			break;
		}
		searchContext = searchContext->parentContext();
	}
	
	// find declarations up to but excluding first namespace scope. here only types are allowed
	if( classContext ){
		searchContext = classContext->parentContext();
		while( searchContext ){
			if( searchContext->owner() && searchContext->owner()->kind() == Declaration::Namespace ){
				break;
			}
			const QVector<Declaration*> found( searchContext->localDeclarations() );
			foreach( Declaration *d, found ){
				if( d->type<StructureType>() ){
					declarations << QPair<Declaration*, int>{ d, 0 };
				}
			}
			searchContext = searchContext->parentContext();
		}
	}
	
	// find declarations in base contexts
	if( classContext ){
		const QVector<QPair<Declaration*, int>> declList( allDeclarationsInBase( *classContext, typeFinder ) );
		foreach( auto each, declList ){
			declarations << QPair<Declaration*, int>{ each.first, each.second + 1 };
		}
	}
	
	// find declaration in namespaces. add first classes then namespace declarations
	// if context declaration is a namespace include context
	if( context.owner() && context.owner()->kind() == Declaration::Namespace ){
		Namespace * const ns = rootNamespace.getNamespace( context.owner()->qualifiedIdentifier() );
		if( ns ){
			foreach( const Namespace::ClassDeclarationPointer &classDecl, ns->classes() ){
				declarations << QPair<Declaration*, int>{ classDecl.data(), 1000 };
			}
			foreach( const Namespace::Ref &nsref, ns->namespaces() ){
				if( nsref && nsref->declaration() ){
					QPair<Declaration*, int>{ nsref->declaration(), 1000 };
				}
			}
		}
	}
	
	if( ! namespaces.isEmpty() ){
		QVector<Declaration*> namespaceDecls;
		foreach( Namespace *each, namespaces ){
			foreach( const Namespace::ClassDeclarationPointer &classDecl, each->classes() ){
				declarations << QPair<Declaration*, int>{ classDecl.data(), 1000 };
			}
			foreach( const Namespace::Ref &nsref, each->namespaces() ){
				if( nsref && nsref->declaration() ){
					namespaceDecls << nsref->declaration();
				}
			}
		}
		foreach( Declaration *nsdecl, namespaceDecls ){
			declarations << QPair<Declaration*, int>{ nsdecl, 1000 };
		}
	}
	
	// find declaration in global scope
	if( withGlobal ){
		foreach( const Namespace::ClassDeclarationPointer &classDecl, rootNamespace.classes() ){
			declarations << QPair<Declaration*, int>{ classDecl.data(), 1000 };
		}
		foreach( const Namespace::Ref &nsref, rootNamespace.namespaces() ){
			if( nsref && nsref->declaration() ){
				declarations << QPair<Declaration*, int>{ nsref->declaration(), 1000 };
			}
		}
	}
	
	return declarations;
}

QVector<QPair<Declaration*, int>> Helpers::allDeclarationsInBase( const DUContext &context, TypeFinder &typeFinder ){
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

QVector<QPair<Declaration*, int>> Helpers::consolidate(
const QVector<QPair<Declaration*, int>> &list, const DUContext &context ){
	QVector<QPair<Declaration*, int>> result;
	
	foreach( auto foundDecl, list ){
		const TypePtr<FunctionType> foundFunc = foundDecl.first->type<FunctionType>();
		bool include = true;
		
		// constructors work a bit different. only the constructors functions matching
		// the provided context are kept. base class constructors are all removed
		if( foundFunc && foundDecl.first->indexedIdentifier() == nameConstructor()
		&& foundDecl.first->context() != &context ){
			continue;
		}
		
		QVector<QPair<Declaration*, int>>::iterator iter;
		for( iter = result.begin(); iter != result.end(); iter++ ){
			// functions can be duplicated because interfaces are base class Object too
			if( foundDecl.first == iter->first ){
				include = false;
				break;
			}
			
			if( foundDecl.first->indexedIdentifier() != iter->first->indexedIdentifier() ){
				continue;
			}
			
			if( foundFunc ){
				// unfortunately we can not use FunctionType->equals() since this also
				// checks the return type. in DS though functions are equal if their
				// arguments are equal disregarding the return type
				/*if( ! foundFunc->equals( iter->first->abstractType().data() ) ){
					continue;
				}*/
				const TypePtr<FunctionType> checkFunc = iter->first->type<FunctionType>();
				if( ! checkFunc ){
					continue;
				}
				
				const QList<AbstractType::Ptr> args1( foundFunc->arguments() );
				const QList<AbstractType::Ptr> args2( checkFunc->arguments() );
				if( args1.size() != args2.size() ){
					continue;
				}
				
				QList<AbstractType::Ptr>::const_iterator iter1, iter2;
				for( iter1 = args1.constBegin(), iter2 = args2.constBegin(); iter1 != args1.constEnd(); iter1++, iter2++ ){
					if( ! iter1->data()->equals( iter2->data() ) ){
						break;
					}
				}
				
				if( iter1 != args1.constEnd() ){
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
	
	// fails on ubuntu... no idea why
	//return QVector<Declaration*>( declarations.cbegin(), declarations.cend() );
	
	QVector<Declaration*> list;
	foreach( Declaration *each, declarations ){
		list << each;
	}
	return list;
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
