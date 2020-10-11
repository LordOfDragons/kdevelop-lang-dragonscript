#ifndef DSCODECOMPLETIONOVERRIDEFUNCTION_H
#define DSCODECOMPLETIONOVERRIDEFUNCTION_H

#include <language/codecompletion/normaldeclarationcompletionitem.h>
#include <language/codecompletion/codecompletionmodel.h>
#include <language/duchain/classfunctiondeclaration.h>


using namespace KDevelop;

namespace DragonScript {

class DSCodeCompletionContext;

class DSCodeCompletionOverrideFunction : public NormalDeclarationCompletionItem{
public:
	DSCodeCompletionOverrideFunction( DSCodeCompletionContext &cccontext, DeclarationPointer declaration );
	
	void executed( KTextEditor::View *view, const KTextEditor::Range &word ) override;
	
	QVariant data( const QModelIndex& index, int role, const CodeCompletionModel* model ) const override;
	
	
	
protected:
	/**
	 * \brief Indent of line.
	 */
	QString lineIndent( KTextEditor::View &view, int line ) const;
	
	CodeCompletionModel::CompletionProperties completionProperties() const override;
	
	QString typeName( const AbstractType::Ptr &type ) const;
	
	
	
private:
	DSCodeCompletionContext &pCodeCompletionContext;
	QString pPrefix;
	CodeCompletionModel::CompletionProperties pProperties;
};

}

#endif
