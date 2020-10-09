#ifndef _USEBUILDER_H
#define _USEBUILDER_H

#include <QStack>
#include <language/duchain/builders/abstractusebuilder.h>

#include "duchainexport.h"
#include "dsp_ast.h"
#include "ContextBuilder.h"
#include "EditorIntegrator.h"


using namespace KDevelop;

namespace DragonScript{

class ParseSession;

typedef AbstractUseBuilder<AstNode, Identifier, ContextBuilder> UseBuilderBase;

class KDEVDSDUCHAIN_EXPORT UseBuilder : public UseBuilderBase{
private:
	ParseSession &pParseSession;
	DUContext *pCurExprContext;
	AbstractType::Ptr pCurExprType;
	bool pEnableErrorReporting;
	bool pAllowVoidType;
	bool pCanBeType;
	bool pAutoThis;
	Declaration *pIgnoreVariable;
	
	
	
public:
	UseBuilder( EditorIntegrator &editor, const QSet<ImportPackage::Ref> &deps,
		TypeFinder &typeFinder, Namespace::Ref &namespaceRef );
	
	/** Parser session. */
	inline ParseSession &parseSession() const{ return pParseSession; }
	
	/** Enable error reporting. */
	inline bool getEnableErrorReporting() const{ return pEnableErrorReporting; }
	
	/** Set if error reporting is enabled. */
	void setEnableErrorReporting( bool enable );
	
	
	
protected:
	void visitFullyQualifiedClassname( FullyQualifiedClassnameAst *node ) override;
	void visitPin( PinAst *node ) override;
	void visitNamespace( NamespaceAst *node ) override;
	void visitClassFunctionDeclareBegin( ClassFunctionDeclareBeginAst *node ) override;
	void visitExpression( ExpressionAst *node ) override;
	void visitExpressionConstant( ExpressionConstantAst *node ) override;
	void visitExpressionMember( ExpressionMemberAst *node ) override;
	void visitExpressionBlock( ExpressionBlockAst *node ) override;
	void visitExpressionAddition( ExpressionAdditionAst *node ) override;
	void visitExpressionAssign( ExpressionAssignAst *node ) override;
	void visitExpressionBitOperation( ExpressionBitOperationAst *node ) override;
	void visitExpressionCompare( ExpressionCompareAst *node ) override;
	void visitExpressionLogic( ExpressionLogicAst *node ) override;
	void visitExpressionMultiply( ExpressionMultiplyAst *node ) override;
	void visitExpressionPostfix( ExpressionPostfixAst *node ) override;
	void visitExpressionSpecial( ExpressionSpecialAst *node ) override;
	void visitExpressionUnary( ExpressionUnaryAst *node ) override;
	void visitExpressionInlineIfElse( ExpressionInlineIfElseAst *node ) override;
	void visitStatementFor( StatementForAst *node ) override;
	void visitStatementVariableDefinitions( StatementVariableDefinitionsAst *node ) override;
	
	/**
	 * Get context at position or current content.
	 * \note Internally locks DUChainReadLocker.
	 */
	DUContext *contextAtOrCurrent( const CursorInRevision &pos );
	
	/**
	 * Find context for function call object.
	 * \note Internally locks DUChainReadLocker.
	 */
	DUContext *functionGetContext( AstNode *node, DUContext *context );
	
	/** Type of node using ExpressionVisitor. */
	AbstractType::Ptr typeOfNode( AstNode *node, DUContext *context );
	
	/**
	 * Check function call.
	 * \note Internally locks DUChainReadLocker.
	 */
	void checkFunctionCall( AstNode *node, DUContext *context, const AbstractType::Ptr &argument );
	
	/**
	 * Check function call.
	 * \note Internally locks DUChainReadLocker.
	 */
	void checkFunctionCall( AstNode *node, DUContext *context, const QVector<AbstractType::Ptr> &signature );
	
	/**
	 * Report semantic error if reporting is enabled.
	 * \note Internally locks \em DUChainWriteLocker.
	 */
	void reportSemanticError( const RangeInRevision &range, const QString &hint );
	
	/**
	 * Report semantic error if reporting is enabled.
	 * \note Internally locks \em DUChainWriteLocker.
	 */
	void reportSemanticError( const RangeInRevision &range, const QString &hint,
		const QVector<IProblem::Ptr> &diagnostics );
	
	/**
	 * Report semantic hint if reporting is enabled.
	 * \note Internally locks \em DUChainWriteLocker.
	 */
	void reportSemanticHint( const RangeInRevision &range, const QString &hint );
};

}

#endif
