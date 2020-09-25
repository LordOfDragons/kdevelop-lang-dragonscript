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

using KDevelop::AliasDeclaration;
using KDevelop::ClassDeclaration;
using KDevelop::DeclarationPointer;
using KDevelop::IntegralType;
using KDevelop::IndexedType;
using KDevelop::StructureType;
using KDevelop::Problem;
using KDevelop::DocumentRange;
using KDevelop::FunctionType;

namespace DragonScript {

ExpressionVisitor::ExpressionVisitor( const EditorIntegrator &editorIntegrator, const DUContext *ctx ) :
DynamicLanguageExpressionVisitor( ctx ),
pEditor( editorIntegrator )
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
			
		}else{
			if( searchContext ){
				decl = Helpers::declarationForName( name, findNameBefore, searchContext );
			}
			
			if( ! decl ){
				if( m_reportUnknownNames ){
					addUnknownName( name );
				}
				encounterUnknown();
				return;
			}
			
			encounterDecl( *decl );
			searchContext = decl->internalContext();
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
		return;
	}
	
	if( node->funcCall != -1 ){
		QVector<KDevelop::AbstractType::Ptr> signature;
		
		if( node->argumentsSequence ){
			const KDevPG::ListNode<ExpressionAst*> *iter = node->argumentsSequence->front();
			const KDevPG::ListNode<ExpressionAst*> *end = iter;
			do{
				m_lastType = nullptr; // important or lastContext() calls will fail
				m_lastDeclaration = nullptr; // important or lastContext() calls will fail
				
				visitNode( iter->element );
				
				if( m_lastType == unknownType() ){
					encounterUnknown();
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
			declarations = Helpers::declarationsForName( name, CursorInRevision::invalid()/*pEditor.findPosition( *node )*/, ctx );
		}
		
		if( declarations.isEmpty() ){
			// if the context is not a class context we are at the beginning of an expression
			// and auto-this has to be used. find the this-context and try again
			if( ctx && ! dynamic_cast<ClassDeclaration*>( ctx->owner() ) ){
				const ClassDeclaration * const classDecl = Helpers::thisClassDeclFor( ctx );
				if( classDecl ){
					ctx = classDecl->internalContext();
					if( ctx ){
						declarations = Helpers::declarationsForName( name, CursorInRevision::invalid()/*pEditor.findPosition( *node )*/, ctx );
					}
				}
			}
		}
		
		if( declarations.isEmpty() || dynamic_cast<ClassFunctionDeclaration*>( declarations.first() ) ){
			encounterUnknown();
			return;
		}
		
		encounterDecl( *declarations.first() );
	}
}

void ExpressionVisitor::visitExpressionBlock( ExpressionBlockAst* ){
	encounterBlock();
}

void ExpressionVisitor::encounterDecl( Declaration &decl ){
	encounter( decl.abstractType(), DeclarationPointer( &decl ) );
}

void ExpressionVisitor::encounterObject(){
	KDevelop::AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeObject( declPtr, typePtr );
	encounter( typePtr, declPtr );
}

void ExpressionVisitor::encounterBool(){
	KDevelop::AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeBool( declPtr, typePtr );
	encounter( typePtr, declPtr );
}

void ExpressionVisitor::encounterByte(){
	KDevelop::AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeByte( declPtr, typePtr );
	encounter( typePtr, declPtr );
}

void ExpressionVisitor::encounterInt(){
	KDevelop::AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeInt( declPtr, typePtr );
	encounter( typePtr, declPtr );
}

void ExpressionVisitor::encounterFloat(){
	KDevelop::AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeFloat( declPtr, typePtr );
	encounter( typePtr, declPtr );
}

void ExpressionVisitor::encounterString(){
	KDevelop::AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeString( declPtr, typePtr );
	encounter( typePtr, declPtr );
}

void ExpressionVisitor::encounterBlock(){
	KDevelop::AbstractType::Ptr typePtr;
	DeclarationPointer declPtr;
	Helpers::getTypeBlock( declPtr, typePtr );
	encounter( typePtr, declPtr );
}

void ExpressionVisitor::checkFunctionCall( AstNode *node, DUChainPointer<const DUContext> ctx,
const QVector<KDevelop::AbstractType::Ptr> &signature ){
	QVector<Declaration*> declarations;
	if( ctx ){
		declarations = Helpers::declarationsForName( pEditor.tokenText( *node ), CursorInRevision::invalid()/*pEditor.findPosition( *node )*/, ctx );
	}
	if( declarations.isEmpty() || ! dynamic_cast<ClassFunctionDeclaration*>( declarations.first() ) ){
		encounterUnknown();
		return;
	}
	
	// find best matching function
	const TopDUContext * const top = topContext();
	ClassFunctionDeclaration *useFunction = Helpers::bestMatchingFunction( top, signature, declarations );
	if( useFunction ){
		encounter( useFunction->type<FunctionType>()->returnType(), DeclarationPointer( useFunction ) );
		return;
	}
	
	// find functions matching with auto-casting
	const QVector<ClassFunctionDeclaration*> possibleFunctions(
		Helpers::autoCastableFunctions( top, signature, declarations ) );
	if( ! possibleFunctions.empty() ){
		useFunction = possibleFunctions.first();
		if( useFunction ){
			encounter( useFunction->type<FunctionType>()->returnType(), DeclarationPointer( useFunction ) );
			return;
		}
	}
	
	encounterUnknown();
}



void ExpressionVisitor::addUnknownName( const QString& name ){
	if( m_parentVisitor ){
		static_cast<ExpressionVisitor*>( m_parentVisitor )->addUnknownName( name );
		
	}else if( ! m_unknownNames.contains( name ) ){
		m_unknownNames.insert( name );
	}
}

}
