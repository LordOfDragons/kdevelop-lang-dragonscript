#include <QDebug>
#include <KLocalizedString>
#include <KTextEditor/View>
#include <language/duchain/declaration.h>
#include <language/duchain/ducontext.h>
#include <language/duchain/use.h>
#include <language/duchain/namespacealiasdeclaration.h>
#include <interfaces/icore.h>
#include <interfaces/isession.h>
#include <interfaces/ilanguagecontroller.h>
#include <interfaces/iprojectcontroller.h>

#include "dsp_ast.h"
#include "dsp_tokenstream.h"
#include "dsp_debugvisitor.h"

#include "DSCodeCompletionCodeBody.h"
#include "DSCodeCompletionContext.h"
#include "DSCodeCompletionModel.h"
#include "ExpressionVisitor.h"
#include "DumpChain.h"
#include "TokenText.h"
#include "ImportPackageLanguage.h"
#include "ImportPackageDragengine.h"
#include "../DSLanguageSupport.h"


using namespace KDevelop;

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
pPosition( position ),
pIndexDocument( document ),
pFullText( contextText ),
pFollowingText( followingText )
{
// 	qDebug() << "KDevDScript: DSCodeCompletionContext():" << pFullText << m_position
// 		<< pFollowingText << m_duContext->range();
	
	pPackage = DSLanguageSupport::self()->importPackages().packageContaining( pIndexDocument );
	
	IProject * const project = ICore::self()->projectController()->findProjectForUrl( document );
	if( project ){
		pProjectFiles = project->fileSet();
	}
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
	pItemGroups.clear();
	
	QList<CompletionTreeItemPointer> items;
	
	DUChainReadLocker lock;
	const DUContextPointer context( m_duContext->findContextAt( m_position, true ) );
	if( ! context ){
		return items;
	}
	lock.unlock();
	
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
	
	prepareTypeFinder();
	prepareNamespaces( context );
	
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
		DSCodeCompletionCodeBody( *this, *context, items, abort, fullCompletion ).completionItems();
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
		DSCodeCompletionCodeBody( *this, *context, items, abort, fullCompletion ).completionItems();
		break;
		
	default:
		break;
	}
	
	return items;
}

QList<CompletionTreeElementPointer> DSCodeCompletionContext::ungroupedElements(){
	return pItemGroups;
}

void DSCodeCompletionContext::addItemGroup( const CompletionTreeElementPointer& group ){
	pItemGroups << group;
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

void DSCodeCompletionContext::prepareTypeFinder(){
	DUChainReadLocker lock;
	QSet<IndexedString> files;
	
	if( pPackage ){
		files.unite( pPackage->files() );
		
	}else{
		files.unite( pProjectFiles );
	}
	
	const DUChain &duchain = *DUChain::self();
	
	// NOTE do not use duchain.chainForDocument(pIndexDocument). it is not going to work
	//      because the current document is altered for code completion and thus the
	//      chain returned for pIndexDocument is outdated and not working
	pTypeFinder.searchContexts() << m_duContext->topContext();
	
	foreach( const IndexedString &file, files ){
		if( file == pIndexDocument ){
			continue;
		}
		
		TopDUContext * const context = duchain.chainForDocument( file );
		if( context ){
			pTypeFinder.searchContexts() << context;
		}
	}
	
	if( pPackage ){
		foreach( const ImportPackage::Ref &depend, pPackage->dependsOn() ){
			preparePackage( *depend );
		}
		
	}else{
		preparePackage( *ImportPackageLanguage::self() );
		preparePackage( *ImportPackageDragengine::self() );
	}
}

void DSCodeCompletionContext::preparePackage( ImportPackage &package ){
	DUChain &duchain = *DUChain::self();
	
	foreach( const IndexedString &file, package.files() ){
		TopDUContext * const context = duchain.chainForDocument( file );
		if( context ){
			pTypeFinder.searchContexts() << context;
		}
	}
	
	const QSet<ImportPackage::Ref> &dependsOn = package.dependsOn();
	foreach( const ImportPackage::Ref &dependency, dependsOn ){
		preparePackage( *dependency );
	}
}

void DSCodeCompletionContext::prepareNamespaces( const DUContextPointer &context ){
	DUChainReadLocker lock;
	
	pRootNamespace = Namespace::Ref( new Namespace( pTypeFinder ) );
	
	// find current namespace and add it to search context
	DUContext *walkContext = context.data();
	while( walkContext ){
		if( walkContext->owner() && walkContext->owner()->kind() == Declaration::Namespace ){
			Namespace *addNS = pRootNamespace->getNamespace( walkContext->owner()->qualifiedIdentifier() );
			while( addNS && addNS->parent() && ! pSearchNamespaces.contains( addNS ) ){
				pSearchNamespaces << addNS;
				addNS = addNS->parent();
			}
			break;
		}
		walkContext = walkContext->parentContext();
	}
	
	// find pinned namespaces. for this find all NamespaceAlias declarations in the top
	// context located before the current position
	const QVector<Declaration*> declarations( context->topContext()->localDeclarations() );
	foreach( const Declaration *decl, declarations ){
		if( decl->kind() != Declaration::NamespaceAlias || decl->range().end > pPosition ){
			continue;
		}
		
		const NamespaceAliasDeclaration * const nsaDecl = static_cast<const NamespaceAliasDeclaration*>( decl );
		if( ! nsaDecl ){
			continue;
		}
		
		Namespace *addNS = pRootNamespace->getNamespace( nsaDecl->importIdentifier() );
		while( addNS && addNS->parent() && ! pSearchNamespaces.contains( addNS ) ){
			pSearchNamespaces << addNS;
			addNS = addNS->parent();
		}
	}
}

}
