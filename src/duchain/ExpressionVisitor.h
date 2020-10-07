#ifndef _EXPRESSIONVISITOR_H
#define _EXPRESSIONVISITOR_H

#include <QHash>
#include <KLocalizedString>

#include <language/duchain/types/abstracttype.h>
#include <language/duchain/types/integraltype.h>
#include <language/duchain/types/typesystemdata.h>
#include <language/duchain/types/typeregister.h>
#include <language/duchain/types/containertypes.h>
#include <language/duchain/duchainpointer.h>
#include <language/duchain/declaration.h>
#include <language/duchain/types/structuretype.h>
#include <language/duchain/classdeclaration.h>
#include <language/duchain/functiondeclaration.h>
#include <language/duchain/builders/dynamiclanguageexpressionvisitor.h>

#include "dsp_defaultvisitor.h"
#include "duchainexport.h"
#include "EditorIntegrator.h"
#include "Helpers.h"
#include "TypeFinder.h"


using namespace KDevelop;

namespace DragonScript {

class EditorIntegrator;

typedef DUChainPointer<FunctionDeclaration> FunctionDeclarationPointer;

/**
 * \brief Expression visitor.
 * 
 * \note DUChainReadLocker required.
 */
class KDEVDSDUCHAIN_EXPORT ExpressionVisitor : public DefaultVisitor,
public DynamicLanguageExpressionVisitor
{
private:
	const EditorIntegrator &pEditor;
	const CursorInRevision pCursorOffset;
	TypeFinder &pTypeFinder;
	
	/** \brief Void type is allowed. */
	bool pAllowVoid = false;
	
	/** \brief used by code completion to disable range checks on declaration searches. */
	bool m_forceGlobalSearching = false;
	
	/** \brief used by code completion to detect unknown NameAst elements in expressions. */
	bool m_reportUnknownNames = false;
	
	CursorInRevision m_scanUntilCursor = CursorInRevision::invalid();
	
	QSet<QString> m_unknownNames;
	
	bool pIsTypeName;
	
	const QVector<const TopDUContext*> &pPinnedNamespaces;
	
	
	
public:
	ExpressionVisitor( const EditorIntegrator &editorIntegrator, const DUContext *ctx,
		const QVector<const TopDUContext*> &pinnedNamespaces, TypeFinder &typeFinder,
		const CursorInRevision cursorOffset = CursorInRevision( 0, 0 ) );
	
	void visitExpressionConstant( ExpressionConstantAst *node ) override;
	void visitFullyQualifiedClassname( FullyQualifiedClassnameAst *node ) override;
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
	void visitStatementFor(StatementForAst *node) override;
	void visitStatementIf(StatementIfAst *node) override;
	void visitStatementReturn(StatementReturnAst *node) override;
	void visitStatementSelect(StatementSelectAst *node) override;
	void visitStatementThrow(StatementThrowAst *node) override;
	void visitStatementTry(StatementTryAst *node) override;
	void visitStatementVariableDefinitions(StatementVariableDefinitionsAst *node) override;
	void visitStatementWhile(StatementWhileAst *node) override;
	
	/** \brief Void type is allowed. */
	inline bool getAllowVoid() const{ return pAllowVoid; }
	
	/** \brief Set if void type is allowed. */
	void setAllowVoid( bool allowVoid );
	
	void enableGlobalSearching(){
		m_forceGlobalSearching = true;
	}

	void enableUnknownNameReporting(){
		m_reportUnknownNames = true;
	}

	void scanUntil( const CursorInRevision &end ){
		m_scanUntilCursor = end;
	}

	QSet<QString> unknownNames() const{
		return m_unknownNames;
	}
	
	/** \brief Last declaration is alias type. */
	bool isLastAlias() const;
	
	/** \brief Context matching last type found or nullptr. */
	const DUContext *lastContext() const;
	
	/** \brief Visited expression represents a type name not an object instance. */
	inline bool isTypeName() const{ return pIsTypeName; }
	
	
	
protected:
	/** \brief Report semantic hint if reporting is enabled. */
	void reportSemanticHint( const RangeInRevision &range, const QString &hint );
	
	/** \brief Simplify common calls. */
	void encounterDecl( Declaration &decl );
	void encounterInternalType( ClassDeclaration *classDecl );
	
	/** \brief Check function call. */
	void checkFunctionCall( AstNode &node, const DUContext &context, const AbstractType::Ptr &argument );
	
	void checkFunctionCall( AstNode &node, const DUContext &ctx, const QVector<AbstractType::Ptr> &signature );
	
	/**
	 * \brief Clear last type and declaration the visit node.
	 * 
	 * If node is not nullptr and after visiting the node a valid type is present true
	 * is returned otherwise calls encounterUnknown() and return false.
	 */
	bool clearVisitNode( AstNode *node );
	
	
	
private:
	void addUnknownName( const QString &name );
};

}

#endif // EXPRESSIONVISITOR_H
