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
	DUChainPointer<const DUContext> pCurExprContext;
	AbstractType::Ptr pCurExprType;
	bool pEnableErrorReporting;
	bool pAllowVoidType;
	
	
	
public:
	UseBuilder( EditorIntegrator &editor, const QVector<ImportPackage::Ref> &deps );
	
	/** \brief Parser session. */
	inline ParseSession &parseSession() const{ return pParseSession; }
	
	/** \brief Enable error reporting. */
	inline bool getEnableErrorReporting() const{ return pEnableErrorReporting; }
	
	/** \brief Set if error reporting is enabled. */
	void setEnableErrorReporting( bool enable );
	
	
	
protected:
	void visitFullyQualifiedClassname( FullyQualifiedClassnameAst *node ) override;
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
	
	/**
	 * \brief Find context for function call object.
	 */
	DUChainPointer<const DUContext> functionGetContext( AstNode *node,
		DUChainPointer<const DUContext> context );
	
	/**
	 * \brief Type of node using ExpressionVisitor.
	 */
	AbstractType::Ptr typeOfNode( AstNode *node, DUChainPointer<const DUContext> context );
	
	/**
	 * \brief Check function call.
	 */
	void checkFunctionCall( AstNode *node, DUChainPointer<const DUContext> context,
		const AbstractType::Ptr &argument );
	
	void checkFunctionCall( AstNode *node, DUChainPointer<const DUContext> context,
		const QVector<AbstractType::Ptr> &signature );
	
	/**
	 * \brief Report semantic error if reporting is enabled.
	 */
	void reportSemanticError( const RangeInRevision &range, const QString &hint );
	
	/**
	 * \brief Report semantic error if reporting is enabled.
	 */
	void reportSemanticError( const RangeInRevision &range, const QString &hint,
		const QVector<IProblem::Ptr> &diagnostics );
	
	/**
	 * \brief Report semantic hint if reporting is enabled.
	 */
	void reportSemanticHint( const RangeInRevision &range, const QString &hint );
	
	
	
private:
	inline int& nextUseIndex()
	{
		return m_nextUseStack.top();
	}
	QStack<int> m_nextUseStack;
	
	/**
	 * \brief Get context at position or current content.
	 */
	DUContext *contextAtOrCurrent( const CursorInRevision &pos );
	
	QVector<IndexedString> m_ignoreVariables;
};

}

#endif
