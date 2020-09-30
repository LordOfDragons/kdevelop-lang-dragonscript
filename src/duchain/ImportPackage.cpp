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
pDebug( false ){
}

ImportPackage::ImportPackage( const QString &name, const QVector<IndexedString> &files ) :
pName( name ),
pFiles( files ),
pDebug( false ){
}

ImportPackage::~ImportPackage(){
}

QVector<TopDUContext*> ImportPackage::contexts(){
	DUChain &duchain = *DUChain::self();
	QVector<TopDUContext*> list;
	bool allReady = true;
	
	foreach( const IndexedString &file, pFiles ){
		TopDUContext * const context = duchain.chainForDocument( file );
		
		if( context ){
			if( pDebug ){
				qDebug() << "ImportPackage.getContexts" << pName << ": File has context:" << file;
			}
			list.append( context );
			
		}else{
			if( pDebug ){
				qDebug() << "ImportPackage.getContexts" << pName << ": File has no context, parsing it:" << file;
			}
			ICore::self()->languageController()->backgroundParser()->addDocument( file, TopDUContext::ForceUpdate );
// 			ICore::self()->languageController()->backgroundParser()->addDocument(
// 				file, TopDUContext::ForceUpdate, BackgroundParser::BestPriority,
// 				0, ParseJob::FullSequentialProcessing );
			allReady = false;
		}
	}
	
	if( ! allReady ){
		return {};
	}
	
#if 0
	// re-parse all script files. this is required since script files usually
	// contain inter-script dependencies and the parsing order is undefined.
	// this reparsing has no effect on the context but will trigger reparsing
	// of source files once the individual files are updated
	BackgroundParser &bgparser = *ICore::self()->languageController()->backgroundParser();
	const TopDUContext::Features feat = static_cast<TopDUContext::Features>(
		TopDUContext::VisibleDeclarationsAndContexts
		| DSParseJob::Resheduled /*| DSParseJob::UpdateUses*/ );
	
	foreach( const IndexedString &file, pFiles ){
		bgparser.addDocument( file, feat );
// 		bgparser.addDocument( file, feat, BackgroundParser::BestPriority, 0, ParseJob::FullSequentialProcessing );
	}
#endif
	
	return list;
}

void ImportPackage::reparse()
{
	BackgroundParser &backgroundParser = *ICore::self()->languageController()->backgroundParser();
	foreach( const IndexedString &file, pFiles ){
		backgroundParser.addDocument( file, TopDUContext::ForceUpdate );
	}
}

}
