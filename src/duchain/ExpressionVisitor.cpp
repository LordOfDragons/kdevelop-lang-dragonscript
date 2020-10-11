#include <language/duchain/types/containertypes.h>
#include <language/duchain/types/unsuretype.h>
#include <language/duchain/types/integraltype.h>
#include <language/duchain/types/typeregister.h>
#include <language/duchain/types/typesystemdata.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/declaration.h>
#include <language/duchain/functiondeclaration.h>
#include <language/duchain/classdeclaration.h>
#include <language/duchain/aliasdeclaration.h>
#include <language/duchain/ducontext.h>

#include <QDebug>

#include <KLocalizedString>

#include <functional>

#include "ExpressionVisitor.h"
#include "duchainexport.h"
#include "EditorIntegrator.h"
#include "Helpers.h"


using namespace KDevelop;

namespace DragonScript {

ExpressionVisitor::ExpressionVisitor( const EditorIntegrator &editorIntegrator,
	const DUContext *ctx, const QVector<Namespace*> &searchNamespaces,
	TypeFinder &typeFinder, Namespace &rootNamespace, const CursorInRevision cursorOffset ) :
DynamicLanguageExpressionVisitor( ctx ),
pEditor( editorIntegrator ),
pCursorOffset( cursorOffset ),
pTypeFinder( typeFinder ),
pLastContext( nullptr ),
pIsTypeName( false ),
pSearchNamespaces( searchNamespaces ),
pRootNamespace( rootNamespace )
{
	Q_ASSERT( m_context );
	Q_ASSERT( m_context->topContext() );
}



void ExpressionVisitor::setAllowVoid( bool allowVoid ){
	pAllowVoid = allowVoid;
}

bool ExpressionVisitor::isLastAlias() const{
	Declaration * const d = m_lastDeclaration.data();
	return d && ( dynamic_cast<AliasDeclaration*>( d ) || dynamic_cast<ClassDeclaration*>( d ) );
}



void ExpressionVisitor::visitExpressionConstant( ExpressionConstantAst *node ){
	switch( pEditor.session().tokenStream()->at( node->value ).kind ){
	case TokenType::Token_LITERAL_BYTE:
		encounterInternalType( pTypeFinder.typeByte() );
		break;
		
	case TokenType::Token_LITERAL_INTEGER:
		encounterInternalType( pTypeFinder.typeInt() );
		break;
		
	case TokenType::Token_LITERAL_FLOAT:
		encounterInternalType( pTypeFinder.typeFloat() );
		break;
		
	case TokenType::Token_LITERAL_STRING:
		encounterInternalType( pTypeFinder.typeString() );
		break;
		
	case TokenType::Token_TRUE:
	case TokenType::Token_FALSE:
		encounterInternalType( pTypeFinder.typeBool() );
		break;
		
	case TokenType::Token_NULL:
		encounter( Helpers::getTypeNull() );
		pLastContext = nullptr;
		pIsTypeName = false;
		break;
		
	case TokenType::Token_THIS:{
		ClassDeclaration * const d = Helpers::thisClassDeclFor( *m_context );
		if( d ){
			encounterDecl( *d );
			
		}else{
			encounterInvalid();
		}
		}break;
		
	case TokenType::Token_SUPER:{
		ClassDeclaration * const d = Helpers::superClassDeclFor( *m_context, pTypeFinder );
		if( d ){
			encounterDecl( *d );
			
		}else{
			encounterInvalid();
		}
		}break;
		
	default:
		encounterInvalid(); // should never happen
	}
}

void ExpressionVisitor::visitFullyQualifiedClassname( FullyQualifiedClassnameAst *node ){
	if( ! node->nameSequence || node->nameSequence->count() == 0 ){
		return;
	}
	
	const KDevPG::ListNode<IdentifierAst*> *iter = node->nameSequence->front();
	const KDevPG::ListNode<IdentifierAst*> *end = iter;
	const DUContext *searchContext = m_context;
	bool checkForVoid = pAllowVoid;
	bool useReachable = true;
	
	do{
		const QString name( pEditor.tokenText( iter->element->name ) );
		
		Declaration *decl = nullptr;
		
		if( checkForVoid && name == "void" ){
			encounterVoid();
			searchContext = nullptr;
			
		}else{
			if( searchContext ){
				const IndexedIdentifier identifier( (Identifier( name )) );
				decl = Helpers::declarationForName( identifier, CursorInRevision::invalid(),
					*searchContext, useReachable ? pSearchNamespaces : QVector<Namespace*>(),
					pTypeFinder, pRootNamespace );
			}
			
			if( ! decl ){
				if( m_reportUnknownNames ){
					addUnknownName( name );
				}
				encounterInvalid();
				return;
			}
			
			encounterDecl( *decl );
			searchContext = pLastContext;
			pIsTypeName = true;
		}
		
		checkForVoid = false;
		useReachable = false;
		iter = iter->next;
	}while( iter != end );
}

void ExpressionVisitor::visitExpressionMember( ExpressionMemberAst *node ){
	const DUContext * const ctx = pLastContext ? pLastContext : m_context;
	
	if( node->funcCall != -1 ){
		if( ! ctx ){
			encounterInvalid();
			return;
		}
		
		QVector<AbstractType::Ptr> signature;
		
		if( node->argumentsSequence ){
			const KDevPG::ListNode<ExpressionAst*> *iter = node->argumentsSequence->front();
			const KDevPG::ListNode<ExpressionAst*> *end = iter;
			do{
				if( ! clearVisitNode( iter->element ) ){
					return;
				}
				signature.append( m_lastType );
				iter = iter->next;
			}while( iter != end );
		}
		
		checkFunctionCall( *node->name, *ctx, signature, pIsTypeName );
		
	}else if( pIsTypeName ){
		if( ! ctx ){
			encounterInvalid();
			return;
		}
		
		// base object is a type so find an inner type or a constant member
		const IndexedIdentifier identifier( Identifier( pEditor.tokenText( *node->name ) ) );
		Declaration * const decl = Helpers::declarationForName( identifier,
			pCursorOffset + pEditor.findPosition( *node->name ), *ctx,
			pSearchNamespaces, pTypeFinder, pRootNamespace );
		
		if( decl ){
			encounterDecl( *decl );
			pIsTypeName = decl->kind() == Declaration::Kind::Type;
			
		}else{
			encounterInvalid();
		}
		
	}else{
		const IndexedIdentifier identifier( Identifier( pEditor.tokenText( *node->name ) ) );
		QVector<Declaration*> declarations;
		
		if( ctx ){
			declarations = Helpers::declarationsForName( identifier,
				pCursorOffset + pEditor.findPosition( *node, EditorIntegrator::BackEdge ),
				*ctx, {}, pTypeFinder, pRootNamespace );
		}
		
		if( declarations.isEmpty() ){
			// if the context is not a class context we are at the beginning of an expression
			// and auto-this has to be used. find the this-context and try again
			if( ctx && ! dynamic_cast<ClassDeclaration*>( ctx->owner() ) ){
				const ClassDeclaration * const classDecl = Helpers::thisClassDeclFor( *ctx );
				if( classDecl ){
					DUContext * const classContext = classDecl->internalContext();
					if( classContext ){
						//declarations = Helpers::declarationsForName( identifier, CursorInRevision::invalid(), *ctx );
						declarations = Helpers::declarationsForName( identifier,
							pCursorOffset + pEditor.findPosition( *node, EditorIntegrator::BackEdge ),
							*classContext, pSearchNamespaces, pTypeFinder, pRootNamespace );
					}
				}
			}
		}
		
		if( declarations.isEmpty() ){
			encounterInvalid();
			return;
		}
		
		Declaration * const decl = declarations.first();
		if( dynamic_cast<ClassFunctionDeclaration*>( decl ) ){
			encounterInvalid();
			return;
		}
		
		encounterDecl( *decl );
		pIsTypeName = decl->kind() == Declaration::Kind::Type;
	}
}

void ExpressionVisitor::visitExpressionAddition( ExpressionAdditionAst *node ){
	if( ! clearVisitNode( node->left ) || ! node->moreSequence /*fall through*/ ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionAdditionMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionAdditionMoreAst*> *end = iter;
	do{
		const DUContext *ctx = pLastContext;
		if( ! ctx || ! clearVisitNode( iter->element->right ) ){
			return;
		}
		
		checkFunctionCall( *iter->element->op, *ctx, m_lastType );
		iter = iter->next;
	}while( iter != end );
}

void ExpressionVisitor::visitExpressionBlock( ExpressionBlockAst* ){
	encounterInternalType( pTypeFinder.typeBlock() );
}

void ExpressionVisitor::visitExpressionAssign( ExpressionAssignAst *node ){
	clearVisitNode( node->left );
	// assign is the type of the left node. this is the same code used for fall-through
}

void ExpressionVisitor::visitExpressionBitOperation( ExpressionBitOperationAst *node ){
	if( ! clearVisitNode( node->left ) || ! node->moreSequence /*fall through*/ ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionBitOperationMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionBitOperationMoreAst*> *end = iter;
	do{
		const DUContext *ctx = pLastContext;
		if( ! ctx || ! clearVisitNode( iter->element->right ) ){
			return;
		}
		
		checkFunctionCall( *iter->element->op, *ctx, m_lastType );
		iter = iter->next;
	}while( iter != end );
}

void ExpressionVisitor::visitExpressionCompare( ExpressionCompareAst *node ){
	if( ! clearVisitNode( node->left ) || ! node->moreSequence /*fall through*/ ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionCompareMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionCompareMoreAst*> *end = iter;
	do{
		const DUContext *ctx = pLastContext;
		if( ! ctx || ! clearVisitNode( iter->element->right ) ){
			return;
		}
		
		checkFunctionCall( *iter->element->op, *ctx, m_lastType );
		iter = iter->next;
	}while( iter != end );
}

void ExpressionVisitor::visitExpressionLogic( ExpressionLogicAst *node ){
	if( node->moreSequence ){
		encounterInternalType( pTypeFinder.typeBool() ); // always bool
		
	}else{
		clearVisitNode( node->left ); // fall-through
	}
}

void ExpressionVisitor::visitExpressionMultiply( ExpressionMultiplyAst *node ){
	if( ! clearVisitNode( node->left ) || ! node->moreSequence /*fall through*/ ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionMultiplyMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionMultiplyMoreAst*> *end = iter;
	do{
		const DUContext *ctx = pLastContext;
		if( ! ctx || ! clearVisitNode( iter->element->right ) ){
			return;
		}
		
		checkFunctionCall( *iter->element->op, *ctx, m_lastType );
		iter = iter->next;
	}while( iter != end );
}

void ExpressionVisitor::visitExpressionPostfix( ExpressionPostfixAst *node ){
	if( ! clearVisitNode( node->left ) || ! node->opSequence /*fall through*/ ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionPostfixOpAst*> *iter = node->opSequence->front();
	const KDevPG::ListNode<ExpressionPostfixOpAst*> *end = iter;
	do{
		const DUContext *ctx = pLastContext;
		if( ! ctx ){
			return;
		}
		
		checkFunctionCall( *iter->element, *ctx, QVector<AbstractType::Ptr>() );
		iter = iter->next;
	}while( iter != end );
}

void ExpressionVisitor::visitExpressionSpecial( ExpressionSpecialAst *node ){
	if( ! clearVisitNode( node->left ) || ! node->moreSequence /*fall through*/ ){
		return;
	}
	
	const KDevPG::ListNode<ExpressionSpecialMoreAst*> *iter = node->moreSequence->front();
	const KDevPG::ListNode<ExpressionSpecialMoreAst*> *end = iter;
	
	do{
		if( ! iter->element->op || iter->element->op->op == -1 ){
			encounterInvalid(); // should never happen
			return;
		}
		
		switch( pEditor.session().tokenStream()->at( iter->element->op->op ).kind ){
		case TokenType::Token_CAST:{
			const DUContext *ctx = pLastContext;
			if( ! ctx || ! clearVisitNode( iter->element->type ) ){
				return;
			}
			pIsTypeName = false;
			}break;
			
		case TokenType::Token_CASTABLE:
		case TokenType::Token_TYPEOF:
			encounterInternalType( pTypeFinder.typeBool() );
			break;
			
		default:
			encounterInvalid(); // should never happen
			return;
		}
		
		iter = iter->next;
	}while( iter != end );
}

void ExpressionVisitor::visitExpressionUnary( ExpressionUnaryAst *node ){
	if( ! clearVisitNode( node->right ) || ! node->opSequence /*fall through*/ ){
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
		const DUContext *ctx = pLastContext;
		if( ! ctx ){
			return;
		}
		checkFunctionCall( *each, *ctx, QVector<AbstractType::Ptr>() );
	}
}

void ExpressionVisitor::visitExpressionInlineIfElse( ExpressionInlineIfElseAst *node ){
	if( node->more ){
		// if-case becomes expression type so condition and else-case is ignored
		clearVisitNode( node->more->expressionIf );
		
	}else{ /*fall through*/
		clearVisitNode( node->condition );
	}
}

void ExpressionVisitor::visitStatementFor( StatementForAst* ){
	encounterInvalid();
}

void ExpressionVisitor::visitStatementIf( StatementIfAst* ){
	encounterInvalid();
}

void ExpressionVisitor::visitStatementReturn( StatementReturnAst* ){
	encounterInvalid();
}

void ExpressionVisitor::visitStatementSelect( StatementSelectAst* ){
	encounterInvalid();
}

void ExpressionVisitor::visitStatementThrow( StatementThrowAst* ){
	encounterInvalid();
}

void ExpressionVisitor::visitStatementTry( StatementTryAst* ){
	encounterInvalid();
}

void ExpressionVisitor::visitStatementVariableDefinitions( StatementVariableDefinitionsAst* ){
	encounterInvalid();
}

void ExpressionVisitor::visitStatementWhile( StatementWhileAst* ){
	encounterInvalid();
}

void ExpressionVisitor::encounterInvalid(){
	encounterUnknown();
	pLastContext = nullptr;
	pIsTypeName = false;
}

void ExpressionVisitor::encounterVoid(){
	encounter( Helpers::getTypeVoid() );
	pLastContext = nullptr;
	pIsTypeName = true;
}

void ExpressionVisitor::encounterDecl( Declaration &decl ){
	encounter( decl.abstractType(), DeclarationPointer( &decl ) );
	pIsTypeName = false;
	
	if( decl.abstractType() ){
		pLastContext = pTypeFinder.contextFor( decl.abstractType() );
		
	}else{
		pLastContext = nullptr;
	}
}

void ExpressionVisitor::encounterInternalType( ClassDeclaration *classDecl ){
	if( classDecl ){
		encounterDecl( *classDecl );
		
	}else{
		encounterInvalid();
	}
}

void ExpressionVisitor::encounterFunction( ClassFunctionDeclaration *funcDecl ){
	if( ! funcDecl ){
		encounterInvalid();
		return;
	}
	
	const TypePtr<FunctionType> funcType = funcDecl->type<FunctionType>();
	if( ! funcType ){
		encounterInvalid(); // should never happen
		return;
	}
	
	encounter( funcType->returnType(), DeclarationPointer( funcDecl ) );
	pIsTypeName = false;
	
	if( funcType->returnType() ){
		pLastContext = pTypeFinder.contextFor( funcType->returnType() );
		
	}else{
		pLastContext = nullptr;
	}
}

void ExpressionVisitor::checkFunctionCall( AstNode &node, const DUContext &context,
const AbstractType::Ptr &argument, bool staticOnly ){
	QVector<AbstractType::Ptr> signature;
	signature.append( argument );
	checkFunctionCall( node, context, signature, staticOnly );
	pIsTypeName = false;
}

void ExpressionVisitor::checkFunctionCall( AstNode &node, const DUContext &ctx,
const QVector<AbstractType::Ptr> &signature, bool staticOnly ){
	QVector<Declaration*> declarations;
	
	if( staticOnly ){
		const QList<Declaration*> list( ctx.findLocalDeclarations( Identifier( pEditor.tokenText( node ) ) ) );
		foreach( Declaration *each, list ){
			ClassMemberDeclaration * const memberDecl = dynamic_cast<ClassMemberDeclaration*>( each );
			if( memberDecl && memberDecl->isStatic() ){
				declarations << each;
			}
		}
		
	}else{
		declarations = Helpers::declarationsForName(
			IndexedIdentifier( Identifier( pEditor.tokenText( node ) ) ),
			CursorInRevision::invalid(), ctx, {}, pTypeFinder, pRootNamespace, true, false );
	}
	
	if( declarations.isEmpty() || ! dynamic_cast<ClassFunctionDeclaration*>( declarations.first() ) ){
		encounterInvalid();
		return;
	}
	
	// find best matching function
	ClassFunctionDeclaration *useFunction = Helpers::bestMatchingFunction( signature, declarations );
	if( useFunction ){
		encounterFunction( useFunction );
		return;
	}
	
	// find functions matching with auto-casting
	const QVector<ClassFunctionDeclaration*> possibleFunctions(
		Helpers::autoCastableFunctions( signature, declarations, pTypeFinder ) );
	if( ! possibleFunctions.isEmpty() ){
		useFunction = possibleFunctions.first();
		if( useFunction ){
			encounterFunction( useFunction );
			return;
		}
	}
	
	encounterInvalid();
}

bool ExpressionVisitor::clearVisitNode( AstNode *node ){
	m_lastType = nullptr;
	m_lastDeclaration = nullptr;
	pLastContext = nullptr;
	
	if( node ){
		visitNode( node );
		if( m_lastType != unknownType() ){
			return true;
		}
	}
	
	encounterInvalid();
	return false;
}



void ExpressionVisitor::addUnknownName( const QString& name ){
	if( m_parentVisitor ){
		static_cast<ExpressionVisitor*>( m_parentVisitor )->addUnknownName( name );
		
	}else if( ! m_unknownNames.contains( name ) ){
		m_unknownNames.insert( name );
	}
}

}
