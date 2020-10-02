#include <QMutexLocker>
#include <QDebug>

#include <language/backgroundparser/backgroundparser.h>
#include <interfaces/icore.h>
#include <interfaces/ilanguagecontroller.h>

#include "DelayedParsing.h"


namespace DragonScript {

// global instance
DelayedParsing DelayedParsing::pSelf;


void DelayedParsing::waitFor( const IndexedString &file, const FileSet &dependencies,
TopDUContext::Features features, int priority, ParseJob::SequentialProcessingFlags flags, int delay ){
	waitFor( file, dependencies, ScheduleParameters{
		.remainingFileCount = 0,
		.features = features,
		.priority = priority,
		.flags = flags,
		.delay = delay
	} );
}

void DelayedParsing::waitFor( const IndexedString &file, const FileSet &dependencies,
const ScheduleParameters &parameters ){
	if( dependencies.isEmpty() ){
		// if file set is empty schedule right away
		if( pDebugEnabled ){
			qDebug() << "file" << file << "has empty dependencies. schedule now";
		}
		
		ICore::self()->languageController()->backgroundParser()->addDocument( file,
			parameters.features, parameters.priority, nullptr, parameters.flags, parameters.delay );
		return;
	}
	
	// unregister file first
	cancelWaiting( file );
	
	// create shared data storing the state of the waiting
	QMutexLocker lock( &pMutex );
	
	ScheduleParametersReference state( new ScheduleParameters( parameters ) );
	state->remainingFileCount = dependencies.size();
	
	// register shared data with all files
	if( pDebugEnabled ){
		qDebug() << "DelayedParsing: waiting for" << file << "on" << dependencies.size() << "files";
	}
	
	FileSet::const_iterator iterFileSet;
	for( iterFileSet = dependencies.cbegin(); iterFileSet != dependencies.cend(); iterFileSet++ ){
		// find dependency file slot
		DependencyMap::iterator iterDependency( pDependencyMap.find( *iterFileSet ) );
		if( iterDependency == pDependencyMap.end() ){
			// nobody is waiting for this file. add slot for it
			iterDependency = pDependencyMap.insert( *iterFileSet, WaiterMap() );
		}
		
		// insert file replacing existing entry if present
		iterDependency->insert( file, state );
		
// 		if( pDebugEnabled ){
// 			qDebug() << "DelayedParsing: waiting for" << file << "on" << *iterFileSet;
// 		}
	}
}

void DelayedParsing::cancelWaiting( const IndexedString &file ){
	QMutexLocker lock( &pMutex );
	
	// iterate over all dependency slots
	DependencyMap::iterator iterDependency;
	for( iterDependency = pDependencyMap.begin(); iterDependency != pDependencyMap.end(); iterDependency++ ){
		// find waiter matching file
		const WaiterMap::iterator iterWaiter( iterDependency.value().find( file ) );
		if( iterWaiter == iterDependency.value().end() ){
			// file not waiting for dependency
			continue;
		}
		
		// remove file from dependency
		iterDependency.value().erase( iterWaiter );
		
		if( pDebugEnabled ){
			qDebug() << "DelayedParsing: cancelled waiting for" << file << "on" << iterDependency.key();
		}
	}
}

void DelayedParsing::parsingFinished( const IndexedString &file ){
	QMutexLocker lock( &pMutex );
	
	// find dependency file slot
	const DependencyMap::iterator iterDependency( pDependencyMap.find( file ) );
	if( iterDependency == pDependencyMap.end() ){
		// nobody is waiting for this document. go home
		if( pDebugEnabled ){
			qDebug() << "DelayedParsing: file" << file << "finished parsing. no waiters";
		}
		
		return;
	}
	
	// copy waiters aside and clear them. this avoids potential dead-locking
	const WaiterMap waiters( *iterDependency );
	pDependencyMap.erase( iterDependency );
	
	// drop the lock in case rescheduling calls back to us
	lock.unlock();
	
	// reschedule all files if they are not already scheduled and have their count reached 0
	BackgroundParser &backgroundParser = *ICore::self()->languageController()->backgroundParser();
	WaiterMap::const_iterator iterWaiter;
	
	if( pDebugEnabled ){
		qDebug() << "DelayedParsing: file" << file << "finished parsing. scheduling"
			<< waiters.size() << "waiters";
	}
	
	for( iterWaiter = waiters.cbegin(); iterWaiter != waiters.cend(); iterWaiter++ ){
		ScheduleParameters &parameters = *iterWaiter->data();
		parameters.remainingFileCount--;
		if( parameters.remainingFileCount != 0 ){
			// not ready yet
			continue;
		}
		
		if( backgroundParser.isQueued( iterWaiter.key() ) ){
			// file is already queued for parsing. ignore it
			if( pDebugEnabled ){
				qDebug() << "- file already scheduled" << iterWaiter.key();
			}
			
			continue;
		}
		
		// schedule the file using the stored parameters
		if( pDebugEnabled ){
			qDebug() << "- schedule file" << iterWaiter.key();
		}
		
		backgroundParser.addDocument( iterWaiter.key(), parameters.features,
			parameters.priority, nullptr, parameters.flags, parameters.delay );
	}
}

DelayedParsing::DependencyMap DelayedParsing::dependencyMap(){
	QMutexLocker lock( &pMutex );
	return pDependencyMap;
}

void DelayedParsing::setDebugEnabled( bool enabled ){
	pDebugEnabled = enabled;
}

}
