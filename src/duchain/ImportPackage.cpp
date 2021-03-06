#include <QPair>
#include <QDebug>

#include <language/duchain/duchain.h>
#include <language/duchain/duchainlock.h>
#include <language/backgroundparser/backgroundparser.h>
#include <interfaces/iproject.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/icore.h>
#include <interfaces/ilanguagecontroller.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/ipartcontroller.h>

#include "ImportPackage.h"
#include "DelayedParsing.h"
#include "../DSParseJob.h"


using namespace KDevelop;

namespace DragonScript {

ImportPackage::ImportPackage() :
pDebug( false ){
}

ImportPackage::ImportPackage( const QString &name, const QSet<IndexedString> &files ) :
pName( name ),
pFiles( files ),
pDebug( false ){
}

ImportPackage::~ImportPackage(){
}

void ImportPackage::contexts( State &state, int minRequiredPhase ){
	minRequiredPhase = qMin( qMax( minRequiredPhase, 1 ), 3 );
	
	BackgroundParser &bp = *ICore::self()->languageController()->backgroundParser();
	DUChain &duchain = *DUChain::self();
	state.ready = true;
	state.reparsePriority = 0;
	state.importContexts.clear();
	state.waitForFiles.clear();
	
	// dragonscript works a bit different than other languages. script files always
	// belong to a namespace and all files in the namespace are visible without
	// needing an import statement. due to this the normal way of parsing files once
	// is not working. we have to parse the files up to three times to properly
	// resolve all uses. this is done by using custom feature flags on the top context.
	// only when all files in the package reached phase 3 the package can be used.
	// 
	// phases are always run through from 1 to 3 no matter if something can not be
	// resolved. this ensures no deadloops can happen nor possible resolves are missed.
	// we get a finished context after each phase is completed
	foreach( const IndexedString &file, pFiles ){
		TopDUContext * const context = duchain.chainForDocument( file );
		
		if( context ){
			const int phase = DSParseJob::phaseFromFlags( context->features() );
			if( pDebug ){
				qDebug() << "ImportPackage.getContexts" << pName << ": File has context with phase" << phase << ":" << file;
			}
			
			if( phase < minRequiredPhase ){
// 				qDebug() << "ImportPackage.getContexts" << pName << ": File has context with phase" << phase << ":" << file;
				if( bp.isQueued( file ) ){
					state.reparsePriority = qMax( state.reparsePriority, bp.priorityForDocument( file ) );
					
				}else if( ! DelayedParsing::self().isWaiting( file ) ){
					// not in the right state and neither pending to be parsed nor waiting for
					// other files. kick the job in the buts to get running again
					if( pDebug ){
						qDebug() << "ImportPackage.getContexts" << pName << ": File has context with phase"
							<< phase << "and is not pending nor waiting (resheduling):" << file;
					}
					const int features = context->features()
						| TopDUContext::VisibleDeclarationsAndContexts
						| DSParseJob::Resheduled
						| DSParseJob::phaseFlags( phase + 1 );
					
					ICore::self()->languageController()->backgroundParser()->addDocument( file,
						static_cast<TopDUContext::Features>( features ), 0, nullptr,
						ParseJob::IgnoresSequentialProcessing, 10 );
				}
				state.waitForFiles << file;
				state.ready = false;
				
			}else{
				state.importContexts << context;
			}
			
		}else{
			if( pDebug ){
				qDebug() << "ImportPackage.getContexts" << pName << ": File has no context, parsing it:" << file;
			}
			ICore::self()->languageController()->backgroundParser()->addDocument( file, TopDUContext::ForceUpdate );
			state.waitForFiles << file;
			state.ready = false;
		}
	}
	
	if( ! state.ready ){
		state.importContexts.clear();
	}
}

QSet<TopDUContext*> ImportPackage::deepAllContexts(){
	DUChain &duchain = *DUChain::self();
	QSet<TopDUContext*> list;
	
	foreach( const IndexedString &file, pFiles ){
		TopDUContext * const context = duchain.chainForDocument( file );
		if( context ){
			list << context;
		}
	}
	
	foreach( const ImportPackage::Ref &package, pDependsOn ){
		list.unite( package->deepAllContexts() );
	}
	
	return list;
}

int ImportPackage::dependencyDepth() const{
	int depth = 0;
	
	foreach( const Ref &each, pDependsOn ){
		depth = qMax( depth, each->dependencyDepth() + 1 );
	}
	
	return depth;
}

void ImportPackage::reparse()
{
	BackgroundParser &backgroundParser = *ICore::self()->languageController()->backgroundParser();
	foreach( const IndexedString &file, pFiles ){
		backgroundParser.addDocument( file, TopDUContext::ForceUpdate );
	}
}

}
