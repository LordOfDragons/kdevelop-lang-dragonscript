#ifndef _PARSEJOB_H_
#define _PARSEJOB_H_

#include <QSet>
#include <QVector>

#include <language/backgroundparser/parsejob.h>
#include <language/duchain/problem.h>
#include <language/duchain/topducontext.h>
#include <serialization/indexedstring.h>

#include <ktexteditor/range.h>

#include "DSProjectSettings.h"
#include "dsp_ast.h"
#include "ImportPackage.h"
#include "TypeFinder.h"
#include "Namespace.h"


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
		
		/**
		 * Phase 1 builds type declarations only (classes, interfaces, namespaces.
		 * This constant is here for documentation purpose and is not actively used.
		 */
		Phase1 = TopDUContext::LastFeature << 1,
		
		/**
		 * Phase 2 builds all declarations. Requires phase 1 to have run on all source
		 * files before phase 2 can be started.
		 */
		Phase2 = TopDUContext::LastFeature << 2,
		
		/**
		 * Phase 3 builds all uses. Requires phase 2 to have run on all source files
		 * before phase 3 can be started.
		 */
		Phase3 = TopDUContext::LastFeature << 3
	};
	
	
	explicit DSParseJob( const IndexedString &url, ILanguageSupport *languageSupport );
	
	void setParentJob( DSParseJob *job );
	
	static int phaseFlags( int phase );
	static int phaseFromFlags( int flags );
	
protected:
	void run( ThreadWeaver::JobPointer self, ThreadWeaver::Thread *thread ) override;
	
	bool prepare();
	bool checkAbort();
	bool verifyUpdateRequired();
	void prepareTopContext();
	void findPackage();
	void findDependencies();
	bool allFilesRequiredPhase();
	void reparseLater( int phase );
	bool buildDeclaration( EditorIntegrator &editor );
	bool buildUses( EditorIntegrator &editor );
	void parseFailed();
	void finishTopContext();
	
	
private:
	DSParseJob *pParentJob; ///< parent job if this one is an include
	DSProjectSettings pProjectSettings;
	StartAst *pStartAst;
	TypeFinder pTypeFinder;
	Namespace::Ref pRootNamespace;
	ImportPackage::Ref pPackage;
	QSet<ImportPackage::Ref> pDependencies;
	QSet<IndexedString> pProjectFiles;
	QStringList pProjectIncludePath;
	int pReparsePriority;
	QSet<IndexedString> pWaitForFiles;
	int pPhase;
	
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
