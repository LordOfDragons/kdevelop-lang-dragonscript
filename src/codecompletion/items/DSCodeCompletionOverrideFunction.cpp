#include <QDebug>
#include <language/duchain/declaration.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/types/structuretype.h>
#include <KLocalizedString>
#include <KTextEditor/View>

#include "Helpers.h"
#include "DSCodeCompletionOverrideFunction.h"
#include "DSCodeCompletionModel.h"
#include "DSCodeCompletionContext.h"


using namespace KDevelop;

namespace DragonScript {

DSCodeCompletionOverrideFunction::DSCodeCompletionOverrideFunction(
	DSCodeCompletionContext &cccontext, DeclarationPointer declaration ) :
DSCodeCompletionBaseItem( cccontext, declaration, 0 ){
}

void DSCodeCompletionOverrideFunction::executed( KTextEditor::View *view, const KTextEditor::Range &word ){
	Declaration * const decl = declaration().data();
	DUContext * const contextArguments = DUChainUtils::argumentContext( decl );
	if( ! contextArguments ){
		return;
	}
	
	const DUChainPointer<ClassMemberDeclaration> membDecl = declaration().dynamicCast<ClassMemberDeclaration>();
	if( ! membDecl ){
		return;
	}
	
	const FunctionType::Ptr funcType = declaration()->type<FunctionType>();
	if( ! funcType ){
		return;
	}
	
	KTextEditor::Document &document = *view->document();
	const int column = word.end().column();
	const int line = word.end().line();
	
	// replace text with function declaration
	const QString indent( lineIndent( *view, line ) );
	
	QString text;
	text += "/**\n";
	
	text += indent;
	text += " * Overrides ";
	text += tightTypeName( declaration()->context()->owner()->abstractType() );
	text += ".";
	text += declaration()->identifier().toString();
	text += "()";
	text += "\n";
	
	text += indent;
	text += " */";
	text += "\n";
	
	text += indent;
	if( membDecl->accessPolicy() == Declaration::Private ){
		text += "private func ";
		
	}else if( membDecl->accessPolicy() == Declaration::Protected ){
		text += "protected func ";
		
	}else{
		text += "public func ";
	}
	
	AbstractType::Ptr returnType = funcType->returnType();
	if( returnType ){
		text += shortTypeName( returnType );
		
	}else{
		text += "void";
	}
	text += " ";
	
	text += decl->identifier().toString();
	text += "(";
	
	// for some strange reasons this returns sometimes more declarations than function
	// arguments. it seems the context somehow picks up the code context and finds
	// variable declarations in there. it is beyond me how this is possible using
	// localDeclarations(). only work around so far is erasing all declaratins beyond
	// the function argument count.
	QVector<Declaration*> args( contextArguments->localDeclarations() );
	
	if( funcType && args.size() > funcType->arguments().size() ){
		args.erase( args.begin() + funcType->arguments().size(), args.end() );
	}
	
	int i;
	for( i=0; i<args.size(); i++ ){
		if( i > 0 ){
			text += ", ";
		}
		text += shortTypeName( args.at( i )->abstractType() );
		text += " ";
		text += args.at( i )->identifier().toString();
	}
	text += ")";
	text += "\n";
	
	text += indent + "\t";
	text += "\n";
	
	text += indent;
	text += "end";
	
	document.replaceText( word, text );
	
	view->setCursorPosition( KTextEditor::Cursor( line + 4, column + 1 ) );
	
	return;
}

QVariant DSCodeCompletionOverrideFunction::data( const QModelIndex &index, int role, const CodeCompletionModel *model ) const{
	switch( role ){
	case CodeCompletionModel::BestMatchesCount:
		return 0;
		
	case CodeCompletionModel::MatchQuality:
		// perfect match (10), medium match (5), no match (QVariant())
		return 5;
	}
	
	return DSCodeCompletionBaseItem::data( index, role, model );
}

}
