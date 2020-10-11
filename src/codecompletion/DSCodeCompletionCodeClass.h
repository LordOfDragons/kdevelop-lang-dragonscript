#ifndef DSCODECOMPLETIONCODECLASS_H
#define DSCODECOMPLETIONCODECLASS_H

#include <QStack>
#include <QVector>
#include <QPair>

#include <language/codecompletion/codecompletioncontext.h>
#include <language/duchain/duchainpointer.h>

#include "dsp_tokenstream.h"


using namespace KDevelop;

using KTextEditor::Range;
using KTextEditor::View;


namespace DragonScript {

class DSCodeCompletionContext;


/**
 * Code completion for class or interface body.
 */
class DSCodeCompletionCodeClass{
public:
	/** Create code completion context. */
	DSCodeCompletionCodeClass( DSCodeCompletionContext &completionContext,
		const DUContext &context, QList<CompletionTreeItemPointer> &completionItems,
		bool &abort, bool fullCompletion );
	
	/**
	 * Request completion items.
	 */
	void completionItems();
	
	/**
	 * Add not overriden super class funcctions.
	 */
	void addOverrideFunctions();
	
	
	
protected:
	void addItemGroupNotEmpty( const char *name, int priority, const QList<CompletionTreeItemPointer> &items );
	
	
	
private:
	DSCodeCompletionContext &pCodeCompletionContext;
	QList<CompletionTreeItemPointer> &pCompletionItems;
	
	const DUContext &pContext;
	bool &pAbort;
	const bool pFullCompletion;
	
	const DUContext *pCompletionContext;
	QVector<QPair<Declaration*, int>> pAllDefinitions;
	
	QList<CompletionTreeItemPointer> pOverrideItems;
};

}

#endif
