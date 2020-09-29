#ifndef _PARSEJOB_H_
#define _PARSEJOB_H_

#include <language/backgroundparser/parsejob.h>
#include <language/duchain/problem.h>

#include <QStringList>

#include <ktexteditor/range.h>

#include <language/duchain/topducontext.h>

#include "DSProjectSettings.h"
#include "dsp_ast.h"
#include "ImportPackage.h"


using namespace KDevelop;

namespace DragonScript{

struct AstNode;
class DSLanguageSupport;
class EditorIntegrator;

class DSParseJob : public ParseJob{
	Q_OBJECT
	
public:
	enum {
		Resheduled = TopDUContext::LastFeature,
		UpdateUses
	};
	
	explicit DSParseJob( const IndexedString &url, ILanguageSupport *languageSupport );
	~DSParseJob() override;
	
	void setParentJob( DSParseJob *job );
	
protected:
	void run( ThreadWeaver::JobPointer self, ThreadWeaver::Thread *thread ) override;
	
	bool checkAbort();
	bool verifyUpdateRequired();
	void prepareTopContext();
	void findPackage();
	void findDependencies();
	bool ensureDependencies();
	void reparseLater( int addFeatures = 0 );
	bool buildDeclaration( EditorIntegrator &editor );
	bool buildUses( EditorIntegrator &editor );
	void parseFailed();
	void finishTopContext();
	
	
private:
	DSParseJob *pParentJob; ///< parent job if this one is an include
	DSProjectSettings pProjectSettings;
	StartAst *pStartAst;
	ImportPackage::Ref pPackage;
	QVector<ImportPackage::Ref> pDependencies;
	
	/**
	 * Checks if a parent job parses already \p document. Used to prevent
	 * endless recursions in include statements
	 */
	bool hasParentDocument(const IndexedString &document);
	
	/** Create a problem pointer for the current document */
	ProblemPointer createProblem( const QString &description, AstNode *node,
		EditorIntegrator *editor, IProblem::Source source,
		IProblem::Severity severity = IProblem::Error );
};

}

#endif
