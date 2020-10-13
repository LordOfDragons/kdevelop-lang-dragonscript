#include <QDebug>
#include <KLocalizedString>
#include <KTextEditor/View>
#include <language/duchain/declaration.h>
#include <language/duchain/ducontext.h>
#include <language/duchain/use.h>

#include "dsp_ast.h"
#include "dsp_tokenstream.h"
#include "dsp_debugvisitor.h"

#include "DSCodeCompletionCodeClass.h"
#include "DSCodeCompletionContext.h"
#include "DSCodeCompletionModel.h"
#include "ExpressionVisitor.h"
#include "DumpChain.h"
#include "TokenText.h"
#include "Helpers.h"
#include "items/DSCodeCompletionOverrideFunction.h"


using namespace KDevelop;

using KTextEditor::Range;
using KTextEditor::View;

namespace DragonScript {

DSCodeCompletionCodeClass::DSCodeCompletionCodeClass( DSCodeCompletionContext &completionContext,
	const DUContext &context, QList<CompletionTreeItemPointer> &completionItems,
	bool &abort, bool fullCompletion ) :
pCodeCompletionContext( completionContext ),
pCompletionItems( completionItems ),
pContext( context ),
pAbort( abort ),
pFullCompletion( fullCompletion )
{
}



void DSCodeCompletionCodeClass::completionItems(){
	const Declaration *completionDecl = nullptr;
	AbstractType::Ptr completionType;
	
	pCompletionContext = nullptr;
	pAllDefinitions.clear();
	
	{
	DUChainReadLocker lock;
	pCompletionContext = &pContext;
	completionDecl = Helpers::thisClassDeclFor( pContext );
	if( completionDecl ){
		completionType = completionDecl->abstractType();
	}
	}
	
	if( ! completionDecl || ! completionType || ! pCompletionContext ){
// 		qDebug() << "DSCodeCompletionCodeClass: can not determine completion type";
		pCompletionContext = nullptr;
		return;
	}
	
	// do completion
// 	qDebug() << "DSCodeCompletionCodeClass: completion context" << completionDecl->toString();
	
	DUChainReadLocker lock;
	
	pAllDefinitions = Helpers::consolidate( Helpers::allDeclarations( CursorInRevision::invalid(),
		*pCompletionContext, {}, pCodeCompletionContext.typeFinder(),
		*pCodeCompletionContext.rootNamespace(), false ), *pCompletionContext );
	
	addOverrideFunctions();
	
	// add item groups
	// 
	// NOTE: it seems CompletionCustomGroupNode messes up in the UI if it has no items.
	//       not creating the group seems to be the only working fix for this problem
	addItemGroupNotEmpty( "Override Function", 100, pOverrideItems );
}

void DSCodeCompletionCodeClass::addOverrideFunctions(){
	const TokenStream &tokenStream = pCodeCompletionContext.tokenStream();
	if( tokenStream.size() > 0
	&& tokenStream.at( tokenStream.size() - 1 ).kind != TokenType::Token_LINEBREAK ){
		// overrides show only if at the beginning of a line with no token typed
		return;
	}
	
	ClassDeclaration * const classDecl = Helpers::thisClassDeclFor( *pCompletionContext );
	if( ! classDecl ){
		return;
	}
	
	DUContext * const classContext = classDecl->internalContext();
	if( ! classContext ){
		return;
	}
	
	foreach( auto each, pAllDefinitions ){
		if( ! each.first->isFunctionDeclaration() ){
			continue;
		}
		if( each.first->context() == classContext ){
			continue;
		}
		
		ClassMemberDeclaration * const membDecl = dynamic_cast<ClassMemberDeclaration*>( each.first );
		if( ! membDecl ){
			continue;
		}
		
		if( membDecl->isStatic() ){
			continue;
		}
		if( membDecl->identifier().toString() == "new" ){
			continue;
		}
		
		const FunctionType::Ptr funcType = each.first->type<FunctionType>();
		if( ! funcType ){
			continue;
		}
		
		pOverrideItems << CompletionTreeItemPointer( new DSCodeCompletionOverrideFunction(
			pCodeCompletionContext, DeclarationPointer( each.first ) ) );
	}
}

void DSCodeCompletionCodeClass::addItemGroupNotEmpty( const char *name, int priority,
const QList<CompletionTreeItemPointer> &items ){
	if( items.isEmpty() ){
		return;
	}
	
	CompletionCustomGroupNode * const group = new CompletionCustomGroupNode( name, priority );
	group->appendChildren( items );
	pCodeCompletionContext.addItemGroup( CompletionTreeElementPointer( group ) );
}

}
