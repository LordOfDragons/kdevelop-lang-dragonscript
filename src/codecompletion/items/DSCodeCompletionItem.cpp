#include <QDebug>
#include <language/duchain/declaration.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/types/structuretype.h>
#include <KLocalizedString>
#include <KTextEditor/View>

#include "Helpers.h"
#include "DSCodeCompletionItem.h"
#include "DSCodeCompletionModel.h"
#include "DSCodeCompletionContext.h"


using namespace KDevelop;

namespace DragonScript {

DSCodeCompletionItem::DSCodeCompletionItem( DSCodeCompletionContext &cccontext,
	DeclarationPointer declaration, int depth ) :
DSCodeCompletionBaseItem( cccontext, declaration, depth ){
}

void DSCodeCompletionItem::executed( KTextEditor::View *view, const KTextEditor::Range &word ){
	if( completeFunctionCall( *view, word ) ){
		return;
	}
	
	DSCodeCompletionBaseItem::executed( view, word );
}

QVariant DSCodeCompletionItem::data( const QModelIndex &index, int role, const CodeCompletionModel *model ) const{
	switch( role ){
	case CodeCompletionModel::BestMatchesCount:
		return 0; //5;
		
	case CodeCompletionModel::MatchQuality:
		// perfect match (10), medium match (5), no match (QVariant())
		if( pAccessType == AccessType::Local ){
			return 5;
			
		}else if( pAccessType == AccessType::Global ){
			return 0;
			
		}else{
			return 3;
		}
	}
	
	return DSCodeCompletionBaseItem::data( index, role, model );
}



// Protected Functions
////////////////////////

bool DSCodeCompletionItem::completeFunctionCall( KTextEditor::View &view, const KTextEditor::Range &word ){
	Declaration * const decl = declaration().data();
	DUContext * const contextArguments = DUChainUtils::argumentContext( decl );
	if( ! contextArguments ){
		return false;
	}
	
	KTextEditor::Document &document = *view.document();
	const int column = word.end().column();
	const int line = word.end().line();
	
	// replace text with function name and parenthesis
	QString text( decl->identifier().toString() );
	
	KTextEditor::Range checkSuffix( line, column, line, document.lineLength( line ) );
	const QString checkText( document.text( checkSuffix ) );
	const bool hasLeftParan = checkText.startsWith( '(' );
	if( ! hasLeftParan ){
		text += "(";
	}
	
	// for some strange reasons this returns sometimes more declarations than function
	// arguments. it seems the context somehow picks up the code context and finds
	// variable declarations in there. it is beyond me how this is possible using
	// localDeclarations(). only work around so far is erasing all declaratins beyond
	// the function argument count.
	QVector<Declaration*> args( contextArguments->localDeclarations() );
	
	TypePtr<FunctionType> funcType( decl->type<FunctionType>() );
	if( funcType && args.size() > funcType->arguments().size() ){
		args.erase( args.begin() + funcType->arguments().size(), args.end() );
	}
	
	const bool hasRightParan = checkText.endsWith( ')' );
	if( ( ! hasLeftParan || ! hasRightParan ) && args.isEmpty() ){
		text += ")";
	}
	
	document.replaceText( word, text );
	AbstractType::Ptr type( decl->abstractType() );
	if( ! hasLeftParan && ! args.isEmpty() ){
		view.setCursorPosition( KTextEditor::Cursor( line, column + 1 ) );
	}
	
	// if arguments are present insert them using templating
	if( ! hasLeftParan && ! args.isEmpty() ){
		bool hasCursorTag = false;
		bool hasFields = false;
		text.clear();
		int i;
		
		for( i=0; i<args.size(); i++ ){
			if( i > 0 ){
				text += ", ";
			}
			
			StructureType::Ptr structType( args.at( i )->type<StructureType>() );
			QString argTypeName;
			if( structType ){
				argTypeName = structType->qualifiedIdentifier().toString();
			}
			
			if( argTypeName == "Block" ){
				text += QString( "block ${arg%1=\"Object %2\"}\n%3\t${cursor}\n%3end" )
					.arg( i ).arg( args.at( i )->identifier().toString() )
					.arg( lineIndent( view, line ) );
				hasFields = true;
				hasCursorTag = true;
				
			}else{
				text += QString( "${arg%1=\"%2 %3\"}" ).arg( i )
					.arg( tightTypeName( args.at( i )->abstractType() ) )
					.arg( args.at( i )->identifier().toString() );
				hasFields = true;
			}
		}
		
		text += ")";
		
		if( hasFields ){
			if( ! hasCursorTag ){
				text += "${cursor}";
			}
			view.insertTemplate( KTextEditor::Cursor( line, column + 1 ), text );
			// insertTemplate() can return false if inserting failed. this can be ignored
			
		}else{
			document.insertText( KTextEditor::Cursor( line, column + 1 ), text );
		}
	}
	
	return true;
}

}
