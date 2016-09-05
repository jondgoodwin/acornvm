/** Manage memory garbage collection.
 * @file
 * 
 * The garbage collector implements a tri-color, incremental, single- (or dual-) generation
 * mark-and-sweep algorithm. It does no copy-compaction.
 *
 * All pointer-based Values (e.g., string, symbol) share a common header
 * at the start of the allocated area. The implementation code for such pointer-based Values 
 * is interwoven with the garbage collection routines in this file.
 * In particular, any value that allocates memory must:
 * - Use mem_new to allocate the fixed-position info block for the Value
 *   (a GC cycle may be performed just before the block is allocated)
 * - Define how to mark all Values it contains (when requested by the GC)
 * - Define how to free all memory it has allocated (when swept by the GC)
 * - Call mem_markchk whenever a value is stored within another non-thread (stack) value.
 *
 * These garbage collection algorithms are inspired by the garbage collection
 * approach used by Lua.
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"
#include <stdlib.h>

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

#if !defined(GCSTEPSIZE)
/** how much to allocate before next GC step  (~100 empty symbols) */
#define GCSTEPSIZE	((Aint)(100 * sizeof(SymInfo)))
#endif
/** cost of sweeping one element (adjusted size of a small object) */
#define GCSWEEPCOST	((sizeof(SymInfo) + 4) / 4)
/** maximum number of elements to sweep in each single step */
#define GCSWEEPMAX	((Aint)((GCSTEPSIZE / GCSWEEPCOST) / 4))
/** maximum number of finalizers to call in each GC step */
#define GCFINALIZENUM	4
/** Divisor for adjusting 'stepmul' (value chosen by tests) */
#define STEPMULADJ		200
/** Divisor for adjusting 'pause' (value chosen by tests) */
#define PAUSEADJ		100


#define MAX_UMEM	((Auint)(~(Auint)0)-2)			//!< Maximum value for an unsigned integer
#define MAX_MEM     ((Aint) ((MAX_UMEM >> 1) - 2))	//!< Maximum value for a signed integer

// Garbage collector modes 
#define GC_FULLMODE		0	//!< gc does full (non-generational) mark and sweep
#define GC_EMERGENCY	1	//!< gc does full, forced by an allocation failure
#define GC_GENMODE		2	//!< gc only marks and sweeps new objects in threads

// Garbage collector states
#define GCSbegin	0		//!< The start of the GC collection cycle
#define GCSmark	1			//!< Incrementally mark gray objects
#define GCSatomic	2		//!< Atomic marking of threads
#define GCSsweepsymbol	3	//!< Sweeping symbol table
#define GCSsweep	4		//!< General purpose sweep stage

/** true during all sweep stages */
#define GCSsweepphases (bitmask(GCSsweepsymbol) | bitmask(GCSsweep))

/** Initialize the global state for garbage collection */
void mem_init(VmInfo *vm) {
	vm->gcrunning = 0;
	vm->gcmode = GC_FULLMODE;
	vm->gcnextmode = 0;
	vm->gcstate = GCSbegin;
	vm->gcbarrieron = 0;
	vm->currentwhite = bitmask(WHITE0BIT);
	vm->gray = NULL;

	vm->objlist = NULL;
	vm->sweepgc = NULL;

	vm->gcnewtrigger = GCNEWTRIGGER;
	vm->gcoldtrigger = GCOLDTRIGGER;

	vm->gcnbrnew = 0;
	vm->gcnbrold = 0;
	vm->gctrigger = -vm->gcnewtrigger;
	vm->gcstepdelay = 1;

	vm->totalbytes = sizeof(VmInfo);
}

/* ====================================================================== */

// Test the color of a memory object
#define isgray(x)  /* neither white nor black */  \
	(!testbits((x)->marked, WHITEBITS | bitmask(BLACKBIT))) //!< Return true if object is gray

// Change the color of a memory object
/** Turn off white bits */
#define white2gray(x)	resetbits((x)->marked, WHITEBITS)
/** Turn off black bits */
#define black2gray(x)	resetbit((x)->marked, BLACKBIT)
/** Change color of object to black */
#define gray2black(x)	l_setbit((x)->marked, BLACKBIT)
/** mask for all color and old bits */
#define maskcolors	(~(bit2mask(BLACKBIT, OLDBIT) | WHITEBITS))
/** Erases all color/old bits, then sets to current white color */
#define makewhite(th, x)	\
	(x->marked = cast_byte(((x)->marked & maskcolors) | cast_white(th)))
/** Flip white to opposite color */
#define changewhite(x)	((x)->marked ^= WHITEBITS)

/** Return current white */
#define currentwhite(th) (vm(th)->currentwhite & WHITEBITS)
/** Return the non-current white */
#define otherwhite(th)	(vm(th)->currentwhite ^ WHITEBITS)
/** Return true if marked flags show object is dead (otherwhite) */
#define isdeadm(ow,m)	(!(((m) ^ WHITEBITS) & (ow)))
/** Return true if object is dead (otherwhite) */
#define isdead(th, v)	isdeadm(otherwhite(th), (v)->marked)

/** Is collection mode set to generational? */
#define isgenerational(th)	(vm(th)->gcmode == GC_GENMODE)

void mem_sweepfree(Value th, MemInfo *mb);

/* ====================================================================== */

/** @file
 * Mark Stage
 * ----------
 *
 * Marking begins with the root object (the VM). We recursively traverse from root 
 * to all the current white Values it references, changing their color to black or gray:
 * - gray if it contains any Values. It will be 
 *   placed on the top of the gray chain for later such processing.
 * - black if it contains no Values to check or if its Values have all been checked and marked. 
 *
 * To reduce lag spikes, marking is done incrementally, one or more objects in a step.
 * Thus, objects containing value references to other objects are first marked
 * gray, and on the next iteration marked black as their references are traversed.
 *
 * At the end of all marking (including 'atomic' finalization),
 * mem_gcmemtrav measures how much memory is in all objects marked black.
 * The current white color will be flipped.
*/


/** Perform this mark check every time a Value is put into a parent Value (other than a thread/stack).
 * Why? With incremental garbage collection, it is possible for an unmarked object (white) to be added
 * to an already scanned and marked (black) parent. Doing nothing during the marking phase
 * means the object will never get marked and therefore will be swept away prematurely.
 * We fix this by marking the white value, thereby moving the barrier forward. 
 * This prevents violating the GC invariance principle that no black value should ever point to a white value.
 * Doing this mark check would be onerous for the very common scenario of putting temporary values into the stack.
 * With thread parents, we employ a different strategy:
 * re-marking all stacks in an uninterrupted (atomic) fashion at the end of the marking phase.
 */
void mem_markChk(Value th, Value parent, Value val) {
	if (isPtr(val) && vm(th)->gcbarrieron
		&& isblack((MemInfo*)parent) && iswhite((MemInfo*)val) && !isdead(th, (MemInfo*)val))
			mem_markobjraw(th, (MemInfo*)val);
}

/** Mark a current white object to black or gray (ASSUMES valid white object).
 * (In most cases, use macro mem_markobj to call this only if it is a valid white object).
 * Object is marked black for simple objects without embedded Values, updating gcmemtrav.
 * Object is marked gray for collections with embedded Values, 
 * and added to a gray chain for later marking by mem_marktopgray. */
void mem_markobjraw(Value th, MemInfo *mem) {
	VmInfo* vm = vm(th);
	assert(vm->gcbarrieron);
	white2gray(mem);
	switch (mem->enctyp) {

	// Mark to black all symbols (which have no embedded Values).
	// size is used to update gcmemtrav is size of black-marked mrmory areas.
	case SymEnc: break;

    // We mark to gray the collections that have have embedded values
    // Push it on top of gray list for later handling by mem_marktopgray()
	case StrEnc:
	case ArrEnc:
	case TblEnc: 
	case PartEnc:
	case MethEnc:
	case ThrEnc:
	case VmEnc:
	case LexEnc:
	case CompEnc:
	{
		((MemInfoGray*)mem) -> graylink = vm->gray;
		vm->gray = (MemInfoGray*)mem;
		return;
	}

	// Should never get here
	default: 
		vmLog("GC error: gray marking %p unknown object type (deleted?)", mem); 
		assert(0); 
		return;
	}

	// For non-collections, mark object black and accumulate its size
	gray2black(mem);
}

/** Pop gray object, marking it black and marking any values in it
 * (except threads which stay gray and are not marked now). 
 * This gray object is known to have other values within it.
 * gcmemtrav is incremented by the size of the gray object. */
void mem_marktopgray(Value th) {

	vm(th)->gcstepunits -= GCMARKCOST;

	// Pop the next gray object from list and mark it black
	MemInfoGray *o = vm(th)->gray;
	vm(th)->gray = ((MemInfoGray*) o) -> graylink;
	assert(isgray(o));
	gray2black(o);

	// Go mark object's embedded values, incrementing traversed memory count.
	// This uses encoding-specific macros that know what Values they contain.
	switch (o->enctyp) {
	case StrEnc: strMark(th, (StrInfo*) o); break;
	case ArrEnc: arrMark(th, (ArrInfo*) o);	break;
	case TblEnc: tblMark(th, (TblInfo*)o); break;
	case MethEnc: methodMark(th, (MethodInfo *)o); break;
	case LexEnc: lexMark(th, (LexInfo *)o); break;
	case CompEnc: compMark(th, (CompInfo *)o); break;

	// Thread/Stacks use a different strategy for avoiding invariance violations:
	// keeping it gray until atomic marking, so it is never black pointing to a white value.
	// Threads are not write protected, so it makes little sense to waste time marking it
	case ThrEnc: 
		if (vm(th)->gcstate == GCSmark)
			black2gray(o);
		break;

	// VmEnc has only one instance, the root, which is marked when marking begins for full cycle
	// Should never get here
	default: vmLog("GC error: black marking unknown object type (deleted?)"); assert(0); return;
	}
}

/** Mark all gray objects in the gray list */
void mem_markallgray(Value th) {
	while (vm(th)->gray)
		mem_marktopgray(th);
}

/** Mark everything that should not be interrupted by ongoing object changes */
void mem_markatomic(Value th) {
	assert(!iswhite((ThreadInfo*)th));

	// Clear out any grays that might have accumulated
	mem_markallgray(th);

	// Mark and sweep threads (note: we have not flipped white yet)
	MemInfo **threads = &vm(th)->threads;
	while (*threads) {
		ThreadInfo* thread = (ThreadInfo*) *threads;
		if (thread->marked & vm(th)->currentwhite & WHITEBITS
			&& !(vm(th)->gcnextmode == GC_GENMODE && thread->marked & bitmask(OLDBIT))) {
			*threads = thread->next;
			mem_sweepfree(th, (MemInfo*)thread);
			vm(th)->gcstepunits -= GCSWEEPDEADCOST;
		}
		else {
			thrMark(th, thread);
			if (vm(th)->gcnextmode == GC_GENMODE)
				thread->marked |= bitmask(OLDBIT);
			else
				thread->marked = (thread->marked & ~WHITEBITS) | otherwhite(th);
			vm(th)->gcstepunits -= GCSWEEPLIVECOST;
		}
		threads = &(*threads)->next;
	}
	mem_markallgray(th);
}

/* Keep value alive, if dead but not yet collected */
void mem_keepalive(Value th, MemInfo* blk) {
	if (isdead(th, blk))  // symbol is dead (but was not collected yet)?
		changewhite(blk);  // resurrect it
}

/* ====================================================================== */

/** \file
 * Sweep Stage
 * -----------
 *
 * Sweeping scans the linked list of all allocated objects, freeing all 
 * objects that are marked with previous white color and removing them
 * from the list of allocated objects.
 *
 * To reduce lag spikes, this process is run incrementally, freeing 
 * only a few objects per cycle, with the current sweep position preserved.
 *
 * In generational mode, sweeping stops when the first "old" object is found.
 * Full collection is done periodically to sweep any unreferenced objects after this.
*/

/** Free memory allocated to an unreferenced object.
 * This uses encoding-specific macros that understand the allocated structures. */
void mem_sweepfree(Value th, MemInfo *mb) {
	vm(th)->gcnbrfrees++;
	switch (mb->enctyp) {
	case SymEnc: symFree(th, (SymInfo*)mb); break;
	case StrEnc: strFree(th, (StrInfo*)mb); break;
	case ArrEnc: arrFree(th, (ArrInfo *)mb); break;
	case TblEnc: tblFree(th, (TblInfo *)mb); break;
	case MethEnc: methodFree(th, (MethodInfo*)mb); break;
	case ThrEnc: thrFree(th, (ThreadInfo *)mb); break;
	case LexEnc: lexFree(th, (LexInfo *)mb); break;
	case CompEnc: compFree(th, (CompInfo *)mb); break;
	default: assert(0);
	}
}

/** Sweep at most 'count' elements from passed list of objects, erasing dead ones.
 * A dead (not alive) object is one marked with the "old"
 * (non current) white and not fixed.
 * - In non-generational mode, change all non-dead objects back to white,
 *   preparing for next collection cycle.
 * - In generational mode, keep black objects black, and also mark them as
 *   old; stop when hitting an old object, as all objects after that
 *   one will be old too.
 * When object is a thread, sweep its list of open upvalues too.
 * \param th current thread
 * \param p pointer to linked chain of objects to sweep
 * \param count Maximum number of objects to sweep
 * \return Where we stopped sweep */
MemInfo **mem_sweeplist(Value th, MemInfo **p, bool doall) {
	int ow = otherwhite(th);
	int toclear, toset;  // bits to clear and to set in all live objects
	int tostop;  // stop sweep when this is true

	if (vm(th)->gcnextmode == GC_GENMODE) {  /* generational mode? */
		toclear = ~0;  /* clear nothing */
		toset = bitmask(OLDBIT);  /* set the old bit of all surviving objects */
		tostop = bitmask(OLDBIT);  /* do not sweep old generation */
	}
	else {  /* full mode */
		toclear = maskcolors;  /* clear all color bits + old bit */
		toset = currentwhite(th);  /* make object white */
		tostop = 0;  /* do not stop */
	}

	// Sweep loop for 'count' objects (or end of list)
	while (*p != NULL && (doall || vm(th)->gcstepunits > 0)) {
		MemInfo *curr = *p;
		int marked = curr->marked;
		// If 'curr' object is dead, remove from list and free
		if (isdeadm(ow, marked)) {
			*p = curr->next;
			mem_sweepfree(th, curr);
			vm(th)->gcstepunits -= GCSWEEPDEADCOST;
		}
		// If 'curr' object is live, mark it white or old, as needed
		else {
			if (testbits(marked, tostop))
				return NULL;  /* stop sweeping this list */
			// In gen mode, count new's converted to old,
			// used to trigger a full GC cycle
			vm(th)->gcstepunits -= GCSWEEPLIVECOST;
			if (tostop)
				vm(th)->gcnbrold++;
			// update marks
			curr->marked = ((marked & toclear) | toset);
			p = &curr->next;  // go to next element
		}
	}

	return (*p == NULL) ? NULL : p;
}

/** Sweep the entire list (rather than incremental) */
MemInfo **mem_sweepwholelist(Value th, MemInfo **p) { 
	return mem_sweeplist(th, p, 1); 
}

/** Clean up after sweep by collapsing buffers, as needed */
void mem_sweepcleanup(Value th) {
	// do not change sizes in emergency
	if (vm(th)->gcmode == GC_EMERGENCY) return;

	// Shrink symbol table, if usage is grown too small
	SymTable* sym_tbl = &vm(th)->sym_table;
	sym_tblshrinkcheck(th);
}

/* Free all allocated objects, ahead of VM shut-down */
void mem_freeAll(Value th) {
	VmInfo* vm = vm(th);
	Auint i;

	// Handle objects with finalizers
	// separatetobefnz(th, 1);  /* separate all objects with finalizers */
	// assert(vm->finobj == NULL);
	// callallpendingfinalizers(th, 0);

	vm->currentwhite = WHITEBITS; /* this "white" makes all objects look dead */
	vm->gcstate = GC_FULLMODE;
	// mem_sweepwholelist(th, &vm->finobj);  /* finalizers can create objs. in 'finobj' */
	mem_sweepwholelist(th, &vm->objlist);
	for (i = 0; i < vm->sym_table.nbrAvail; i++)  /* free all symbol lists */
		mem_sweepwholelist(th, (MemInfo**) &vm->sym_table.symArray[i]);
	assert(vm->sym_table.nbrUsed == 0);
}


/** \file
 * Garbage Collector
 * -----------------
 *
 * The garbage collector implements a tri-color, incremental, single- (or dual-) generation 
 * mark-and-sweep algorithm. 
 *
 * Every time new memory is allocated, the debt (pause threshold) is increased. When it 
 * gets over 0, incremental collection proceeds in step-wise fashion through the phases 
 * of marking and sweeping at a step-size controlled by the GC multiplier. At the end,
 * there is a pause on GC activity for a period established by the GC pause factor.
 *
*/

/** Perform a single step of the collection process based on its current state
 * This is the heart of the incremental collection process, progressively stepping the 
 * collector through the mark and sweep phases.
 * Returns the amount of memory traversed during the step. */
void mem_gconestep(Value th) {
	VmInfo *vm = vm(th);

	// Perform next step based on the current state of the garbage collector
	switch (vm->gcstate) {

	// Begin the collection process anew, initializing the marking cycle
	case GCSbegin: {
		vm->gctrigger = 0 - vm->gcnewtrigger;
		vm->gcnbrnew = 0;

		vm->gcnbrmarks = 0;
		vm->gcnbrfrees = 0;
		vm->gcmicrodt = 0;

		// Start incremental marking
		vm->gcstate = GCSmark;
		vm->gcbarrieron = 1;

		// If we are doing full GC, start with root (the VM)
		if (vm->gcmode == GC_FULLMODE) {
			vm->gray = NULL;
			vmMark(th, (VmInfo *)vm(th));
		}
		return;
	}

	// Marks gray objects one-at-a-time
	// When all gray objects are marked, do the atomic marking then start sweep phase
	case GCSmark: {
		if (vm->gray) {
			mem_marktopgray(th);
			return;
		}

		// Done with all gray marking, pause for a bit
		vm->gcstate = GCSatomic;
		return;
	}

	// Mark the write-unprotected threads in uninterrupted fashion (stop the world)
	case GCSatomic: {
		// Since sweeping is about to start, we must ensure
		// next cycle's mode is decided, so sweep sets up objects correctly
		if (vm->gcnextmode == 0) {
			if (vm->gcnbrold >= vm->gcoldtrigger)
				vm->gcnextmode = GC_FULLMODE;
			else
				vm->gcnextmode = GC_GENMODE;
		}
		if (vm->gcnextmode != GC_GENMODE)
			vm->gcnbrold = 0;
		vm->gcnbrnew = 0;

		// Do atomic (finalization) marking
		mem_markatomic(th);  // add what was traversed by 'atomic'

		// Begin sweep phase
		vm->gcstate = GCSsweepsymbol;
		vm->currentwhite = otherwhite(th);  // flip current white
		assert(vm(th)->sweepgc == NULL);
		vm(th)->sweepsymgc = 0;
		vm(th)->sweepgc = &vm(th)->objlist; // Point to list of objects at start of sweep

		if (vm->gcnextmode == GC_FULLMODE)
			vm->gcbarrieron = 0;
		return;
	}

    // Sweep unreferenced symbols first
    case GCSsweepsymbol: {
		Auint i;
		for (i = 0; i < GCSWEEPMAX && vm->sweepsymgc + i < vm->sym_table.nbrAvail; i++)
			mem_sweepwholelist(th, (MemInfo**) &vm->sym_table.symArray[vm->sweepsymgc + i]);
		vm->sweepsymgc += i;
		if (vm->sweepsymgc >= vm->sym_table.nbrAvail)  /* no more symbols to sweep? */
			vm->gcstate = GCSsweep;
		return;
    }

	// Sweep all other unreferenced objects
	case GCSsweep: {
		if (vm->sweepgc) {
			vm->sweepgc = mem_sweeplist(th, vm->sweepgc, 0);
			return;
		}
		else {
			mem_sweepcleanup(th);

#ifdef GCLOG
			vmLog("Completed %s garbage collection cycle. %d marked, %d freed.", 
				vm->gcmode==GC_GENMODE? "generational" : "full",
				vm->gcnbrmarks, vm->gcnbrfrees);
#endif

			vm->gcstate = GCSbegin;  // finish collection
			vm->gcmode = vm->gcnextmode;
			vm->gcnextmode = 0;
			vm->gctrigger = vm->gcnbrnew - vm->gcnewtrigger;
			return;
		}
	}
    default: assert(0); return;
	}
}

#ifdef GCLOG
#include <Windows.h>
#endif

/* Perform a step's worth of garbage collection. */
void mem_gcstep(Value th) {
	if (!vm(th)->gcrunning)
		return;

#ifdef GCLOG
	int steptype = vm(th)->gcstate;
	LARGE_INTEGER freq, start, end, dur;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);
#endif

	// Always perform at least one single step
	vm(th)->gcstepunits = GCMAXSTEPCOST;
	do {
		mem_gconestep(th);
	} while (vm(th)->gcstepunits > 0 && vm(th)->gcstate!=GCSbegin);

#ifdef GCLOG
	QueryPerformanceCounter(&end);
	dur.QuadPart = (end.QuadPart - start.QuadPart)*1000000 / freq.QuadPart;
	vmLog("GC steps %d-%d took: %llu usec", steptype, vm(th)->gcstate, dur.QuadPart);
#endif
}

/** Finish (or perform) a full garbage collection cycle */
void mem_gcfullcycle(Value th) {
	while (vm(th)->gcstate == GCSbegin)
		mem_gconestep(th);
	while (vm(th)->gcstate != GCSbegin)
		mem_gconestep(th);
}

/** Perform a full garbage collection cycle.
 * It will be automatically called in emergency mode if memory fills up.
 * If emergency, do not call finalizers, which could change stack positions
 */
void mem_gcfull(Value th, int isemergency) {

	// If we are in GC cycle past where next cycle can be decided,
	// then just finish the cycle
	if (vm(th)->gcstate >= GCSatomic)
		mem_gcfullcycle(th);

	// Reset GC mode based on isemergency
	if (isemergency)
		vm(th)->gcnextmode = GC_EMERGENCY;
	else {
		vm(th)->gcnextmode = GC_FULLMODE;
	}

	mem_gcfullcycle(th); // Do/finish a cycle in current mode
	mem_gcfullcycle(th); // This is the requested full/emergency cycle
}


/** Start garbage collection */
void mem_gcstart(Value th) {
	vm(th)->gcrunning = 1;
}

/** Stop garbage collection */
void mem_gcstop(Value th) {
	vm(th)->gcrunning = 0;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
