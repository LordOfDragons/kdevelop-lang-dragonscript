#ifndef DELAYEDPARSING_H
#define DELAYEDPARSING_H

#include <QHash>
#include <QSet>
#include <QSharedPointer>
#include <QMutex>

#include <language/backgroundparser/parsejob.h>
#include <language/interfaces/ilanguagesupport.h>
#include <language/duchain/topducontext.h>
#include <serialization/indexedstring.h>


using namespace KDevelop;

namespace DragonScript {

/**
 * Provides support for delayed parsing.
 * 
 * The parse in KDevelop is not suitable for parsing jobs requiring parsing dependency
 * files before the can continue. Trying to reschedule jobs like this can result in
 * the background parser being unable to process jobs.
 * 
 * This class allows jobs to be rescheduled when a file they require as dependency
 * finished parsing.
 * 
 * This class works as singleton. Get the one and only instance using self();
 * 
 * Jobs ending up with a source file they need to be fully processed before they can
 * continue add themselves using a call to \ref waitFor().
 * 
 * Jobs having finished parsing have to call \ref parsingFinished(). This will reschedule
 * all files previously registeres using \ref waitFor() if they are not already scheduled
 * for parsing.
 * 
 * If a job aborts it has to remove itself from the DelayedParsing instance by calling
 * \ref cancelWaiting(). Not doing so just causes them to be rescheduled although not needed.
 * 
 * This class uses an internal locking and is thread safe. No locks need to be held while
 * using this class unless noted.
 */
class DelayedParsing{
public:
	/**
	 * Scheduling parameters to use for scheduing file.
	 */
	struct ScheduleParameters{
		/**
		 * Remaining number of files to finish parsing before scheduling file.
		 */
		int remainingFileCount;
		
		/**
		 * Context features to request parsing.
		 */
		TopDUContext::Features features;
		
		/**
		 * Parsing priority.
		 */
		int priority;
		
		/**
		 * Sequential processing flags.
		 */
		ParseJob::SequentialProcessingFlags flags;
		
		/**
		 * Delay in milliseconds.
		 */
		int delay;
	};
	
	/**
	 * Shared schedule parameters.
	 */
	typedef QSharedPointer<ScheduleParameters> ScheduleParametersReference;
	
	/**
	 * Waiter map type.
	 */
	typedef QHash<IndexedString, ScheduleParametersReference> WaiterMap;
	
	/**
	 * Dependency map type.
	 */
	typedef QHash<IndexedString, WaiterMap> DependencyMap;
	
	/**
	 * File set.
	 */
	typedef QSet<IndexedString> FileSet;
	
	
	
	
private:
	QMutex pMutex;
	DependencyMap pDependencyMap;
	
	static DelayedParsing pSelf;
	
	bool pDebugEnabled = false;
	
	
	
public:
	/**
	 * Global instance.
	 */
	static inline DelayedParsing &self(){ return pSelf; }
	
	DelayedParsing() = default;
	
	
	/**
	 * Register \em file to be scheduled if all \em dependencies finished parsing once.
	 * If \em file is already registered it is first unregistered.
	 */
	void waitFor( const IndexedString &file, const FileSet &dependencies,
		TopDUContext::Features features = TopDUContext::VisibleDeclarationsAndContexts,
		int priority = 0,
		ParseJob::SequentialProcessingFlags flags = ParseJob::IgnoresSequentialProcessing,
		int delay = 10 /*ILanguageSupport::DefaultDelay*/ );
	
	/**
	 * \copydoc
	 */
	void waitFor( const IndexedString &file, const FileSet &dependencies,
		const ScheduleParameters &parameters );
	
	/**
	 * Unregister \em file from waiting for all files it is attached to if any.
	 */
	void cancelWaiting( const IndexedString &file );
	
	/**
	 * Returns true if \em file is waiting for other files.
	 */
	bool isWaiting( const IndexedString &file );
	
	/**
	 * Notify waiters that \em file finished parsing.
	 */
	void parsingFinished( const IndexedString &file );
	
	/**
	 * Debug output is enabled.
	 */
	inline bool debugEnabled() const{ return pDebugEnabled; }
	
	/**
	 * Set if debug output is enabled.
	 */
	void setDebugEnabled( bool enabled );
	
	
	
	/**
	 * Retrieve copy of dependency map.
	 */
	DependencyMap dependencyMap();
};

}

#endif
