#include <QDebug>
#include <KLocalizedString>
#include <KTextEditor/View>
#include <language/duchain/declaration.h>
#include <language/duchain/ducontext.h>
#include <language/duchain/use.h>

#include "dsp_ast.h"
#include "dsp_tokenstream.h"
#include "dsp_debugvisitor.h"

#include "DSCodeCompletionCodeBody.h"
#include "DSCodeCompletionContext.h"
#include "DSCodeCompletionModel.h"
#include "ExpressionVisitor.h"
#include "DumpChain.h"
#include "TokenText.h"
#include "Helpers.h"
#include "items/DSCCItemFunctionCall.h"
#include "items/DSCodeCompletionItem.h"


using namespace KDevelop;

using KTextEditor::Range;
using KTextEditor::View;

namespace DragonScript {

DSCodeCompletionCodeBody::DSCodeCompletionCodeBody( DSCodeCompletionContext &completionContext,
	const DUContext &context, QList<CompletionTreeItemPointer> &completionItems,
	bool &abort, bool fullCompletion ) :
pCodeCompletionContext( completionContext ),
pContext( context ),
pCompletionItems( completionItems ),
pAbort( abort ),
pFullCompletion( fullCompletion )
{
}



void DSCodeCompletionCodeBody::completionItems(){
	const Declaration *completionDecl = nullptr;
	CursorInRevision completionPosition;
	AbstractType::Ptr completionType;
	Mode mode = Mode::everything;
	bool firstWord = true;
	
	pCompletionContext = nullptr;
	pAllDefinitions.clear();
	
	TokenStream &tokenStream = pCodeCompletionContext.tokenStream();
	if( tokenStream.size() > 0 ){
		const QByteArray ptext( pCodeCompletionContext.text().toUtf8() );
		ParseSession session( IndexedString( pCodeCompletionContext.document() ), ptext );
		Parser parser;
		
		session.prepareCompletion( parser );
		
		// find the first token from the end of the token stream where parsing should begin.
		// we want to find the start of the sub expression
		int lastIndex;
		
		switch( tokenStream.back().kind ){
		case Parser::Token_PERIOD:
			lastIndex = -2;
			break;
			
		case Parser::Token_IDENTIFIER:
			lastIndex = -1;
			break;
			
		default:
			lastIndex = -1;
		}
		
		const int startIndex = findFirstParseToken( tokenStream, lastIndex );
		
			/*
			{for(int i=startIndex; i<tokenStream.size()-1; i++ ){
				const Token &token = tokenStream.at( i );
				qDebug() << "DSCodeCompletionCodeBody.findStart: kind=" << TokenText( token.kind )
					<< QString::fromLatin1( pCompletionContext.tokenStreamText().mid( token.begin, token.end - token.begin + 1 ) );
			}}
			*/
		
		if( startIndex < tokenStream.size() ){
			// copy tokens except the final period
			copyTokens( tokenStream, *session.tokenStream(), startIndex, lastIndex );
			
			// try parsing the token stream into an expression. since we figured out the tokens
			// forming the sub expression parsing succeeds.
			ExpressionAst *ast = nullptr;
			parser.rewind( 0 ); // required otherwise parser fails
			if( ! parser.parseExpression( &ast ) || ! ast
			|| session.tokenStream()->index() != session.tokenStream()->size() ){
				qDebug() << "DSCodeCompletionCodeBody: parsing sub expression failed";
				return;
			}
			/*
			int i;
			for( i=0; i<session.tokenStream()->size(); i++ ){
				parser.rewind( i );
				if( parser.parseExpression( &ast ) && session.tokenStream()->index() == session.tokenStream()->size() ){
					break;
				}
				ast = nullptr;
			}
			*/
			
			// get the declaration to show completions for
	// 		DebugVisitor( session.tokenStream(), QString::fromLatin1( ptext ) ).visitNode( ast );
			
			EditorIntegrator editor( session );
			DUChainReadLocker lock;
			ExpressionVisitor exprvisitor( editor, &pContext, pCodeCompletionContext.searchNamespaces(),
				pCodeCompletionContext.typeFinder(), *pCodeCompletionContext.rootNamespace(),
				pCodeCompletionContext.position() );
			exprvisitor.visitExpression( ast );
			if( ! exprvisitor.lastType() || ! exprvisitor.lastDeclaration() ){
				qDebug() << "DSCodeCompletionCodeBody: can not determine completion type";
				return;
			}
			
			/*
			if( exprvisitor.lastType() && exprvisitor.lastType()->whichType() == AbstractType::TypeStructure ){
				qDebug() << "DSCodeCompletionCodeBody: completion type " << exprvisitor.lastType()->toString()
					<< "declaration " << ( exprvisitor.lastDeclaration().data() ? exprvisitor.lastDeclaration().data()->toString() : "-" );
			}
			*/
			
			completionType = exprvisitor.lastType();
			completionDecl = exprvisitor.lastDeclaration().data();
			pCompletionContext = exprvisitor.lastContext();
			completionPosition = CursorInRevision::invalid();
			
			if( exprvisitor.isTypeName() ){
				mode = Mode::type;
				
			}else{
				mode = Mode::instance;
			}
			firstWord = false;
		}
	}
	
	if( firstWord ){
		// completion at the first word. assume context type and declaration
		DUChainReadLocker lock;
		pCompletionContext = &pContext;
		completionDecl = Helpers::thisClassDeclFor( pContext );
		if( completionDecl ){
			completionType = completionDecl->abstractType();
		}
		completionPosition = pCodeCompletionContext.position();
	}
	
	qDebug() << "type" << (completionType ? completionType->toString() : "-")
		<< "decl" << (completionDecl ? completionDecl->toString() : "-")
		<< "ctx" << (pCompletionContext ? pCompletionContext->scopeIdentifier(true).toString() : "-");
	
	if( ! completionDecl || ! completionType || ! pCompletionContext ){
		qDebug() << "DSCodeCompletionCodeBody: can not determine completion type";
		pCompletionContext = nullptr;
		return;
	}
	
	// do completion
	qDebug() << "DSCodeCompletionCodeBody: completion context" << completionDecl->toString();
	
	DUChainReadLocker lock;
	
	if( completionDecl->kind() == Declaration::Namespace ){
		Namespace * const ns = pCodeCompletionContext.rootNamespace()->
			getNamespace( completionDecl->qualifiedIdentifier() );
		if( ! ns ){
			pCompletionContext = nullptr;
			return;
		}
		
		foreach( const Namespace::ClassDeclarationPointer &each, ns->classes() ){
			pAllDefinitions << QPair<Declaration*, int>{ each.data(), 0 };
		}
		foreach( auto each, ns->namespaces() ){
			if( each->declaration() ){
				pAllDefinitions << QPair<Declaration*, int>{ each->declaration(), 0 };
			}
		}
		
		mode = Mode::type;
		
	}else{
		pAllDefinitions = Helpers::consolidate( Helpers::allDeclarations(
			completionPosition, *pCompletionContext,
			firstWord ? pCodeCompletionContext.searchNamespaces() : QVector<Namespace*>(),
			pCodeCompletionContext.typeFinder(),
			*pCodeCompletionContext.rootNamespace().data(), firstWord ) );
	}
	
	addFunctionCalls();
	addAllMembers( mode );
// 	addAllTypes();
	
	// sort items
	/* not working... why?
	std::sort( pLocalItems.begin(), pLocalItems.end(), compareDeclarations );
	std::sort( pMemberItems.begin(), pMemberItems.end(), compareDeclarations );
	std::sort( pOperatorItems.begin(), pOperatorItems.end(), compareDeclarations );
	std::sort( pConstructorItems.begin(), pConstructorItems.end(), compareDeclarations );
	std::sort( pStaticItems.begin(), pStaticItems.end(), compareDeclarations );
	std::sort( pGlobalItems.begin(), pGlobalItems.end(), compareDeclarations );
	*/
	
	// add item groups
	// 
	// NOTE: it seems CompletionCustomGroupNode messes up in the UI if it has no items.
	//       not creating the group seems to be the only working fix for this problem
	addItemGroupNotEmpty( "Local", 100, pLocalItems );
	addItemGroupNotEmpty( "Class Members", 200, pMemberItems );
	addItemGroupNotEmpty( "Operators", 300, pOperatorItems );
	addItemGroupNotEmpty( "Object Construction", 350, pConstructorItems );
	addItemGroupNotEmpty( "Class Static", 400, pStaticItems );
	addItemGroupNotEmpty( "Global", 1000, pGlobalItems );
}

void DSCodeCompletionCodeBody::copyTokens( const TokenStream &in, TokenStream &out, int start, int end ){
	if( end < 0 ){
		end += in.size();
	}
	
	out.clear();
	int i;
	for( i=start; i<=end; i++ ){
		out.push( in.at( i ) );
	}
}

int DSCodeCompletionCodeBody::findFirstParseToken( const TokenStream& tokenStream, int lastIndex ) const{
	int i, countGroups = 0, countBlocks = 0;
	
	if( lastIndex < 0 ){
		lastIndex += tokenStream.size();
	}
	
	for( i=lastIndex; i>=0; i-- ){
		switch( tokenStream.at( i ).kind ){
		// these are potentially part of the sub expression
		case TokenType::Token_IDENTIFIER:
		case TokenType::Token_LINESPLICE:
		case TokenType::Token_PERIOD:
			break;
			
		// end of grouped expression. we can skip anything up to the start of the group
		case TokenType::Token_RPAREN:
			countGroups++;
			break;
			
		// start of grouped expression
		case TokenType::Token_LPAREN:
			if( countGroups-- > 0 ){
				break;
			}
			return i + 1;
			
		// end of statement block. we are looking for code blocks here but inside code blocks
		// more statement blocks can be found. for this reason we have to skip them all
		case TokenType::Token_END:
			countBlocks++;
			break;
			
		// start of statement blocks. we catch all here due to the reason above.
		// the last one has to be a block token
		case TokenType::Token_BLOCK:
		case TokenType::Token_FOR:
		case TokenType::Token_SELECT:
		case TokenType::Token_TRY:
		case TokenType::Token_WHILE:
			if( countBlocks-- > 0 ){
				break;
			}
			return i + 1;
			
		// the if token is a special case. it can show up in two situations which have to
		// be handled differently. if the if shows up at the start of a line it is like
		// above a statement block and shouldbe handled like above. if the if token is
		// though somewhere inside the line then it is an inline if-else. in this case it
		// is not a statement block and should not reduce countBlocks
		case TokenType::Token_IF:
			if( i == 0 || tokenStream.at( i - 1 ).kind == TokenType::Token_LINEBREAK ){
				// this is a statement block if
				if( countBlocks-- > 0 ){
					break;
				}
				return i + 1;
				
			}else{
				// this is an inline if-else
				if( countGroups > 0 || countBlocks > 0 ){
					break;
				}
				return i + 1;
			}
			
		// these are starting tokens. parsing has to start here unless we are in a group or block
		case TokenType::Token_THIS:
		case TokenType::Token_SUPER:
		case TokenType::Token_TRUE:
		case TokenType::Token_FALSE:
		case TokenType::Token_NULL:
		case TokenType::Token_LITERAL_BYTE:
		case TokenType::Token_LITERAL_FLOAT:
		case TokenType::Token_LITERAL_INTEGER:
		case TokenType::Token_LITERAL_STRING:
// 		case TokenType::Token_COMMA: // only happens inside parenthesis
			if( countGroups > 0 || countBlocks > 0 ){
				break;
			}
			return i;
			
		// all others belong to the previous sub expression. parsing has to start after
		// them unless we are in a group or block
		default:
			if( countGroups > 0 || countBlocks > 0 ){
				break;
			}
			return i + 1;
		}
	}
	
	return 0;
}

void DSCodeCompletionCodeBody::addFunctionCalls(){
	/*
	foreach( auto each, pAllDefinitions ){
		ClassFunctionDeclaration * const funcdecl = dynamic_cast<ClassFunctionDeclaration*>( each.first );
		if( funcdecl ){
			qDebug() << "function" << funcdecl->toString();
			DSCCItemFunctionCall * const item = new DSCCItemFunctionCall(
				*funcdecl, DeclarationPointer( each.first ), each.second, 0 );
			pCompletionItems << CompletionTreeItemPointer( item );
		}
	}
	*/
}

void DSCodeCompletionCodeBody::addAllMembers( Mode mode ){
	ClassDeclaration * const classDecl = Helpers::thisClassDeclFor( *pCompletionContext );
	DUContext *classContext = nullptr;
	if( classDecl ){
		classContext = classDecl->internalContext();
	}
	
	foreach( auto each, pAllDefinitions ){
		DSCodeCompletionItem * const item = new DSCodeCompletionItem( DeclarationPointer( each.first ), each.second );
		DUContext * const declContext = each.first->context();
		bool ignore = false;
		
		switch( mode ){
		case Mode::type:
			ignore = ( ! item->isType() && ! item->isStatic() )
				|| ! declContext->parentContext();
			break;
			
		case Mode::instance:
			ignore = ( item->isType() || item->isConstructor() )
				|| ! declContext->parentContext();
			break;
			
		default:
			break;
		}
		
		if( each.first->kind() == Declaration::NamespaceAlias ){
			ignore = true;
		}
		
		if( ignore ){
			delete item;
			continue;
		}
		
		if( item->isConstructor() ){
			pConstructorItems << CompletionTreeItemPointer( item );
			
		}else if( item->isOperator() ){
			pOperatorItems << CompletionTreeItemPointer( item );
			
		}else if( item->isStatic() ){
			pStaticItems << CompletionTreeItemPointer( item );
			
		}else if( each.first->context() ){
			const DUContext &context = *each.first->context();
			switch( context.type() ){
			case DUContext::Other:
			case DUContext::Function:
				pLocalItems << CompletionTreeItemPointer( item );
				break;
				
			case DUContext::Class:
			case DUContext::Enum:
				pMemberItems << CompletionTreeItemPointer( item );
				break;
				
			default:
				pGlobalItems << CompletionTreeItemPointer( item );
				break;
			}
			
		}else{
			pGlobalItems << CompletionTreeItemPointer( item );
		}
	}
}

void DSCodeCompletionCodeBody::addAllTypes(){
	foreach( auto each, pAllDefinitions ){
		if( each.first->kind() != Declaration::Kind::Type ){
			continue;
		}
		
		DSCodeCompletionItem * const item = new DSCodeCompletionItem( DeclarationPointer( each.first ), each.second );
		
		if( item->isStatic() ){
			pStaticItems << CompletionTreeItemPointer( item );
			
		}else{
			pCompletionItems << CompletionTreeItemPointer( item );
		}
	}
}

void DSCodeCompletionCodeBody::addItemGroupNotEmpty( const char *name, int priority,
const QList<CompletionTreeItemPointer> &items ){
	if( items.isEmpty() ){
		return;
	}
	
	CompletionCustomGroupNode * const group = new CompletionCustomGroupNode( name, priority );
	group->appendChildren( items );
	pCodeCompletionContext.addItemGroup( CompletionTreeElementPointer( group ) );
}

bool DSCodeCompletionCodeBody::compareDeclarations(
const CompletionTreeItemPointer &a, const CompletionTreeItemPointer &b ){
	return a->declaration()->toString().compare( b->declaration()->toString(), Qt::CaseInsensitive ) < 0;
}

}
