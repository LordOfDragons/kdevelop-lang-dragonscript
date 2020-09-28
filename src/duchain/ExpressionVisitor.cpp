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

ExpressionVisitor::ExpressionVisitor( const EditorIntegrator &editorIntegrator, const DUContext *ctx ) :
DynamicLanguageExpressionVisitor( ctx ),
pEditor( editorIntegrator ),
pIsTypeName( false )
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

const DUContext *ExpressionVisitor::lastContext() const{
	// no type has been encountered so far. this check works only with m_lastType.
	// m_lastDeclaration can be nullptr also later on
	if( ! m_lastType ){
		return m_context;
		/*
		ClassDeclaration * const classDecl = Helpers::thisClassDeclFor( DUChainPointer<const DUContext>( m_context ) );
		if( ! classDecl ){
			return nullptr;
		}
		return classDecl->internalContext();
		*/
	}
	
	// find context for type. this is a mess and needs trial and error
	const StructureType::Ptr structType = TypePtr<StructureType>::dynamicCast( m_lastType );
	if( structType ){
		DUContext * const ctx = structType->internalContext( topContext() );
		if( ctx ){
			return ctx;
		}
	}
	
	if( m_lastDeclaration ){
		DUContext * const ctx = m_lastDeclaration->internalContext();
		if( ctx ){
			return ctx;
		}
	}
	
	return nullptr;
}



void ExpressionVisitor::visitExpressionConstant( ExpressionConstantAst *node ){
	switch( pEditor.session().tokenStream()->at( node->value ).kind ){
	case TokenType::Token_LITERAL_BYTE:
		encounterByte();
		break;
		
	case TokenType::Token_LITERAL_INTEGER:
		encounterInt();
		break;
		
	case TokenType::Token_LITERAL_FLOAT:
		encounterFloat();
		break;
		
	case TokenType::Token_LITERAL_STRING:
		encounterString();
		break;
		
	case TokenType::Token_TRUE:
	case TokenType::Token_FALSE:
		encounterBool();
		break;
		
	case TokenType::Token_NULL:
		encounter( Helpers::getTypeNull() );
		pIsTypeName = false;
		break;
		
	case TokenType::Token_THIS:{
		ClassDeclaration * const d = Helpers::thisClassDeclFor( DUChainPointer<const DUContext>( m_context ) );
		if( d ){
			encounterDecl( *d );
		}
		}break;
		
	case TokenType::Token_SUPER:{
		ClassDeclaration * const d = Helpers::superClassDeclFor( DUChainPointer<const DUContext>( m_context ) );
		if( d ){
			encounterDecl( *d );
		}
		}break;
		
	default:
		encounterUnknown(); // should never happen
		pIsTypeName = false;
	}
}

void ExpressionVisitor::visitFullyQualifiedClassname( FullyQualifiedClassnameAst *node ){
	if( ! node->nameSequence ){
		return;
	}
	if( node->nameSequence->count() == 0 ){
		qDebug() << "ExpressionVisitor::visitFullyQualifiedClassname: node->nameSequence->count() is 0!!!!!!";
		return;
	}
	
	const KDevPG::ListNode<IdentifierAst*> *iter = node->nameSequence->front();
	const KDevPG::ListNode<IdentifierAst*> *end = iter;
	DUChainPointer<const DUContext> searchContext( m_context );
	CursorInRevision findNameBefore( pEditor.findPosition( *iter->element, EditorIntegrator::BackEdge ) );
	bool checkForVoid = pAllowVoid;
	
	do{
		/*
		CursorInRevision findNameBefore;
		if( m_scanUntilCursor.isValid() ){
			findNameBefore = m_scanUntilCursor;
			
		}else if( m_forceGlobalSearching ){
			findNameBefore = CursorInRevision::invalid();
			
		}else{
			findNameBefore = pEditor.findPosition( *iter->element, EditorIntegrator::BackEdge );
		}*/
		
		const QString name( pEditor.tokenText( iter->element->name ) );
		
		Declaration *decl = nullptr;
		
		if( checkForVoid && name == "void" ){
			encounter( Helpers::getTypeVoid() );
			searchContext = nullptr;
			pIsTypeName = true;
			
		}else{
			if( searchContext ){
				decl = Helpers::declarationForName( name, findNameBefore, searchContext );
			}
			
			if( ! decl ){
				if( m_reportUnknownNames ){
					addUnknownName( name );
				}
				encounterUnknown();
				pIsTypeName = false;
				return;
			}
			
			encounterDecl( *decl );
			searchContext = decl->internalContext();
			pIsTypeName = true;
		}
		
		checkForVoid = false;
		findNameBefore = CursorInRevision( INT_MAX, INT_MAX );
		iter = iter->next;
	}while( iter != end );
}

void ExpressionVisitor::visitExpressionMember( ExpressionMemberAst *node ){
	DUChainPointer<const DUContext> ctx( lastContext() );
	if( ! ctx ){
		encounterUnknown();
		pIsTypeName = false;
		return;
	}
	
	if( pIsTypeName ){
		// base object is a type so find an inner type
		const QString name( pEditor.tokenText( *node->name ) );
		Declaration * const decl = Helpers::declarationForName( name, CursorInRevision::invalid(), ctx );
		
		if( decl ){
			encounterDecl( *decl );
			pIsTypeName = true;
			
		}else{
			encounterUnknown();
			pIsTypeName = false;
		}
		return;
	}
	
	if( node->funcCall != -1 ){
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
		
		checkFunctionCall( node->name, ctx, signature );
		
	}else{
		const QString name( pEditor.tokenText( *node->name ) );
		QVector<Declaration*> declarations;
		if( ctx ){
			//declarations = Helpers::declarationsForName( name, CursorInRevision::invalid(), ctx );
			declarations = Helpers::declarationsForName( name,
				pEditor.findPosition( *node, EditorIntegrator::BackEdge ), ctx );
		}
		
		if( declarations.isEmpty() ){
			// if the context is not a class context we are at the beginning of an expression
			// and auto-this has to be used. find the this-context and try again
			if( ctx && ! dynamic_cast<ClassDeclaration*>( ctx->owner() ) ){
				const ClassDeclaration * const classDecl = Helpers::thisClassDeclFor( ctx );
				if( classDecl ){
					ctx = classDecl->internalContext();
					if( ctx ){
						//declarations = Helpers::declarationsForName( name, CursorInRevision::invalid(), ctx );
						declarations = Helpers::declarationsForName( name,
							pEditor.findPosition( *node, EditorIntegrator::BackEdge ), ctx );
					}
				}
			}
		}
		
		if( declarations.isEmpty() ){
			encounterUnknown();
			pIsTypeName = false;
			return;
		}
		
		Declaration * const decl = declarations.first();
		if( dynamic_cast<ClassFunctionDeclaration*>( decl ) ){
			encounterUnknown();
			pIsTypeName = false;
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
		DUChainPointer<const DUContext> ctx( lastContext() );
		if( ! ctx || ! clearVisitNode( iter->element->right ) ){
			return;
		}
		
		checkFunctionCall( iter->element->op, ctx, m_lastType );
		iter = iter->next;
	}while( iter != end );
}

void ExpressionVisitor::visitExpressionBlock( ExpressionBlockAst* ){
	encounterBlock();
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
		DUChainPointer<const DUContext> ctx( lastContext() );
		if( ! ctx || ! clearVisitNode( iter->element->right ) ){
			return;
		}
		
		checkFunctionCall( iter->element->op, ctx, m_lastType );
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
		DUChainPointer<const DUContext> ctx( lastContext() );
		if( ! ctx || ! clearVisitNode( iter->element->right ) ){
			return;
		}
		
		checkFunctionCall( iter->element->op, ctx, m_lastType );
		iter = iter->next;
	}while( iter != end );
}

void ExpressionVisitor::visitExpressionLogic( ExpressionLogicAst *node ){
	if( node->moreSequence ){
		encounterBool(); // always bool
		
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
		DUChainPointer<const DUContext> ctx( lastContext() );
		if( ! ctx || ! clearVisitNode( iter->element->right ) ){
			return;
		}
		
		checkFunctionCall( iter->element->op, ctx, m_lastType );
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
		DUChainPointer<const DUContext> ctx( lastContext() );
		if( ! ctx ){
			return;
		}
		
		checkFunctionCall( iter->element, ctx, QVector<KDevelop::AbstractType::Ptr>() );
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
			encounterUnknown(); // should never happen
			pIsTypeName = false;
			return;
		}
		
		switch( pEditor.session().tokenStream()->at( iter->element->op->op ).kind ){
		case TokenType::Token_CAST:{
			DUChainPointer<const DUContext> ctx( lastContext() );
			if( ! ctx || ! clearVisitNode( iter->element->type ) ){
				return;
			}
			}break;
			
		case TokenType::Token_CASTABLE:
		case TokenType::Token_TYPEOF:
			encounterBool();
			break;
			
		default:
			encounterUnknown(); // should never happen
			pIsTypeName = false;
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
		DUChainPointer<const DUContext> ctx( lastContext() );
		if( ! ctx ){
			return;
		}
		checkFunctionCall( each, ctx, QVector<KDevelop::AbstractType::Ptr>() );
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
	encounterUnknown(); // void
	pIsTypeName = false;
}

void ExpressionVisitor::visitStatementIf( StatementIfAst* ){
	encounterUnknown(); // void
	pIsTypeName = false;
}

void ExpressionVisitor::visitStatementReturn( StatementReturnAst* ){
	encounterUnknown(); // void
	pIsTypeName = false;
}

void ExpressionVisitor::visitStatementSelect( StatementSelectAst* ){
	encounterUnknown(); // void
	pIsTypeName = false;
}

void ExpressionVisitor::visitStatementThrow( StatementThrowAst* ){
	encounterUnknown(); // void
	pIsTypeName = false;
}

void ExpressionVisitor::visitStatementTry( StatementTryAst* ){
	encounterUnknown(); // void
	pIsTypeName = false;
}

void ExpressionVisitor::visitStatementVariableDefinitions( StatementVariableDefinitionsAst* ){
	encounterUnknown(); // void
	pIsTypeName = false;
}

void ExpressionVisitor::visitStatementWhile( StatementWhileAst* ){
	encounterUnknown(); // void
	pIsTypeName = false;
}

void ExpressionVisitor::encounterDecl( Declaration &decl ){
	encounter( decl.abstractType(), DeclarationPointer( &decl ) );
	pIsTypeName = false;
}

void ExpressionVisitor::encounterObject(){
	AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeObject( declPtr, typePtr );
	encounter( typePtr, declPtr );
	pIsTypeName = false;
}

void ExpressionVisitor::encounterBool(){
	AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeBool( declPtr, typePtr );
	encounter( typePtr, declPtr );
	pIsTypeName = false;
}

void ExpressionVisitor::encounterByte(){
	AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeByte( declPtr, typePtr );
	encounter( typePtr, declPtr );
	pIsTypeName = false;
}

void ExpressionVisitor::encounterInt(){
	AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeInt( declPtr, typePtr );
	encounter( typePtr, declPtr );
	pIsTypeName = false;
}

void ExpressionVisitor::encounterFloat(){
	AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeFloat( declPtr, typePtr );
	encounter( typePtr, declPtr );
	pIsTypeName = false;
}

void ExpressionVisitor::encounterString(){
	AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeString( declPtr, typePtr );
	encounter( typePtr, declPtr );
	pIsTypeName = false;
}

void ExpressionVisitor::encounterBlock(){
	AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeBlock( declPtr, typePtr );
	encounter( typePtr, declPtr );
	pIsTypeName = false;
}

void ExpressionVisitor::checkFunctionCall( AstNode *node, DUChainPointer<const DUContext> context,
const AbstractType::Ptr &argument ){
	QVector<AbstractType::Ptr> signature;
	signature.append( argument );
	checkFunctionCall( node, context, signature );
	pIsTypeName = false;
}

void ExpressionVisitor::checkFunctionCall( AstNode *node, DUChainPointer<const DUContext> ctx,
const QVector<AbstractType::Ptr> &signature ){
	QVector<Declaration*> declarations;
	if( ctx ){
		//declarations = Helpers::declarationsForName( pEditor.tokenText( *node ), CursorInRevision::invalid(), ctx );
		declarations = Helpers::declarationsForName( pEditor.tokenText( *node ),
			pEditor.findPosition( *node, EditorIntegrator::BackEdge ), ctx );
	}
	if( declarations.isEmpty() || ! dynamic_cast<ClassFunctionDeclaration*>( declarations.first() ) ){
		encounterUnknown();
		pIsTypeName = false;
		return;
	}
	
	// find best matching function
	const TopDUContext * const top = topContext();
	ClassFunctionDeclaration *useFunction = Helpers::bestMatchingFunction( top, signature, declarations );
	if( useFunction ){
		encounter( useFunction->type<FunctionType>()->returnType(), DeclarationPointer( useFunction ) );
		pIsTypeName = false;
		return;
	}
	
	// find functions matching with auto-casting
	const QVector<ClassFunctionDeclaration*> possibleFunctions(
		Helpers::autoCastableFunctions( top, signature, declarations ) );
	if( ! possibleFunctions.isEmpty() ){
		useFunction = possibleFunctions.first();
		if( useFunction ){
			encounter( useFunction->type<FunctionType>()->returnType(), DeclarationPointer( useFunction ) );
			pIsTypeName = false;
			return;
		}
	}
	
	encounterUnknown();
	pIsTypeName = false;
}

bool ExpressionVisitor::clearVisitNode( AstNode *node ){
	m_lastType = nullptr; // important or lastContext() calls will fail
	m_lastDeclaration = nullptr; // important or lastContext() calls will fail
	
	if( node ){
		visitNode( node );
		if( m_lastType != unknownType() ){
			return true;
		}
	}
	
	encounterUnknown();
	pIsTypeName = false;
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
