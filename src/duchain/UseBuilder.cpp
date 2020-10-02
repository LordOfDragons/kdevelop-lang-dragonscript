#include <QDebug>

#include <language/duchain/declaration.h>
#include <language/duchain/use.h>
#include <language/duchain/topducontext.h>
#include <language/duchain/duchain.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/types/structuretype.h>
#include <language/duchain/classfunctiondeclaration.h>
#include <language/duchain/types/functiontype.h>

#include "UseBuilder.h"
#include "ParseSession.h"
#include "EditorIntegrator.h"
#include "ExpressionVisitor.h"
#include "Helpers.h"


using namespace KDevelop;

namespace DragonScript{

UseBuilder::UseBuilder( EditorIntegrator &editor, const QSet<ImportPackage::Ref> &deps ) :
pParseSession( *editor.parseSession() ),
pEnableErrorReporting( true ),
pAllowVoidType( false )
{
	setDependencies( deps );
	setEditor( &editor );
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
	Declaration *decl = nullptr;
	
	do{
		const QString name( editor()->tokenText( *iter->element ) );
		RangeInRevision useRange( editor()->findRange( *iter->element ) );
		
		if( checkForVoid && name == "void" ){
			context = nullptr;
			
		}else{
			bool addUse = true;
			
			if( context ){
				DUChainReadLocker lock;
				const IndexedIdentifier identifier( (Identifier( name )) );
				decl = Helpers::declarationForName( identifier, CursorInRevision( INT_MAX, INT_MAX ), *context );
				context = nullptr;
			}
			
			if( decl ){
				if( decl->kind() == Declaration::Kind::Namespace ){
					// do not add uses for namespaces. they are special in dragonscript
					// since they are shared and there exists no namespace declaration
// 					addUse = false;
				}
				
			}else{
				reportSemanticError( useRange, i18n( "Unknown type: %1", name ) );
			}
			
			if( addUse ){
				UseBuilderBase::newUse( iter->element, DeclarationPointer( decl ) );
// 				UseBuilderBase::newUse( useRange, DeclarationPointer( decl ) );
			}
			
			if( decl ){
				context = decl->internalContext();
			}
		}
		
		checkForVoid = false;
		
		iter = iter->next;
	}while( iter != end );
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
	const DUContext *superContext = context;
	QVector<Declaration*> declarations;
	
	DUChainReadLocker lock;
	const TopDUContext &top = *topContext();
	
	if( superContext && isSuper ){
		const ClassDeclaration *classDecl = Helpers::classDeclFor( superContext->parentContext() );
		superContext = nullptr;
		
		if( classDecl ){
			classDecl = Helpers::superClassOf( top, *classDecl );
			if( classDecl ){
				superContext = classDecl->internalContext();
			}
		}
	}
	
	if( superContext ){
		declarations = Helpers::constructorsInClass( *superContext );
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
			Helpers::autoCastableFunctions( top, signature, declarations ) );
		
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
			reportSemanticError( useRange, i18n( "No suitable constructor found. expected %1",
				classDecl->identifier().toString(), sig ) );
			
		}else{
			reportSemanticError( useRange, i18n( "No suitable constructor found. expected %1", sig ) );
		}
	}
}



void UseBuilder::visitExpression( ExpressionAst *node ){
	pCurExprContext = contextAtOrCurrent( editor()->findPosition( *node ) );
	pCurExprType = nullptr;
	
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
	ExpressionVisitor exprValue( *editor(), context );
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
			declarations = Helpers::declarationsForName(
				identifier, editor()->findPosition( *node ), *context );
		}
		
		if( declarations.isEmpty() ){
			// if the context is not a class context we are at the beginning of an expression
			// and auto-this has to be used. find the this-context and try again
			if( context && ! dynamic_cast<ClassDeclaration*>( context->owner() ) ){
				const ClassDeclaration * const classDecl = Helpers::thisClassDeclFor( *context );
				if( classDecl ){
					context = classDecl->internalContext();
				}
				
				if( context ){
					declarations = Helpers::declarationsForName(
						identifier, editor()->findPosition( *node ), *context );
				}
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
			return;
		}
		lock.unlock();
		
		//UseBuilderBase::newUse( node->name, useRange, DeclarationPointer( declaration ) );
		UseBuilderBase::newUse( useRange, DeclarationPointer( declaration ) );
		
		const StructureType::Ptr structType( declaration->type<StructureType>() );
		if( structType ){
			lock.lock();
			pCurExprContext = structType->internalContext( topContext() );
			
		}else{
			pCurExprContext = nullptr;
		}
		pCurExprType = declaration->abstractType();
	}
}

void UseBuilder::visitExpressionBlock( ExpressionBlockAst *node ){
	UseBuilderBase::visitExpressionBlock( node );
	
	DUChainReadLocker lock;
	Declaration * const typeDecl = Helpers::getInternalTypeDeclaration( *topContext(), Helpers::getTypeBlock() );
	if( typeDecl && typeDecl->abstractType() ){
		pCurExprContext = typeDecl->internalContext();
		pCurExprType = typeDecl->abstractType();
		
	}else{
		pCurExprContext = nullptr;
		pCurExprType = nullptr;
	}
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
}

void UseBuilder::visitExpressionAssign( ExpressionAssignAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	DUContext *contextLeft = functionGetContext( node->left, context );
	
	if( ! node->moreSequence ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionAssignMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionAssignMoreAst*> *end = iter;
	const TopDUContext &top = *topContext();
	
	do{
		bool isAssign = false;
		if( iter->element->op && iter->element->op->op != -1 ){
			isAssign = pParseSession.tokenStream()->at( iter->element->op->op ).kind == TokenType::Token_ASSIGN;
		}
		
		if( isAssign ){
			const AbstractType::Ptr typeLeft( typeOfNode( node->left, context ) );
			const AbstractType::Ptr typeRight( typeOfNode( iter->element->right, context ) );
			
			DUChainReadLocker lock;
			if( ! Helpers::castable( top, typeRight, typeLeft ) ){
				lock.unlock();
				reportSemanticError( editor()->findRange( *iter->element->op ),
					i18n( "Cannot assign object of type %1 to %2",
					typeRight ? typeRight->toString() : "??", typeLeft ? typeLeft->toString() : "??" ) );
				lock.lock();
			}
			
			contextLeft = Helpers::contextForType( top, typeLeft );
			
		}else{
			checkFunctionCall( iter->element->op, contextLeft, typeOfNode( iter->element->right, context ) );
			contextLeft = pCurExprContext;
		}
		
		iter = iter->next;
	}while( iter != end );
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
}

void UseBuilder::visitExpressionCompare( ExpressionCompareAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	DUContext *contextLeft = functionGetContext( node->left, context );
	
	if( ! node->moreSequence ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionCompareMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionCompareMoreAst*> *end = iter;
	const TopDUContext &top = *topContext();
	
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
		
		if( isEquals ){
			const AbstractType::Ptr typeLeft( typeOfNode( node->left, context ) );
			const AbstractType::Ptr typeRight( typeOfNode( iter->element->right, context ) );
			
			DUChainReadLocker lock;
			if( ! Helpers::castable( top, typeRight, typeLeft ) ){
				lock.unlock();
				reportSemanticError( editor()->findRange( *iter->element->op ),
					i18n( "Cannot compare object of type %1 with %2",
					typeLeft ? typeLeft->toString() : "??", typeRight ? typeRight->toString() : "??" ) );
				lock.lock();
			}
			
			Declaration * const typeDecl = Helpers::getInternalTypeDeclaration( *topContext(), Helpers::getTypeBool() );
			if( typeDecl && typeDecl->abstractType() ){
				pCurExprType = typeDecl->abstractType();
				pCurExprContext = typeDecl->internalContext();
				
			}else{
				pCurExprType = nullptr;
				pCurExprContext = nullptr;
			}
			contextLeft = pCurExprContext;
			
		}else{
			checkFunctionCall( iter->element->op, contextLeft, typeOfNode( iter->element->right, context ) );
			contextLeft = pCurExprContext;
		}
		
		iter = iter->next;
	}while( iter != end );
}

void UseBuilder::visitExpressionLogic( ExpressionLogicAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	
	if( ! node->moreSequence ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionLogicMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionLogicMoreAst*> *end = iter;
	const TopDUContext &top = *topContext();
	
	DUChainReadLocker lock;
	Declaration * const typeDecl = Helpers::getInternalTypeDeclaration( *topContext(), Helpers::getTypeBool() );
	AbstractType::Ptr typeBool;
	if( typeDecl ){
		typeBool = typeDecl->abstractType();
	}
	
	do{
		lock.unlock();
		const AbstractType::Ptr typeLeft( typeOfNode( node->left, context ) );
		const AbstractType::Ptr typeRight( typeOfNode( iter->element->right, context ) );
		
		lock.lock();
		if( ! Helpers::castable( top, typeLeft, typeBool ) ){
			lock.unlock();
			reportSemanticError( editor()->findRange( *iter->element->op ),
				i18n( "Left side type %1 is not bool", typeLeft ? typeLeft->toString() : "??" ) );
			lock.lock();
		}
		
		if( ! Helpers::castable( top, typeRight, typeBool ) ){
			lock.unlock();
			reportSemanticError( editor()->findRange( *iter->element->op ),
				i18n( "Right side type %1 not bool", typeRight ? typeRight->toString() : "??" ) );
			lock.lock();
		}
		
		pCurExprType = typeBool;
		pCurExprContext = Helpers::contextForType( top, typeBool );
		
		iter = iter->next;
	}while( iter != end );
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
}

void UseBuilder::visitExpressionSpecial( ExpressionSpecialAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	
	pCurExprContext = context;
	pCurExprType = nullptr;
	visitNode( node->left );
	
	if( ! node->moreSequence ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionSpecialMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionSpecialMoreAst*> *end = iter;
	
	do{
		pCurExprContext = context;
		pCurExprContext = nullptr;
		visitNode( iter->element->type );
		
		pCurExprContext = nullptr;
		pCurExprType = nullptr;
		
		if( iter->element->op && iter->element->op->op != -1 ){
			if( pParseSession.tokenStream()->at( iter->element->op->op ).kind == TokenType::Token_CAST ){
				if( context ){
					DUChainReadLocker lock;
					ExpressionVisitor exprValue( *editor(), context );
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
				DUChainReadLocker lock;
				Declaration * const typeDecl = Helpers::getInternalTypeDeclaration( *topContext(), Helpers::getTypeBool() );
				if( typeDecl && typeDecl->abstractType() ){
					pCurExprContext = typeDecl->internalContext();
					pCurExprType = typeDecl->abstractType();
					
				}else{
					pCurExprContext = nullptr;
					pCurExprType = nullptr;
				}
			}
		}
		
		iter = iter->next;
	}while( iter != end );
}

void UseBuilder::visitExpressionUnary( ExpressionUnaryAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	DUContext *contextRight = functionGetContext( node->right, context );
	
	if( ! node->opSequence ){
		return;
	}
	
	QVector<ExpressionUnaryOpAst*> sequence;
	const KDevPG::ListNode<ExpressionUnaryOpAst*> *iter = node->opSequence->front();
	const KDevPG::ListNode<ExpressionUnaryOpAst*> *end = iter;
	do{
		sequence.push_front( iter->element );
		iter = iter->next;
	}while( iter != end );
	
	foreach( ExpressionUnaryOpAst *each, sequence ){
		checkFunctionCall( each, contextRight, QVector<AbstractType::Ptr>() );
		contextRight = pCurExprContext;
	}
}

void UseBuilder::visitExpressionInlineIfElse( ExpressionInlineIfElseAst *node ){
	DUContext * const context = contextAtOrCurrent( editor()->findPosition( *node ) );
	AbstractType::Ptr typeCondition( typeOfNode( node->condition, context ) );
	
	if( ! node->more ){
		return;
	}
	
	AbstractType::Ptr typeIf( typeOfNode( node->more->expressionIf, context ) );
	AbstractType::Ptr typeElse( typeOfNode( node->more->expressionElse, context ) );
	const TopDUContext &top = *topContext();
	
	DUChainReadLocker lock;
	if( ! Helpers::equalsInternal( typeCondition, Helpers::getTypeBool() ) ){
		lock.unlock();
		reportSemanticError( editor()->findRange( *node->condition ),
			i18n( "Cannot assign object of type %1 to bool", typeCondition ? typeCondition->toString() : "??" ) );
		lock.lock();
	}
	
	if( node->more->expressionIf && node->more->expressionElse && ! Helpers::castable( top, typeElse, typeIf ) ){
		lock.unlock();
		reportSemanticError( editor()->findRange( *node->more->expressionElse ),
			i18n( "Cannot assign object of type %1 to %2",
			typeElse ? typeElse->toString() : "??", typeIf ? typeIf->toString() : "??" ) );
		lock.lock();
	}
	
	pCurExprContext = Helpers::contextForType( top, typeIf );
	pCurExprType = typeIf;
}

void UseBuilder::visitStatementFor( StatementForAst *node ){
	// variable is ExpressionObjectAst. we need to manually clear the search context
	// as if ExpressionAst would be used.
	pCurExprContext = contextAtOrCurrent( editor()->findPosition( *node ) );
	pCurExprType = nullptr;
	
	UseBuilderBase::visitStatementFor( node );
}



DUContext *UseBuilder::functionGetContext( AstNode *node, DUContext *context ){
	pCurExprContext = context;
	pCurExprType = nullptr;
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
	if( node ){
		pCurExprContext = context;
		pCurExprType = nullptr;
		visitNode( node );
	}
	
	AbstractType::Ptr type( pCurExprType );
	if( ! type ){
		type = Helpers::getTypeInvalid();
	}
	return type;
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
		declarations = Helpers::declarationsForName( identifier, editor()->findPosition( *node ), *context );
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
	const TopDUContext &top = *topContext();
	
	DUChainReadLocker lock;
	if( ! useFunction ){
		// find functions matching with auto-casting
		const QVector<ClassFunctionDeclaration*> possibleFunctions(
			Helpers::autoCastableFunctions( top, signature, declarations ) );
		
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
		
		const StructureType::Ptr structType = TypePtr<StructureType>::dynamicCast(
			useFunction->type<FunctionType>()->returnType() );
		if( structType ){
			lock.lock();
			pCurExprContext = structType->internalContext( &top );
		}
		pCurExprType = useFunction->type<FunctionType>()->returnType();
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
	
// 	qDebug() << "KDevDScript: UseBuilder::reportSemanticHint:" << hint;
	
	Problem * const problem = new Problem();
	problem->setFinalLocation( DocumentRange( pParseSession.currentDocument(), range.castToSimpleRange() ) );
	problem->setSource( IProblem::SemanticAnalysis );
	problem->setSeverity( IProblem::Hint );
	problem->setDescription( hint );
	
	DUChainWriteLocker lock;
	topContext()->addProblem( ProblemPointer( problem ) );
}

}
