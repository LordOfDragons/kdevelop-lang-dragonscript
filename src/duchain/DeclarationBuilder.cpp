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
#include "../parser/ParseSession.h"


using namespace KDevelop;

namespace DragonScript{

DeclarationBuilder::DeclarationBuilder( EditorIntegrator &editor,
	const ParseSession &parseSession, const QSet<ImportPackage::Ref> &deps,
	const QVector<ReferencedTopDUContext> &ncs, int phase ) :
DeclarationBuilderBase(),
pParseSession( parseSession ),
pNamespaceContextCount( 0 ),
pPhase( phase )
{
	setDependencies( deps );
	setEditor( &editor );
	foreach( const ReferencedTopDUContext &each, ncs ){
		reachableContexts() << each.data();
	}
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
	const KDevPG::ListNode<IdentifierAst*> *end = iter;
	QualifiedIdentifier identifier;
	
	do{
		identifier += Identifier( editor()->tokenText( *iter->element ) );
		iter = iter->next;
	}while( iter != end );
	
	const RangeInRevision range( editorFindRangeNode( node->name ) );
	NamespaceAliasDeclaration * const decl = openDeclaration<NamespaceAliasDeclaration>(
		globalImportIdentifier(), range, DeclarationFlags::NoFlags );
	eventuallyAssignInternalContext();
	decl->setKind( Declaration::NamespaceAlias );
	decl->setImportIdentifier( identifier );
	openContext( iter->element, range, DUContext::Namespace, iter->element );
	
	{
	DUChainWriteLocker lock;
	currentContext()->setLocalScopeIdentifier( identifier );
	}
	decl->setInternalContext( currentContext() );
	closeContext();
	
	// TODO search for reachable contexts through this pinned namespace and add it to reachableContexts()
	
#if 0
	do{
		NamespaceAliasDeclaration * const decl = openDeclaration<NamespaceAliasDeclaration>(
			iter->element, iter->element, DeclarationFlags::NoFlags );
		eventuallyAssignInternalContext();
		decl->setKind( Declaration::NamespaceAlias );
		decl->setComment( getDocumentationForNode( *node ) );
		openContext( iter->element, editorFindRangeNode( iter->element ), DUContext::Namespace, iter->element );
		currentContext()->setLocalScopeIdentifier( identifierForNode( iter->element ) );
		decl->setInternalContext( currentContext() );
		closeCount++;
	}while( iter != end );
	
	while( closeCount-- > 0 ){
		closeContext();
	}
#endif
	
#if 0
	DUChainReadLocker lock;
	PinNamespaceVisitor pinvisitor( *editor(), currentContext() );
	if( ! node->name ){
		return;
	}
	
	pinvisitor.visitFullyQualifiedClassname( node->name );
	
	const QVector<DUChainPointer<const DUContext>> &namespaces = pinvisitor.namespaces();
	foreach( const DUChainPointer<const DUContext> &each, namespaces ){
//		qDebug() << "KDevDScript: DeclarationBuilder::visitPin add" << each.operator->();
		pPinned.append( each );
	}
#endif
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
		ClassDeclaration *decl = openDeclaration<ClassDeclaration>(
			iter->element, iter->element, DeclarationFlags::NoFlags );
		
		eventuallyAssignInternalContext();
		
		decl->setKind( Declaration::Namespace );
		decl->setComment( getDocumentationForNode( *node ) );
		
		StructureType::Ptr type( new StructureType() );
		type->setDeclaration( decl );
		decl->setType( type );
		
		openType( type );
		
		openContext( iter->element, editorFindRangeNode( iter->element ), DUContext::Namespace, iter->element );
		
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
			ExpressionVisitor exprvisitor( *editor(), currentContext(), reachableContexts() );
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
			DUChainReadLocker lock;
			Declaration * const typeDecl = Helpers::getInternalTypeDeclaration(
				*topContext(), Helpers::getTypeObject(), reachableContexts() );
			if( typeDecl && typeDecl->abstractType() ){
				BaseClassInstance base;
				base.baseClass = typeDecl->abstractType()->indexed();
				base.access = Declaration::Public;
				decl->addBaseClass( base );
			}
		}
		
		if( node->begin->implementsSequence ){
			const KDevPG::ListNode<FullyQualifiedClassnameAst*> *iter = node->begin->implementsSequence->front();
			const KDevPG::ListNode<FullyQualifiedClassnameAst*> *end = iter;
			DUChainReadLocker lock;
			do{
				ExpressionVisitor exprvisitor( *editor(), currentContext(), reachableContexts() );
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
	ExpressionVisitor exprType( *editor(), currentContext(), reachableContexts() );
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
	decl->setStatic( false );  // TODO check type modifiers
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
	
	FunctionDeclaration::FunctionSpecifiers functionSpecifiers( FunctionDeclaration::VirtualSpecifier );
	decl->setFunctionSpecifiers( functionSpecifiers );
	
	FunctionType::Ptr funcType( new FunctionType() );
	
	if( node->begin->type ){
		DUChainReadLocker lock;
		ExpressionVisitor exprRetType( *editor(), currentContext(), reachableContexts() );
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
	
	openContextClassFunction( node );
	decl->setInternalContext( currentContext() );
	
	if( node->begin->argumentsSequence ){
		const KDevPG::ListNode<ClassFunctionDeclareArgumentAst*> *iter = node->begin->argumentsSequence->front();
		const KDevPG::ListNode<ClassFunctionDeclareArgumentAst*> *end = iter;
		do{
			AbstractType::Ptr argType;
			{
			DUChainReadLocker lock;
			ExpressionVisitor exprArgType( *editor(), currentContext(), reachableContexts() );
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
	
	DeclarationBuilderBase::visitClassFunctionDeclare( node );
	
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
		{
		DUChainReadLocker lock;
		Declaration * const typeDecl = Helpers::getInternalTypeDeclaration(
			*topContext(), Helpers::getTypeObject(), reachableContexts() );
		if( typeDecl && typeDecl->abstractType() ){
			BaseClassInstance base;
			base.baseClass = typeDecl->abstractType()->indexed();
			base.access = Declaration::Public;
			decl->addBaseClass( base );
		}
		}
		
		// add interfaces
		if( node->begin->implementsSequence ){
			const KDevPG::ListNode<FullyQualifiedClassnameAst*> *iter = node->begin->implementsSequence->front();
			const KDevPG::ListNode<FullyQualifiedClassnameAst*> *end = iter;
			DUChainReadLocker lock;
			do{
				ExpressionVisitor exprvisitor( *editor(), currentContext(), reachableContexts() );
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
	ExpressionVisitor exprRetType( *editor(), currentContext(), reachableContexts() );
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
			ExpressionVisitor exprArgType( *editor(), currentContext(), reachableContexts() );
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
	
	// base class. this is only done if this is not the enumeration class from
	// the documentation being parsed
	if( pPhase > 1 ){
		DUChainReadLocker lock;
		Declaration * const typeDecl = Helpers::getInternalTypeDeclaration(
			*topContext(), Helpers::getTypeEnumeration(), reachableContexts() );
		if( typeDecl && typeDecl->abstractType() ){
			BaseClassInstance base;
			base.baseClass = typeDecl->abstractType()->indexed();
			base.access = Declaration::Public;
			decl->addBaseClass( base );
		}
	}
	
	// body
	StructureType::Ptr type( new StructureType() );
	type->setDeclaration( decl );
	decl->setType( type );
	
	openType( type );
	
	openContextEnumeration( node );
	decl->setInternalContext( currentContext() );
	
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
			ExpressionVisitor exprArgType( *editor(), currentContext(), reachableContexts() );
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
		ExpressionVisitor exprType( *editor(), currentContext(), reachableContexts() );
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
	ExpressionVisitor exprType( *editor(), currentContext(), reachableContexts() );
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
