#include <ktexteditor/document.h>
#include <interfaces/icore.h>
#include <interfaces/ilanguagecontroller.h>
#include <interfaces/iprojectcontroller.h>
#include <language/backgroundparser/backgroundparser.h>
#include <KF5/KConfigCore/kconfiggroup.h>

#include "ContextBuilder.h"
#include "EditorIntegrator.h"
#include "Helpers.h"
#include "ImportPackageLanguage.h"
#include "ImportPackageDragengine.h"
#include "ImportPackageDirectory.h"
#include "../DSLanguageSupport.h"


using namespace KDevelop;

namespace DragonScript{

void ContextBuilder::setEditor( EditorIntegrator *editor ){
	pEditor = editor;
}

void ContextBuilder::setDependencies( const QSet<ImportPackage::Ref> &deps ){
	pDependencies = deps;
}

void ContextBuilder::setRequiresRebuild( bool rebuild ){
	pRequiresRebuild = rebuild;
}

void ContextBuilder::setReparsePriority( int priority ){
	pReparsePriority = priority;
}

void ContextBuilder::startVisiting( AstNode *node ){
// 	qDebug() << "KDevDScript: ContextBuilder::startVisiting";
	// add depdencies as imports
	if( ! dependencies().isEmpty() ){
		DUChainWriteLocker lock;
		TopDUContext * const top = topContext();
		QVector<QPair<TopDUContext*, CursorInRevision>> imports;
		
		foreach( const ImportPackage::Ref &each, dependencies() ){
// 			qDebug() << "DSParseJob.run: add import" << each->name() << "for" << document();
			
			ImportPackage::State state;
			each->contexts( state );
			if( state.ready ){
				foreach( TopDUContext *each, state.importContexts ){
					imports.append( qMakePair( each, CursorInRevision( 1, 0 ) ) );
				}
				
			}else{
// 				qDebug() << "KDevDScript DeclarationBuilder: failed adding dependency"
// 					<< each->name() << " as import for" << document();
				pRequiresRebuild = true;
				pReparsePriority = qMax( pReparsePriority, state.reparsePriority );
				pWaitForFiles.unite( state.waitForFiles );
// 				return;
			}
		}
		
		if( pRequiresRebuild ){
			return;
		}
		
		top->addImportedParentContexts( imports );
	}
	
	// visit node to start building
	visitNode( node );
	closeNamespaceContexts();
// 	qDebug() << "KDevDScript: ContextBuilder::startVisiting finished";
}

void ContextBuilder::setContextOnNode( AstNode *node, DUContext *context ){
	node->ducontext = context;
}

DUContext *ContextBuilder::contextFromNode( AstNode *node ){
	return node->ducontext;
}

RangeInRevision ContextBuilder::editorFindRange( AstNode *fromNode, AstNode *toNode ){
	return pEditor->findRange( *fromNode, *toNode );
}

RangeInRevision ContextBuilder::editorFindRangeNode( AstNode *node ){
	return pEditor->findRange( *node, *node );
}

QualifiedIdentifier ContextBuilder::identifierForNode( IdentifierAst *node ){
	return QualifiedIdentifier( pEditor->tokenText( *node ) );
}

QualifiedIdentifier ContextBuilder::identifierForToken( qint64 token ){
	return QualifiedIdentifier( pEditor->tokenText( token ) );
}

TopDUContext *ContextBuilder::newTopContext( const RangeInRevision &range,
ParsingEnvironmentFile *file ){
	// file is allowed to be null but some code in kdevelop does not seem to know this causing
	// segfaults if run. ensure the file is present
	if( ! file ){
		file = new ParsingEnvironmentFile( document() );
		file->setLanguage( IndexedString( "DragonScript" ) );
	}
	
	return AbstractContextBuilder::newTopContext( range, file );
}



void ContextBuilder::closeNamespaceContexts(){
// 	qDebug() << "KDevDScript: ContextBuilder::closeNamespaceContexts" << pNamespaceContextCount;
	while( pNamespaceContextCount > 0 ){
		pNamespaceContextCount--;
		closeContext();
	}
}

void ContextBuilder::openContextClass( ClassAst *node ){
	// context starts at the end of the declaration
	const CursorInRevision cursorBegin( pEditor->findPosition( *node->begin, EditorIntegrator::BackEdge ) );
	const CursorInRevision cursorEnd( node->end
		? pEditor->findPosition( *node->end, EditorIntegrator::FrontEdge )
		: CursorInRevision::invalid() );
	const RangeInRevision range( cursorBegin, cursorEnd );
	
	openContext( node, range, DUContext::Class, node->begin->name );
	
	DUChainWriteLocker lock;
	currentContext()->setLocalScopeIdentifier( identifierForNode( node->begin->name ) );
}

void ContextBuilder::openContextInterface( InterfaceAst *node ){
	// context starts at the end of the declaration
	const CursorInRevision cursorBegin( pEditor->findPosition( *node->begin, EditorIntegrator::BackEdge ) );
	const CursorInRevision cursorEnd( node->end
		? pEditor->findPosition( *node->end, EditorIntegrator::FrontEdge )
		: CursorInRevision::invalid() );
	const RangeInRevision range( cursorBegin, cursorEnd );
	
	openContext( node, range, DUContext::Class, node->begin->name );
	
	DUChainWriteLocker lock;
	currentContext()->setLocalScopeIdentifier( identifierForNode( node->begin->name ) );
}

void ContextBuilder::openContextEnumeration( EnumerationAst *node ){
	// context starts at the end of the declaration
	const CursorInRevision cursorBegin( pEditor->findPosition( *node->begin, EditorIntegrator::BackEdge ) );
	const CursorInRevision cursorEnd( node->end
		? pEditor->findPosition( *node->end, EditorIntegrator::FrontEdge )
		: CursorInRevision::invalid() );
	const RangeInRevision range( cursorBegin, cursorEnd );
	
	openContext( node, range, DUContext::Enum, node->begin->name );
	
	DUChainWriteLocker lock;
	currentContext()->setLocalScopeIdentifier( identifierForNode( node->begin->name ) );
}

void ContextBuilder::openContextClassFunction( ClassFunctionDeclareAst *node ){
	// context starts at the end of the declaration
	const CursorInRevision cursorBegin( node->begin->name
		? pEditor->findPosition( *node->begin->name, EditorIntegrator::BackEdge )
		: pEditor->findPosition( *node->begin->op, EditorIntegrator::BackEdge ) );
	const CursorInRevision cursorEnd( node->end
		? pEditor->findPosition( *node->end, EditorIntegrator::FrontEdge )
		: pEditor->findPosition( node->begin->endToken, EditorIntegrator::BackEdge ) );
	const RangeInRevision range( cursorBegin, cursorEnd );
	
	openContext( node, range, DUContext::Function, node->begin->name );
	
	DUChainWriteLocker lock;
	if( node->begin->name ){
		currentContext()->setLocalScopeIdentifier( identifierForNode( node->begin->name ) );
		
	}else if( node->begin->op ){
		currentContext()->setLocalScopeIdentifier( identifierForToken( node->begin->op->op ) );
	}
}

void ContextBuilder::openContextInterfaceFunction( InterfaceFunctionDeclareAst *node ){
	// context starts at the end of the declaration
	const CursorInRevision cursorBegin( node->begin->name
		? pEditor->findPosition( *node->begin->name, EditorIntegrator::BackEdge )
		: pEditor->findPosition( *node->begin->op, EditorIntegrator::BackEdge ) );
	const CursorInRevision cursorEnd(
		pEditor->findPosition( node->begin->endToken, EditorIntegrator::BackEdge ) );
	const RangeInRevision range( cursorBegin, cursorEnd );
	
	openContext( node, range, DUContext::Function, node->begin->name );
	
	if( node->begin->name ){
		currentContext()->setLocalScopeIdentifier( identifierForNode( node->begin->name ) );
		
	}else if( node->begin->op ){
		currentContext()->setLocalScopeIdentifier( identifierForToken( node->begin->op->op ) );
	}
}

}
