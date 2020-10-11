#include <ktexteditor/document.h>
#include <interfaces/icore.h>
#include <interfaces/ilanguagecontroller.h>
#include <interfaces/iprojectcontroller.h>
#include <language/backgroundparser/backgroundparser.h>
#include <KF5/KConfigCore/kconfiggroup.h>

#include "ContextBuilder.h"
#include "EditorIntegrator.h"
#include "Helpers.h"
#include "TypeFinder.h"
#include "ImportPackageLanguage.h"
#include "ImportPackageDragengine.h"
#include "ImportPackageDirectory.h"
#include "../DSLanguageSupport.h"


using namespace KDevelop;

namespace DragonScript{

void ContextBuilder::setEditor( EditorIntegrator *editor ){
	pEditor = editor;
}

void ContextBuilder::setTypeFinder( TypeFinder *typeFinder ){
	pTypeFinder = typeFinder;
}

void ContextBuilder::setRootNamespace( Namespace::Ref &rootNamespace ){
	pRootNamespace = &rootNamespace;
	pCurNamespace = rootNamespace.data();
}

void ContextBuilder::setDependencies( const QSet<ImportPackage::Ref> &deps ){
	pDependencies = deps;
}

void ContextBuilder::setCurNamespace( Namespace *ns ){
	pCurNamespace = ns;
}

void ContextBuilder::updateSearchNamespaces(){
	pSearchNamespaces.clear();
	
	Namespace *addNS = pCurNamespace;
	while( addNS && addNS->parent() && ! pSearchNamespaces.contains( addNS ) ){
		pSearchNamespaces << addNS;
		addNS = addNS->parent();
	}
	
	foreach( Namespace *pinned, pPinnedNamespaces ){
		addNS = pinned;
		while( addNS && addNS->parent() && ! pSearchNamespaces.contains( addNS ) ){
			pSearchNamespaces << addNS;
			addNS = addNS->parent();
		}
	}
}

void ContextBuilder::addPinnedUpdateNamespaces( Namespace *ns ){
	if( pPinnedNamespaces.contains( ns ) ){
		return;
	}
	
	pPinnedNamespaces << ns;
	
	while( ns && ns->parent() && ! pSearchNamespaces.contains( ns ) ){
		pSearchNamespaces << ns;
		ns = ns->parent();
	}
}

void ContextBuilder::setRequiresRebuild( bool rebuild ){
	pRequiresRebuild = rebuild;
}

void ContextBuilder::setReparsePriority( int priority ){
	pReparsePriority = priority;
}

void ContextBuilder::startVisiting( AstNode *node ){
	if( ! pTypeFinder ){
		return;
	}
	
	if( ! pDependencies.isEmpty() ){
		DUChainWriteLocker lock;
		foreach( const ImportPackage::Ref &each, pDependencies ){
			preparePackage( *each );
		}
		
		if( pRequiresRebuild ){
			return;
		}
	}
	
	if( ! *pRootNamespace ){
		*pRootNamespace = Namespace::Ref( new Namespace( *pTypeFinder ) );
	}
	pCurNamespace = pRootNamespace->data();
	
	// visit node to start building
	visitNode( node );
	closeNamespaceContexts();
}

void ContextBuilder::preparePackage( ImportPackage &package ){
// 	TopDUContext * const top = topContext();
	
	ImportPackage::State state;
	package.contexts( state );
	if( state.ready ){
		foreach( TopDUContext *each, state.importContexts ){
			pTypeFinder->searchContexts() << each;
		}
		
	}else{
		pRequiresRebuild = true;
		pReparsePriority = qMax( pReparsePriority, state.reparsePriority );
		pWaitForFiles.unite( state.waitForFiles );
// 		return;
	}
	
	const QSet<ImportPackage::Ref> &dependsOn = package.dependsOn();
	foreach( const ImportPackage::Ref &dependency, dependsOn ){
		preparePackage( *dependency );
	}
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
#if 0
	if( node->begin->argumentsSequence ){
		openContext( node->begin->argumentsSequence->front()->element,
			node->begin->argumentsSequence->back()->element, DUContext::Function );
		
	}else{
		if( node->begin->name ){
			const CursorInRevision location( pEditor->findPosition( *node->begin->name ) );
			openContext( node->begin->name, RangeInRevision( location, location ), DUContext::Function );
			
		}else{
			const CursorInRevision location( pEditor->findPosition( *node->begin->op ) );
			openContext( node->begin->op, RangeInRevision( location, location ), DUContext::Function );
		}
	}
#endif
	
	const CursorInRevision cursorBegin( node->begin->name
		? pEditor->findPosition( *node->begin->name, EditorIntegrator::BackEdge )
		: pEditor->findPosition( *node->begin->op, EditorIntegrator::BackEdge ) );
	const CursorInRevision cursorEnd( node->end
		? pEditor->findPosition( *node->end, EditorIntegrator::FrontEdge )
		: pEditor->findPosition( node->begin->endToken, EditorIntegrator::BackEdge ) );
	const RangeInRevision range( cursorBegin, cursorEnd );
	
	openContext( node, range, DUContext::Function ); //, node->begin->name );
}

void ContextBuilder::openContextInterfaceFunction( InterfaceFunctionDeclareAst *node ){
	// context starts at the end of the declaration
	CursorInRevision cursorBegin;
	QualifiedIdentifier identifier;
	
	if( node->begin->name ){
		cursorBegin = pEditor->findPosition( *node->begin->name, EditorIntegrator::BackEdge );
		identifier = identifierForNode( node->begin->name );
		
	}else{
		cursorBegin = pEditor->findPosition( *node->begin->op, EditorIntegrator::BackEdge );
		identifier = identifierForToken( node->begin->op->op );
	}
	
	const CursorInRevision cursorEnd( pEditor->findPosition( node->begin->endToken, EditorIntegrator::BackEdge ) );
	const RangeInRevision range( cursorBegin, cursorEnd );
	
	openContext( node, range, DUContext::Function, identifier );
}

void ContextBuilder::pinNamespace( PinAst *node ){
	if( ! node->name->nameSequence ){
		return;
	}
	
	/*
	const KDevPG::ListNode<IdentifierAst*> *iter = node->name->nameSequence->front();
	QList<IdentifierAst*, QualifiedIdentifier>> components;
	const KDevPG::ListNode<IdentifierAst*> *end = iter;
	QualifiedIdentifier identifier;
	
	do{
		identifier += Identifier( editor()->tokenText( *iter->element ) );
		components.insert( 0, QPair<IdentifierAst*, QualifiedIdentifier>( iter->element, identifier ) );
		iter = iter->next;
	}while( iter != end );
	
	// dragonscript allows searching in parent namespaces of pinned namespaces.
	// to get this working we need an alias definition for the pinned namespace
	// as well as for all parent namespaces. to get the right search order the
	// alias definitions have to be done in reverse order
	QList<QPair<IdentifierAst*, QualifiedIdentifier>>::const_iterator iterAlias;
	for( iterAlias = aliases.cbegin(); iterAlias != aliases.cend(); iterAlias++ ){
		const RangeInRevision range( editorFindRangeNode( iterAlias->first ) );
		NamespaceAliasDeclaration * const decl = openDeclaration<NamespaceAliasDeclaration>(
			globalImportIdentifier(), range, DeclarationFlags::NoFlags );
		eventuallyAssignInternalContext();
		decl->setKind( Declaration::NamespaceAlias );
		decl->setImportIdentifier( iterAlias->second );
		decl->setIdentifier( iterAlias->second.last() );
		decl->setInternalContext( openContext( iter->element, range, DUContext::Namespace, iterAlias->second ) );
		closeContext();
	}
	*/
}

}
