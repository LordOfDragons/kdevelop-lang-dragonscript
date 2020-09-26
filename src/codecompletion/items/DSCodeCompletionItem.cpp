#include <QDebug>
#include <language/duchain/declaration.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/types/structuretype.h>
#include <KLocalizedString>
#include <KTextEditor/View>

#include "DSCodeCompletionItem.h"
#include "DSCodeCompletionModel.h"
#include "DSCodeCompletionContext.h"


using namespace KDevelop;

namespace DragonScript {

static const QVector<QString> operatorNames = {
	"++", "--", "+", "-", "!", "~", "*", "/", "%", "<<", ">>", "&", "|", "^", "<", ">",
	"<=", ">=", "*=", "/=", "%=", "+=", "-=", "<<=", ">>=", "&=","|=","^="
};

DSCodeCompletionItem::DSCodeCompletionItem( DeclarationPointer declaration, int depth ) :
NormalDeclarationCompletionItem( declaration, QExplicitlySharedDataPointer<CodeCompletionContext>(), depth ),
pIsConstructor( false ),
pIsOperator( false ),
pIsStatic( false ),
pIsType( declaration->kind() == Declaration::Kind::Type )
{
	const DUChainPointer<ClassMemberDeclaration> membDecl = declaration.dynamicCast<ClassMemberDeclaration>();
	const FunctionType::Ptr funcType = declaration->type<FunctionType>();
	
	if( funcType ){
		pCompletionProperties = CodeCompletionModel::Function;
		
		AbstractType::Ptr returnType = funcType->returnType();
		if( returnType ){
			pPrefix = returnType->toString();
			
		}else{
			pPrefix = "void";
		}
		
		// constructor functions are:
		// - functions with name "new"
		// - static functions with return type matching owner class
		if( declaration->identifier().toString() == "new" ){
			pIsConstructor = true;
			
		}else if( returnType && membDecl && membDecl->isStatic()
		&& returnType->equals( declaration->context()->owner()->abstractType().data() ) ){
			pIsConstructor = true;
		}
		
		if( ! pIsConstructor ){
			pIsOperator = operatorNames.contains( declaration->identifier().toString() );
		}
		
	}else if( membDecl ){
		pPrefix = declaration->abstractType()->toString();
	}
	
	if( membDecl ){
		pIsStatic = pIsConstructor || membDecl->isStatic();
	}
}

void DSCodeCompletionItem::executed( KTextEditor::View *view, const KTextEditor::Range &word ){
	if( completeFunctionCall( *view, word ) ){
		return;
	}
	
	NormalDeclarationCompletionItem::executed( view, word );
}

QVariant DSCodeCompletionItem::data( const QModelIndex &index, int role, const CodeCompletionModel *model ) const{
	switch( role ){
	case Qt::DisplayRole:
		switch( index.column() ){
		case CodeCompletionModel::Prefix:
			return pPrefix;
		}
		break;
		
	case CodeCompletionModel::BestMatchesCount:
		return 0; //5;
		
	case CodeCompletionModel::MatchQuality:
		// perfect match (10), medium match (5), no match (QVariant())
		return 5;
	}
	
	return NormalDeclarationCompletionItem::data( index, role, model );
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
	TopDUContext * const topContext = decl->topContext();
	const int column = word.end().column();
	const int line = word.end().line();
	
	// replace text with function name and parenthesis
	QString text( decl->identifier().toString() );
	
	KTextEditor::Range checkSuffix( line, column, line, document.lineLength( line ) );
	if( ! document.text( checkSuffix ).startsWith( '(' ) ){
		text += "(";
	}
	
	const QVector<QPair<Declaration*, int>> args( contextArguments->allDeclarations(
		CursorInRevision::invalid(), topContext, false ) );
	if( args.isEmpty() ){
		text += ")";
	}
	
	document.replaceText( word, text );
	AbstractType::Ptr type( decl->abstractType() );
	if( ! args.isEmpty() ){
		view.setCursorPosition( KTextEditor::Cursor( line, column + 1 ) );
	}
	
	// if arguments are present insert them using templating
	if( ! args.isEmpty() ){
		bool hasCursorTag = false;
		bool hasFields = false;
		text.clear();
		int i;
		
		for( i=0; i<args.size(); i++ ){
			if( i > 0 ){
				text += ", ";
			}
			
			StructureType::Ptr structType = args[ i ].first->type<StructureType>();
			QString argTypeName;
			if( structType ){
				argTypeName = structType->qualifiedIdentifier().toString();
			}
			
			if( argTypeName == "Block" ){
				text += QString( "block ${arg%1=\"Object %2\"}\n%3\t${cursor}\n%3end" )
					.arg( i ).arg( args[ i ].first->identifier().toString() )
					.arg( lineIndent( view, line ) );
				hasFields = true;
				hasCursorTag = true;
				
			}else{
				text += QString( "${arg%1=\"%2\"}" ).arg( i ).arg( args[ i ].first->toString() );
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

QString DSCodeCompletionItem::lineIndent( KTextEditor::View &view, int line ) const{
	KTextEditor::Document &document = *view.document();
	const QString lineText( document.line( line ) );
	const int len = lineText.length();
	int i;
	
	for( i=0; i<len; i++ ){
		if( ! lineText[ i ].isSpace() ){
			break;
		}
	}
	
	return lineText.left( i );
}

}
