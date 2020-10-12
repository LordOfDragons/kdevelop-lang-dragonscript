#ifndef DSCODECOMPLETIONITEM_H
#define DSCODECOMPLETIONITEM_H

#include "DSCodeCompletionBaseItem.h"


using namespace KDevelop;

namespace DragonScript {

class DSCodeCompletionContext;

class DSCodeCompletionItem : public DSCodeCompletionBaseItem{
public:
	DSCodeCompletionItem( DSCodeCompletionContext &cccontext, DeclarationPointer declaration, int depth );
	
	
	void executed( KTextEditor::View *view, const KTextEditor::Range &word ) override;
	
	QVariant data( const QModelIndex& index, int role, const CodeCompletionModel* model ) const override;
	
	
	
protected:
	/**
	 * \brief Try function call completion.
	 * 
	 * If declaration is a function call applies completion and returns true otherwise returns false.
	 */
	bool completeFunctionCall( KTextEditor::View &view, const KTextEditor::Range &word );
};

}

#endif
