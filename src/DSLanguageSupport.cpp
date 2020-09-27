#include "DSLanguageSupport.h"
#include "DSParseJob.h"
#include "Highlighting.h"
#include "DSSessionSettings.h"
#include "codecompletion/DSCodeCompletionModel.h"
#include "configpage/ProjectConfigPage.h"
#include "configpage/SessionConfigPage.h"

#include <interfaces/icore.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/ilanguagecontroller.h>
#include <language/backgroundparser/parsejob.h>
#include <language/codecompletion/codecompletion.h>
#include <language/duchain/duchain.h>
#include <language/duchain/duchainlock.h>

#include <KPluginFactory>
#include <QReadWriteLock>


using namespace KDevelop;

namespace DragonScript{

DSLanguageSupport *DSLanguageSupport::pSelf = nullptr;

K_PLUGIN_FACTORY_WITH_JSON( KDevDragonScriptSupportFactory,
	"kdev_lang_dscript.json", registerPlugin<DSLanguageSupport>(); )

DSLanguageSupport::DSLanguageSupport(QObject *parent, const QVariantList& args) :
IPlugin(QStringLiteral("dragonscriptlanguagesupport"), parent),
pHighlighting( new Highlighting( this ) )
{
	Q_UNUSED(args);
	pSelf = this;
	
	DSSessionSettings::self.update();
	
	DSCodeCompletionModel * const codeCompletion = new DSCodeCompletionModel( this );
	new CodeCompletion( this, codeCompletion, "DragonScript" );
	
	// TODO assistences
}

DSLanguageSupport::~DSLanguageSupport(){
	parseLock()->lockForWrite();
	// By locking the parse-mutexes, we make sure that parse jobs get a chance to finish in a good state
	parseLock()->unlock();
	
	delete pHighlighting;
	pHighlighting = nullptr;
	
	pSelf = nullptr;
}

QString DSLanguageSupport::name() const{
	return "DragonScript";
}

ParseJob *DSLanguageSupport::createParseJob(const IndexedString& url){
	return new DSParseJob( url, this );
}

ICodeHighlighting *DSLanguageSupport::codeHighlighting() const{
	return pHighlighting;
}

int DSLanguageSupport::configPages() const{
	return 1;
}

ConfigPage *DSLanguageSupport::configPage( int number, QWidget *parent ){
	if( number == 0 ){
		return new SessionConfigPage( this, parent );
	}
	return nullptr;
}

int DSLanguageSupport::perProjectConfigPages() const{
	return 1;
}

ConfigPage* DSLanguageSupport::perProjectConfigPage( int number,
const KDevelop::ProjectConfigOptions &options, QWidget *parent ){
	if( number == 0 ){
		return new ProjectConfigPage( this, options, parent );
	}
	return nullptr;
}

}

// needed for QObject class created from K_PLUGIN_FACTORY_WITH_JSON
#include "DSLanguageSupport.moc"
