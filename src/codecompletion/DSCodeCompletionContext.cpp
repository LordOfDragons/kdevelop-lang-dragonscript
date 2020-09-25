#include <QDebug>
#include <KLocalizedString>
#include <KTextEditor/View>
#include <language/duchain/declaration.h>
#include <language/duchain/ducontext.h>
#include <language/duchain/use.h>

#include "dsp_ast.h"
#include "dsp_tokenstream.h"
#include "dsp_debugvisitor.h"

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
CodeCompletionContext( context, extractLastExpression( contextText ), position, depth ),
pDocument( document ),
pFullText( contextText ),
pFollowingText( followingText )
{
	qDebug() << "KDevDScript: DSCodeCompletionContext():" << pFullText << m_position
		<< pFollowingText << m_duContext->range();
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
	Q_UNUSED( fullCompletion );
	Q_UNUSED( abort );
	
	QList<CompletionTreeItemPointer> items;
	
	DUChainReadLocker lock;
	const DUContextPointer context( m_duContext->findContextAt( m_position ) );
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
	debugLogTokenStream();
	
	// test parsing
	{
	if( pTokenStream.size() == 0 || pTokenStream.back().kind != Parser::Token_PERIOD ){
		qDebug() << "not doing completion";
		return items;
	}
	
	const QByteArray ptext( m_text.toUtf8() );
	ParseSession session( IndexedString( pDocument ), ptext );
	EditorIntegrator editor( session );
	Parser parser;
	
	session.prepareCompletion( parser );
	
	// copy tokens except the final period
	copyTokens( pTokenStream, *session.tokenStream(), 0, -2 );
	
	// try parsing the token stream into an expression. this usually fails since the expression
	// is incomplete and can not be fully parsed. if the parsing fails remove one token from
	// the beginning and try again. at some point the expression will be short enough to be
	// valid. this should be the expression we have to evaluate to obtain the object type
	// to use for completion
	ExpressionAst *ast = nullptr;
	int i;
	for( i=0; i<session.tokenStream()->size(); i++ ){
		parser.rewind( i );
		if( parser.parseExpression( &ast ) && session.tokenStream()->index() == session.tokenStream()->size() ){
			break;
		}
		ast = nullptr;
	}
	
	if( ast ){
		DebugVisitor( session.tokenStream(), QString::fromLatin1( ptext ) ).visitNode( ast );
		
		ExpressionVisitor exprvisitor( editor, context.data() );
		exprvisitor.visitExpression( ast );
		
		if( exprvisitor.lastType() && exprvisitor.lastType()->whichType() == KDevelop::AbstractType::TypeStructure ){
			qDebug() << "exprVisitor lastType=" << exprvisitor.lastType()->toString()
				<< "lastDecl=" << ( exprvisitor.lastDeclaration().data() ? exprvisitor.lastDeclaration().data()->toString() : "-" );
		}
		/*
		if( exprvisitor.lastType() && exprvisitor.lastType()->whichType() == KDevelop::AbstractType::TypeStructure ){
			const StructureType::Ptr type = exprvisitor.lastType().cast<StructureType>();
			qDebug() << "found expression type" << type->toString();
		}
		*/
		

	}
	
#if 0
	KDevPG::MemoryPool pool;
	TokenStream pts;
	Parser parser;
	parser.setContents( &ptext );
	parser.setTokenStream( &pts );
	parser.setMemoryPool( &pool );
	parser.setDebug( true );
	parser.tokenize();
	
	/*
	StatementAst *astSta = nullptr;
	parser.parseStatement( &astSta );
	if( astSta ){
		DebugAst( pts, ptext ).visitStatement( astSta );
	}
	*/
	
	ExpressionInlineIfElseAst *ast = nullptr;
	parser.parseExpressionInlineIfElse( &ast );
	if( ast ){
		DebugVisitor( &pts, QString::fromLatin1( ptext ) ).visitNode( ast );
		
		ExpressionVisitor exprvisitor( *editor(), currentContext() );
		exprvisitor.visitNode( node->begin->extends );
		
		if( exprvisitor.lastType()
		&& exprvisitor.lastType()->whichType() == KDevelop::AbstractType::TypeStructure ){
			const StructureType::Ptr baseType = exprvisitor.lastType().cast<StructureType>();
			BaseClassInstance base;
			base.baseClass = baseType->indexed();
			base.access = KDevelop::Declaration::Public;
			decl->addBaseClass( base );
		}

	}
#endif
	}
	
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
		addCompletionCodeBody( items );
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
		addCompletionCodeBody( items );
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

void DSCodeCompletionContext::copyTokens( const TokenStream &in, TokenStream &out, int start, int end ){
	if( end < 0 ){
		end += in.size();
	}
	
	out.clear();
	int i;
	for( i=start; i<=end; i++ ){
		out.push( in.at( i ) );
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

void DSCodeCompletionContext::addCompletionCodeBody( QList<CompletionTreeItemPointer> &items ){
	// inside functions completion operates on expressions. this is usually a function call
	// but can be also the special "=" operator. in this case we consider it a function call
	// with the object type as only parameter. then we can parse everything as a function call
	
	( void )typeToMatch();
}



bool DSCodeCompletionContext::endsWithDot( const QString &str ) const{
	const int dotPos = str.lastIndexOf( '.' );
	if( dotPos == -1 ){
		return false;
	}
	
	int i;
	for( i=dotPos+1; i<str.size(); i++ ){
		if( ! str[ i ].isSpace() ){
			return false;
		}
	}
	
	return true;
}

KDevelop::AbstractType::Ptr DSCodeCompletionContext::typeToMatch() const{
	QStack<ExpressionStackEntry> stack = expressionStack( m_text );
	if( stack.empty() ){
		return KDevelop::AbstractType::Ptr();
	}
	
	{
		qDebug() << "DSCodeCompletionContext.typeToMatch: stack";
		foreach( const ExpressionStackEntry each, stack ){
			qDebug() << "- startPosition" << each.startPosition << "operatorStart"
				<< each.operatorStart << "operatorEnd" << each.operatorEnd << "commas"
				<< each.commas << "token '" << m_text.mid( each.operatorStart, each.operatorEnd - each.operatorStart ) << "'";
		}
	}
	
	const ExpressionStackEntry entry = stack.pop();
	if( entry.operatorStart <= entry.startPosition ){
		return KDevelop::AbstractType::Ptr();
	}
	
	const QString operatorText( m_text.mid( entry.operatorStart, entry.operatorEnd - entry.operatorStart ) );
	qDebug() << "operatorText" << operatorText;
	
	return KDevelop::AbstractType::Ptr();
}

QStack<DSCodeCompletionContext::ExpressionStackEntry>
DSCodeCompletionContext::expressionStack( const QString& expression ) const{
	QStack<ExpressionStackEntry> stack;
	QByteArray expr(expression.toUtf8());
	KDevPG::QByteArrayIterator iter(expr);
	Lexer lexer(iter);
	bool atEnd=false;
	ExpressionStackEntry entry;

	entry.startPosition = 0;
	entry.operatorStart = 0;
	entry.operatorEnd = 0;
	entry.commas = 0;

	stack.push(entry);

	qint64 line, lineEnd, column, columnEnd;
	while(!atEnd)
	{
		KDevPG::Token token(lexer.read());
		switch(token.kind)
		{
		case Parser::Token_EOF:
			atEnd=true;
			break;
		case Parser::Token_LPAREN:
			qint64 sline, scolumn;
			lexer.locationTable()->positionAt(token.begin, &sline, &scolumn);
			entry.startPosition = scolumn+1;
			entry.operatorStart = entry.startPosition;
			entry.operatorEnd = entry.startPosition;
			entry.commas = 0;

			stack.push(entry);
			break;
		case Parser::Token_RPAREN:
			if (stack.count() > 1) {
				stack.pop();
			}
			break;
		case Parser::Token_IDENTIFIER:
			//temporary hack to allow completion in variable declarations
			//two identifiers in a row is not possible?
			if(lexer.size() > 1 && lexer.at(lexer.index()-2).kind == Parser::Token_IDENTIFIER){
				lexer.locationTable()->positionAt(lexer.at(lexer.index()-2).begin, &line, &column);
				lexer.locationTable()->positionAt(lexer.at(lexer.index()-2).end+1, &lineEnd, &columnEnd);
				stack.top().operatorStart = column;
				stack.top().operatorEnd = columnEnd;
			}
			break;
		case Parser::Token_PERIOD:
			break;
		case Parser::Token_COMMA:
			stack.top().commas++;
			break;
		default:
			// The last operator of every sub-expression is stored on the stack
			// so that "A = foo." can know that attributes of foo having the same
			// type as A should be highlighted.
			qDebug() << token.kind;
			lexer.locationTable()->positionAt(token.begin, &line, &column);
			lexer.locationTable()->positionAt(token.end+1, &lineEnd, &columnEnd);
			stack.top().operatorStart = column;
			stack.top().operatorEnd = columnEnd;
		}
	}
	return stack;
}

}
