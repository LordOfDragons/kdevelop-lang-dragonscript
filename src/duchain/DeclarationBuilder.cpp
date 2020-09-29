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

DeclarationBuilder::DeclarationBuilder( EditorIntegrator &editor, int ownPriority,
	const ParseSession &parseSession, const QVector<ImportPackage::Ref> &deps ) :
DeclarationBuilderBase(),
pParseSession( parseSession )
{
	Q_UNUSED( ownPriority );
	
	setDependencies( deps );
	setEditor( &editor );
}

DeclarationBuilder::~DeclarationBuilder(){
	/*
	if ( ! m_scheduledForDeletion.isEmpty() ){
		DUChainWriteLocker lock;
		foreach( DUChainBase *d, m_scheduledForDeletion ){
			delete d;
		}
		m_scheduledForDeletion.clear();
	}
	*/
}

// ReferencedTopDUContext DeclarationBuilder::build( const IndexedString &url,
// AstNode *node, ReferencedTopDUContext updateContext ){
// 	qDebug() << "KDevDScript: DeclarationBuilder::build";
// 	pPinned.clear();
// 	return DeclarationBuilderBase::build( url, node, updateContext );
// }

void DeclarationBuilder::closeNamespaceContexts(){
// 	qDebug() << "KDevDScript: DeclarationBuilder::closeNamespaceContexts" << pNamespaceContexts.size();
	// for each namespace component add a context
	while( ! pNamespaceContexts.isEmpty() ){
		if( pNamespaceContexts.last() ){
			closeContext();
			//m_currentClassTypes.removeLast();
			closeType();
		}
		closeDeclaration();
		pNamespaceContexts.removeLast();
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
	if( ! node->name->nameSequence ){
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
	currentContext()->setLocalScopeIdentifier( identifier );
	decl->setInternalContext( currentContext() );
	closeContext();
	
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
		StructureType::Ptr type( new StructureType() );
		
		ClassDeclaration *decl = nullptr;
		bool newDecl = false;
		
		if( ! decl ){
			decl = openDeclaration<ClassDeclaration>( iter->element, iter->element, DeclarationFlags::NoFlags );
			newDecl = true;
			
			eventuallyAssignInternalContext();
			
			decl->setKind( Declaration::Namespace );
			decl->setComment( getDocumentationForNode( *node ) );
			
			type->setDeclaration( decl );
			decl->setType( type );
			
			openType( type );
			
			openContext( iter->element, editorFindRangeNode( iter->element ), DUContext::Namespace, iter->element );
			
			currentContext()->setLocalScopeIdentifier( identifierForNode( iter->element ) );
			decl->setInternalContext( currentContext() );
		}
		
		iter = iter->next;
// 		qDebug() << "KDevDScript: DeclarationBuilder::visitNamespace push back" << newDecl;
		pNamespaceContexts.push_back( newDecl );
		
	}while( iter != end );
}

void DeclarationBuilder::visitClass( ClassAst *node ){
// 	qDebug() << "KDevDScript: DeclarationBuilder::visitClass";
	
	StructureType::Ptr type( new StructureType() );
	
	ClassDeclaration * const decl = openDeclaration<ClassDeclaration>( node->begin->name,
		node->begin->name, DeclarationFlags::NoFlags );
// 	decl->setAlwaysForceDirect( true ); // nobody seems to be using this?
	
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
	if( node->begin->extends ){
		DUChainReadLocker lock;
		ExpressionVisitor exprvisitor( *editor(), currentContext() );
		exprvisitor.visitNode( node->begin->extends );
		
		if( exprvisitor.lastType()
		&& exprvisitor.lastType()->whichType() == AbstractType::TypeStructure ){
			const StructureType::Ptr baseType = exprvisitor.lastType().cast<StructureType>();
			BaseClassInstance base;
			base.baseClass = baseType->indexed();
			base.access = Declaration::Public;
			decl->addBaseClass( base );
		}
		
	}else if( document() != Helpers::getDocumentationFileObject() ){
		// set object as base class. this is only done if this is not the object class from
		// the documentation being parsed
		const AbstractType::Ptr typeObject = Helpers::getTypeObject();
		if( typeObject ){
			BaseClassInstance base;
			base.baseClass = typeObject->indexed();
			base.access = Declaration::Public;
			decl->addBaseClass( base );
		}
	}
	
	if( node->begin->implementsSequence ){
		const KDevPG::ListNode<FullyQualifiedClassnameAst*> *iter = node->begin->implementsSequence->front();
		const KDevPG::ListNode<FullyQualifiedClassnameAst*> *end = iter;
		DUChainReadLocker lock;
		do{
			ExpressionVisitor exprvisitor( *editor(), currentContext() );
			exprvisitor.visitNode( iter->element );
			if( exprvisitor.lastType()
			&& exprvisitor.lastType()->whichType() == AbstractType::TypeStructure ){
				StructureType::Ptr baseType = exprvisitor.lastType().cast<StructureType>();
				BaseClassInstance base;
				base.baseClass = baseType->indexed();
				base.access = Declaration::Public;
				decl->addBaseClass( base );
			}
			iter = iter->next;
		}while( iter != end );
	}
	
	// continue
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
	if( ! node->variablesSequence ){
		return;
	}
	
	DUChainReadLocker lock;
	ExpressionVisitor exprType( *editor(), currentContext() );
	exprType.visitNode( node->type );
	AbstractType::Ptr type( exprType.lastType() );
	lock.unlock();
	
	ClassMemberDeclaration::StorageSpecifiers storageSpecifiers;
	if( ( pLastModifiers & etmStatic ) == etmStatic ){
		storageSpecifiers |= ClassMemberDeclaration::StaticSpecifier;
	}
	if( ( pLastModifiers & etmFixed ) == etmFixed ){
		// NOTE existing in an older version of KDevelop but now gone... damn it!
// 		storageSpecifiers |= ClassMemberDeclaration::FinalSpecifier;
	}
	
	const KDevPG::ListNode<ClassVariableDeclareAst*> *iter = node->variablesSequence->front();
	const KDevPG::ListNode<ClassVariableDeclareAst*> *end = iter;
	do{
		ClassMemberDeclaration * const decl = openDeclaration<ClassMemberDeclaration>(
			iter->element->name, iter->element->name, DeclarationFlags::NoFlags );
// 		decl->setAlwaysForceDirect( true ); // nobody seems to be using this?
		decl->setType( type );
		decl->setKind( Declaration::Instance );
		decl->setComment( getDocumentationForNode( *node ) );
		decl->setAccessPolicy( accessPolicyFromLastModifiers() );
		decl->setStorageSpecifiers( storageSpecifiers );
		
		if( iter->element->value ){
			lock.unlock();
			visitNode( iter->element->value );
			lock.lock();
		}
		
		closeDeclaration();
		
		iter = iter->next;
	}while( iter != end );
}

void DeclarationBuilder::visitClassFunctionDeclare( ClassFunctionDeclareAst *node ){
	ClassFunctionDeclaration *decl;
	
	if( node->begin->name ){
		decl = openDeclaration<ClassFunctionDeclaration>( node->begin->name,
			node->begin->name, DeclarationFlags::NoFlags );
		
	}else if( node->begin->op ){
		// WARNING according to documentation of openDeclaration (abstracttypebuilder.h:68)
		//         if name is nullptr range is used. unfortunately this method calls
		//         identifierForNode with name which is nullptr. we could change our
		//         implementation of this method to deal with this but with a nullptr we can
		//         not return the correct qualified identifier. thus we resort to the
		//         Identifier version since the QualifiedIdentifier version seems deprecated
		
		// decl = openDeclaration<ClassFunctionDeclaration>(
		//	nullptr, node->begin->op, DeclarationFlags::NoFlags );
		
		// this is now a huge problem. on some systems the function expects QualifiedIdentifier
		// as argument while on others this is deprecated and Identifier is required. We stick
		// for the time being with the QualifiedIdentifier version until this mess is solved
		/*decl = openDeclaration<ClassFunctionDeclaration>(
			Identifier( editor()->tokenText( *node->begin->op ) ),
			editorFindRangeNode( node->begin->op ), DeclarationFlags::NoFlags );*/
		
		decl = openDeclaration<ClassFunctionDeclaration>(
			Identifier( editor()->tokenText( *node->begin->op ) ),
			editorFindRangeNode( node->begin->op ), DeclarationFlags::NoFlags );
		
	}else{
		return;
	}
	
// 	decl->setAlwaysForceDirect( true ); // nobody seems to be using this?
	decl->setKind( Declaration::Instance );
	decl->setStatic( false );  // TODO check type modifiers
	decl->setComment( getDocumentationForNode( *node ) );
	decl->setAccessPolicy( accessPolicyFromLastModifiers() );
	
	ClassMemberDeclaration::StorageSpecifiers storageSpecifiers;
	//storageSpecifiers |= ClassMemberDeclaration::AbstractSpecifier; // virtual?
	if( ( pLastModifiers & etmNative ) == etmNative ){
		// NOTE existing in an older version of KDevelop but now gone... damn it!
// 		storageSpecifiers |= ClassMemberDeclaration::NativeSpecifier;
	}
	if( ( pLastModifiers & etmStatic ) == etmStatic ){
		storageSpecifiers |= ClassMemberDeclaration::StaticSpecifier;
	}
	if( ( pLastModifiers & etmFixed ) == etmFixed ){
		// NOTE existing in an older version of KDevelop but now gone... damn it!
// 		storageSpecifiers |= ClassMemberDeclaration::FinalSpecifier;
	}
	if( ( pLastModifiers & etmAbstract ) == etmAbstract ){
		// NOTE existing in an older version of KDevelop but now gone... damn it!
// 		storageSpecifiers |= ClassMemberDeclaration::AbstractSpecifier; // pure virtual ?
	}
	decl->setStorageSpecifiers( storageSpecifiers );
	
	FunctionDeclaration::FunctionSpecifiers functionSpecifiers;
	functionSpecifiers |= FunctionDeclaration::VirtualSpecifier;
	decl->setFunctionSpecifiers( functionSpecifiers );
	
	FunctionType::Ptr funcType( new FunctionType() );
	
	if( node->begin->type ){
		DUChainReadLocker lock;
		ExpressionVisitor exprRetType( *editor(), currentContext() );
		exprRetType.setAllowVoid( true );
		exprRetType.visitNode( node->begin->type );
		funcType->setReturnType( exprRetType.lastType() );
		
	}else{
		// used only for constructors. return type is the object class type
		const Declaration * const classDecl = Helpers::classDeclFor(
			DUChainPointer<const DUContext>( decl->context() ) );
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
			DUChainReadLocker lock;
			ExpressionVisitor exprArgType( *editor(), currentContext() );
			exprArgType.visitNode( iter->element->type );
			AbstractType::Ptr argType( exprArgType.lastType() );
			lock.unlock();
			
			funcType->addArgument( argType );
			
			Declaration * const declArg = openDeclaration<Declaration>( iter->element->name,
				iter->element->name, DeclarationFlags::NoFlags );
// 			declArg->setAlwaysForceDirect( true ); // nobody seems to be using this?
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
// 	qDebug() << "KDevDScript: DeclarationBuilder::visitInterface";
	
	StructureType::Ptr type( new StructureType() );
	
	ClassDeclaration * const decl = openDeclaration<ClassDeclaration>( node->begin->name,
		node->begin->name, DeclarationFlags::NoFlags );
// 	decl->setAlwaysForceDirect( true ); // nobody seems to be using this?
	
	eventuallyAssignInternalContext();
	
	decl->setKind( Declaration::Type );
	decl->setComment( getDocumentationForNode( *node ) );
	decl->clearBaseClasses();
	decl->setClassType( ClassDeclarationData::Interface );
	decl->setClassModifier( ClassDeclarationData::Abstract );
	decl->setAccessPolicy( accessPolicyFromLastModifiers() );
	
	// set object as base class
	const AbstractType::Ptr typeObject = Helpers::getTypeObject();
	if( typeObject ){
		BaseClassInstance base;
		base.baseClass = typeObject->indexed();
		base.access = Declaration::Public;
		decl->addBaseClass( base );
	}
	
	// add interfaces
	if( node->begin->implementsSequence ){
		const KDevPG::ListNode<FullyQualifiedClassnameAst*> *iter = node->begin->implementsSequence->front();
		const KDevPG::ListNode<FullyQualifiedClassnameAst*> *end = iter;
		DUChainReadLocker lock;
		do{
			ExpressionVisitor exprvisitor( *editor(), currentContext() );
			exprvisitor.visitNode( iter->element );
			
			if( exprvisitor.lastType()
			&& exprvisitor.lastType()->whichType() == AbstractType::TypeStructure ){
				const StructureType::Ptr baseType = exprvisitor.lastType().cast<StructureType>();
				BaseClassInstance base;
				base.baseClass = baseType->indexed();
				base.access = Declaration::Public;
				decl->addBaseClass( base );
			}
			iter = iter->next;
		}while( iter != end );
	}
	
	// body
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
	ExpressionVisitor exprRetType( *editor(), currentContext() );
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
			DUChainReadLocker lock;
			ExpressionVisitor exprArgType( *editor(), currentContext() );
			exprArgType.visitNode( iter->element->type );
			AbstractType::Ptr argType( exprArgType.lastType() );
			lock.unlock();
			
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
// 	qDebug() << "KDevDScript: DeclarationBuilder::visitEnumeration";
	
	StructureType::Ptr type( new StructureType() );
	
	ClassDeclaration * const decl = openDeclaration<ClassDeclaration>(
		node->begin->name, node->begin->name, DeclarationFlags::NoFlags );
// 	decl->setAlwaysForceDirect( true ); // nobody seems to be using this?
	
	eventuallyAssignInternalContext();
	
	decl->setKind( Declaration::Type );
	decl->setComment( getDocumentationForNode( *node ) );
	decl->clearBaseClasses();
	decl->setClassType( ClassDeclarationData::Class );
	decl->setClassModifier( ClassDeclarationData::Final );
	decl->setAccessPolicy( accessPolicyFromLastModifiers() );
	
	// base class. this is only done if this is not the enumeration class from
	// the documentation being parsed
	const AbstractType::Ptr typeEnumeration = Helpers::getTypeEnumeration();
	if( typeEnumeration ){
		BaseClassInstance base;
		base.baseClass = typeEnumeration->indexed();
		base.access = Declaration::Public;
		decl->addBaseClass( base );
	}
	
	// body
	type->setDeclaration( decl );
	decl->setType( type );
	
	openType( type );
	
	openContextEnumeration( node );
	decl->setInternalContext( currentContext() );
	
	DeclarationBuilderBase::visitEnumeration( node );
	
	closeContext();
	closeType();
	closeDeclaration();
}

void DeclarationBuilder::visitEnumerationBody( EnumerationBodyAst *node ){
	ClassMemberDeclaration * const decl = openDeclaration<ClassMemberDeclaration>(
		node->name, node->name, DeclarationFlags::NoFlags );
// 	decl->setAlwaysForceDirect( true ); // nobody seems to be using this?
	decl->setKind( Declaration::Instance );
	decl->setComment( getDocumentationForNode( *node ) );
	decl->setAccessPolicy( Declaration::Public );
	decl->setStorageSpecifiers( ClassMemberDeclaration::StaticSpecifier );
	
	// type is the enumeration class
	const Declaration * const enumDecl = Helpers::classDeclFor( DUChainPointer<const DUContext>( decl->context() ) );
	if( enumDecl ){
		decl->setType( enumDecl->abstractType() );
	}
	
	if( node->value ){
		visitNode( node->value );
	}
	
	closeDeclaration();
}

void DeclarationBuilder::visitExpressionBlock( ExpressionBlockAst *node ){
	openContext( node, DUContext::Other, QualifiedIdentifier( "{block}" ) );
	
	if( node->begin->argumentsSequence ){
		const KDevPG::ListNode<ExpressionBlockArgumentAst*> *iter = node->begin->argumentsSequence->front();
		const KDevPG::ListNode<ExpressionBlockArgumentAst*> *end = iter;
		do{
			DUChainReadLocker lock;
			ExpressionVisitor exprArgType( *editor(), currentContext() );
			exprArgType.visitNode( iter->element->type );
			const AbstractType::Ptr argType( exprArgType.lastType() );
			lock.unlock();
			
			Declaration * const declArg = openDeclaration<Declaration>( iter->element->name,
				iter->element->name, DeclarationFlags::NoFlags );
// 			declArg->setAlwaysForceDirect( true ); // nobody seems to be using this?
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
	// if
	visitNode( node->condition );
	
	if( node->bodySequence ){
		openContext( node, DUContext::Other, QualifiedIdentifier( "{if}" ) );
		
		const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
		const KDevPG::ListNode<StatementAst*> *end = iter;
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
	if( node->elseSequence ){
		openContext( node, DUContext::Other, QualifiedIdentifier( "{else}" ) );
		
		const KDevPG::ListNode<StatementAst*> *iter = node->elseSequence->front();
		const KDevPG::ListNode<StatementAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
		
		closeContext();
	}
}

void DeclarationBuilder::visitStatementElif( StatementElifAst *node ){
	visitNode( node->condition );
	
	if( node->bodySequence ){
		openContext( node, DUContext::Other, QualifiedIdentifier( "{elif}" ) );
		
		const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
		const KDevPG::ListNode<StatementAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
		
		closeContext();
	}
}

void DeclarationBuilder::visitStatementSelect( StatementSelectAst *node ){
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
	if( node->elseSequence ){
		openContext( node, DUContext::Other, QualifiedIdentifier( "{else}" ) );
		
		const KDevPG::ListNode<StatementAst*> *iter = node->elseSequence->front();
		const KDevPG::ListNode<StatementAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
		
		closeContext();
	}
}

void DeclarationBuilder::visitStatementCase( StatementCaseAst *node ){
	if( node->matchesSequence ){
		const KDevPG::ListNode<ExpressionAst*> *iter = node->matchesSequence->front();
		const KDevPG::ListNode<ExpressionAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
	}
	
	if( node->bodySequence ){
		openContext( node, DUContext::Other, QualifiedIdentifier( "{case}" ) );
		
		const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
		const KDevPG::ListNode<StatementAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
		
		closeContext();
	}
}

void DeclarationBuilder::visitStatementFor( StatementForAst *node ){
	visitNode( node->variable );
	visitNode( node->from );
	visitNode( node->to );
	visitNode( node->downto );
	visitNode( node->step );
	
	if( node->bodySequence ){
		openContext( node, DUContext::Other, QualifiedIdentifier( "{for}" ) );
		
		const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
		const KDevPG::ListNode<StatementAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
		
		closeContext();
	}
}

void DeclarationBuilder::visitStatementWhile( StatementWhileAst *node ){
	visitNode( node->condition );
	
	if( node->bodySequence ){
		openContext( node, DUContext::Other, QualifiedIdentifier( "{while}" ) );
		
		const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
		const KDevPG::ListNode<StatementAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
		
		closeContext();
	}
}

void DeclarationBuilder::visitStatementTry( StatementTryAst *node ){
	if( node->bodySequence ){
		openContext( node, DUContext::Other, QualifiedIdentifier( "{try}" ) );
		
		const KDevPG::ListNode<StatementAst*> *iter = node->bodySequence->front();
		const KDevPG::ListNode<StatementAst*> *end = iter;
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
	openContext( node, DUContext::Other, QualifiedIdentifier( "{catch}" ) );
	
	if( node->variable ){
		DUChainReadLocker lock;
		ExpressionVisitor exprType( *editor(), currentContext() );
		exprType.visitNode( node->type );
		const AbstractType::Ptr type( exprType.lastType() );
		lock.unlock();
		
		Declaration * const decl = openDeclaration<Declaration>(
			node->variable, node->variable, DeclarationFlags::NoFlags );
// 		decl->setAlwaysForceDirect( true ); // nobody seems to be using this?
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
	if( ! node->variablesSequence ){
		return;
	}
	
	DUChainReadLocker lock;
	ExpressionVisitor exprType( *editor(), currentContext() );
	exprType.visitNode( node->type );
	AbstractType::Ptr type( exprType.lastType() );
	lock.unlock();
	
	const KDevPG::ListNode<StatementVariableDefinitionAst*> *iter = node->variablesSequence->front();
	const KDevPG::ListNode<StatementVariableDefinitionAst*> *end = iter;
	do{
		Declaration * const decl = openDeclaration<Declaration>(
			iter->element->name, iter->element->name, DeclarationFlags::NoFlags );
// 		decl->setAlwaysForceDirect( true ); // nobody seems to be using this?
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
