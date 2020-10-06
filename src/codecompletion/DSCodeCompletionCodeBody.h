#ifndef DSCODECOMPLETIONCODEBODY_H
#define DSCODECOMPLETIONCODEBODY_H

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
 * Code completion for body code.
 */
class DSCodeCompletionCodeBody{
public:
	/** Completion mode. */
	enum class Mode{
		/**
		 * Completion can be anything so showe anything.
		 */
		everything,
		
		/**
		 * Completion on class type. Show only inner types, static members and constructors.
		 */
		type,
		
		/**
		 * Completion on object instance. Show all methods except constructors. Show no inner types.
		 */
		instance
	};
	
	
	/** Create code completion context. */
	DSCodeCompletionCodeBody( DSCodeCompletionContext &completionContext,
		const DUContext &context, QList<CompletionTreeItemPointer> &completionItems,
		bool &abort, bool fullCompletion );
	
	/**
	 * Request completion items.
	 */
	void completionItems();
	
	/**
	 * Copy range of tokens from one token stream to the other.
	 */
	static void copyTokens( const TokenStream &in, TokenStream &out, int start, int end = -1 );
	
	/**
	 * Find first token from end of token stream where parsing should begin.
	 * 
	 * We want to find the start of the right most sub expression.
	 */
	int findFirstParseToken( const TokenStream& tokenStream, int lastIndex = -1 ) const;
	
	/**
	 * Add matching possible function calls.
	 */
	void addFunctionCalls();
	
	/**
	 * Add all members.
	 */
	void addAllMembers( Mode mode );
	
	/**
	 * Add types only.
	 */
	void addAllTypes();
	
	
	
protected:
	void addItemGroupNotEmpty( const char *name, int priority, const QList<CompletionTreeItemPointer> &items );
	
	
	
private:
	DSCodeCompletionContext &pCodeCompletionContext;
	
	const DUContext &pContext;
	QList<CompletionTreeItemPointer> &pCompletionItems;
	bool &pAbort;
	const bool pFullCompletion;
	
	const DUContext *pCompletionContext;
	QVector<QPair<Declaration*, int>> pAllDefinitions;
	
	QList<CompletionTreeItemPointer> pConstructorItems;
	QList<CompletionTreeItemPointer> pOperatorItems;
	QList<CompletionTreeItemPointer> pStaticItems;
};

}

#endif
