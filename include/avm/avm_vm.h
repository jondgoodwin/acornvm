/** Manage the Virtual Machine instance
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_vm_h
#define avm_vm_h

#include "avm/avm_memory.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

	/** Virtual Machine instance information 
	 *  Is never garbage collected, but is the root for garbage collection. */
	typedef struct VmInfo {
		Value main_thread;			/**< VM's main thread */
		struct SymTable* sym_table;	/**< global symbol table */
		AHash hashseed;				/**< randomized seed for hashing strings */

		// Global state for all collectable objects
		MemInfo *objlist;  //!< linked list of all collectable objects
		MemInfo *rootobj; //!< All descendents of this object won't be collected
		MemInfo **sweepgc;  //!< current position of sweep in list 'objlist'
		MemInfo *gray;  //!< list of gray objects
		MemInfo *grayagain;  //!< list of objects to be traversed atomically

		Auint totalbytes;  //!< number of bytes currently allocated - GCdebt
		Aint gcdebt;		//!< bytes allocated, not yet compensated by the collector
		Auint gcmemtrav;  //!< cumulative size of all objects marked black
		Auint gcestimate;  //!< an estimate of the non-garbage memory in use

		char gcmode;  //!< Collection mode: Normal, Emergency, Gen
		char gcstate;  //!< state of garbage collector
		char gcrunning;  //!< true if GC is running
		char currentwhite; //!< Current white color for new objects

		int gcpause;  //!< size of pause between successive GCs 
		int gcmajorinc;  //!< pause between major collections (only in gen. mode)
		int gcstepmul;  //!< GC `granularity' 

		int sweepstrgc;  //!< position of sweep in `strt'
	} VmInfo;

	/** Called when VM cannot allocate a new object */
	void vm_outofmemory(void);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif