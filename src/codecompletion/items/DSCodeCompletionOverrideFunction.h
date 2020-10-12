#ifndef DSCODECOMPLETIONOVERRIDEFUNCTION_H
#define DSCODECOMPLETIONOVERRIDEFUNCTION_H

#include "DSCodeCompletionBaseItem.h"


using namespace KDevelop;

namespace DragonScript {

class DSCodeCompletionContext;

class DSCodeCompletionOverrideFunction : public DSCodeCompletionBaseItem{
public:
	DSCodeCompletionOverrideFunction( DSCodeCompletionContext &cccontext, DeclarationPointer declaration );
	
	void executed( KTextEditor::View *view, const KTextEditor::Range &word ) override;
	
	QVariant data( const QModelIndex& index, int role, const CodeCompletionModel* model ) const override;
};

}

#endif
