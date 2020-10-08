#include <QDebug>

#include <language/duchain/declaration.h>
#include <language/duchain/use.h>
#include <language/duchain/topducontext.h>
#include <language/duchain/duchain.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/types/structuretype.h>
#include <language/duchain/classfunctiondeclaration.h>
#include <language/duchain/types/functiontype.h>

#include <language/duchain/persistentsymboltable.h>

#include "UseBuilder.h"
#include "ParseSession.h"
#include "EditorIntegrator.h"
#include "ExpressionVisitor.h"
#include "Helpers.h"
#include "DumpChain.h"


using namespace KDevelop;

namespace DragonScript{

UseBuilder::UseBuilder( EditorIntegrator &editor, const QSet<ImportPackage::Ref> &deps, TypeFinder &typeFinderr ) :
pParseSession( *editor.parseSession() ),
pEnableErrorReporting( true ),
pAllowVoidType( false ),
pCanBeType( true ),
pAutoThis( true ),
pIgnoreVariable( nullptr )
{
	setDependencies( deps );
	setEditor( &editor );
	setTypeFinder( &typeFinderr );
}



void UseBuilder::setEnableErrorReporting( bool enable ){
	pEnableErrorReporting = enable;
}



DUContext *UseBuilder::contextAtOrCurrent( const CursorInRevision &pos ){
	DUChainReadLocker lock;
	DUContext * const context = topContext()->findContextAt( pos, true );
	return context ? context : currentContext();
}


void UseBuilder::visitFullyQualifiedClassname( FullyQualifiedClassnameAst *node ){
	if( ! node->nameSequence ){
		return;
	}
	
	const DUContext *context = contextAtOrCurrent( editor()->findPosition( *node ) );
	
	const KDevPG::ListNode<IdentifierAst*> *iter = node->nameSequence->front();
	const KDevPG::ListNode<IdentifierAst*> *end = iter;
	bool checkForVoid = pAllowVoidType;
	bool useReachable = true;
	
	do{
		const QString name( editor()->tokenText( *iter->element ) );
		RangeInRevision useRange( editor()->findRange( *iter->element ) );
		
		if( checkForVoid && name == "void" ){
			context = nullptr;
			
		}else{
			Declaration *decl = nullptr;
			
			if( context ){
				const IndexedIdentifier identifier( (Identifier( name )) );
				DUChainReadLocker lock;
				decl = Helpers::declarationForName( identifier, CursorInRevision::invalid(), *context,
					useReachable ? pinnedNamespaces() : QVector<const TopDUContext*>(), *typeFinder() );
			}
			
			if( ! decl ){
				if( context ){
					reportSemanticError( useRange, i18n( "Unknown type: %1 in %2",
						name, context->localScopeIdentifier().toString() ) );
#if 0
					// findLocalDeclarations() is broken for namespaces
					{
						DUChainReadLocker lock;
						const QVector<QPair<Declaration*, int>> d(context->allDeclarations(
							CursorInRevision::invalid(), context->topContext(), false));
						const IndexedIdentifier identifier( (Identifier( name )) );
						qDebug() << "CHECK" << identifier.identifier().toString() << identifier.index()
							<< context->inSymbolTable() << context->scopeIdentifier(false);
						foreach(auto each, d){
							qDebug() << "AllDecl" << each.first->toString()
								<< each.first->indexedIdentifier().index()
								<< each.first->inSymbolTable()
								<< (IndexedDeclaration(each.first).topContextIndex() == context->topContext()->ownIndex())
								<< (each.first->context() == context);
						}
						QList<Declaration*> d2( context->findLocalDeclarations(identifier,
							CursorInRevision::invalid()));
						foreach(Declaration *each, d2){
							qDebug() << "LocalDecl" << each->toString() << each->indexedIdentifier().index();
						}
						const IndexedQualifiedIdentifier id(context->scopeIdentifier(true) + identifier);
						const IndexedDeclaration* d3;
						uint count;
						PersistentSymbolTable::self().declarations(id, count, d3);
						qDebug() << "SHIT" << id.identifier().toString() << count << d3 << (d3 ? d3->declaration() : nullptr)
							<< (d3 && d3->declaration() ? d3->declaration()->toString() : "null");
					}
#endif
					
				}else{
					reportSemanticError( useRange, i18n( "Unknown type: %1", name ) );
				}
			}
			
			UseBuilderBase::newUse( iter->element, DeclarationPointer( decl ) );
			
			context =  nullptr;
			if( decl ){
				context = decl->internalContext();
			}
		}
		
		checkForVoid = false;
		useReachable = false;
		
		iter = iter->next;
	}while( iter != end );
}

void UseBuilder::visitPin( PinAst* ){
	// ducontext findLocalDeclarations() is broken beyond repair for namespaces
// 	UseBuilderBase::visitPin( node );
}

void UseBuilder::visitNamespace( NamespaceAst* ){
	// ducontext findLocalDeclarations() is broken beyond repair for namespaces
// 	UseBuilderBase::visitNamespace( node );
}

void UseBuilder::visitClassFunctionDeclareBegin( ClassFunctionDeclareBeginAst *node ){
	const DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	
	// overwritten to allow void type in FullyQualifiedClassnameAst
	pAllowVoidType = true;
	visitNode( node->type );
	pAllowVoidType = false;
	
	if( node->argumentsSequence ){
		const KDevPG::ListNode<ClassFunctionDeclareArgumentAst*> *iter = node->argumentsSequence->front();
		const KDevPG::ListNode<ClassFunctionDeclareArgumentAst*> *end = iter;
		do{
			visitNode( iter->element );
			iter = iter->next;
		}while( iter != end );
	}
	
	// process this/super call
	if( node->super == -1 ){
		return;
	}
	
	const bool isSuper = pParseSession.tokenStream()->at( node->super ).kind == TokenTypeWrapper::Token_SUPER;
	
	// find this/super class constructors
	const RangeInRevision useRange( editor()->findRange( node->super ) );
	const DUContext *searchContext = context->parentContext();
	QVector<Declaration*> declarations;
	
	DUChainReadLocker lock;
	
	if( searchContext && isSuper ){
		const ClassDeclaration *classDecl = Helpers::classDeclFor( searchContext );
		searchContext = nullptr;
		
		if( classDecl ){
			classDecl = Helpers::superClassOf( *classDecl, *typeFinder() );
			if( classDecl ){
				searchContext = classDecl->internalContext();
			}
		}
	}
	
	if( searchContext ){
		declarations = Helpers::constructorsInClass( *searchContext );
	}
	lock.unlock();
	
	// process super arguments building signature at the same time
	QVector<AbstractType::Ptr> signature;
	
	if( node->superArgumentsSequence ){
		const KDevPG::ListNode<ExpressionAst*> *iter = node->superArgumentsSequence->front();
		const KDevPG::ListNode<ExpressionAst*> *end = iter;
		do{
			visitNode( iter->element );
			
			if( pCurExprType ){
				signature.append( pCurExprType );
				
			}else{
				signature.append( Helpers::getTypeVoid() ); // error
			}
			
			iter = iter->next;
		}while( iter != end );
	}
	
	// find best matching function
	lock.lock();
	ClassFunctionDeclaration *useFunction = Helpers::bestMatchingFunction( signature, declarations );
	
	if( ! useFunction ){
		// find functions matching with auto-casting
		const QVector<ClassFunctionDeclaration*> possibleFunctions(
			Helpers::autoCastableFunctions( signature, declarations, *typeFinder() ) );
		
		if( possibleFunctions.size() == 1 ){
			useFunction = possibleFunctions.at( 0 );
			lock.unlock();
			
		}else if( possibleFunctions.size() > 1 ){
			useFunction = possibleFunctions.at( 0 );
			
			QVector<IProblem::Ptr> diagnostics;
			foreach( ClassFunctionDeclaration* each, possibleFunctions ){
				Problem * const problem = new Problem();
				problem->setFinalLocation( DocumentRange( each->url(), each->range().castToSimpleRange() ) );
				problem->setSource( IProblem::SemanticAnalysis );
				problem->setSeverity( IProblem::Hint );
				
				const ClassDeclaration * const classDecl = Helpers::classDeclFor( each->context() );
				if( classDecl ){
					problem->setDescription( i18n( "Candidate: %1.%2",
						classDecl->identifier().toString(), each->toString() ) );
					
				}else{
					problem->setDescription( i18n( "Candidate: %1", each->toString() ) );
				}
				
				diagnostics.append( IProblem::Ptr( problem ) );
			}
			
			const char *separator = "";
			QString sig( isSuper ? "super(" : "this(" );
			foreach( const AbstractType::Ptr &each, signature ){
				sig.append( separator ).append( each->toString() );
				separator = ", ";
			}
			sig.append( ")" );
			
			const ClassDeclaration * const classDecl = Helpers::classDeclFor( context );
			lock.unlock();
			
			if( classDecl ){
				reportSemanticError( useRange, i18n( "Ambiguous constructor call: found %1.%2",
					classDecl->identifier().toString(), sig ), diagnostics );
				
			}else{
				reportSemanticError( useRange,
					i18n( "Ambiguous constructor call: found %1", sig ), diagnostics );
			}
			
		}else{
			lock.unlock();
		}
		
	}else{
		lock.unlock();
	}
	
	// use function if found. no context required since this is outside function block
	if( useFunction ){
		//UseBuilderBase::newUse( node->name, useRange, DeclarationPointer( useFunction ) );
		UseBuilderBase::newUse( useRange, DeclarationPointer( useFunction ) );
		
	}else{
		const char *separator = "";
		QString sig( isSuper ? "super(" : "this(" );
		foreach( const AbstractType::Ptr &each, signature ){
			sig.append( separator ).append( each->toString() );
			separator = ", ";
		}
		sig.append( ")" );
		
		lock.lock();
		const ClassDeclaration * const classDecl = Helpers::classDeclFor( context );
		lock.unlock();
		
		if( classDecl ){
			reportSemanticError( useRange, i18n( "No suitable constructor found. expected %1.%2",
				classDecl->identifier().toString(), sig ) );
			
		}else{
			reportSemanticError( useRange, i18n( "No suitable constructor found. expected %1", sig ) );
		}
	}
}



void UseBuilder::visitExpression( ExpressionAst *node ){
	pCurExprContext = contextAtOrCurrent( editor()->findPosition( *node ) );
	pCurExprType = nullptr;
	pCanBeType = true;
	pAutoThis = true;
	
	UseBuilderBase::visitExpression( node );
}

void UseBuilder::visitExpressionConstant( ExpressionConstantAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	
	const QString name( editor()->tokenText( *node ) );
	const RangeInRevision useRange( editor()->findRange( *node ) );
	
	DeclarationPointer declaration;
	AbstractType::Ptr type;
	{
	DUChainReadLocker lock;
	ExpressionVisitor exprValue( *editor(), context, pinnedNamespaces(), *typeFinder() );
	exprValue.visitNode( node );
	declaration = exprValue.lastDeclaration();
	type = exprValue.lastType();
	}
	
	if( declaration ) {
		// add a use only for "this" and "super". the rest only updated the expr-context
		switch( pParseSession.tokenStream()->at( node->value ).kind ){
		case TokenTypeWrapper::TokenType::Token_THIS:
		case TokenTypeWrapper::TokenType::Token_SUPER:
			//UseBuilderBase::newUse( node, useRange, declaration );
			UseBuilderBase::newUse( useRange, declaration );
			break;
		}
	}
	
	pCurExprContext = declaration ? declaration->internalContext() : nullptr;
	pCurExprType = type;
	pCanBeType = false;
	pAutoThis = false;
}

void UseBuilder::visitExpressionMember( ExpressionMemberAst *node ){
	DUContext *context = pCurExprContext;
	
	if( node->funcCall != -1 ){
		// process function arguments building signature at the same time
		QVector<AbstractType::Ptr> signature;
		
		if( node->argumentsSequence ){
			const KDevPG::ListNode<ExpressionAst*> *iter = node->argumentsSequence->front();
			const KDevPG::ListNode<ExpressionAst*> *end = iter;
			do{
				visitNode( iter->element );
				
				if( pCurExprType ){
					signature.append( pCurExprType );
					
				}else{
					signature.append( Helpers::getTypeInvalid() );
				}
				
				iter = iter->next;
			}while( iter != end );
		}
		
		checkFunctionCall( node->name, context, signature );
		
	}else{
		const IndexedIdentifier identifier( Identifier( editor()->tokenText( *node->name ) ) );
		QVector<Declaration*> declarations;
		DUChainReadLocker lock;
		if( context ){
			declarations = Helpers::declarationsForName( identifier, editor()->findPosition( *node ),
				*context, pCanBeType ? pinnedNamespaces() : QVector<const TopDUContext*>(), *typeFinder() );
			
			if( pIgnoreVariable ){
				QVector<Declaration*>::iterator iter;
				for( iter = declarations.begin(); iter != declarations.end(); iter++ ){
					if( *iter == pIgnoreVariable ){
						declarations.erase( iter );
						break;
					}
				}
			}
		}
		
		if( declarations.isEmpty() && pAutoThis && context && ! dynamic_cast<ClassDeclaration*>( context->owner() ) ){
			const ClassDeclaration * const classDecl = Helpers::thisClassDeclFor( *context );
			if( classDecl ){
				context = classDecl->internalContext();
			}
			
			if( context ){
				declarations = Helpers::declarationsForName( identifier,
					editor()->findPosition( *node ), *context, {}, *typeFinder() );
			}
		}
		
		const RangeInRevision useRange( editor()->findRange( *node->name ) );
		
		if( declarations.isEmpty() ){
			const ClassDeclaration * const classDecl = Helpers::classDeclFor( context );
			lock.unlock();
			
			if( classDecl ){
				reportSemanticError( useRange, i18n( "Unknown object: %1.%2",
					classDecl->identifier().toString(), identifier.identifier().toString() ) );
				
			}else{
				reportSemanticError( useRange, i18n( "Unknown object: %1",
					identifier.identifier().toString() ) );
			}
			UseBuilderBase::visitExpressionMember( node );
			pCurExprContext = nullptr;
			pCurExprType = nullptr;
			pCanBeType = false;
			pAutoThis = false;
			return;
		}
		
		Declaration *declaration = declarations.at( 0 );
		if( dynamic_cast<ClassFunctionDeclaration*>( declaration ) ){
			const ClassDeclaration * const classDecl = Helpers::classDeclFor( declaration->context() );
			lock.unlock();
			
			if( classDecl ){
				reportSemanticError( useRange, i18n( "Expected object but found function: %1.%2",
					classDecl->identifier().toString(), declaration->toString() ) );
				
			}else{
				reportSemanticError( useRange, i18n( "Expected object but found function: %1",
					declaration->toString() ) );
			}
			
			pCurExprContext = nullptr;
			pCurExprType = nullptr;
			pCanBeType = false;
			pAutoThis = false;
			return;
		}
		lock.unlock();
		
		//UseBuilderBase::newUse( node->name, useRange, DeclarationPointer( declaration ) );
		UseBuilderBase::newUse( useRange, DeclarationPointer( declaration ) );
		
		pCurExprType = declaration->abstractType();
		pCurExprContext = typeFinder()->contextFor( pCurExprType );
		pCanBeType = declaration->type<StructureType>();
		pAutoThis = false;
	}
}

void UseBuilder::visitExpressionBlock( ExpressionBlockAst *node ){
	UseBuilderBase::visitExpressionBlock( node );
	
	ClassDeclaration * const typeBlock = typeFinder()->typeBlock();
	if( typeBlock ){
		pCurExprContext = typeBlock->internalContext();
		pCurExprType = typeBlock->abstractType();
		
	}else{
		pCurExprContext = nullptr;
		pCurExprType = nullptr;
	}
	pCanBeType = false;
	pAutoThis = false;
}

void UseBuilder::visitExpressionAddition( ExpressionAdditionAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	DUContext *contextLeft = functionGetContext( node->left, context );
	
	if( ! node->moreSequence ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionAdditionMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionAdditionMoreAst*> *end = iter;
	do{
		checkFunctionCall( iter->element->op, contextLeft, typeOfNode( iter->element->right, context ) );
		contextLeft = pCurExprContext;
		iter = iter->next;
	}while( iter != end );
	
	pCanBeType = false;
	pAutoThis = false;
}

void UseBuilder::visitExpressionAssign( ExpressionAssignAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	DUContext *contextLeft = functionGetContext( node->left, context );
	
	if( ! node->moreSequence ){
		return;
	}
	
	const AbstractType::Ptr typeLeft( pCurExprType ? pCurExprType : Helpers::getTypeInvalid() );
	const KDevPG::ListNode<ExpressionAssignMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionAssignMoreAst*> *end = iter;
	
	do{
		bool isAssign = false;
		if( iter->element->op && iter->element->op->op != -1 ){
			isAssign = pParseSession.tokenStream()->at( iter->element->op->op ).kind == TokenType::Token_ASSIGN;
		}
		
		const AbstractType::Ptr typeRight( typeOfNode( iter->element->right, context ) );
		
		if( isAssign ){
			DUChainReadLocker lock;
			if( ! Helpers::castable( typeRight, typeLeft, *typeFinder() ) ){
				lock.unlock();
				reportSemanticError( editor()->findRange( *iter->element->op ),
					i18n( "Cannot assign object of type %1 to %2",
					typeRight ? typeRight->toString() : "??", typeLeft ? typeLeft->toString() : "??" ) );
			}
			
		}else{
			checkFunctionCall( iter->element->op, contextLeft, typeRight );
			contextLeft = pCurExprContext;
		}
		
		iter = iter->next;
	}while( iter != end );
	
	pCanBeType = false;
	pAutoThis = false;
}

void UseBuilder::visitExpressionBitOperation( ExpressionBitOperationAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	DUContext *contextLeft = functionGetContext( node->left, context );
	
	if( ! node->moreSequence ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionBitOperationMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionBitOperationMoreAst*> *end = iter;
	do{
		checkFunctionCall( iter->element->op, contextLeft, typeOfNode( iter->element->right, context ) );
		contextLeft = pCurExprContext;
		iter = iter->next;
	}while( iter != end );
	
	pCanBeType = false;
	pAutoThis = false;
}

void UseBuilder::visitExpressionCompare( ExpressionCompareAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	DUContext *contextLeft = functionGetContext( node->left, context );
	
	if( ! node->moreSequence ){
		return;
	}
	
	const AbstractType::Ptr typeLeft( pCurExprType ? pCurExprType : Helpers::getTypeInvalid() );
	const KDevPG::ListNode<ExpressionCompareMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionCompareMoreAst*> *end = iter;
	
	do{
		bool isEquals = false;
		if( iter->element->op && iter->element->op->op != -1 ){
			switch( pParseSession.tokenStream()->at( iter->element->op->op ).kind ){
			case TokenType::Token_EQUALS:
			case TokenType::Token_NEQUALS:
				isEquals = true;
				break;
			}
		}
		
		const AbstractType::Ptr typeRight( typeOfNode( iter->element->right, context ) );
		
		if( isEquals ){
			DUChainReadLocker lock;
			if( ! Helpers::castable( typeRight, typeLeft, *typeFinder() )
			&& ! Helpers::castable( typeLeft, typeRight, *typeFinder() ) ){
				lock.unlock();
				reportSemanticError( editor()->findRange( *iter->element->op ),
					i18n( "Cannot compare object of type %1 with %2",
					typeLeft ? typeLeft->toString() : "??", typeRight ? typeRight->toString() : "??" ) );
				lock.lock();
			}
			
			ClassDeclaration * const typeBool = typeFinder()->typeBool();
			if( typeBool ){
				pCurExprType = typeBool->abstractType();
				pCurExprContext = typeBool->internalContext();
				
			}else{
				pCurExprType = nullptr;
				pCurExprContext = nullptr;
			}
			contextLeft = pCurExprContext;
			
		}else{
			checkFunctionCall( iter->element->op, contextLeft, typeRight );
			contextLeft = pCurExprContext;
		}
		
		iter = iter->next;
	}while( iter != end );
	
	pCanBeType = false;
	pAutoThis = false;
}

void UseBuilder::visitExpressionLogic( ExpressionLogicAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	AbstractType::Ptr typeExpr( typeOfNode( node->left, context ) );
	
	if( ! node->moreSequence ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionLogicMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionLogicMoreAst*> *end = iter;
	
	ClassDeclaration *declBool = nullptr;
	AbstractType::Ptr typeBool;
	declBool = typeFinder()->typeBool();
	if( declBool ){
		typeBool = declBool->abstractType();
	}
	
	{
	DUChainReadLocker lock;
	if( ! Helpers::castable( typeExpr, typeBool, *typeFinder() ) ){
		lock.unlock();
		reportSemanticError( editor()->findRange( *iter->element->op ),
			i18n( "Type %1 is not bool", typeExpr ? typeExpr->toString() : "??" ) );
	}
	}
	
	do{
		typeExpr = typeOfNode( iter->element->right, context );
		
		{
		DUChainReadLocker lock;
		if( ! Helpers::castable( typeExpr, typeBool, *typeFinder() ) ){
			lock.unlock();
			reportSemanticError( editor()->findRange( *iter->element->op ),
				i18n( "Type %1 not bool", typeExpr ? typeExpr->toString() : "??" ) );
		}
		}
		
		iter = iter->next;
	}while( iter != end );
	
	pCurExprType = typeBool;
	pCurExprContext = declBool->internalContext();
	pCanBeType = false;
	pAutoThis = false;
}

void UseBuilder::visitExpressionMultiply( ExpressionMultiplyAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	DUContext *contextLeft = functionGetContext( node->left, context );
	
	if( ! node->moreSequence ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionMultiplyMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionMultiplyMoreAst*> *end = iter;
	do{
		checkFunctionCall( iter->element->op, contextLeft, typeOfNode( iter->element->right, context ) );
		contextLeft = pCurExprContext;
		iter = iter->next;
	}while( iter != end );
	pCanBeType = false;
	pAutoThis = false;
}

void UseBuilder::visitExpressionPostfix( ExpressionPostfixAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
 	DUContext *contextLeft = functionGetContext( node->left, context );
	
	if( ! node->opSequence ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionPostfixOpAst*> *iter = node->opSequence->front();
	const KDevPG::ListNode<ExpressionPostfixOpAst*> *end = iter;
	do{
		checkFunctionCall( iter->element, contextLeft, QVector<AbstractType::Ptr>() );
		contextLeft = pCurExprContext;
		iter = iter->next;
	}while( iter != end );
	pCanBeType = false;
	pAutoThis = false;
}

void UseBuilder::visitExpressionSpecial( ExpressionSpecialAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	( void )typeOfNode( node->left, context );
	
	if( ! node->moreSequence ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionSpecialMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionSpecialMoreAst*> *end = iter;
	
	do{
		pCurExprContext = context;
		pCurExprContext = nullptr;
		pCanBeType = true;
		pAutoThis = false;
		visitNode( iter->element->type );
		
		pCurExprContext = nullptr;
		pCurExprType = nullptr;
		pCanBeType = false;
		pAutoThis = false;
		
		if( iter->element->op && iter->element->op->op != -1 ){
			if( pParseSession.tokenStream()->at( iter->element->op->op ).kind == TokenType::Token_CAST ){
				if( context ){
					DUChainReadLocker lock;
					ExpressionVisitor exprValue( *editor(), context, pinnedNamespaces(), *typeFinder() );
					exprValue.visitNode( iter->element->type );
					const DeclarationPointer declaration( exprValue.lastDeclaration() );
					if( declaration ){
						ClassDeclaration * const classDeclRight = dynamic_cast<ClassDeclaration*>( declaration.data() );
						if( classDeclRight ){
							pCurExprContext = classDeclRight->internalContext();
						}
						pCurExprType = declaration->abstractType();
					}
				}
				
			}else{
				ClassDeclaration * const typeBool = typeFinder()->typeBool();
				if( typeBool ){
					pCurExprContext = typeBool->internalContext();
					pCurExprType = typeBool->abstractType();
					
				}else{
					pCurExprContext = nullptr;
					pCurExprType = nullptr;
				}
			}
		}
		
		iter = iter->next;
	}while( iter != end );
	
	pCanBeType = false;
	pAutoThis = false;
}

void UseBuilder::visitExpressionUnary( ExpressionUnaryAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	DUContext *contextRight = functionGetContext( node->right, context );
	
	if( ! node->opSequence ){
		return;
	}
	
	AbstractType::Ptr typeRight( pCurExprType );
	
	QVector<ExpressionUnaryOpAst*> sequence;
	const KDevPG::ListNode<ExpressionUnaryOpAst*> *iter = node->opSequence->front();
	const KDevPG::ListNode<ExpressionUnaryOpAst*> *end = iter;
	do{
		sequence.push_front( iter->element );
		iter = iter->next;
	}while( iter != end );
	
	foreach( ExpressionUnaryOpAst *each, sequence ){
		if( pParseSession.tokenStream()->at( each->op ).kind == TokenType::Token_LOGICAL_NOT ){
			AbstractType::Ptr typeBool;
			ClassDeclaration * const declBool = typeFinder()->typeBool();
			if( declBool ){
				typeBool = declBool->abstractType();
				contextRight = declBool->internalContext();
			}
			
			{
			DUChainReadLocker lock;
			if( ! Helpers::castable( typeRight, typeBool, *typeFinder() ) ){
				lock.unlock();
				reportSemanticError( editor()->findRange( each->op ),
					i18n( "Type %1 is not bool", typeRight ? typeRight->toString() : "??" ) );
			}
			}
			
		}else{
			checkFunctionCall( each, contextRight, QVector<AbstractType::Ptr>() );
			contextRight = pCurExprContext;
		}
	}
	
	pCanBeType = false;
	pAutoThis = false;
}

void UseBuilder::visitExpressionInlineIfElse( ExpressionInlineIfElseAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	AbstractType::Ptr typeCondition( typeOfNode( node->condition, context ) );
	
	if( ! node->more ){
		return;
	}
	
	AbstractType::Ptr typeIf( typeOfNode( node->more->expressionIf, context ) );
	AbstractType::Ptr typeElse( typeOfNode( node->more->expressionElse, context ) );
	
	bool validCondition = true, validCast = true;
	{
	DUChainReadLocker lock;
	
	validCondition = Helpers::equalsInternal( typeCondition, Helpers::getTypeBool() );
	
	if( node->more->expressionIf && node->more->expressionElse ){
		validCast = Helpers::castable( typeElse, typeIf, *typeFinder() );
	}
	
	pCurExprContext = typeFinder()->contextFor( typeIf );
	pCurExprType = typeIf;
	pCanBeType = false;
	pAutoThis = false;
	}
	
	if( ! validCondition ){
		reportSemanticError( editor()->findRange( *node->condition ),
			i18n( "Cannot assign object of type %1 to bool", typeCondition ? typeCondition->toString() : "??" ) );
	}
	if( ! validCast ){
		reportSemanticError( editor()->findRange( *node->more->expressionElse ),
			i18n( "Cannot assign object of type %1 to %2",
			typeElse ? typeElse->toString() : "??", typeIf ? typeIf->toString() : "??" ) );
	}
}

void UseBuilder::visitStatementFor( StatementForAst *node ){
	// variable is ExpressionObjectAst. we need to manually clear the search context
	// as if ExpressionAst would be used.
	pCurExprContext = contextAtOrCurrent( editor()->findPosition( *node ) );
	pCurExprType = nullptr;
	pCanBeType = false;
	pAutoThis = false;
	
	UseBuilderBase::visitStatementFor( node );
}

void UseBuilder::visitStatementVariableDefinitions( StatementVariableDefinitionsAst *node ){
	// init expression does pick up variable definitions but this is not allowed
	
// 	UseBuilderBase::visitStatementVariableDefinitions( node );
	
	visitNode(node->type);
	
	if( ! node->variablesSequence ){
		return;
	}
	
	const KDevPG::ListNode<StatementVariableDefinitionAst*> *iter = node->variablesSequence->front();
	const KDevPG::ListNode<StatementVariableDefinitionAst*> *end = iter;
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	
	do{
		if( iter->element->value ){
			pIgnoreVariable = context->findDeclarationAt(
				editor()->findPosition( *iter->element->name, EditorIntegrator::FrontEdge ) );
			visitNode( iter->element );
			pIgnoreVariable = nullptr;
			
		}else{
			visitNode( iter->element );
		}
		
		iter = iter->next;
	}while( iter != end );
}



DUContext *UseBuilder::functionGetContext( AstNode *node, DUContext *context ){
	pCurExprContext = context;
	pCurExprType = nullptr;
	pCanBeType = true;
	pAutoThis = true;
	visitNode( node );
	
	if( ! pCurExprContext ){
		return {};
	}
	
	DUChainReadLocker lock;
	const ClassDeclaration * const classDecl = Helpers::classDeclFor( pCurExprContext );
	if( classDecl ){
		return classDecl->internalContext();
	}
	return {};
}

AbstractType::Ptr UseBuilder::typeOfNode( AstNode *node, DUContext *context ){
	if( ! node ){
		return Helpers::getTypeInvalid();
	}
	
	pCurExprContext = context;
	pCurExprType = nullptr;
	pCanBeType = true;
	pAutoThis = true;
	visitNode( node );
	
	return pCurExprType ? pCurExprType : Helpers::getTypeInvalid();
}

void UseBuilder::checkFunctionCall( AstNode *node, DUContext *context, const AbstractType::Ptr &argument ){
	QVector<AbstractType::Ptr> signature;
	signature.append( argument );
	checkFunctionCall( node, context, signature );
}

void UseBuilder::checkFunctionCall( AstNode *node, DUContext *context, const QVector<AbstractType::Ptr> &signature ){
	const IndexedIdentifier identifier( Identifier( editor()->tokenText( *node ) ) );
	const RangeInRevision useRange( editor()->findRange( *node ) );
	
	QVector<Declaration*> declarations;
	if( context ){
		DUChainReadLocker lock;
		
		if( identifier == Helpers::nameConstructor() ){
			// constructors are only looked up in the current class
			const ClassDeclaration * const classDecl = Helpers::thisClassDeclFor( *context );
			if( classDecl && classDecl->internalContext() ){
				declarations = Helpers::constructorsInClass( *classDecl->internalContext() );
			}
			
		}else{
			declarations = Helpers::declarationsForName( identifier,
				CursorInRevision::invalid(), *context, {}, *typeFinder(), true );
		}
	}
	
	if( declarations.isEmpty() ){
		DUChainReadLocker lock;
		const ClassDeclaration * const classDecl = Helpers::classDeclFor( context );
		lock.unlock();
		
		if( classDecl ){
			reportSemanticError( useRange, i18n( "Unknown function: %1.%2",
				classDecl->identifier().toString(), identifier.identifier().toString() ) );
			
		}else{
			reportSemanticError( useRange, i18n( "Unknown function: %1",
				identifier.identifier().toString() ) );
			
			if( identifier.identifier().toString() == "equals" ){
				DumpChain().dump( context );
			}
		}
		
		pCurExprContext = nullptr;
		pCurExprType = nullptr;
		return;
	}
	
	// if the first found declaration is not a function definition something is wrong
	if( ! dynamic_cast<ClassFunctionDeclaration*>( declarations.at( 0 ) ) ){
		DUChainReadLocker lock;
		const ClassDeclaration * const classDecl = Helpers::classDeclFor( context );
		lock.unlock();
		
		if( classDecl ){
			reportSemanticError( useRange, i18n( "Expected function but found object: %1.%2",
				classDecl->identifier().toString(), identifier.identifier().toString() ) );
			
		}else{
			reportSemanticError( useRange, i18n( "Expected function but found object: %1",
				identifier.identifier().toString() ) );
		}
		
		pCurExprContext = nullptr;
		pCurExprType = nullptr;
		return;
	}
	
	// find best matching function
	ClassFunctionDeclaration *useFunction = Helpers::bestMatchingFunction( signature, declarations );
	
	DUChainReadLocker lock;
	if( ! useFunction ){
		// find functions matching with auto-casting
		const QVector<ClassFunctionDeclaration*> possibleFunctions(
			Helpers::autoCastableFunctions( signature, declarations, *typeFinder() ) );
		
		if( possibleFunctions.size() == 1 ){
			useFunction = possibleFunctions.at( 0 );
			
		}else if( possibleFunctions.size() > 1 ){
			useFunction = possibleFunctions.at( 0 );
			
			QVector<IProblem::Ptr> diagnostics;
			foreach( ClassFunctionDeclaration* each, possibleFunctions ){
				Problem * const problem = new Problem();
				problem->setFinalLocation( DocumentRange( each->url(), each->range().castToSimpleRange() ) );
				problem->setSource( IProblem::SemanticAnalysis );
				problem->setSeverity( IProblem::Hint );
				
				const ClassDeclaration * const classDecl = dynamic_cast<ClassDeclaration*>( each->context() );
				if( classDecl ){
					problem->setDescription( i18n( "Candidate: %1.%2", classDecl->identifier().toString(), each->toString() ) );
					
				}else{
					problem->setDescription( i18n( "Candidate: %1", each->toString() ) );
				}
				
				diagnostics.append( IProblem::Ptr( problem ) );
			}
			
			const char *separator = "";
			QString sig( identifier.identifier().toString() + "(" );
			foreach( const AbstractType::Ptr &each, signature ){
				sig.append( separator ).append( each->toString() );
				separator = ", ";
			}
			sig.append( ")" );
			
			const ClassDeclaration * const classDecl = Helpers::classDeclFor( context );
			
			lock.unlock();
			if( classDecl ){
				reportSemanticError( useRange, i18n( "Ambiguous function call: found %1.%2",
					classDecl->identifier().toString(), sig ), diagnostics );
				
			}else{
				reportSemanticError( useRange, i18n( "Ambiguous function call: found %1", sig ), diagnostics );
			}
			lock.lock();
		}
	}
	
	// use function if found
	if( useFunction ){
		lock.unlock();
		//UseBuilderBase::newUse( node, useRange, DeclarationPointer( useFunction ) );
		UseBuilderBase::newUse( useRange, DeclarationPointer( useFunction ) );
		
		pCurExprType = useFunction->type<FunctionType>()->returnType();
		if( pCurExprType ){
			pCurExprContext = typeFinder()->contextFor( pCurExprType );
		}
		return;
	}
	
	const char *separator = "";
	QString sig( identifier.identifier().toString() + "(" );
	foreach( const AbstractType::Ptr &each, signature ){
		sig.append( separator ).append( each->toString() );
		separator = ", ";
	}
	sig.append( ")" );
	
	const ClassDeclaration * const classDecl = Helpers::classDeclFor( context );
	
	lock.unlock();
	if( classDecl ){
		reportSemanticError( useRange, i18n( "No suitable function found: expected %1.%2",
			classDecl->identifier().toString(), sig ) );
		
	}else{
		reportSemanticError( useRange, i18n( "No suitable function found: expected %1", sig ) );
	}
	
	pCurExprContext = nullptr;
	pCurExprType = nullptr;
}



void UseBuilder::reportSemanticError( const RangeInRevision &range, const QString &hint ){
	reportSemanticError( range, hint, QVector<IProblem::Ptr>() );
}

void UseBuilder::reportSemanticError( const RangeInRevision &range, const QString &hint,
const QVector<IProblem::Ptr> &diagnostics ){
	if( ! pEnableErrorReporting ){
		return;
	}
	
	Problem * const problem = new Problem();
	problem->setFinalLocation( DocumentRange( pParseSession.currentDocument(), range.castToSimpleRange() ) );
	problem->setSource( IProblem::SemanticAnalysis );
	problem->setSeverity( IProblem::Error );
	problem->setDescription( hint );
	problem->setDiagnostics( diagnostics );
	
	DUChainWriteLocker lock;
	topContext()->addProblem( ProblemPointer( problem ) );
}

void UseBuilder::reportSemanticHint( const RangeInRevision &range, const QString &hint ){
	if( ! pEnableErrorReporting ){
		return;
	}
	
	Problem * const problem = new Problem();
	problem->setFinalLocation( DocumentRange( pParseSession.currentDocument(), range.castToSimpleRange() ) );
	problem->setSource( IProblem::SemanticAnalysis );
	problem->setSeverity( IProblem::Hint );
	problem->setDescription( hint );
	
	DUChainWriteLocker lock;
	topContext()->addProblem( ProblemPointer( problem ) );
}

}
