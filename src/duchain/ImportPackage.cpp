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
#include "../DSParseJob.h"


using namespace KDevelop;

namespace DragonScript {

ImportPackage::ImportPackage() :
pMode( Mode::Pending ),
pDebug( false ){
}

ImportPackage::ImportPackage( const QString &name, const QVector<IndexedString> &files ) :
pName( name ),
pFiles( files ),
pMode( Mode::Pending ),
pDebug( false ){
}

ImportPackage::~ImportPackage(){
	dropContexts();
}

const QVector<ReferencedTopDUContext> &ImportPackage::getContexts(){
	if( ensureReady() ){
		return pContexts;
	}
	
	static QVector<ReferencedTopDUContext> empty;
	return empty;
}

bool ImportPackage::ensureReady(){
	switch( pMode ){
	case Mode::Ready:
		return true;
		
	case Mode::Pending:{
		DUChainReadLocker lock;
		bool allReady = true;
		DUChain &duchain = *DUChain::self();
		
		pMode = Mode::Parsing;
		pContexts.clear();
		
		foreach( const IndexedString &file, pFiles ){
			TopDUContext * const context = duchain.chainForDocument( file );
			
			if( context ){
				if( pDebug ){
					qDebug() << "ImportPackage.getContexts" << pName << ": File has context:" << file;
				}
				pContexts.append( ReferencedTopDUContext( context ) );
				
			}else{
				if( pDebug ){
					qDebug() << "ImportPackage.getContexts" << pName << ": File has no context, parsing it:" << file;
				}
				ICore::self()->languageController()->backgroundParser()->addDocument(
					file, TopDUContext::ForceUpdate, BackgroundParser::BestPriority,
					0, ParseJob::FullSequentialProcessing );
				allReady = false;
			}
		}
		
		if( ! allReady ){
			pContexts.clear();
		}
		}break;
		
	case Mode::Parsing:{
		DUChainReadLocker lock;
		DUChain &duchain = *DUChain::self();
		
		pContexts.clear();
		
		foreach( const IndexedString &file, pFiles ){
			TopDUContext * const context = duchain.chainForDocument( file );
			if( context ){
				if( pDebug ){
					qDebug() << "ImportPackage.getContexts" << pName << ": File ready:" << file;
				}
				pContexts.append( ReferencedTopDUContext( context ) );
				
			}else{
				if( pDebug ){
					qDebug() << "ImportPackage.getContexts" << pName << ": File still parsing:" << file;
				}
				pContexts.clear();
				break;
			}
		}
		
		if( pContexts.isEmpty() ){
			break;
		}
		
		if( pDebug ){
			qDebug() << "ImportPackage.getContexts" << pName << ": All files read";
		}
		
		// re-parse all script files. this is required since script files usually
		// contain inter-script dependencies and the parsing order is undefined.
		// this reparsing has no effect on the context but will trigger reparsing
		// of source files once the individual files are updated
		BackgroundParser &bgparser = *ICore::self()->languageController()->backgroundParser();
		const TopDUContext::Features feat = static_cast<TopDUContext::Features>(
			TopDUContext::VisibleDeclarationsAndContexts
			| DSParseJob::Resheduled /*| DSParseJob::UpdateUses*/ );
		
// 		pMode = Mode::BuildUses;
		pMode = Mode::Ready;
		
		foreach( const IndexedString &file, pFiles ){
			bgparser.addDocument( file, feat, BackgroundParser::BestPriority, 0, ParseJob::FullSequentialProcessing );
		}
		}break;
		
	case Mode::BuildUses:{
		/*
		DUChainReadLocker lock;
		foreach( const ReferencedTopDUContext &each, pContexts ){
			if( ! ( each->features() & DSParseJob::UpdateUses ) ){
				return false;
			}
		}
		*/
		pMode = Mode::Ready;
		}return true;
	}
	
	return false;
}

bool ImportPackage::addImports( TopDUContext *context ){
	if( ! ensureReady() ){
		if( pDebug ){
			qDebug() << "ImportPackage.addImports" << pName << ": Not all contexts ready yet.";
		}
		return false;
	}
	
	DUChainWriteLocker lock;
	QVector<QPair<TopDUContext*, CursorInRevision>> imports;
	
	foreach( const ReferencedTopDUContext &each, pContexts ){
		imports.append( qMakePair( each, CursorInRevision( 1, 0 ) ) );
	}
	context->addImportedParentContexts( imports );
	
	return true;
}

void ImportPackage::dropContexts(){
	DUChainWriteLocker lock;
	pContexts.clear();
	pMode = Mode::Pending;
}

}
