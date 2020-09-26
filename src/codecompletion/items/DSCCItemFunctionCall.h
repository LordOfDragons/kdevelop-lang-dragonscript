#ifndef DSCCITEMFUNCTIONCALL_H
#define DSCCITEMFUNCTIONCALL_H

#include <language/codecompletion/codecompletionworker.h>
#include <language/codecompletion/normaldeclarationcompletionitem.h>
#include <language/duchain/classfunctiondeclaration.h>


using namespace KDevelop;

namespace DragonScript {

class DSCCItemFunctionCall : public NormalDeclarationCompletionItem{
public:
	DSCCItemFunctionCall( ClassFunctionDeclaration &declClassFunc,
		DeclarationPointer declaration, int depth, int atArgument );
	
	
	void executed( KTextEditor::View* view, const KTextEditor::Range& word ) override;
	
	QVariant data( const QModelIndex& index, int role, const CodeCompletionModel* model ) const override;
	
	KTextEditor::CodeCompletionModel::CompletionProperties completionProperties() const override;
	
	int argumentHintDepth() const override;
	
	int inheritanceDepth() const override;
	
	inline const QString &getItemName() const{ return pItemName; }
	
	
	
private:
	int pDepth;
	int pAtArgument;
	int pCurrentArgStart;
	int pCurrentArgEnd;
	QString pPrefix;
	QString pArguments;
	bool pHasArguments;
	QString pItemName;
};

}

#endif
