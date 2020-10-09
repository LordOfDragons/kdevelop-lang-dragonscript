#ifndef _CONTEXTBUILDER_H
#define _CONTEXTBUILDER_H

#include <QPair>

#include <language/duchain/builders/abstractcontextbuilder.h>
#include <language/editor/rangeinrevision.h>
#include <language/duchain/topducontext.h>
#include <serialization/indexedstring.h>

#include "dsp_defaultvisitor.h"
#include "duchainexport.h"
#include "ImportPackage.h"
#include "Namespace.h"


using namespace KDevelop;

namespace DragonScript{

class EditorIntegrator;

typedef AbstractContextBuilder<AstNode, IdentifierAst> ContextBuilderBase;

/**
 * \brief Context builder calculating scopes in file.
 *
 * For practical reasons, some building of scopes also happens in the declaration builder.
 */
class KDEVDSDUCHAIN_EXPORT ContextBuilder : public ContextBuilderBase, public DefaultVisitor{
private:
	EditorIntegrator *pEditor = nullptr;
	int pNamespaceContextCount = 0;
	QSet<ImportPackage::Ref> pDependencies;
	Namespace *pCurNamespace;
	QVector<Namespace*> pSearchNamespaces;
	QVector<Namespace*> pPinnedNamespaces;
	bool pRequiresRebuild = false;
	int pReparsePriority = 0;
	QSet<IndexedString> pWaitForFiles;
	TypeFinder *pTypeFinder = nullptr;
	Namespace::Ref *pRootNamespace = nullptr;
	
	
	
public:
	ContextBuilder() = default;
	
	inline EditorIntegrator *editor() const{ return pEditor; }
	void setEditor( EditorIntegrator *editor );
	
	inline TypeFinder *typeFinder() const{ return pTypeFinder; }
	void setTypeFinder( TypeFinder *typeFindef );
	
	inline Namespace::Ref &rootNamespace(){ return *pRootNamespace; }
	void setRootNamespace( Namespace::Ref &rootNamespace );
	
	inline const QSet<ImportPackage::Ref> &dependencies() const{ return pDependencies; }
	void setDependencies( const QSet<ImportPackage::Ref> &deps );
	
	inline Namespace *curNamespace() const{ return pCurNamespace; }
	void setCurNamespace( Namespace *ns );
	
	QVector<Namespace*> &searchNamespaces(){ return pSearchNamespaces; }
	const QVector<Namespace*> &searchNamespaces() const{ return pSearchNamespaces; }
	
	QVector<Namespace*> &pinnedNamespaces(){ return pPinnedNamespaces; }
	const QVector<Namespace*> &pinnedNamespaces() const{ return pPinnedNamespaces; }
	
	inline bool requiresRebuild() const{ return pRequiresRebuild; }
	void setRequiresRebuild( bool failed );
	
	inline int reparsePriority() const{ return pReparsePriority; }
	void setReparsePriority( int priority );
	
	inline QSet<IndexedString> &waitForFiles(){ return pWaitForFiles; }
	inline const QSet<IndexedString> &waitForFiles() const{ return pWaitForFiles; }
	
	void startVisiting( AstNode *node ) override;
	
	void preparePackage( ImportPackage &package );
	
	/** \brief Close namespace contexts. */
	void closeNamespaceContexts();
	
	/**
	 * \brief Set \p context as the context of \p node.
	 * The context is stored inside the AST itself.
	 */
	void setContextOnNode( AstNode *node, DUContext *context ) override;
	
	/**
	 * \brief Get the context set on \p node as previously set by \ref setContextOnNode.
	 */
	DUContext *contextFromNode( AstNode *node ) override;
	
	RangeInRevision editorFindRange( AstNode *fromNode, AstNode *toNode ) override;
	RangeInRevision editorFindRangeNode( AstNode *node );
	
	QualifiedIdentifier identifierForNode( IdentifierAst *node ) override;
	QualifiedIdentifier identifierForToken( qint64 token );
	
	TopDUContext *newTopContext( const RangeInRevision &range, ParsingEnvironmentFile *file ) override;
	
	
	void openContextClass( ClassAst *node );
	void openContextInterface( InterfaceAst *node );
	void openContextEnumeration( EnumerationAst *node );
	void openContextClassFunction( ClassFunctionDeclareAst *node );
	void openContextInterfaceFunction( InterfaceFunctionDeclareAst *node );
	void pinNamespace( PinAst *node );
	
	/*
	void visitNamespace( NamespaceAst *node ) override;
	void visitClass( ClassAst *node ) override;
	void visitInterface( InterfaceAst *node ) override;
	void visitEnumeration( EnumerationAst *node ) override;
	*/
};

}

#endif
