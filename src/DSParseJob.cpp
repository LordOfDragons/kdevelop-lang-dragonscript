#include <QFile>
#include <QReadWriteLock>
#include <QDirIterator>

#include <ktexteditor/document.h>

#include <language/duchain/duchainlock.h>
#include <language/duchain/duchain.h>
#include <language/duchain/topducontext.h>
#include <interfaces/icore.h>
#include <interfaces/isession.h>
#include <interfaces/ilanguagecontroller.h>
#include <interfaces/iprojectcontroller.h>
#include <language/backgroundparser/backgroundparser.h>
#include <language/backgroundparser/urlparselock.h>
#include <language/editor/documentrange.h>

#include <QtCore/QReadLocker>
#include <QtCore/QThread>
#include <language/duchain/duchainutils.h>

#include <KF5/KConfigCore/kconfiggroup.h>

#include <mutex>

#include "DSParseJob.h"
#include "ParseSession.h"
#include "DSLanguageSupport.h"
#include "DSSessionSettings.h"
#include "dsp_debugvisitor.h"
#include "DeclarationBuilder.h"
#include "EditorIntegrator.h"
#include "DumpChain.h"
#include "UseBuilder.h"
#include "Helpers.h"
#include "ImportPackageLanguage.h"
#include "ImportPackageDragengine.h"
#include "ImportPackageDirectory.h"
#include "DelayedParsing.h"


using namespace KDevelop;

namespace DragonScript{

static const IndexedString languageString( "DragonScript" );

DSParseJob::DSParseJob( const IndexedString &url, ILanguageSupport *languageSupport ) :
ParseJob( url, languageSupport ),
pParentJob( nullptr ),
pStartAst( nullptr )
{
// 	DelayedParsing::self().setDebugEnabled( true );
	
	// accprdomg to milian project object is only allowed to be accessed in the constructor
	IProject * const project = ICore::self()->projectController()->findProjectForUrl( url.toUrl() );
	if( project ){
		pProjectFiles = project->fileSet();
		
		const KConfigGroup config( project->projectConfiguration()->group( "dragonscriptsupport" ) );
		pProjectIncludePath = config.readEntry( "pathInclude", QStringList() );
	}
}

void DSParseJob::run( ThreadWeaver::JobPointer self, ThreadWeaver::Thread *thread ){
	Q_UNUSED(self)
	Q_UNUSED(thread)
	
	const QReadLocker parseLock( languageSupport()->parseLock() );
	pReparsePriority = parsePriority();
// 	qDebug() << "DSParseJob: RUN phase" << phaseFromFlags(minimumFeatures()) << "priority" << parsePriority() << "for" << document();
	
	// prepare for parsing
	if( checkAbort() || ! prepare() ){
		return;
	}
// 	qDebug() << "DSParseJob.run: start phase" << pPhase << "for" << document();
	
	// features set if user forces a reload of the file:
	// - SimplifiedVisibleDeclarationsAndContexts
	// - VisibleDeclarationsAndContexts
	// - AllDeclarationsAndContexts
	// - AllDeclarationsContextsAndUses
	// - ForceUpdate
	// 
	// not set:
	// - AST
	// - Recursive
	// - ForceUpdateRecursive
	// - all custom flags
	// 
	// priority is best priority
	// 
	// while loading the duchain no parsing seems to happen. for this reason we use the absence
	// of all custom flags and the presence of ForceUpdate as signal to start fresh
// 	qDebug() << "DSParseJob: RUN flags=" << minimumFeatures() << "phase"
// 		<< phaseFromFlags(minimumFeatures()) << "priority" << parsePriority() << "for" << document();
// 	if( minimumFeatures() & ( TopDUContext::ForceUpdate )
// 	&& ! ( minimumFeatures() & ( Resheduled | Phase1 | Phase2 | Phase3 ) ) ){
// 		// force phase to 1 
// 	}
	
	// ForceUpdate seems to be present in scheduled reparsing although the flag is not set.
	// seems to be added automatically. we can not use it thus for anything. Instead check
	// if Resheduled flag and phase flags are missing. 
// 	if( minimumFeatures() & TopDUContext::ForceUpdate ){
// 		return 1;
// 	}
	
	// parse document
	ParseSession session( document(), contents().contents );
	//session.setDebug( true );
	
	pStartAst = nullptr;
	if( session.parse( &pStartAst ) ){
		if( checkAbort() ){
			return;
		}
		
		// dragonscript works a bit different than other languages. script files always
		// belong to a namespace and all files in the namespace are visible without
		// needing an import statement
		findPackage();
		findDependencies();
		
		// verify all files in the package or project are on the same phase or higher
		if( ! allFilesRequiredPhase() ){
			abortJob();
			reparseLater( pPhase );
			return;
		}
		
// 		qDebug() << "DSParseJob.run: dependencies ready for" << document();
		EditorIntegrator editor( session );
		
// 		qDebug() << "DSParseJob.run: build declaration phase" << pPhase << "features" << minimumFeatures() << "for" << document();
		if( ! buildDeclaration( editor ) ){
			abortJob();
			reparseLater( pPhase );
			return;
		}
		if( checkAbort() ){
			return;
		}
		
		if( pPhase > 2 ){
			if( ! buildUses( editor ) ){
				abortJob();
				reparseLater( pPhase );
				return;
			}
			if( checkAbort() ){
				return;
			}
		}
		
		highlightDUChain();
		
		qDebug() << "DSParseJob.run: finished phase" << pPhase << "for" << document();
		
		if( pPhase < 3 ){
			if( duChain() ){
				// use builder updates features, other phases not. fix this here
				const int features = minimumFeatures() | phaseFlags( pPhase );
				DUChainWriteLocker lock;
				duChain()->setFeatures( static_cast<TopDUContext::Features>( features ) );
			}
			
			pReparsePriority += 10;
			reparseLater( pPhase + 1 );
		}
		
	}else{
		parseFailed();
	}
	
	if( checkAbort() ){
		return;
	}
	
	// The parser might have given us some syntax errors, which are now added to the document.
	{
	DUChainWriteLocker lock;
	foreach( const ProblemPointer &p, session.problems() ){
		duChain()->addProblem( p );
	}
	}
	
	finishTopContext();
	
	DelayedParsing::self().parsingFinished( document() );
	
	DUChain::self()->emitUpdateReady( document(), duChain() );
}

bool DSParseJob::prepare(){
	// read content
	if( readContents() ){
		qDebug() << "DSParseJob.run: readContents() failed for" << document();
		abortJob();
		return false;
	}
	
	// verify update requirements
	if( verifyUpdateRequired() ){
// 		qDebug() << "DSParseJob.run: no update required for" << document();
		return false;
	}
	
	// determine the phase to use for parsing
	pPhase = phaseFromFlags( minimumFeatures() );
	
	// prepare top context
	prepareTopContext();
	return true;
}

bool DSParseJob::checkAbort(){
	if( abortRequested() || ICore::self()->shuttingDown() ){
		abortJob();
		return true;
	}
	return false;
}

bool DSParseJob::verifyUpdateRequired(){
	if( minimumFeatures() & Resheduled ){
		// reuse top context on rescheduled parsing as much as possible.
		// 
		// WARNING we have to put this check up ahead because KDevelop for some strange reason
		//         fabricates TopDUContext::ForceUpdate into all rescheduled parsing requests
		//         even though it is explicitly not used
		DUChainWriteLocker lock;
		foreach( const ParsingEnvironmentFilePointer &file, DUChain::self()->allEnvironmentFiles( document() ) ){
			if( file->language() == languageString && file->topContext() ){
				setDuChain( file->topContext() );
			}
			break;
		}
	}
	
	if( minimumFeatures() & ( TopDUContext::ForceUpdate | Resheduled ) ){
		return false;
	}
	
	{
	const UrlParseLock parseLock( document() );
	if( isUpdateRequired( languageString ) ){
		return false;
	}
	}
	
	DUChainWriteLocker lock;
	foreach( const ParsingEnvironmentFilePointer &file, DUChain::self()->allEnvironmentFiles( document() ) ){
		if( file->language() != languageString ){
			continue;
		}
		
		if( ! file->needsUpdate() && file->featuresSatisfied( minimumFeatures() ) && file->topContext() ){
			setDuChain( file->topContext() );
			if( ICore::self()->languageController()->backgroundParser()->trackerForUrl( document() ) ){
				lock.unlock();
				highlightDUChain();
			}
			return true;
		}
		break;
	}
	
	return false;
}

void DSParseJob::prepareTopContext(){
	{
	DUChainWriteLocker lock;
	setDuChain( DUChainUtils::standardContextForUrl( document().toUrl() ) );
	}
	
	if( ! duChain() ){
		return;
	}
	
	translateDUChainToRevision( duChain() );
	
	DUChainWriteLocker lock;
	duChain()->setRange( RangeInRevision( 0, 0, INT_MAX, INT_MAX ) );
}

void DSParseJob::findPackage(){
	// find the package the source file belongs to. if the package can be found then
	// this file is parsed as a dependency of some other file. if the package can not
	// be found then this is a project file to be parsed
	pPackage = DSLanguageSupport::self()->importPackages().packageContaining( document() );
}

void DSParseJob::findDependencies(){
	ImportPackages &importPackages = DSLanguageSupport::self()->importPackages();
	
	pDependencies.clear();
	
	// if the file belongs to a package add the dependencies of the package
	if( pPackage ){
		pDependencies.unite( pPackage->dependsOn() );
		return;
	}
	
	// otherwise this is a project document
	
	// add language import package
	pDependencies << ImportPackageLanguage::self();
	
	// add dragengine import package
	pDependencies << ImportPackageDragengine::self();
	
	// add import package for each project import set by the user
	/*
	const KSharedConfigPtr config( project->projectConfiguration() );
	foreach( const QString &each, config->additionalConfigSources() ){
		qDebug() << "KDevDScript ContextBuilder: additional config sources" << each;
	}
	*/
	
	foreach( const QString &dir, pProjectIncludePath ){
		const QString name( QString( "#dir#" ) + dir );
		ImportPackage::Ref package( importPackages.packageNamed( name ) );
		if( ! package ){
			package = ImportPackage::Ref( new ImportPackageDirectory( name, dir ) );
			importPackages.addPackage( package );
		}
		pDependencies << package;
	}
}

bool DSParseJob::allFilesRequiredPhase(){
	// verify all other files in the same package or project have phase which is are least
	// one less than the parsing phase. hence all other files finished the previouis phase.
	// this check is not done for phase 1 since this is the first phase
	if( pPhase < 2 ){
		return true;
	}
	
	DUChainReadLocker lock;
	QSet<IndexedString> files;
	
	if( pPackage ){
		files.unite( pPackage->files() );
		
	}else{
		files.unite( pProjectFiles );
	}
	
	BackgroundParser &bp = *ICore::self()->languageController()->backgroundParser();
	DUChain &duchain = *DUChain::self();
	const int minPhase = pPhase - 1;
	bool allPassed = true;
	
	pTypeFinder.searchContexts() << duchain.chainForDocument( document() );
	
	foreach( const IndexedString &file, files ){
		if( file == document() ){
			continue;
		}
		
		TopDUContext * const context = duchain.chainForDocument( file );
		if( ! context ){
			// file has no context. we do not know yet what phase it has
// 			qDebug() << "DSParseJob.run: no context for" << file << "for" << document();
			
			if( bp.isQueued( file ) ){
				pReparsePriority = qMax( pReparsePriority, bp.priorityForDocument( file ) );
				
			}else if( ! DelayedParsing::self().isWaiting( file ) ){
				const int features = TopDUContext::VisibleDeclarationsAndContexts
					| DSParseJob::Resheduled
					| DSParseJob::phaseFlags( 1 );
				
				ICore::self()->languageController()->backgroundParser()->addDocument( file,
					static_cast<TopDUContext::Features>( features ), 0, nullptr,
					ParseJob::IgnoresSequentialProcessing, 10 );
			}
			pWaitForFiles << file;
			allPassed = false;
			continue;
			//return false;
		}
		
		const int phase = phaseFromFlags( context->features() );
		if( phase < minPhase ){
			// context not at the required phase yet
// 			qDebug() << "DSParseJob.run: phase" << phase << "file" << file << "too low" << pPhase << "for" << document();
			
			if( bp.isQueued( file ) ){
				pReparsePriority = qMax( pReparsePriority, bp.priorityForDocument( file ) );
				
			}else if( ! DelayedParsing::self().isWaiting( file ) ){
				// not in the right state and neither pending to be parsed nor waiting for
				// other files. kick the job in the buts to get running again
				const int features = context->features()
					| TopDUContext::VisibleDeclarationsAndContexts
					| DSParseJob::Resheduled
					| DSParseJob::phaseFlags( phase + 1 );
				
				ICore::self()->languageController()->backgroundParser()->addDocument( file,
					static_cast<TopDUContext::Features>( features ), 0, nullptr,
					ParseJob::IgnoresSequentialProcessing, 10 );
			}
			pWaitForFiles << file;
			allPassed = false;
			continue;
			//return false;
		}
		
		pTypeFinder.searchContexts() << context;
	}
	
	return allPassed;
}

void DSParseJob::reparseLater( int phase ){
	const int features = minimumFeatures()
		| TopDUContext::VisibleDeclarationsAndContexts
// 		| TopDUContext::AllDeclarationsContextsAndUses /* this is not working */
// 		| TopDUContext::ForceUpdate /* this is not working */
		| Resheduled
		| phaseFlags( phase );
	
	// this one here is beyond tricky.
	// 
	// NOTE things figured out dissecting the kdevelop source code:
	// 
	// - addDocument() queues document in a documents list as well as in a dictionary
	//   mapping priorities to documents
	//   
	// - when a thread picks the next document to parse it checks first for priority.
	//   this check finds the highgest priority (meaning least proprity value) from
	//   all "running jobs" that "respect sequential processing". jobs requiring
	//   sequential processing are not picked if their priority value is larger than
	//   the found threshold.
	//   
	//   this system has a huge problem because in contrary to what one would expect
	//   from reading the header comments this does not run the jobs in the expect order.
	//   
	//   one reason is that only "running jobs" priorities are examined instead
	//   of "pending jobs" and "running jobs" priorities.
	//   
	//   the second problem is that the pending documents list seems to be a QSet.
	//   I'm not sure about this since I could not find the variable declaration
	//   but the code uses insert(x) and this method exists in QSet. if this is the
	//   case then re-parsing a file adds the file in the same spot in the QSet.
	//   pairing this with the problem above results in the same tasks being picked
	//   over and over. because only running jobs priorities are examine the jobs
	//   with higher priority never get a chance to run and the background parser
	//   loops forever.
	//   
	//   in a nutshell this problem causes task rescheduled to wait for a condition
	//   to become true to end up in the same queue spot causing the background
	//   parser to dead-loop. in particular this means:
	//   - priorities will not solve the problem
	//   - using sequential will not solve the problem
	//   - using non-sequential will not solve the problem
	//   
	//   and as a result of these problems the following holds true:
	//   - re-scheduling parse jobs waiting for condition WILL dead-loop
	//   
	// - ThreadWeaver::QObjectDecorator::done and ThreadWeaver::QObjectDecorator::failed
	//   both link to the same method. aborting jobs has thus no effect at all since the
	//   underlaying code does not recognize the difference
	// 
// 	const int priority = qMin( pReparsePriority, (int)KDevelop::BackgroundParser::WorstPriority );
	int priority = 0;
	/*
	switch( phase ){
	case 2:
		priority = 25;
		break;
		
	case 3:
		priority = 50;
		break;
	}
	
	int dependencyDepth = 0;
	foreach( const ImportPackage::Ref &dependency, pDependencies ){
		dependencyDepth = qMax( dependencyDepth, dependency->dependencyDepth() + 1 );
	}
	priority += 100 * dependencyDepth;
	*/
	priority = parsePriority();
// 	qDebug() << "DSParseJob.reparseLater: phase" << phase << "priority" << priority << "for" << document();
	
	if( pWaitForFiles.isEmpty() ){
		ICore::self()->languageController()->backgroundParser()->addDocument( document(),
			static_cast<TopDUContext::Features>( features ), priority, nullptr,
			IgnoresSequentialProcessing, 10 );
		
	}else{
		DelayedParsing::self().waitFor( document(), pWaitForFiles,
			static_cast<TopDUContext::Features>( features ), priority );
	}
	
// 	ICore::self()->languageController()->backgroundParser()->addDocument( document(),
// 		static_cast<TopDUContext::Features>( features ), priority,
// 		nullptr, FullSequentialProcessing );
	
// 	ICore::self()->languageController()->backgroundParser()->addDocument( document(),
// 		static_cast<TopDUContext::Features>( features ), priority,
// 		nullptr, FullSequentialProcessing, 500 );
}

bool DSParseJob::buildDeclaration( EditorIntegrator &editor ){
	if( duChain() ){
		DUChainWriteLocker lock;
		duChain()->clearImportedParentContexts();
		duChain()->parsingEnvironmentFile()->clearModificationRevisions();
		duChain()->clearProblems();
	}
	
	DeclarationBuilder builder( editor, editor.session(), pDependencies, pTypeFinder, pPhase );
	setDuChain( builder.build( document(), pStartAst, duChain() ) );
	
// 	DumpChain().dump( duChain() );
	if( builder.requiresRebuild() ){
		pReparsePriority = qMax( pReparsePriority, builder.reparsePriority() + 10 );
		pWaitForFiles.unite( builder.waitForFiles() );
		return false;
	}
	
	return true;
}

bool DSParseJob::buildUses( EditorIntegrator &editor ){
	// gather uses of variables and functions on the document
	UseBuilder builder( editor, pDependencies, pTypeFinder );
	builder.buildUses( pStartAst );
	
	if( builder.requiresRebuild() ){
		pReparsePriority = qMax( pReparsePriority, builder.reparsePriority() + 10 );
		pWaitForFiles.unite( builder.waitForFiles() );
		return false;
	}
	
	const int features = minimumFeatures() | phaseFlags( 3 );
	DUChainWriteLocker lock;
	duChain()->setFeatures( static_cast<TopDUContext::Features>( features ) );
	
	ParsingEnvironmentFilePointer parsingEnvironmentFile = duChain()->parsingEnvironmentFile();
	if( parsingEnvironmentFile ){
		parsingEnvironmentFile->setModificationRevision( contents().modification );
		DUChain::self()->updateContextEnvironment( duChain(), parsingEnvironmentFile.data() );
	}
	
	return true;
}

void DSParseJob::parseFailed(){
	qDebug() << "DSParseJob.parseFailed:" << document();
	
	// force context to phase 3
	const int features = minimumFeatures() | phaseFlags( 3 );
	pPhase = 3;
	
	// if there's already a chain for the document, do some cleanup.
	if( duChain() ){
		ParsingEnvironmentFilePointer parsingEnvironmentFile = duChain()->parsingEnvironmentFile();
		if( parsingEnvironmentFile ){
			//parsingEnvironmentFile->clearModificationRevisions();
			parsingEnvironmentFile->setModificationRevision( contents().modification );
		}
		
		DUChainWriteLocker lock;
		duChain()->setFeatures( static_cast<TopDUContext::Features>( features ) );
		duChain()->clearProblems();
		
	// otherwise, create a new, empty top context for the file. This serves as a
	// placeholder until the syntax is fixed; for example, it prevents the document
	// from being reparsed again until it is modified.
	}else{
		ParsingEnvironmentFile * const file = new ParsingEnvironmentFile( document() );
		file->setLanguage( languageString );
		
		DUChainWriteLocker lock;
		const ReferencedTopDUContext context( new TopDUContext(
			document(), RangeInRevision( 0, 0, INT_MAX, INT_MAX ), file ) );
		context->setType( DUContext::Global );
		context->setFeatures( static_cast<TopDUContext::Features>( features ) );
		
		DUChain::self()->addDocumentChain( context );
		setDuChain( context );
	}
}

void DSParseJob::finishTopContext(){
	/*
	if( minimumFeatures() & TopDUContext::AST ){
		DUChainWriteLocker lock;
		//m_currentSession->ast = m_ast; // ??
		duChain()->setAst( QExplicitlySharedDataPointer<IAstContainer>( session.data() ) ); ??
	}
	*/
}

void DSParseJob::setParentJob( DSParseJob *job ){
	pParentJob = job;
}

int DSParseJob::phaseFlags( int phase ){
	switch( phase ){
	case 2:
		return Phase1 | Phase2;
		
	case 3:
		return Phase1 | Phase2 | Phase3;
		
	default:
		return Phase1;
	}
}

int DSParseJob::phaseFromFlags( int flags ){
	// ForceUpdate seems to be present in scheduled reparsing although the flag is not set.
	// seems to be added automatically. we can not use it thus for anything
// 	if( minimumFeatures() & TopDUContext::ForceUpdate ){
// 		return 1;
// 	}
	
	if( flags & Phase3 ){
		return 3;
	}
	if( flags & Phase2 ){
		return 2;
	}
	return 1;
}

bool DSParseJob::hasParentDocument( const IndexedString &doc ){
	if( document() == doc ){
		return true;
	}
	if( ! pParentJob ){
		return false;
	}
	if( pParentJob->document() == doc ){
		return true;
	}
	
	return pParentJob->hasParentDocument( doc );
}

ProblemPointer DSParseJob::createProblem( const QString &description, AstNode *node,
EditorIntegrator *editor, IProblem::Source source, IProblem::Severity severity ){
	Q_UNUSED(editor)
	Q_UNUSED(node)
	
	ProblemPointer p( new Problem() );
	p->setSource( source );
	p->setSeverity( severity );
	p->setDescription( description );
	p->setFinalLocation( DocumentRange( document(), editor->findRange( *node ).castToSimpleRange() ) );
	return p;
}

}
