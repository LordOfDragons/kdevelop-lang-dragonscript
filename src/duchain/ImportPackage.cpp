#include <QPair>
#include <QDebug>

#include <language/duchain/duchain.h>
#include <language/backgroundparser/backgroundparser.h>
#include <interfaces/iproject.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/icore.h>
#include <interfaces/ilanguagecontroller.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/ipartcontroller.h>

#include "ImportPackage.h"


using namespace KDevelop;

namespace DragonScript {

ImportPackage::ImportPackage() :
pReady( false ),
pParsing( false ),
pDebug( true ){
}

ImportPackage::ImportPackage( const QString &name, const QVector<IndexedString> &files ) :
pName( name ),
pFiles( files ),
pReady( files.isEmpty() ),
pParsing( false ),
pDebug( true ){
}

ImportPackage::~ImportPackage(){
}

QVector<ReferencedTopDUContext> ImportPackage::getContexts(){
// 	qDebug() << "ImportPackage.getContexts: ready" << pReady;
	if( pReady ){
		QVector<ReferencedTopDUContext> contexts;
		foreach( const ReferencedTopDUContext &context, pContexts ){
			contexts.append( context );
		}
		return contexts;
	}
	
	BackgroundParser &bgparser = *ICore::self()->languageController()->backgroundParser();
	DUChain &duchain = *DUChain::self();
	
	pContexts.clear();
	
	if( pParsing ){
		foreach( const IndexedString &file, pFiles ){
			TopDUContext * const context = duchain.chainForDocument( file );
			if( context ){
				if( pDebug ){
					qDebug() << "ImportPackage.getContexts: File ready:" << file;
				}
				pContexts.append( ReferencedTopDUContext( context ) );
				
			}else{
				if( pDebug ){
					qDebug() << "ImportPackage.getContexts: File still parsing:" << file;
				}
				pContexts.clear();
				break;
			}
		}
		
	}else{
		bool allReady = true;
		pParsing = true;
		
		foreach( const IndexedString &file, pFiles ){
			TopDUContext * const context = duchain.chainForDocument( file );
			
			if( context ){
				if( pDebug ){
					qDebug() << "ImportPackage.getContexts: File has context:" << file;
				}
				pContexts.append( ReferencedTopDUContext( context ) );
				
			}else{
				if( pDebug ){
					qDebug() << "ImportPackage.getContexts: File has no context, parsing it:" << file;
				}
				bgparser.addDocument( file, TopDUContext::ForceUpdate, BackgroundParser::BestPriority,
					0, ParseJob::FullSequentialProcessing );
				allReady = false;
			}
		}
		
		if( ! allReady ){
			pContexts.clear();
		}
	}
	
	if( ! pContexts.isEmpty() ){
		if( pDebug ){
			qDebug() << "ImportPackage.getContexts: All files read";
		}
		
		if( pParsing ){
			// re-parse all script files. this is required since script files usually
			// contain inter-script dependencies and the parsing order is undefined.
			// this reparsing has no effect on the context but will trigger reparsing
			// of source files once the individual files are updated
			foreach( const IndexedString &file, pFiles ){
				bgparser.addDocument( file, TopDUContext::ForceUpdate,
					BackgroundParser::BestPriority, 0, ParseJob::FullSequentialProcessing );
			}
		}
		
		pParsing = false;
		pReady = true;
	}
	
	return {};
}

bool ImportPackage::addImports( TopDUContext *context ){
	const QVector<ReferencedTopDUContext> contexts( getContexts() );
	if( ! pReady ){
		if( pDebug ){
			qDebug() << "ImportPackage.addImports: Not all contexts ready yet.";
		}
		return false;
	}
	
	QVector<QPair<TopDUContext*, CursorInRevision>> imports;
	
	foreach( const ReferencedTopDUContext &each, contexts ){
		imports.append( qMakePair( each, CursorInRevision( 1, 0 ) ) );
	}
	context->addImportedParentContexts( imports );
	
	return true;
}

void ImportPackage::dropContexts(){
	pContexts.clear();
	pReady = false;
	pParsing = false;
}

}
