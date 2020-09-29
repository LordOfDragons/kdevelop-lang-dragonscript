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


using namespace KDevelop;

namespace DragonScript{

static const IndexedString languageString( "DragonScript" );

DSParseJob::DSParseJob( const IndexedString &url, ILanguageSupport *languageSupport ) :
ParseJob( url, languageSupport ),
pParentJob( nullptr ),
pStartAst( nullptr ){
}

DSParseJob::~DSParseJob(){
}

void DSParseJob::run( ThreadWeaver::JobPointer self, ThreadWeaver::Thread *thread ){
	Q_UNUSED(self)
	Q_UNUSED(thread)
	
	// prepare
	if( checkAbort() ){
		return;
	}
	
	readContents();
	
	if( verifyUpdateRequired() ){
// 		qDebug() << "DSParseJob.run: no update required for" << document();
		return;
	}
	
	prepareTopContext();
	
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
		// needing an import statement. this requires parsing and declaration building
		// to be done for all files in a package before uses can be build
		findPackage();
		findDependencies();
		
		if( ! ensureDependencies() ){
			reparseLater();
			return;
		}
		if( checkAbort() ){
			return;
		}
		
// 		qDebug() << "DSParseJob.run: dependencies ready for" << document();
		EditorIntegrator editor( session );
		
		if( ! buildDeclaration( editor ) ){
			reparseLater();
			return;
		}
		if( checkAbort() ){
			return;
		}
		
		//if( ( minimumFeatures() & UpdateUses ) || ! pPackage ){
			// for packages wait until all files finished building declarations.
			// we will be triggered again once this happens to update uses
			if( ! buildUses( editor ) ){
				reparseLater();
				return;
			}
			if( checkAbort() ){
				return;
			}
			
			highlightDUChain();
		//}
		
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
	
	DUChain::self()->emitUpdateReady( document(), duChain() );
}

bool DSParseJob::checkAbort(){
	if( abortRequested() || ICore::self()->shuttingDown() ){
		abortJob();
		return true;
	}
	return false;
}

bool DSParseJob::verifyUpdateRequired(){
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
	DUChainWriteLocker lock;
	setDuChain( DUChainUtils::standardContextForUrl( document().toUrl() ) );
	
	if( duChain() ){
		lock.unlock(); // due to translateDUChainToRevision
		translateDUChainToRevision( duChain() );
		lock.lock();
		
		duChain()->setRange( RangeInRevision( 0, 0, INT_MAX, INT_MAX ) );
	}
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
		pDependencies << pPackage->dependsOn();
		return;
	}
	
	// otherwise this is a project document
	
	// add language import package
	pDependencies << ImportPackageLanguage::self();
	
	// add dragengine import package
	pDependencies << ImportPackageDragengine::self();
	
	// add import package for each project import set by the user
	IProject * const project = ICore::self()->projectController()->findProjectForUrl( document().toUrl() );
	if( ! project ){
		return;
	}
	
	/*
	const KSharedConfigPtr config( project->projectConfiguration() );
	foreach( const QString &each, config->additionalConfigSources() ){
		qDebug() << "KDevDScript ContextBuilder: additional config sources" << each;
	}
	*/
	
	const KConfigGroup config( project->projectConfiguration()->group( "dragonscriptsupport" ) );
	const QStringList list( config.readEntry( "pathInclude", QStringList() ) );
	
	foreach( const QString &dir, list ){
		const QString name( QString( "#dir#" ) + dir );
		ImportPackage::Ref package( importPackages.packageNamed( name ) );
		if( ! package ){
			package = ImportPackage::Ref( new ImportPackageDirectory( name, dir ) );
			importPackages.addPackage( package );
		}
		pDependencies << package;
	}
}

bool DSParseJob::ensureDependencies(){
	// make sure all required packages have been parsed and prepared
	bool ready = true;
	foreach( const ImportPackage::Ref &each, pDependencies ){
		if( ! each->ensureReady() ){
// 			qDebug() << "DSParseJob.run: depdendy" << each->name() << "not ready for" << document();
			ready = false;
		}
	}
	return ready;
}

void DSParseJob::reparseLater( int addFeatures ){
	abortJob();
	
	const TopDUContext::Features feat = static_cast<TopDUContext::Features>(
			minimumFeatures() | TopDUContext::VisibleDeclarationsAndContexts
			| Resheduled | addFeatures );
	ICore::self()->languageController()->backgroundParser()->addDocument( document(),
		feat, parsePriority(), nullptr, DSParseJob::FullSequentialProcessing, 500 );
}

bool DSParseJob::buildDeclaration( EditorIntegrator &editor ){
// 	qDebug() << "DSParseJob.run: build declarations for " << document();
	if( duChain() ){
		DUChainWriteLocker lock;
		duChain()->clearImportedParentContexts();
		duChain()->parsingEnvironmentFile()->clearModificationRevisions();
		duChain()->clearProblems();
	}
	
	DeclarationBuilder builder( editor, parsePriority(), editor.session(), pDependencies );
	setDuChain( builder.build( document(), pStartAst, duChain() ) );
	
// 	DumpChain().dump( pDUContext );
	return ! builder.requiresRebuild();
}

bool DSParseJob::buildUses( EditorIntegrator &editor ){
// 	qDebug() << "DSParseJob.run: build uses for " << document();
	// gather uses of variables and functions on the document
	UseBuilder usebuilder( editor, pDependencies );
	usebuilder.buildUses( pStartAst );
	
	if( usebuilder.requiresRebuild() ){
		return false;
	}
	
	// some internal housekeeping work
	DUChainWriteLocker lock;
	duChain()->setFeatures( minimumFeatures() );
	ParsingEnvironmentFilePointer parsingEnvironmentFile = duChain()->parsingEnvironmentFile();
	if( parsingEnvironmentFile ){
		parsingEnvironmentFile->setModificationRevision( contents().modification );
		DUChain::self()->updateContextEnvironment( duChain(), parsingEnvironmentFile.data() );
	}
	
	return true;
}

void DSParseJob::parseFailed(){
//	qDebug() << "KDevDScript: DSParseJob::run: Parsing failed" << document().str();
	DUChainWriteLocker lock;
	// if there's already a chain for the document, do some cleanup.
	if( duChain() ){
		ParsingEnvironmentFilePointer parsingEnvironmentFile = duChain()->parsingEnvironmentFile();
		if( parsingEnvironmentFile ){
			//parsingEnvironmentFile->clearModificationRevisions();
			parsingEnvironmentFile->setModificationRevision( contents().modification );
		}
		duChain()->clearProblems();
		
	// otherwise, create a new, empty top context for the file. This serves as a
	// placeholder until the syntax is fixed; for example, it prevents the document
	// from being reparsed again until it is modified.
	}else{
		ParsingEnvironmentFile * const file = new ParsingEnvironmentFile( document() );
		file->setLanguage( languageString );
		ReferencedTopDUContext context = new TopDUContext( document(), RangeInRevision( 0, 0, INT_MAX, INT_MAX ), file );
		context->setType( DUContext::Global );
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
// 	qDebug() << "KDevDScript: DSParseJob::run:" << p->description();
	return p;
}

}
