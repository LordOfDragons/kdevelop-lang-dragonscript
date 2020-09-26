#ifndef DSCODECOMPLETIONITEM_H
#define DSCODECOMPLETIONITEM_H

#include <language/codecompletion/normaldeclarationcompletionitem.h>
#include <language/codecompletion/codecompletionmodel.h>
#include <language/duchain/classfunctiondeclaration.h>


using namespace KDevelop;

namespace DragonScript {

class DSCodeCompletionItem : public NormalDeclarationCompletionItem{
public:
	DSCodeCompletionItem( DeclarationPointer declaration, int depth );
	
	
	void executed( KTextEditor::View *view, const KTextEditor::Range &word ) override;
	
	QVariant data( const QModelIndex& index, int role, const CodeCompletionModel* model ) const override;
	
	inline bool isConstructor() const{ return pIsConstructor; }
	inline bool isOperator() const{ return pIsOperator; }
	inline bool isStatic() const{ return pIsStatic; }
	inline bool isType() const{ return pIsType; }
	
	
	
protected:
	/**
	 * \brief Try function call completion.
	 * 
	 * If declaration is a function call applies completion and returns true otherwise returns false.
	 */
	bool completeFunctionCall( KTextEditor::View &view, const KTextEditor::Range &word );
	
	/**
	 * \brief Indent of line.
	 */
	QString lineIndent( KTextEditor::View &view, int line ) const;
	
private:
	QString pPrefix;
	CodeCompletionModel::CompletionProperties pCompletionProperties;
	bool pIsConstructor;
	bool pIsOperator;
	bool pIsStatic;
	bool pIsType;
};

}

#endif
