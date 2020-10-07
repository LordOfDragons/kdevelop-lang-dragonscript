#include <language/duchain/namespacealiasdeclaration.h>
#include <language/duchain/aliasdeclaration.h>
#include <language/duchain/classdeclaration.h>
#include <language/duchain/types/functiontype.h>

#include <QByteArray>
#include <QtGlobal>
#include <QDebug>

#include <functional>

#include "DeclarationBuilder.h"
#include "EditorIntegrator.h"
#include "ExpressionVisitor.h"
#include "PinNamespaceVisitor.h"
#include "Helpers.h"
#include "TypeFinder.h"
#include "../parser/ParseSession.h"


using namespace KDevelop;

namespace DragonScript{

DeclarationBuilder::DeclarationBuilder( EditorIntegrator &editor, const ParseSession &parseSession,
	const QSet<ImportPackage::Ref> &deps, TypeFinder &typeFinderr, int phase ) :
DeclarationBuilderBase(),
pParseSession( parseSession ),
pNamespaceContextCount( 0 ),
pPhase( phase )
{
	setDependencies( deps );
	setEditor( &editor );
	setTypeFinder( &typeFinderr );
}

DeclarationBuilder::~DeclarationBuilder(){
}

void DeclarationBuilder::closeNamespaceContexts(){
	while( pNamespaceContextCount > 0 ){
		closeContext();
		closeType();
		closeDeclaration();
		pNamespaceContextCount--;
	}
}

QList<Declaration*> DeclarationBuilder::existingDeclarationsForNode( IdentifierAst *node ){
	return currentContext()->findDeclarations( identifierForNode( node ).last(),
		CursorInRevision::invalid(), nullptr, ( DUContext::SearchFlag )
			( DUContext::DontSearchInParent | DUContext::DontResolveAliases ) );
}

QString DeclarationBuilder::getDocumentationForNode( const AstNode &node ) const{
	// TODO process the documentation string to convert it to a user readable form
	return pParseSession.documentation( node );
}



void DeclarationBuilder::visitScript( ScriptAst *node ){
	pLastModifiers = etmPublic;
	DeclarationBuilderBase::visitScript( node );
	closeNamespaceContexts();
}

void DeclarationBuilder::visitScriptDeclaration( ScriptDeclarationAst *node ){
	pLastModifiers = etmNone;
	DeclarationBuilderBase::visitScriptDeclaration( node );
}

void DeclarationBuilder::visitPin( PinAst *node ){
	if( ! node->name->nameSequence || pPhase < 2 ){
		return;
	}
	
	const KDevPG::ListNode<IdentifierAst*> *iter = node->name->nameSequence->front();
	QList<QPair<IdentifierAst*, QualifiedIdentifier>> aliases;
	const KDevPG::ListNode<IdentifierAst*> *end = iter;
	QualifiedIdentifier identifier;
	
	do{
		identifier += Identifier( editor()->tokenText( *iter->element ) );
		aliases.insert( 0, QPair<IdentifierAst*, QualifiedIdentifier>( iter->element, identifier ) );
		iter = iter->next;
	}while( iter != end );
	
	// dragonscript allows searching in parent namespaces of pinned namespaces.
	// to get this working we need an alias definition for the pinned namespace
	// as well as for all parent namespaces. to get the right search order the
	// alias definitions have to be done in reverse order
	QList<QPair<IdentifierAst*, QualifiedIdentifier>>::const_iterator iterAlias;
	for( iterAlias = aliases.cbegin(); iterAlias != aliases.cend(); iterAlias++ ){
		const RangeInRevision range( editorFindRangeNode( iterAlias->first ) );
		NamespaceAliasDeclaration * const decl = openDeclaration<NamespaceAliasDeclaration>(
			globalImportIdentifier(), range, DeclarationFlags::NoFlags );
		eventuallyAssignInternalContext();
		decl->setKind( Declaration::NamespaceAlias );
		decl->setImportIdentifier( iterAlias->second );
		decl->setIdentifier( iterAlias->second.last() );
		decl->setInternalContext( openContext( iter->element, range, DUContext::Namespace, iterAlias->second ) );
		closeContext();
	}
}

void DeclarationBuilder::visitRequires( RequiresAst *node ){
	// this would trigger loading additional scripts but how to handle?
}

void DeclarationBuilder::visitNamespace( NamespaceAst *node ){
// 	qDebug() << "KDevDScript: DeclarationBuilder::visitNamespace";
	closeNamespaceContexts();
	
	const KDevPG::ListNode<IdentifierAst*> *iter = node->name->nameSequence->front();
	const KDevPG::ListNode<IdentifierAst*> *end = iter;
	
	do{
		const RangeInRevision range( editorFindRangeNode( iter->element ) );
		
		ClassDeclaration *decl = openDeclaration<ClassDeclaration>(
			iter->element, iter->element, DeclarationFlags::NoFlags );
// 		NamespaceAliasDeclaration *decl = openDeclaration<NamespaceAliasDeclaration>(
// 			globalImportIdentifier(), range, DeclarationFlags::NoFlags );
		
		eventuallyAssignInternalContext();
		
		decl->setKind( Declaration::Namespace );
// 		decl->setKind( Declaration::NamespaceAlias );
		decl->setComment( getDocumentationForNode( *node ) );
		
		StructureType::Ptr type( new StructureType() );
		type->setDeclaration( decl );
		decl->setType( type );
		
		openType( type );
		
		openContext( iter->element, range, DUContext::Namespace, iter->element );
		
		{
		DUChainWriteLocker lock;
		currentContext()->setLocalScopeIdentifier( identifierForNode( iter->element ) );
		decl->setInternalContext( currentContext() );
		}
		
		iter = iter->next;
		pNamespaceContextCount++;
		
	}while( iter != end );
}

void DeclarationBuilder::visitClass( ClassAst *node ){
	ClassDeclaration * const decl = openDeclaration<ClassDeclaration>(
		node->begin->name, node->begin->name, DeclarationFlags::NoFlags );
	
	eventuallyAssignInternalContext();
	
	decl->setKind( Declaration::Type );
	decl->setComment( getDocumentationForNode( *node ) );
	decl->clearBaseClasses();
	decl->setClassType( ClassDeclarationData::Class );
	decl->setAccessPolicy( accessPolicyFromLastModifiers() );
	
	if( ( pLastModifiers & etmFixed ) == etmFixed ){
		decl->setClassModifier( ClassDeclarationData::Final );
		
	}else if( ( pLastModifiers & etmAbstract ) == etmAbstract ){
		decl->setClassModifier( ClassDeclarationData::Abstract );
		
	}else{
		decl->setClassModifier( ClassDeclarationData::None );
	}
	
	// add base class and interfaces
	if( pPhase > 1 ){
		if( node->begin->extends ){
			DUChainReadLocker lock;
			ExpressionVisitor exprvisitor( *editor(), currentContext(), pinnedNamespaces(), *typeFinder() );
			exprvisitor.visitNode( node->begin->extends );
			
			if( exprvisitor.lastType()
			&& exprvisitor.lastType()->whichType() == AbstractType::TypeStructure ){
				const StructureType::Ptr baseType( exprvisitor.lastType().cast<StructureType>() );
				BaseClassInstance base;
				base.baseClass = baseType->indexed();
				base.access = Declaration::Public;
				decl->addBaseClass( base );
			}
			
		}else if( document() != Helpers::getDocumentationFileObject() ){
			// set object as base class. this is only done if this is not the object class
			// from the documentation being parsed
			ClassDeclaration * const typeObject = typeFinder()->typeObject();
			if( typeObject ){
				BaseClassInstance base;
				base.baseClass = typeObject->abstractType()->indexed();
				base.access = Declaration::Public;
				decl->addBaseClass( base );
			}
		}
		
		if( node->begin->implementsSequence ){
			const KDevPG::ListNode<FullyQualifiedClassnameAst*> *iter = node->begin->implementsSequence->front();
			const KDevPG::ListNode<FullyQualifiedClassnameAst*> *end = iter;
			DUChainReadLocker lock;
			do{
				ExpressionVisitor exprvisitor( *editor(), currentContext(), pinnedNamespaces(), *typeFinder() );
				exprvisitor.visitNode( iter->element );
				if( exprvisitor.lastType() && exprvisitor.lastType()->whichType() == AbstractType::TypeStructure ){
					const StructureType::Ptr baseType( exprvisitor.lastType().cast<StructureType>() );
					BaseClassInstance base;
					base.baseClass = baseType->indexed();
					base.access = Declaration::Public;
					decl->addBaseClass( base );
				}
				iter = iter->next;
			}while( iter != end );
		}
	}
	
	// continue
	StructureType::Ptr type( new StructureType() );
	type->setDeclaration( decl );
	decl->setType( type );
	
	openType( type );
	
	openContextClass( node );
	decl->setInternalContext( currentContext() );
	
	DeclarationBuilderBase::visitClass( node );
	
	closeContext();
	closeType();
	closeDeclaration();
}

void DeclarationBuilder::visitClassBodyDeclaration( ClassBodyDeclarationAst *node ){
	pLastModifiers = 0;
	DeclarationBuilderBase::visitClassBodyDeclaration( node );
}

void DeclarationBuilder::visitClassVariablesDeclare( ClassVariablesDeclareAst *node ){
	if( pPhase < 2 ){
		return;
	}
	if( ! node->variablesSequence ){
		return;
	}
	
	AbstractType::Ptr type;
	{
	DUChainReadLocker lock;
	ExpressionVisitor exprType( *editor(), currentContext(), pinnedNamespaces(), *typeFinder() );
	exprType.visitNode( node->type );
	type = exprType.lastType();
	}
	
	ClassMemberDeclaration::StorageSpecifiers storageSpecifiers;
	if( ( pLastModifiers & etmStatic ) == etmStatic ){
		storageSpecifiers |= ClassMemberDeclaration::StaticSpecifier;
	}
	if( ( pLastModifiers & etmFixed ) == etmFixed ){
		// not existing anymore
// 		storageSpecifiers |= ClassMemberDeclaration::FinalSpecifier;
	}
	
	const KDevPG::ListNode<ClassVariableDeclareAst*> *iter = node->variablesSequence->front();
	const KDevPG::ListNode<ClassVariableDeclareAst*> *end = iter;
	do{
		ClassMemberDeclaration * const decl = openDeclaration<ClassMemberDeclaration>(
			iter->element->name, iter->element->name, DeclarationFlags::NoFlags );
		decl->setType( type );
		decl->setKind( Declaration::Instance );
		decl->setComment( getDocumentationForNode( *node ) );
		decl->setAccessPolicy( accessPolicyFromLastModifiers() );
		decl->setStorageSpecifiers( storageSpecifiers );
		
		if( iter->element->value ){
			visitNode( iter->element->value );
		}
		
		closeDeclaration();
		
		iter = iter->next;
	}while( iter != end );
}

void DeclarationBuilder::visitClassFunctionDeclare( ClassFunctionDeclareAst *node ){
	if( pPhase < 2 ){
		return;
	}
	
	ClassFunctionDeclaration *decl;
	
	if( node->begin->name ){
		decl = openDeclaration<ClassFunctionDeclaration>( node->begin->name,
			node->begin->name, DeclarationFlags::NoFlags );
		
	}else if( node->begin->op ){
		decl = openDeclaration<ClassFunctionDeclaration>(
			Identifier( editor()->tokenText( *node->begin->op ) ),
			editorFindRangeNode( node->begin->op ), DeclarationFlags::NoFlags );
		
	}else{
		return;
	}
	
	decl->setKind( Declaration::Instance );
	decl->setComment( getDocumentationForNode( *node ) );
	decl->setAccessPolicy( accessPolicyFromLastModifiers() );
	
	ClassMemberDeclaration::StorageSpecifiers storageSpecifiers;
	if( ( pLastModifiers & etmNative ) == etmNative ){
		// not existing anymore. anyways a special type in dragonscript
// 		storageSpecifiers |= ClassMemberDeclaration::NativeSpecifier;
	}
	if( ( pLastModifiers & etmStatic ) == etmStatic ){
		storageSpecifiers |= ClassMemberDeclaration::StaticSpecifier;
	}
	if( ( pLastModifiers & etmFixed ) == etmFixed ){
		// not existing anymore
// 		storageSpecifiers |= ClassMemberDeclaration::FinalSpecifier;
	}
	if( ( pLastModifiers & etmAbstract ) == etmAbstract ){
		// not existing anymore
// 		storageSpecifiers |= ClassMemberDeclaration::AbstractSpecifier; // pure virtual ?
	}
	decl->setStorageSpecifiers( storageSpecifiers );
	
	decl->setFunctionSpecifiers( FunctionDeclaration::VirtualSpecifier );
	
	FunctionType::Ptr funcType( new FunctionType() );
	
	if( node->begin->type ){
		DUChainReadLocker lock;
		ExpressionVisitor exprRetType( *editor(), currentContext(), pinnedNamespaces(), *typeFinder() );
		exprRetType.setAllowVoid( true );
		exprRetType.visitNode( node->begin->type );
		funcType->setReturnType( exprRetType.lastType() );
		
	}else{
		// used only for constructors. return type is the object class type
		DUChainReadLocker lock;
		const Declaration * const classDecl = Helpers::classDeclFor( *decl->context() );
		if( classDecl ){
			funcType->setReturnType( classDecl->abstractType() );
		}
	}
	
	// this one is strange. it looks as if kdevelop wants two contexts for a function.
	// the "Function" type context has to contain all arguments while the "Other" type
	// context has to contain the function body.
	// 
	// but by doing it that way function arguments are not visible. according to other
	// plugins you have to import the function context to the body context. very complicated
	openContextClassFunction( node );
	DUContext * const functionContext = currentContext();
	decl->setInternalContext( functionContext );
	
	if( node->begin->argumentsSequence ){
		const KDevPG::ListNode<ClassFunctionDeclareArgumentAst*> *iter = node->begin->argumentsSequence->front();
		const KDevPG::ListNode<ClassFunctionDeclareArgumentAst*> *end = iter;
		do{
			AbstractType::Ptr argType;
			{
			DUChainReadLocker lock;
			ExpressionVisitor exprArgType( *editor(), currentContext(), pinnedNamespaces(), *typeFinder() );
			exprArgType.visitNode( iter->element->type );
			argType = exprArgType.lastType();
			}
			
			funcType->addArgument( argType );
			
			Declaration * const declArg = openDeclaration<Declaration>( iter->element->name,
				iter->element->name, DeclarationFlags::NoFlags );
			declArg->setAbstractType( argType );
			declArg->setKind( Declaration::Instance );
			closeDeclaration();
			
			iter = iter->next;
		}while( iter != end );
	}
	
	// assign function type to function declaration. this has to be done after setting return
	// type and arguments in function type otherwise the information will not show up in browsing
	decl->setType( funcType );
	
	// update context local scope identifier. DUChain reacts badly to multiple functions
	// sharing the same name but different signature. we can though not determine the scope
	// name until after arguments have been parsed
	const QualifiedIdentifier scopeIdentifier( decl->identifier().toString()
		+ funcType->partToString( FunctionType::SignatureArguments ) );
	{
	DUChainWriteLocker lock;
	functionContext->setLocalScopeIdentifier( scopeIdentifier );
	}
	
	// kdeveop seems to expect a "Function" type context containing the arguments and a "Other"
	// type context containing the function body. this requires împorting the function arguments
	// context into the function body context
// 	closeContext();
	
	// function body
	if( node->bodySequence ){
		const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
		const KDevPG::ListNode<StatementAst*> *end = iter;
		
// 		if( node->begin->super != -1 ){
// 			openContext( iter->element, RangeInRevision( editor()->findPosition( node->begin->super ),
// 				editor()->findPosition( *node->bodySequence->back()->element ) ),
// 				DUContext::Other, scopeIdentifier );
// 			
// 		}else{
// 			openContext( iter->element, node->bodySequence->back()->element,
// 				DUContext::Other, scopeIdentifier );
// 		}
// 		{
// 		DUChainWriteLocker lock;
// 		currentContext()->addImportedParentContext( functionContext );
// 		}
		
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
		
// 		closeContext();
	}
	
	closeContext();
	closeDeclaration();
}

void DeclarationBuilder::visitInterface( InterfaceAst *node ){
	ClassDeclaration * const decl = openDeclaration<ClassDeclaration>( node->begin->name,
		node->begin->name, DeclarationFlags::NoFlags );
	
	eventuallyAssignInternalContext();
	
	decl->setKind( Declaration::Type );
	decl->setComment( getDocumentationForNode( *node ) );
	decl->clearBaseClasses();
	decl->setClassType( ClassDeclarationData::Interface );
	decl->setClassModifier( ClassDeclarationData::Abstract );
	decl->setAccessPolicy( accessPolicyFromLastModifiers() );
	
	if( pPhase > 1 ){
		// set object as base class
		ClassDeclaration * const typeObject = typeFinder()->typeObject();
		if( typeObject ){
			BaseClassInstance base;
			base.baseClass = typeObject->abstractType()->indexed();
			base.access = Declaration::Public;
			decl->addBaseClass( base );
		}
		
		// add interfaces
		if( node->begin->implementsSequence ){
			const KDevPG::ListNode<FullyQualifiedClassnameAst*> *iter = node->begin->implementsSequence->front();
			const KDevPG::ListNode<FullyQualifiedClassnameAst*> *end = iter;
			DUChainReadLocker lock;
			do{
				ExpressionVisitor exprvisitor( *editor(), currentContext(), pinnedNamespaces(), *typeFinder() );
				exprvisitor.visitNode( iter->element );
				if( exprvisitor.lastType() && exprvisitor.lastType()->whichType() == AbstractType::TypeStructure ){
					const StructureType::Ptr baseType( exprvisitor.lastType().cast<StructureType>() );
					BaseClassInstance base;
					base.baseClass = baseType->indexed();
					base.access = Declaration::Public;
					decl->addBaseClass( base );
				}
				iter = iter->next;
			}while( iter != end );
		}
	}
	
	// body
	StructureType::Ptr type( new StructureType() );
	type->setDeclaration( decl );
	decl->setType( type );
	
	openType( type );
	
	openContextInterface( node );
	decl->setInternalContext( currentContext() );
	
	DeclarationBuilderBase::visitInterface( node );
	
	closeContext();
	closeType();
	closeDeclaration();
}

void DeclarationBuilder::visitInterfaceBodyDeclaration( InterfaceBodyDeclarationAst *node ){
	pLastModifiers = 0;
	DeclarationBuilderBase::visitInterfaceBodyDeclaration( node );
}

void DeclarationBuilder::visitInterfaceFunctionDeclare( InterfaceFunctionDeclareAst *node ){
	if( pPhase < 2 ){
		return;
	}
	if( ! node->begin->type ){
		return; // used for constructors. should never happen here
	}
	
	ClassFunctionDeclaration *decl;
	
	if( node->begin->name ){
		decl = openDeclaration<ClassFunctionDeclaration>( node->begin->name,
			node->begin->name, DeclarationFlags::NoFlags );
		
	}else if( node->begin->op ){
		decl = openDeclaration<ClassFunctionDeclaration>(
			Identifier( editor()->tokenText( *node->begin->op ) ),
			editorFindRangeNode( node->begin->op ), DeclarationFlags::NoFlags );
		
	}else{
		return;
	}
	
	decl->setKind( Declaration::Instance );
	decl->setComment( getDocumentationForNode( *node ) );
	decl->setAccessPolicy( ClassDeclaration::AccessPolicy::Public );
	decl->setFunctionSpecifiers( FunctionDeclaration::VirtualSpecifier );
	
	FunctionType::Ptr funcType( new FunctionType() );
	{
	DUChainReadLocker lock;
	ExpressionVisitor exprRetType( *editor(), currentContext(), pinnedNamespaces(), *typeFinder() );
	exprRetType.setAllowVoid( true );
	exprRetType.visitNode( node->begin->type );
	funcType->setReturnType( exprRetType.lastType() );
	}
	
	openContextInterfaceFunction( node );
	decl->setInternalContext( currentContext() );
	
	if( node->begin->argumentsSequence ){
		const KDevPG::ListNode<ClassFunctionDeclareArgumentAst*> *iter = node->begin->argumentsSequence->front();
		const KDevPG::ListNode<ClassFunctionDeclareArgumentAst*> *end = iter;
		do{
			AbstractType::Ptr argType;
			{
			DUChainReadLocker lock;
			ExpressionVisitor exprArgType( *editor(), currentContext(), pinnedNamespaces(), *typeFinder() );
			exprArgType.visitNode( iter->element->type );
			argType = exprArgType.lastType();
			}
			
			funcType->addArgument( argType );
			
			Declaration * const declArg = openDeclaration<Declaration>( iter->element->name,
				iter->element->name, DeclarationFlags::NoFlags );
			declArg->setAbstractType( argType );
			declArg->setKind( Declaration::Instance );
			closeDeclaration();
			
			iter = iter->next;
		}while( iter != end );
	}
	
	// assign function type to function declaration. this has to be done after setting return
	// type and arguments in function type otherwise the information will not show up in browsing
	decl->setType( funcType );
	
	DeclarationBuilderBase::visitInterfaceFunctionDeclare( node );
	
	closeContext();
	closeDeclaration();
}

void DeclarationBuilder::visitEnumeration( EnumerationAst *node ){
	ClassDeclaration * const decl = openDeclaration<ClassDeclaration>(
		node->begin->name, node->begin->name, DeclarationFlags::NoFlags );
	
	eventuallyAssignInternalContext();
	
	decl->setKind( Declaration::Type );
	decl->setComment( getDocumentationForNode( *node ) );
	decl->clearBaseClasses();
	decl->setClassType( ClassDeclarationData::Class );
	decl->setClassModifier( ClassDeclarationData::Final );
	decl->setAccessPolicy( accessPolicyFromLastModifiers() );
	
	// open type. we need it for generating functions
	StructureType::Ptr type( new StructureType() );
	type->setDeclaration( decl );
	decl->setType( type );
	
	openType( type );
	
	openContextEnumeration( node );
	decl->setInternalContext( currentContext() );
	
	// base class. this is only done if this is not the enumeration class from
	// the documentation being parsed
	if( pPhase > 1 && document() != Helpers::getDocumentationFileEnumeration() ){
		ClassDeclaration * const typeEnum = typeFinder()->typeEnumeration();
		if( typeEnum ){
			BaseClassInstance base;
			base.baseClass = typeEnum->abstractType()->indexed();
			base.access = Declaration::Public;
			decl->addBaseClass( base );
			
			// enumeration class is special in that a bunch of generated functions
			// are added matching the class type
			const QVector<Declaration*> sourceDecl( typeEnum->internalContext()->localDeclarations() );
			static const IndexedIdentifier identWithOrder( (Identifier( "withOrder" )) );
			static const IndexedIdentifier identNamed( (Identifier( "named" )) );
			
			foreach( Declaration *each, sourceDecl ){
				const ClassFunctionDeclaration * const funcDecl = dynamic_cast<ClassFunctionDeclaration*>( each );
				if( ! funcDecl || ( funcDecl->indexedIdentifier() != identNamed
				&&  funcDecl->indexedIdentifier() != identWithOrder ) ){
					continue;
				}
				
				ClassFunctionDeclaration * copyDecl = openDeclaration<ClassFunctionDeclaration>(
					funcDecl->identifier(), RangeInRevision::invalid() );
				copyDecl->setKind( funcDecl->kind() );
				copyDecl->setComment( funcDecl->comment() );
				copyDecl->setAccessPolicy( funcDecl->accessPolicy() );
				copyDecl->setStorageSpecifiers( ClassMemberDeclaration::StaticSpecifier );
				copyDecl->setFunctionSpecifiers( FunctionDeclaration::VirtualSpecifier );
				
				FunctionType::Ptr copyFuncType( dynamic_cast<FunctionType*>( funcDecl->type<FunctionType>()->clone() ) );
				copyFuncType->setReturnType( type );
				copyDecl->setType( AbstractType::Ptr( copyFuncType ) );
				
				closeDeclaration();
			}
		}
	}
	
	// body
	if( pPhase > 1 ){
		DeclarationBuilderBase::visitEnumeration( node );
	}
	
	closeContext();
	closeType();
	closeDeclaration();
}

void DeclarationBuilder::visitEnumerationEntry( EnumerationEntryAst *node ){
	// NOTE called only if pPhase > 1
	
	ClassMemberDeclaration * const decl = openDeclaration<ClassMemberDeclaration>(
		node->name, node->name, DeclarationFlags::NoFlags );
	
	decl->setKind( Declaration::Instance );
	decl->setComment( getDocumentationForNode( *node ) );
	decl->setAccessPolicy( Declaration::Public );
	decl->setStorageSpecifiers( ClassMemberDeclaration::StaticSpecifier );
	
	// type is the enumeration class
	{
	DUChainReadLocker lock;
	const Declaration * const enumDecl = Helpers::classDeclFor( *decl->context() );
	if( enumDecl ){
		decl->setType( enumDecl->abstractType() );
	}
	}
	
	if( node->value ){
		visitNode( node->value );
	}
	
	closeDeclaration();
}

void DeclarationBuilder::visitExpressionBlock( ExpressionBlockAst *node ){
	// NOTE called only if pPhase > 1
	
	openContext( node, DUContext::Other ); //, QualifiedIdentifier( "{block}" ) );
	
	if( node->begin->argumentsSequence ){
		const KDevPG::ListNode<ExpressionBlockArgumentAst*> *iter = node->begin->argumentsSequence->front();
		const KDevPG::ListNode<ExpressionBlockArgumentAst*> *end = iter;
		AbstractType::Ptr argType;
		do{
			{
			DUChainReadLocker lock;
			ExpressionVisitor exprArgType( *editor(), currentContext(), pinnedNamespaces(), *typeFinder() );
			exprArgType.visitNode( iter->element->type );
			argType = exprArgType.lastType();
			}
			
			Declaration * const declArg = openDeclaration<Declaration>( iter->element->name,
				iter->element->name, DeclarationFlags::NoFlags );
			declArg->setAbstractType( argType );
			declArg->setKind( Declaration::Instance );
			closeDeclaration();
			
			iter = iter->next;
		}while( iter != end );
	}
	
	DefaultVisitor::visitExpressionBlock( node );
	
	closeContext();
}

void DeclarationBuilder::visitStatementIf( StatementIfAst *node ){
	// NOTE called only if pPhase > 1
	
	// if
	visitNode( node->condition );
	
	if( node->bodySequence ){
		const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
		const KDevPG::ListNode<StatementAst*> *end = iter;
		
		openContext( iter->element, node->bodySequence->back()->element, DUContext::Other );
			//, QualifiedIdentifier( "{if}" ) );
		
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
		
		closeContext();
	}
	
	// elif
	if( node->elifSequence ){
		const KDevPG::ListNode<StatementElifAst*> *iter = node->elifSequence->front();
		const KDevPG::ListNode<StatementElifAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
	}
	
	// else
	if( ! node->elseSequence ){
		return;
	}
	
	const KDevPG::ListNode<StatementAst*> *iter = node->elseSequence->front();
	const KDevPG::ListNode<StatementAst*> *end = iter;
	
	openContext( iter->element, node->elseSequence->back()->element, DUContext::Other );
		//, QualifiedIdentifier( "{else}" ) );
	
	do{
		visitNode( iter->element );
		iter = iter->next;
	}while( iter != end );
	
	closeContext();
}

void DeclarationBuilder::visitStatementElif( StatementElifAst *node ){
	// NOTE called only if pPhase > 1
	
	visitNode( node->condition );
	
	if( ! node->bodySequence ){
		return;
	}
	
	const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
	const KDevPG::ListNode<StatementAst*> *end = iter;
	
	openContext( iter->element, node->bodySequence->back()->element, DUContext::Other );
		//, QualifiedIdentifier( "{elif}" ) );
	
	do{
		visitNode( iter->element );
		iter = iter->next;
	}while( iter != end );
	
	closeContext();
}

void DeclarationBuilder::visitStatementSelect( StatementSelectAst *node ){
	// NOTE called only if pPhase > 1
	
	visitNode( node->value );
	
	// cases
	if( node->caseSequence ){
		const KDevPG::ListNode<StatementCaseAst*> *iter = node->caseSequence->front();
		const KDevPG::ListNode<StatementCaseAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
	}
	
	// else
	if( ! node->elseSequence ){
		return;
	}
	
	const KDevPG::ListNode<StatementAst*> *iter = node->elseSequence->front();
	const KDevPG::ListNode<StatementAst*> *end = iter;
	
	openContext( iter->element, node->elseSequence->back()->element, DUContext::Other );
		//, QualifiedIdentifier( "{switch-else}" ) );
	
	do{
		visitNode( iter->element );
		iter = iter->next;
	}while( iter != end );
	
	closeContext();
}

void DeclarationBuilder::visitStatementCase( StatementCaseAst *node ){
	// NOTE called only if pPhase > 1
	
	if( node->matchesSequence ){
		const KDevPG::ListNode<ExpressionAst*> *iter = node->matchesSequence->front();
		const KDevPG::ListNode<ExpressionAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
	}
	
	if( ! node->bodySequence ){
		return;
	}
	
	const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
	const KDevPG::ListNode<StatementAst*> *end = iter;
	
	openContext( iter->element, node->bodySequence->back()->element, DUContext::Other );
		//, QualifiedIdentifier( "{case}" ) );
	
	do{
		visitNode( iter->element );
		iter = iter->next;
	}while( iter != end );
	
	closeContext();
}

void DeclarationBuilder::visitStatementFor( StatementForAst *node ){
	// NOTE called only if pPhase > 1
	
	visitNode( node->variable );
	visitNode( node->from );
	visitNode( node->to );
	visitNode( node->downto );
	visitNode( node->step );
	
	if( ! node->bodySequence ){
		return;
	}
	
	const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
	const KDevPG::ListNode<StatementAst*> *end = iter;
	
	openContext( iter->element, node->bodySequence->back()->element, DUContext::Other );
		//, QualifiedIdentifier( "{for}" ) );
	
	do{
		visitNode( iter->element );
		iter = iter->next;
	}while( iter != end );
	
	closeContext();
}

void DeclarationBuilder::visitStatementWhile( StatementWhileAst *node ){
	// NOTE called only if pPhase > 1
	
	visitNode( node->condition );
	
	if( ! node->bodySequence ){
		return;
	}
	
	const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
	const KDevPG::ListNode<StatementAst*> *end = iter;
	
	openContext( iter->element, node->bodySequence->back()->element, DUContext::Other );
		//, QualifiedIdentifier( "{while}" ) );
	
	do{
		visitNode( iter->element );
		iter = iter->next;
	}while( iter != end );
	
	closeContext();
}

void DeclarationBuilder::visitStatementTry( StatementTryAst *node ){
	// NOTE called only if pPhase > 1
	
	if( node->bodySequence ){
		const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
		const KDevPG::ListNode<StatementAst*> *end = iter;
		
		openContext( iter->element, node->bodySequence->back()->element, DUContext::Other );
			//, QualifiedIdentifier( "{try}" ) );
		
		iter = node->bodySequence->front();
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
		
		closeContext();
	}
	
	if( node->catchesSequence ){
		const KDevPG::ListNode<StatementCatchAst*> *iter = node->catchesSequence->front();
		const KDevPG::ListNode<StatementCatchAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
	}
}

void DeclarationBuilder::visitStatementCatch( StatementCatchAst *node ){
	// NOTE called only if pPhase > 1
	
	openContext( node, DUContext::Other ); //, QualifiedIdentifier( "{catch}" ) );
	
	if( node->variable ){
		AbstractType::Ptr type;
		{
		DUChainReadLocker lock;
		ExpressionVisitor exprType( *editor(), currentContext(), pinnedNamespaces(), *typeFinder() );
		exprType.visitNode( node->type );
		type = exprType.lastType();
		}
		
		Declaration * const decl = openDeclaration<Declaration>(
			node->variable, node->variable, DeclarationFlags::NoFlags );
		decl->setType( type );
		decl->setKind( Declaration::Instance );
		
		closeDeclaration();
	}
	
	if( node->bodySequence ){
		const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
		const KDevPG::ListNode<StatementAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
	}
	
	closeContext();
}

void DeclarationBuilder::visitStatementVariableDefinitions( StatementVariableDefinitionsAst *node ){
	// NOTE called only if pPhase > 1
	
	if( ! node->variablesSequence ){
		return;
	}
	
	AbstractType::Ptr type;
	{
	DUChainReadLocker lock;
	ExpressionVisitor exprType( *editor(), currentContext(), pinnedNamespaces(), *typeFinder() );
	exprType.visitNode( node->type );
	type = exprType.lastType();
	}
	
	const KDevPG::ListNode<StatementVariableDefinitionAst*> *iter = node->variablesSequence->front();
	const KDevPG::ListNode<StatementVariableDefinitionAst*> *end = iter;
	do{
		Declaration * const decl = openDeclaration<Declaration>(
			iter->element->name, iter->element->name, DeclarationFlags::NoFlags );
		decl->setType( type );
		decl->setKind( Declaration::Instance );
		
		if( iter->element->value ){
			visitNode( iter->element->value );
		}
		
		closeDeclaration();
		
		iter = iter->next;
	}while( iter != end );
}

void DeclarationBuilder::visitTypeModifier( TypeModifierAst *node ){
	pLastModifiers |= node->modifiers;
}



// Protected Functions
////////////////////////

ClassMemberDeclaration::AccessPolicy DeclarationBuilder::accessPolicyFromLastModifiers() const{
	if( ( pLastModifiers & etmPrivate ) == etmPrivate ){
		return ClassMemberDeclaration::Private;
		
	}else if( ( pLastModifiers & etmProtected ) == etmProtected ){
		return ClassMemberDeclaration::Protected;
		
	}else{
		return ClassMemberDeclaration::Public;
	}
}

}
