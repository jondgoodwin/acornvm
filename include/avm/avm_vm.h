/** Manage the Virtual Machine instance.
 *
 * This is the heart of the Acorn Virtual Machine. It manages:
 * - All memory and garbage collection (avm_memory.h), working with the 
 *   different encoding types.
 * - The symbol table, which is shared across everywhere
 * - The main thread, which is the recursive root for garbage collection.
 *   The thread manages the global namespace, including all registered 
 *   core types (including the Acorn compiler and resource types).
 * 
 * See newVm() for more detailed information on VM initialization.
 *
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
		MemCommonInfo;
		Value main_thread;			/**< VM's main thread */
		struct SymTable* sym_table;	/**< global symbol table */
		AuintIdx hashseed;				/**< randomized seed for hashing strings */

		Value *defEncTypes;    /**< array of default types for each encoding */

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

	/** Lock the Vm */
	void vm_lock(Value th);
	/** Unlock the Vm */
	void vm_unlock(Value th);

	/** Call when VM cannot allocate a new object */
	void vm_outofmemory(void);
	/** Call when we want to overflow max stack size */
	void vm_outofstack(void);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif