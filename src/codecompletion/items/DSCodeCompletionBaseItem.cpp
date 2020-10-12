#include <QDebug>
#include <language/duchain/declaration.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/types/structuretype.h>
#include <KLocalizedString>
#include <KTextEditor/View>

#include "Helpers.h"
#include "DSCodeCompletionBaseItem.h"
#include "DSCodeCompletionModel.h"
#include "DSCodeCompletionContext.h"


using namespace KDevelop;

namespace DragonScript {

static const QVector<QString> operatorNames = {
	"++", "--", "+", "-", "!", "~", "*", "/", "%", "<<", ">>", "&", "|", "^", "<", ">",
	"<=", ">=", "*=", "/=", "%=", "+=", "-=", "<<=", ">>=", "&=","|=","^="
};

DSCodeCompletionBaseItem::DSCodeCompletionBaseItem( DSCodeCompletionContext &cccontext,
	DeclarationPointer declaration, int depth ) :
NormalDeclarationCompletionItem( declaration, QExplicitlySharedDataPointer<CodeCompletionContext>(), depth ),
pCodeCompletionContext( cccontext ),
pName( declaration->identifier().toString() ),
pIsConstructor( false ),
pIsOperator( false ),
pIsStatic( false ),
pIsType( declaration->kind() == Declaration::Kind::Type || declaration->kind() == Declaration::Namespace ),
pAccessType( AccessType::Local ),
pProperties( CodeCompletionModel::NoProperty )
{
	const DUChainPointer<ClassMemberDeclaration> membDecl = declaration.dynamicCast<ClassMemberDeclaration>();
	const FunctionType::Ptr funcType = declaration->type<FunctionType>();
	const bool parentIsNamespace = declaration->context() && declaration->context()->owner()
		&& declaration->context()->owner()->kind() == Declaration::Namespace;
	
	if( funcType ){
		AbstractType::Ptr returnType = funcType->returnType();
		if( returnType ){
			pPrefix = tightTypeName( returnType );
			
		}else{
			pPrefix = "void";
		}
		
		// constructor functions are:
		// - functions with name "new"
		// - static functions with return type matching owner class
		if( declaration->identifier().toString() == "new" ){
			pIsConstructor = true;
			
		}else if( returnType && membDecl && membDecl->isStatic() ){
			const ClassDeclaration * const classDecl = Helpers::thisClassDeclFor( *declaration->context() );
			if( classDecl && returnType->equals( classDecl->abstractType().data() ) ){
				pIsConstructor = true;
			}
		}
		
		if( ! pIsConstructor ){
			pIsOperator = operatorNames.contains( declaration->identifier().toString() );
		}
		
		pProperties = CodeCompletionModel::Function | CodeCompletionModel::Virtual;
		
		// CodeCompletionModel::Override can be used to signal an overriden function
		// TODO figure out when to add this flag somehow
		
		pArguments = "(";
		const QList<AbstractType::Ptr> arguments( funcType->arguments() );
		const int count = arguments.count();
		int i;
		for( i=0; i<count; i++ ){
			if( i > 0 ){
				pArguments += ", ";
			}
			pArguments += tightTypeName( arguments.at( i ) );
		}
		pArguments += ")";
		
	}else{
		if( declaration->kind() == Declaration::Namespace ){
			pAccessType = AccessType::Global;
			pProperties = CodeCompletionModel::Public | CodeCompletionModel::Namespace
				| CodeCompletionModel::NamespaceScope;
			
		}else if( declaration->kind() == Declaration::Type ){
			if( declaration->internalContext() ){
				const DUContext &context = *declaration->internalContext();
				if( context.type() == DUContext::Class ){
					pProperties = CodeCompletionModel::Class;
					
					DUChainPointer<ClassDeclaration> classDecl( declaration.dynamicCast<ClassDeclaration>() );
					if( classDecl ){
						if( classDecl->classType() == ClassDeclarationData::Interface ){
							pProperties |= CodeCompletionModel::Virtual;
						}
					}
					
				}else if( context.type() == DUContext::Enum ){
					pProperties = CodeCompletionModel::Enum;
				}
			}
			
		}else{
			pProperties |= CodeCompletionModel::Variable;
			
			if( declaration->abstractType() ){
				pPrefix = Helpers::formatShortestIdentifier( declaration->abstractType(),
					cccontext.searchNamespaces(), cccontext.typeFinder(), *cccontext.rootNamespace() );
			}
		}
	}
	
	if( membDecl ){
		if( declaration->kind() == Declaration::Namespace || parentIsNamespace ){
			pAccessType = AccessType::Global;
			
		}else{
			pIsStatic = pIsConstructor || membDecl->isStatic();
			if( pIsStatic ){
				pProperties |= CodeCompletionModel::Static;
			}
			
			bool isParentNamespace = false;
			if( declaration->internalContext() && declaration->internalContext()->owner() ){
				isParentNamespace = declaration->internalContext()->owner()->kind() == Declaration::Namespace;
			}
			
			if( isParentNamespace ){
				pAccessType = AccessType::Global;
				pProperties = CodeCompletionModel::Public;
				
			}else if( membDecl->accessPolicy() == Declaration::Public ){
				pAccessType = AccessType::Public;
				pProperties |= CodeCompletionModel::Public;
				
			}else if( membDecl->accessPolicy() == Declaration::Protected ){
				pAccessType = AccessType::Protected;
				pProperties |= CodeCompletionModel::Protected;
				
			}else if( membDecl->accessPolicy() == Declaration::Private ){
				pAccessType = AccessType::Private;
				pProperties |= CodeCompletionModel::Private;
				
			}else{
				pAccessType = AccessType::Public;
				pProperties |= CodeCompletionModel::Public;
			}
		}
		
	}else if( declaration->context() ){
		pAccessType = AccessType::Local;
		pProperties &= ~( CodeCompletionModel::Protected | CodeCompletionModel::Private );
		pProperties |= CodeCompletionModel::Public | CodeCompletionModel::LocalScope;
	}
}

QVariant DSCodeCompletionBaseItem::data( const QModelIndex &index, int role, const CodeCompletionModel *model ) const{
	switch( role ){
	case Qt::DisplayRole:
		switch( index.column() ){
		case CodeCompletionModel::Prefix:
			return pPrefix;
			
		case CodeCompletionModel::Name:
			return pName;
			
		case CodeCompletionModel::Arguments:
			return pArguments;
		}
		break;
		
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
	
	return NormalDeclarationCompletionItem::data( index, role, model );
}



// Protected Functions
////////////////////////

QString DSCodeCompletionBaseItem::lineIndent( KTextEditor::View &view, int line ) const{
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

QString DSCodeCompletionBaseItem::tightTypeName( const AbstractType::Ptr &type ){
	const ClassDeclaration * const classDecl = pCodeCompletionContext.typeFinder().declarationFor( type );
	if( classDecl ){
		return classDecl->identifier().toString();
		
	}else{
		return type->toString();
	}
}

QString DSCodeCompletionBaseItem::shortTypeName( const AbstractType::Ptr &type ){
	return Helpers::formatShortestIdentifier( type, pCodeCompletionContext.searchNamespaces(),
		pCodeCompletionContext.typeFinder(), *pCodeCompletionContext.rootNamespace() );
}

QString DSCodeCompletionBaseItem::fullTypeName( const AbstractType::Ptr &type ){
	const ClassDeclaration * const classDecl = pCodeCompletionContext.typeFinder().declarationFor( type );
	if( classDecl ){
		return Helpers::formatIdentifier( classDecl->qualifiedIdentifier() );
		
	}else{
		return type->toString();
	}
}

CodeCompletionModel::CompletionProperties DSCodeCompletionBaseItem::completionProperties() const{
	// the properties calculate by NormalDeclarationCompletionItem are off for some reason
	// so we have to calculate them here on our own
	return pProperties;
}

}
