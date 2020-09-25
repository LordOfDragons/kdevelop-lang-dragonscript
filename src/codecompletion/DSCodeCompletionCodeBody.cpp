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


using KDevelop::CodeCompletionWorker;
using KDevelop::CodeCompletionContext;
using KDevelop::CompletionTreeItemPointer;
using KDevelop::CursorInRevision;
using KDevelop::DUContext;
using KDevelop::DUContextPointer;
using KDevelop::DUChainReadLocker;
using KDevelop::Use;
using KDevelop::StructureType;

using KTextEditor::Range;
using KTextEditor::View;



namespace DragonScript {

DSCodeCompletionCodeBody::DSCodeCompletionCodeBody( DSCodeCompletionContext &completionContext,
	const DUContextPointer &context, QList<CompletionTreeItemPointer> &completionItems,
	bool &abort, bool fullCompletion ) :
pCompletionContext( completionContext ),
pContext( context ),
pCompletionItems( completionItems ),
pAbort( abort ),
pFullCompletion( fullCompletion )
{
}



void DSCodeCompletionCodeBody::completionItems(){
	TokenStream &tokenStream = pCompletionContext.tokenStream();
	if( tokenStream.size() == 0 ){
		return;
	}
	
	const QByteArray ptext( pCompletionContext.text().toUtf8() );
	ParseSession session( IndexedString( pCompletionContext.document() ), ptext );
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
	
	DUContextPointer completionContext;
	DeclarationPointer completionDecl;
	AbstractType::Ptr completionType;
	
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
		ExpressionVisitor exprvisitor( editor, pContext.data() );
		exprvisitor.visitExpression( ast );
		if( ! exprvisitor.lastType() || ! exprvisitor.lastDeclaration() ){
			qDebug() << "DSCodeCompletionCodeBody: can not determine completion type";
			return;
		}
		
		/*
		if( exprvisitor.lastType() && exprvisitor.lastType()->whichType() == KDevelop::AbstractType::TypeStructure ){
			qDebug() << "DSCodeCompletionCodeBody: completion type " << exprvisitor.lastType()->toString()
				<< "declaration " << ( exprvisitor.lastDeclaration().data() ? exprvisitor.lastDeclaration().data()->toString() : "-" );
		}
		*/
		
		completionType = exprvisitor.lastType();
		completionDecl = exprvisitor.lastDeclaration();
		completionContext = completionDecl->context();
		
	}else{
		// completion at the first word. assume context type and declaration
		completionContext = pContext;
		completionDecl = pContext->owner();
		if( completionDecl ){
			completionType = completionDecl->abstractType();
		}
	}
	
	if( ! completionDecl || ! completionType || ! completionContext ){
		qDebug() << "DSCodeCompletionCodeBody: can not determine completion type";
		return;
	}
	
	// do completion
	qDebug() << "DSCodeCompletionCodeBody: completion context " << completionDecl.data()->toString();
	
	// TODO
	/*
	const QVector<QPair<Declaration*, int>> declarations = completionDecl->context()->allDeclarations(
		CursorInRevision::invalid(), completionDecl->context()->topContext(), true );
	*/
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

}
