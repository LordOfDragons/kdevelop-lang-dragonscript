#ifndef DSCODECOMPLETIONBASEITEM_H
#define DSCODECOMPLETIONBASEITEM_H

#include <language/codecompletion/normaldeclarationcompletionitem.h>
#include <language/codecompletion/codecompletionmodel.h>


using namespace KDevelop;

namespace DragonScript {

class DSCodeCompletionContext;

class DSCodeCompletionBaseItem : public NormalDeclarationCompletionItem{
public:
	enum class AccessType{
		Local,
		Public,
		Protected,
		Private,
		Global
	};
	
	DSCodeCompletionBaseItem( DSCodeCompletionContext &cccontext, DeclarationPointer declaration, int depth );
	
	QVariant data( const QModelIndex& index, int role, const CodeCompletionModel* model ) const override;
	
	inline bool isConstructor() const{ return pIsConstructor; }
	inline bool isOperator() const{ return pIsOperator; }
	inline bool isStatic() const{ return pIsStatic; }
	inline bool isType() const{ return pIsType; }
	inline AccessType accessType() const{ return pAccessType; }
	
	
	
protected:
	/**
	 * Indent of line.
	 */
	QString lineIndent( KTextEditor::View &view, int line ) const;
	
	/**
	 * Tight type name. Shows only the type identifier without namespaces.
	 */
	QString tightTypeName( const AbstractType::Ptr &type );
	
	/**
	 * Short type name. Shows type identifier with shortest namespaces required to be unique.
	 */
	QString shortTypeName( const AbstractType::Ptr &type );
	
	/**
	 * Full type name. Shows type identifier with all namespaces.
	 */
	QString fullTypeName( const AbstractType::Ptr &type );
	
	CodeCompletionModel::CompletionProperties completionProperties() const override;
	
	
	
protected:
	DSCodeCompletionContext &pCodeCompletionContext;
	QString pPrefix;
	QString pName;
	QString pArguments;
	bool pIsConstructor;
	bool pIsOperator;
	bool pIsStatic;
	bool pIsType;
	AccessType pAccessType;
	CodeCompletionModel::CompletionProperties pProperties;
};

}

#endif
