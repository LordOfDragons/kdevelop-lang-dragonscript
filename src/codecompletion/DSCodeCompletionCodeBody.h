#ifndef DSCODECOMPLETIONCODEBODY_H
#define DSCODECOMPLETIONCODEBODY_H

#include <language/codecompletion/codecompletioncontext.h>
#include <language/duchain/duchainpointer.h>

#include "dsp_tokenstream.h"

#include <QStack>

using KDevelop::CodeCompletionContext;
using KDevelop::CompletionTreeItemPointer;
using KDevelop::CursorInRevision;
using KDevelop::DUContextPointer;

using KTextEditor::Range;
using KTextEditor::View;


namespace DragonScript {

class DSCodeCompletionContext;


/**
 * \brief Code completion for body code.
 */
class DSCodeCompletionCodeBody{
public:
	/** \brief Create code completion context. */
	DSCodeCompletionCodeBody( DSCodeCompletionContext &completionContext,
		const DUContextPointer &context, QList<CompletionTreeItemPointer> &completionItems,
		bool &abort, bool fullCompletion );
	
	/**
	 * \brief Request completion items.
	 */
	void completionItems();
	
	/**
	 * \brief Copy range of tokens from one token stream to the other.
	 */
	static void copyTokens( const TokenStream &in, TokenStream &out, int start, int end = -1 );
	
	/**
	 * \brief Find first token from end of token stream where parsing should begin.
	 * 
	 * We want to find the start of the right most sub expression.
	 */
	int findFirstParseToken( const TokenStream& tokenStream, int lastIndex = -1 ) const;
	
	
	
private:
	DSCodeCompletionContext &pCompletionContext;
	const DUContextPointer pContext;
	QList<CompletionTreeItemPointer> &pCompletionItems;
	bool &pAbort;
	const bool pFullCompletion;
};

}

#endif
