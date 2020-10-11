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
NormalDeclarationCompletionItem( declaration, QExplicitlySharedDataPointer<CodeCompletionContext>(), 0 ),
pCodeCompletionContext( cccontext ),
pProperties( CodeCompletionModel::Function | CodeCompletionModel::Virtual )
{
	const FunctionType::Ptr funcType = declaration->type<FunctionType>();
	if( funcType ){
		AbstractType::Ptr returnType = funcType->returnType();
		if( returnType ){
			pPrefix = returnType->toString();
			
		}else{
			pPrefix = "void";
		}
	}
	
	const DUChainPointer<ClassMemberDeclaration> membDecl = declaration.dynamicCast<ClassMemberDeclaration>();
	if( membDecl ){
		if( membDecl->accessPolicy() == Declaration::Public ){
			pProperties |= CodeCompletionModel::Public;
			
		}else if( membDecl->accessPolicy() == Declaration::Protected ){
			pProperties |= CodeCompletionModel::Protected;
			
		}else if( membDecl->accessPolicy() == Declaration::Private ){
			pProperties |= CodeCompletionModel::Private;
			
		}else{
			pProperties |= CodeCompletionModel::Public;
		}
	}
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
	TopDUContext * const topContext = decl->topContext();
	const int column = word.end().column();
	const int line = word.end().line();
	
	// replace text with function declaration
	const QString indent( lineIndent( *view, line ) );
	
	QString text;
	text += "/**\n";
	
	text += indent;
	text += " * Overrides ";
	text += typeName( declaration()->context()->owner()->abstractType() );
	text += ".";
	text += declaration()->identifier().toString();
	text += "()";
	text += "\n";
	
	text += indent;
	text += "*/";
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
		text += typeName( returnType );
		
	}else{
		text += "void";
	}
	text += " ";
	
	text += decl->identifier().toString();
	text += "(";
	
	const QVector<QPair<Declaration*, int>> args( contextArguments->allDeclarations(
		CursorInRevision::invalid(), topContext, false ) );
	int i;
	for( i=0; i<args.size(); i++ ){
		if( i > 0 ){
			text += ", ";
		}
		text += typeName( args[ i ].first->abstractType() );
		text += " ";
		text += args[ i ].first->identifier().toString();
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
	case Qt::DisplayRole:
		switch( index.column() ){
		case CodeCompletionModel::Prefix:
			return pPrefix;
		}
		break;
		
	case CodeCompletionModel::BestMatchesCount:
		return 0;
		
	case CodeCompletionModel::MatchQuality:
		// perfect match (10), medium match (5), no match (QVariant())
		return 5;
	}
	
	return NormalDeclarationCompletionItem::data( index, role, model );
}



// Protected Functions
////////////////////////

QString DSCodeCompletionOverrideFunction::lineIndent( KTextEditor::View &view, int line ) const{
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

CodeCompletionModel::CompletionProperties DSCodeCompletionOverrideFunction::completionProperties() const{
	// the properties calculate by NormalDeclarationCompletionItem are off for some reason
	// so we have to calculate them here on our own
	return pProperties;
}

QString DSCodeCompletionOverrideFunction::typeName( const AbstractType::Ptr &type ) const{
	DUContext * const classContext = pCodeCompletionContext.typeFinder().contextFor( type );
	if( ! classContext || ! classContext->owner() ){
		return type->toString();
	}
	
	const QualifiedIdentifier qid( classContext->owner()->qualifiedIdentifier() );
	const int count = qid.count();
	QString name;
	int i;
	
	for( i=0; i<count; i++ ){
		if( i > 0 ){
			name += ".";
		}
		name += qid.at( i ).toString();
	}
	
	return name;
}

}
