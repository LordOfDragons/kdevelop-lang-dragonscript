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

DSCodeCompletionContext::DSCodeCompletionContext( DUContextPointer context,
	const QString& contextText, const QString& followingText,
	const CursorInRevision& position, int depth, const QUrl &document ) :
CodeCompletionContext( context, contextText /*extractLastExpression( contextText )*/, position, depth ),
	// extractLastExpression() can not be used because block blocks need to be parsed
	// and these spawn multiple non-spliced lines
pDocument( document ),
pFullText( contextText ),
pFollowingText( followingText )
{
// 	qDebug() << "KDevDScript: DSCodeCompletionContext():" << pFullText << m_position
// 		<< pFollowingText << m_duContext->range();
}



QString DSCodeCompletionContext::extractLastExpression( const QString &string ) const{
	int index = string.lastIndexOf( '\n' );
	while( index > 0 && string[ index - 1 ] == '\\' ){
		index = string.lastIndexOf( '\n', index - 1 );
	}
	return string.mid( max( index + 1, 0 ) );
}

QList<CompletionTreeItemPointer> DSCodeCompletionContext::completionItems(
bool &abort, bool fullCompletion ){
	QList<CompletionTreeItemPointer> items;
	
	DUChainReadLocker lock;
	const DUContextPointer context( m_duContext->findContextAt( m_position, true ) );
	if( ! context ){
		return items;
	}
	
	// if the end of the expression is located inside a string or comment do nothing.
	// later on this could be changed for example to allow adding tags in doxygen comments
	// or proposing comment escaping or more. but right now we just exit
	if( textEndsInsideCommentOrString( pFullText ) ){
        return items;
	}
	
	// tokenize the text into pTokenStream. this will be reused by different code
	// below to use the token stream to find the right completions to show
	tokenizeText( m_text );
// 	debugLogTokenStream();
	
	// depending on the context different completions are reasonable
	switch( context->type() ){
	case DUContext::ContextType::Global:
		// global script scope and inside "namespace", "pin" and "requires" definitions
		qDebug() << "DSCodeCompletionContext context type Global";
		break;
		
	case DUContext::ContextType::Namespace:
		// context that declares namespace members
		qDebug() << "DSCodeCompletionContext context type Namespace";
		break;
		
	case DUContext::ContextType::Class:
		// inside "class" or "interface" definition
		qDebug() << "DSCodeCompletionContext context type Class";
		break;
		
	case DUContext::ContextType::Function:
		// inside "func" definition
		qDebug() << "DSCodeCompletionContext context type Function";
		DSCodeCompletionCodeBody( *this, context, items, abort, fullCompletion ).completionItems();
		break;
		
	case DUContext::ContextType::Template:
		// not used
		qDebug() << "DSCodeCompletionContext context type Template";
		break;
		
	case DUContext::ContextType::Enum:
		// inside "enum" definition
		qDebug() << "DSCodeCompletionContext context type Enum";
		break;
		
	case DUContext::ContextType::Helper:
		// not used
		qDebug() << "DSCodeCompletionContext context type Helper";
		break;
		
	case DUContext::ContextType::Other:
		// inside code body of functions, blocks, conditionals, loops, switches,
		// try catching and variable definitions
		qDebug() << "DSCodeCompletionContext context type Other";
		DSCodeCompletionCodeBody( *this, context, items, abort, fullCompletion ).completionItems();
		break;
		
	default:
		break;
	}
	
	return items;
}

bool DSCodeCompletionContext::textEndsInsideCommentOrString( const QString &text ) const{
	const int end = text.size() - 1;
	bool inDoubleQuotes = false;
	bool inLineComment = false;
	bool inComment = false;
	bool inQuotes = false;
	int i;
	
	for( i=0; i<end; i++ ){
		const QChar c = text.at( i );
		const QChar next = text.at( i + 1 );
		if( inLineComment ){
			if( c == QLatin1Char( '\n' ) ){
				inLineComment = false;
			}
		}
		if( inComment ){
			if( c == QLatin1Char( '*' ) && next == QLatin1Char( '/' ) ){
				inComment = false;
			}
			
		}else if( inQuotes ){
			if( c != QLatin1Char( '\\' ) && next == QLatin1Char( '\'' ) ){
				inQuotes = false;
			}
			
		}else if( inDoubleQuotes ){
			if( c != QLatin1Char( '\\' ) && next == QLatin1Char( '"' ) ){
				inDoubleQuotes = false;
			}
			
		}else{
			if( c == QLatin1Char( '/' ) && next == QLatin1Char( '/' ) ){
				inLineComment = true;
				
			}else if( c == QLatin1Char( '/' ) && next == QLatin1Char( '*' ) ){
				inComment = true;
				
			}else if( next == QLatin1Char( '\'' ) ){
				inQuotes = true;
				
			}else if( next == QLatin1Char( '"' ) ){
				inDoubleQuotes = true;
			}
		}
	}
	
	return inLineComment || inComment || inQuotes || inDoubleQuotes;
}

void DSCodeCompletionContext::tokenizeText( const QString &expression ){
	pTokenStreamText = expression.toUtf8();
	KDevPG::QByteArrayIterator iterContent( pTokenStreamText );
	Lexer lexer( iterContent );
	bool endOfStream = false;
	
	while( ! endOfStream ){
		const Token &token = lexer.read();
		
		switch( token.kind ){
		case TokenType::Token_WHITESPACE:
		case TokenType::Token_LINESPLICE:
		case TokenType::Token_COMMENT_MULTILINE:
		case TokenType::Token_COMMENT_SINGLELINE:
		case TokenType::Token_DOC_COMMENT_MULTILINE:
		case TokenType::Token_DOC_COMMENT_SINGLELINE:
			break;
			
		case TokenType::Token_EOF:
			endOfStream = true;
			break;
			
		default:
			pTokenStream.push() = token;
		}
	}
}

void DSCodeCompletionContext::debugLogTokenStream( const QString &prefix ) const{
	const int count = pTokenStream.size();
	int i;
	for( i=0; i<count; i++ ){
		const Token &token = pTokenStream.at( i );
		qDebug() << qUtf8Printable( prefix ) << "kind=" << TokenText( token.kind )
			<< token.kind << "begin=" << token.begin << "end=" << token.end
			<< QString::fromLatin1( pTokenStreamText.mid( token.begin, token.end - token.begin + 1 ) );
	}
}

}
