#ifndef _DECLARATIONBUILDER_H
#define _DECLARATIONBUILDER_H

#include <language/duchain/builders/abstracttypebuilder.h>
#include <language/duchain/builders/abstractdeclarationbuilder.h>

#include <QList>

#include "ContextBuilder.h"
#include "dsp_defaultvisitor.h"
#include "duchainexport.h"
#include "dsp_defaultvisitor.h"


using namespace KDevelop;

namespace DragonScript{

class CorrectionHelper;
class TokenStream;
class EditorIntegrator;
class ParseSession;

typedef AbstractTypeBuilder<AstNode, IdentifierAst, ContextBuilder> TypeBuilderBase;
typedef AbstractDeclarationBuilder<AstNode, IdentifierAst, TypeBuilderBase> DeclarationBuilderBase;

class KDEVDSDUCHAIN_EXPORT DeclarationBuilder : public DeclarationBuilderBase{

private:
	const ParseSession &pParseSession;
	QList<bool> pNamespaceContexts;
	int pPhase;
	int pLastModifiers;
	
	
	
public:
	DeclarationBuilder( EditorIntegrator &editor, const ParseSession &parseSession,
		const QSet<ImportPackage::Ref> &deps, int phase );
	~DeclarationBuilder() override;
	
	/** Close namespace contexts. */
	void closeNamespaceContexts();
	
	/**
	 * Find all existing declarations for the identifier \p node.
	 * \note DUChainReadLocker required.
	 */
	QList<Declaration*> existingDeclarationsForNode( IdentifierAst *node );
	
	/** Get documentation for node. */
	QString getDocumentationForNode( const AstNode &node ) const;
	
	void visitScript( ScriptAst *node ) override;
    void visitScriptDeclaration( ScriptDeclarationAst *node ) override;
	void visitPin( PinAst *node ) override;
	void visitRequires( RequiresAst *node ) override;
	void visitNamespace( NamespaceAst *node ) override;
	void visitClass( ClassAst *node ) override;
	void visitClassBodyDeclaration( ClassBodyDeclarationAst *node ) override;
	void visitClassVariablesDeclare( ClassVariablesDeclareAst *node ) override;
	void visitClassFunctionDeclare( ClassFunctionDeclareAst *node ) override;
	void visitInterface( InterfaceAst *node ) override;
    void visitInterfaceBodyDeclaration( InterfaceBodyDeclarationAst *node ) override;
	void visitInterfaceFunctionDeclare( InterfaceFunctionDeclareAst *node ) override;
	void visitEnumeration( EnumerationAst *node ) override;
	void visitEnumerationBody( EnumerationBodyAst *node ) override;
	void visitExpressionBlock( ExpressionBlockAst *node ) override;
	void visitStatementIf( StatementIfAst *node ) override;
	void visitStatementElif( StatementElifAst *node ) override;
	void visitStatementSelect( StatementSelectAst *node ) override;
	void visitStatementCase( StatementCaseAst *node ) override;
	void visitStatementFor( StatementForAst *node ) override;
	void visitStatementWhile( StatementWhileAst *node ) override;
	void visitStatementTry( StatementTryAst *node ) override;
	void visitStatementCatch( StatementCatchAst *node ) override;
	void visitStatementVariableDefinitions( StatementVariableDefinitionsAst *node ) override;
	void visitTypeModifier( TypeModifierAst *node ) override;
	
	
	
protected:
	/** Access policy from last modifiers. */
	ClassMemberDeclaration::AccessPolicy accessPolicyFromLastModifiers() const;
};

}

#endif
