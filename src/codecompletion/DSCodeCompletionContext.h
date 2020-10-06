#ifndef DSCODECOMPLETIONCONTEXT_H
#define DSCODECOMPLETIONCONTEXT_H

#include <QStack>

#include <language/codecompletion/codecompletioncontext.h>
#include <language/duchain/duchainpointer.h>

#include "codecompletionexport.h"
#include "dsp_tokenstream.h"

#include "ImportPackage.h"


using namespace KDevelop;

using KTextEditor::Range;
using KTextEditor::View;


namespace DragonScript {

/**
 * \brief Code completion context.
 */
class KDEVDSCODECOMPLETION_EXPORT DSCodeCompletionContext : public CodeCompletionContext{
public:
	/** \brief Create code completion context. */
	DSCodeCompletionContext( DUContextPointer context, const QString& contextText,
		const QString& followingText, const CursorInRevision& position, int depth,
		const QUrl &document );
	
	/**
	 * \brief Extract the last full expression from the string.
	 * 
	 * Usually the last expression is the last line in the text but can be the last N lines
	 * in the text if line splicing is used.
	 */
	QString extractLastExpression( const QString &string ) const;
	
	/**
	 * \brief Request completion items.
	 */
	QList<CompletionTreeItemPointer> completionItems( bool &abort, bool fullCompletion = true ) override;
	
	/**
	 * After completionItems(..) has been called, this may return completion-elements that
	 * are already grouped, for example using custom grouping(@see CompletionCustomGroupNode
	 */
	QList<CompletionTreeElementPointer> ungroupedElements() override;
	
	/**
	 * \brief Add grouped element.
	 */
	void addItemGroup( const CompletionTreeElementPointer& group );
	
	/**
	 * \brief Returns true if text ends inside a string or comment.
	 */
	bool textEndsInsideCommentOrString( const QString &text ) const;
	
	/**
	 * \brief Tokenize text into token stream.
	 */
	void tokenizeText( const QString &expression );
	
	/**
	 * \brief Log content token stream to debug output.
	 */
	void debugLogTokenStream( const QString &prefix = "DSCodeCompletionContext.tokenStream:" ) const;
	
	
	
	/** \brief Completion text before the cursor. */
	inline const QString &text() const{ return m_text; }
	
	/** \brief Document. */
	inline const QUrl &document() const{ return pDocument; }
	
	/** \brief Completion position in document. */
	inline const CursorInRevision &position() const{ return pPosition; }
	
	/** \brief The full text leading up to the last separation token ("." for example). */
	inline const QString &getFullText() const{ return pFullText; }
	
	/** \brief The text following after the last separation token (partial member name for example). */
	inline const QString &getFollowingText() const{ return pFollowingText; }
	
	/** \brief Token stream. */
	inline TokenStream &tokenStream(){ return pTokenStream; }
	
	/** \brief Token stream text. */
	inline const QByteArray &tokenStreamText() const{ return pTokenStreamText; }
	
	/** \brief Project files. */
	inline const QSet<IndexedString> &projectFiles() const{ return pProjectFiles; }
	
	/** \brief Reachable contexts. */
	inline const QVector<const TopDUContext*> &reachableContexts() const{ return pReachableContexts; }
	
	
	
protected:
	void findReachableContexts();
	
	
	
private:
	QUrl pDocument;
    CursorInRevision pPosition;
	IndexedString pIndexDocument;
	ImportPackage::Ref pPackage;
	QSet<IndexedString> pProjectFiles;
	
	// the full text leading up to the last separation token ("." for example)
	const QString pFullText;
	
	// the text following after the last separation token (partial member name for example)
	const QString pFollowingText;
	
	TokenStream pTokenStream;
	QByteArray pTokenStreamText;
	
	QList<CompletionTreeElementPointer> pItemGroups;
	
	QVector<const TopDUContext*> pReachableContexts;
};

}

#endif
