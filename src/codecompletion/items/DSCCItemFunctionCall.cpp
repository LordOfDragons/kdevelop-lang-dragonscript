#include <QDebug>
#include <language/duchain/declaration.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/duchainutils.h>
#include <KLocalizedString>
#include <KTextEditor/View>

#include "DSCCItemFunctionCall.h"
#include "DSCodeCompletionModel.h"
#include "DSCodeCompletionContext.h"


using namespace KDevelop;

namespace DragonScript {

DSCCItemFunctionCall::DSCCItemFunctionCall( ClassFunctionDeclaration &declClassFunc,
	DeclarationPointer declaration, int depth, int atArgument ) :
NormalDeclarationCompletionItem( declaration, QExplicitlySharedDataPointer<CodeCompletionContext>(), 0 ),
pDepth( depth ),
pAtArgument( atArgument ),
pHasArguments( false ),
pItemName( declClassFunc.identifier().toString() )
{
	const FunctionType::Ptr funcType = declClassFunc.type<FunctionType>();
	
	const AbstractType::Ptr returnType = funcType->returnType();
	if( returnType ){
		pPrefix = returnType->toString();
		
	}else{
		pPrefix = "void";
	}
	
	pArguments = "(";
	int i;
	
	DUContext *argsContext = DUChainUtils::argumentContext( declaration.data() );
	if( argsContext ){
		QVector<QPair<Declaration*, int>> args = argsContext->allDeclarations(
			CursorInRevision::invalid(), declaration->topContext(), false );
		const int argCount = args.size();
		for( i=0; i<argCount; i++ ){
			const Declaration &declArg = *args.at( i ).first;
			
			if( i == atArgument ){
				pCurrentArgStart = pArguments.length();
			}
			
			pArguments += declArg.toString();
			
			if( i == atArgument ){
				pCurrentArgEnd = pArguments.length();
			}
			
			if( i < argCount - 1 ){
				pArguments += ", ";
			}
		}
	
	}else{
		const int argCount = funcType->arguments().size();
		
		for( i=0; i<argCount; i++ ){
			const AbstractType::Ptr type = funcType->arguments().at( i );
			
			if( i == atArgument ){
				pCurrentArgStart = pArguments.length();
			}
			
			pArguments += type->toString();
			
			if( i == atArgument ){
				pCurrentArgEnd = pArguments.length();
			}
			
			if( i < argCount - 1 ){
				pArguments += ", ";
			}
		}
	}
	
	pArguments += ")";
}

void DSCCItemFunctionCall::executed( KTextEditor::View *view, const KTextEditor::Range &word ){
	KTextEditor::Document &document = *view->document();
	QString suffix( "()" );
	KTextEditor::Range checkSuffix( word.end().line(), word.end().column(),
		word.end().line(), document.lineLength( word.end().line() ) );
	
	if( document.text( checkSuffix ).startsWith( '(' ) ){
		suffix.clear();
	}
	
	document.replaceText( word, declaration()->identifier().toString() + suffix );
	AbstractType::Ptr type( declaration()->abstractType() );
	if( pHasArguments ){
		view->setCursorPosition( KTextEditor::Cursor( word.end().line(), word.end().column() + 1 ) );
	}
}

QVariant DSCCItemFunctionCall::data( const QModelIndex &index, int role, const CodeCompletionModel *model ) const{
	switch( role ){
	case Qt::DisplayRole:
		switch( index.column() ){
		case CodeCompletionModel::Prefix:
			return pPrefix;
			
		case CodeCompletionModel::Arguments:
			return pArguments;
		}
		break;
		
	case CodeCompletionModel::BestMatchesCount:
		return 5;
		
	case CodeCompletionModel::MatchQuality:
		// perfect match (10), medium match (5), no match (QVariant())
		return 5;
		
	case CodeCompletionModel::CompletionRole:
		return ( int )completionProperties();
		
	case CodeCompletionModel::HighlightingMethod:
		if( index.column() == CodeCompletionModel::Arguments ){
			return ( int )CodeCompletionModel::CustomHighlighting;
		}
		break;
		
	case CodeCompletionModel::CustomHighlight:
		if( index.column() == CodeCompletionModel::Arguments && pAtArgument != -1 ){
			QTextFormat format;
			format.setBackground( QBrush( QColor::fromRgb( 142, 186, 255 ) ) ); // same color as kdev-python
			format.setProperty( QTextFormat::FontWeight, 99 );
			
			return QVariantList()
				<< pCurrentArgStart
				<< pCurrentArgEnd - pCurrentArgStart
				<< format;
		}
		break;
	}
	
	return NormalDeclarationCompletionItem::data( index, role, model );
}

CodeCompletionModel::CompletionProperties DSCCItemFunctionCall::completionProperties() const{
	return CodeCompletionModel::Function;
}

int DSCCItemFunctionCall::argumentHintDepth() const{
	return pDepth;
}

int DSCCItemFunctionCall::inheritanceDepth() const{
	return 0;
}

}
