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
#define GCSpause	2		//!< Pause collection activity
#define GCSatomic	3		//!< Atomic marking of threads
#define GCSsweepsymbol	4	//!< Sweeping symbol table
#define GCSsweep	5		//!< General purpose sweep stage

/** true during all sweep stages */
#define GCSsweepphases (bitmask(GCSsweepsymbol) | bitmask(GCSsweep))

/** Initialize the global state for garbage collection */
void mem_init(VmInfo *vm) {
	vm->gcestimate = 0;
	vm->gcdebt = 0;
	vm->totalbytes = sizeof(VmInfo);
	vm->gcpause = AVM_GCPAUSE;
	vm->gcmajorinc = AVM_GCMAJOR;
	vm->gcstepmul = AVM_GCMUL;

	vm->gcrunning = 0;
	vm->gcmode = GC_FULLMODE;
	vm->gcstate = GCSbegin;
	vm->gcbarrieron = 0;
	vm->currentwhite = bit2mask(WHITE0BIT, FIXEDBIT);
	vm->gray = vm->grayagain = NULL;

	vm->objlist = NULL;
	vm->sweepgc = NULL;
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

/** Tell us whether we must enforce the invariance rule that black objects
 * never point to white ones. True always, except during non-generational sweep.
 * During a non-generational collection, the sweep phase may break the invariant, 
 * as such a sweep only frees old white objects, not current white. */
#define enforceinvariant(th)	(isgenerational(th) || vm(th)->gcstate <= GCSatomic)

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
#include <stdio.h.>
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
	Aint size = 0;
	white2gray(mem);
	switch (mem->enctyp) {

	// Mark to black all symbols (which have no embedded Values).
	// size is used to update gcmemtrav is size of black-marked mrmory areas.
	case SymEnc: {
		size = sym_memsize(sym_size(mem));
		break;
	}

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
	vm->gcmemtrav += size;
}

/** Pop gray object, marking it black and marking any values in it
 * (except threads which are moved to grayagain list, instead of black). 
 * This gray object is known to have other values within it.
 * gcmemtrav is incremented by the size of the gray object. */
void mem_marktopgray(Value th) {

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
	case ThrEnc: 
		{
			ThreadInfo *thread = (ThreadInfo*) o;

			// We keep the thread gray to avoid having to do mem_markchk every time
			// white values are put into a thread/stack (which is often).
			// By remembering the thread in grayagain, we can traverse it a second time
			// during the atomic marking step at the end of marking.
			if (vm(th)->gcstate == GCSmark) {
				thread->graylink = vm(th)->grayagain;
				vm(th)->grayagain = o;
				black2gray(o);
			}

			// Fully mark all values in the thread, including all values on the stack
			thrMark(th, thread);
		}
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
Aint mem_markatomic(Value th) {
	assert(!iswhite((MemInfo*)th));
	Aint work = -(Aint) vm(th)->gcmemtrav;  // start counting work

	mem_markallgray(th);

	// Mark values in all threads (which are unprotected by a white barrier)
	if (vm(th)->gcmode == GC_GENMODE) {
		thrMark(th, (ThreadInfo*) th);
		mem_markallgray(th);
	}

	// Re-mark the objects in grayagain (threads and what they point to)
	work += vm(th)->gcmemtrav;  /* stop counting (do not (re)count grays) */
	assert(vm(th)->gray == NULL);
	vm(th)->gray = vm(th)->grayagain;
	vm(th)->grayagain = NULL;
	mem_markallgray(th);
	work -= vm(th)->gcmemtrav;  /* re-start counting */
	
	work += vm(th)->gcmemtrav;  /* complete counting */
	return work;  /* estimate of memory marked by 'atomic' */
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
MemInfo **mem_sweeplist(Value th, MemInfo **p, Auint count) {
	int ow = otherwhite(th);
	int toclear, toset;  // bits to clear and to set in all live objects
	int tostop;  // stop sweep when this is true

	if (isgenerational(th)) {  /* generational mode? */
		toclear = ~0;  /* clear nothing */
		toset = bitmask(OLDBIT);  /* set the old bit of all surviving objects */
		tostop = bitmask(OLDBIT);  /* do not sweep old generation */
	}
	else {  /* normal mode */
		toclear = maskcolors;  /* clear all color bits + old bit */
		toset = currentwhite(th);  /* make object white */
		tostop = 0;  /* do not stop */
	}

	// Sweep loop for 'count' objects (or end of list)
	while (*p != NULL && count-- > 0) {
		MemInfo *curr = *p;
		int marked = curr->marked;
		// If 'curr' object is dead, remove from list and free
		if (isdeadm(ow, marked)) {
			*p = curr->next;
			mem_sweepfree(th, curr);
		}
		// If 'curr' object is live, mark it white or old, as needed
		else {
			if (testbits(marked, tostop))
				return NULL;  /* stop sweeping this list */
			// update marks
			curr->marked = ((marked & toclear) | toset);
			p = &curr->next;  // go to next element
		}
	}

	return (*p == NULL) ? NULL : p;
}

/** Sweep the entire list (rather than incremental) */
MemInfo **mem_sweepwholelist(Value th, MemInfo **p) { 
	return mem_sweeplist(th, p, MAX_MEM); 
}

/** Sweep a list until we find a live object (or end of list)
 * \param th current thread
 * \param p List of objects to sweep
 * \param n pointer to a nbr to increment by how many objects were swept
 * \return Pointer to object inside the list */
MemInfo **mem_sweepToLive(Value th, MemInfo **p, int *n) {
	MemInfo **start = p;
	int i = 0;
	do {
		i++;
		p = mem_sweeplist(th, p, 1);
	} while (p == start);
	if (n) *n += i;
	return p;
}

/** Initialize the start of sweep stage
 * Note: The call to 'sweepToLive' ensures pointers point to an
 * object inside the list (instead of to the header), so that we
 * do not sweep objects created between "now" and the start
 * of the real sweep.
 * Return how many objects were swept. */
Aint mem_sweepbegin(Value th) {
	int n = 0;
	assert(vm(th)->sweepgc == NULL);

	// prepare to sweep objects
	vm(th)->sweepsymgc = 0;
	vm(th)->sweepgc = mem_sweepToLive(th, &vm(th)->objlist, &n);
	return n;
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

/** Total byte + gcdebt = how many bytes of allocated memory we have */
#define gettotalbytes(th) (vm(th)->totalbytes + vm(th)->gcdebt)

/** Reset GCdebt so that (totalbytes + GCdebt) stays equivalent.
 * garbage collection only occurs when gcdebt >= 0 */
void mem_gcsetdebt(Value th, Aint debt) {
	vm(th)->totalbytes -= (debt - vm(th)->gcdebt);
	vm(th)->gcdebt = debt;
}

/** Set a reasonable "time" to wait before starting a new GC cycle.
 * Collection will start when allocated memory use hits a calculated threshold
 * of 'estimate*gcpause/PAUSEADJ'.
 * In incremental mode, estimate is how much memory was marked (kept) in last cycle.
 * With gcpause at 200 and PAUSEADJ at 100, this means wait until used memory doubles. */
void mem_setpause(Value th, Aint estimate) {
	Aint debt, threshold;
	Aint oldest = estimate;
	Aint pauseadj = PAUSEADJ;
	Aint pau = vm(th)->gcpause;
	estimate = estimate / PAUSEADJ;  // adjust 'estimate'
	threshold = (vm(th)->gcpause < MAX_MEM / estimate)  // overflow?
            ? estimate * vm(th)->gcpause  // no overflow
            : MAX_MEM;  // overflow; truncate to maximum
	debt = -(Aint) (threshold - gettotalbytes(th));
	mem_gcsetdebt(th, debt);
}

/** Perform a single step of the collection process based on its current state
 * This is the heart of the incremental collection process, progressively stepping the 
 * collector through the mark and sweep phases.
 * Returns the amount of memory traversed during the step. */
Aint mem_gconestep(Value th) {
	VmInfo *vm = vm(th);

	// Perform next step based on the current state of the garbage collector
	switch (vm->gcstate) {

	// Begin the collection process anew, initializing the marking cycle
	case GCSbegin: {

		// Start incremental marking
		vm->gcstate = GCSmark;
		vm->gcbarrieron = 1;
		vm->gcmemtrav = 0;

		// If we are doing full GC, start with root (the VM)
		if (vm->gcmode == GC_FULLMODE) {
			vm->gray = vm(th)->grayagain = NULL;
			vmMark(th, (VmInfo *)vm(th));
			vm->gcmemtrav = sym_tblsz(th); // Indicate we traversed symbol table
		}
		return vm->gcmemtrav;
	}

	// Marks gray objects one-at-a-time
	// When all gray objects are marked, do the atomic marking then start sweep phase
	case GCSmark: {
		if (vm->gray) {
			Aint oldtrav = vm->gcmemtrav;
			mem_marktopgray(th);
			return vm->gcmemtrav - oldtrav;  /* memory traversed in this step */
		}

		// Done with all gray marking, pause for a bit
		vm->gcstate = GCSpause;
		vm->gcnextmode = GC_FULLMODE; // Set a default mode
		return 0;
	}

	// Pause for a bit. Write barrier is on.
	// While here, outsider could trigger forward movement and reset gcnextmode 
	case GCSpause: {
		vm->gcstate = GCSatomic;
		return 0;
	}

	// Mark the write-unprotected threads in uninterrupted fashion (stop the world)
	case GCSatomic: {
		// Do atomic (finalization) marking
		Aint work;
		vm->gcestimate = vm->gcmemtrav;  // save what was counted
		work = mem_markatomic(th);  // add what was traversed by 'atomic'
		vm->gcestimate += work;  // estimate of total memory traversed

		// Begin sweep phase
		vm->gcstate = GCSsweepsymbol;
		vm->currentwhite = otherwhite(th);  // flip current white
		if (vm->gcnextmode == GC_FULLMODE)
			vm->gcbarrieron = 0;
		return work + mem_sweepbegin(th) * GCSWEEPCOST;
	}

    // Sweep unreferenced symbols first
    case GCSsweepsymbol: {
		Auint i;
		for (i = 0; i < GCSWEEPMAX && vm->sweepsymgc + i < vm->sym_table.nbrAvail; i++)
			mem_sweepwholelist(th, (MemInfo**) &vm->sym_table.symArray[vm->sweepsymgc + i]);
		vm->sweepsymgc += i;
		if (vm->sweepsymgc >= vm->sym_table.nbrAvail)  /* no more symbols to sweep? */
			vm->gcstate = GCSsweep;
		return i * GCSWEEPCOST;
    }

	// Sweep all other unreferenced objects
	case GCSsweep: {
		if (vm->sweepgc) {
			vm->sweepgc = mem_sweeplist(th, vm->sweepgc, GCSWEEPMAX);
			return GCSWEEPMAX*GCSWEEPCOST;
		}
		else {
			/* sweep main thread */
			MemInfo *mt = (MemInfo*) vm(th)->main_thread;
			mem_sweeplist(th, &mt, 1);
			mem_sweepcleanup(th);
			vm->gcmode = vm->gcnextmode;
			vm->gcstate = GCSbegin;  // finish collection
			return GCSWEEPCOST;
		}
	}
    default: assert(0); return 0;
	}
}

/** Finish (or perform) a full garbage collection cycle */
void mem_gcfullcycle(Value th) {
	while (vm(th)->gcstate == GCSpause)
		mem_gconestep(th);
	while (vm(th)->gcstate == GCSpause)
		mem_gconestep(th);
}

/** advance the garbage collector until it reaches a state allowed by 'statemask' */
void mem_gcuntilstate(Value th, int statesmask) {
	while (!testbit(statesmask, vm(th)->gcstate))
		mem_gconestep(th);
}

/** Perform a single step in generational mode */
void mem_gcgenstep(Value th) {
	assert(vm(th)->gcstate == GCSmark);
	if (vm(th)->gcestimate == 0) {  // signal for another major collection?
		mem_gcfull(th, 0);  // perform a full regular collection
		vm(th)->gcestimate = gettotalbytes(th);  // update control
	}
	else {
		Auint estimate = vm(th)->gcestimate;
		mem_gcfullcycle(th);  // run complete (minor) cycle
		vm(th)->gcstate = GCSmark;  // skip restart
		int x = gettotalbytes(th);
		int y = (estimate / 100) * vm(th)->gcmajorinc;
		if (gettotalbytes(th) > (estimate / 100) * vm(th)->gcmajorinc)
			vm(th)->gcestimate = 0;  // signal for a major collection
		else
			vm(th)->gcestimate = estimate;  // keep estimate from last major coll.
	}
	mem_setpause(th, gettotalbytes(th));
	assert(vm(th)->gcstate == GCSmark);
}

/** Perform a single incremental mode garbage collection step */
void mem_gcnormalstep(Value th) {
	Aint stepwork, stepmul;

	// convert debt from Kb to 'work units' (avoid zero debt and overflows)
	stepwork = (vm(th)->gcdebt / STEPMULADJ) + 1;
	stepmul = vm(th)->gcstepmul;
	if (stepmul < 40) stepmul = 40;  // avoid ridiculous low values (and 0)
	stepwork = (stepwork < MAX_MEM / stepmul) ? stepwork * stepmul : MAX_MEM;

	// Always perform at least one single step
	do {
		stepwork -= mem_gconestep(th);
	} while (stepwork > -GCSTEPSIZE && vm(th)->gcstate != GCSpause);

	// Adjust debts and thresholds 
	if (vm(th)->gcstate == GCSbegin)
		mem_setpause(th, vm(th)->gcestimate);  // pause until next cycle
	else {
		stepwork = (stepwork / stepmul) * STEPMULADJ;  // convert 'work units' to Kb
		mem_gcsetdebt(th, stepwork);
	}
}

/** Perform a basic GC step */
void mem_gcforcestep(Value th) {
	if (isgenerational(th)) 
		mem_gcgenstep(th);
	else mem_gcnormalstep(th);
}

/* Perform a step's worth of garbage collection. */
void mem_gcstep(Value th) {
	if (vm(th)->gcrunning) 
		mem_gcforcestep(th);
	else 
		// If GC not running, reduce how often gcstep is called
		mem_gcsetdebt(th, -GCSTEPSIZE);
}

/** Perform a full garbage collection cycle.
 * It will be automatically called in emergency mode if memory fills up.
 * If emergency, do not call finalizers, which could change stack positions
 */
void mem_gcfull(Value th, int isemergency) {

	// Save original gc mode
	int origmode = vm(th)->gcmode;
	assert(origmode != GC_EMERGENCY);

	// Reset GC mode based on isemergency
	if (isemergency)
		vm(th)->gcmode = GC_EMERGENCY;
	else {
		vm(th)->gcmode = GC_FULLMODE;
	}

	// If there might be black objects, sweep all objects to turn them back to white
	// (since white has not changed, nothing will be collected)
	if (enforceinvariant(th)) {
		mem_sweepbegin(th);
	}
	// finish any pending sweep phase to start a new cycle
	mem_gcuntilstate(th, bitmask(GCSpause));

	// Start and run entire collection
	mem_gcuntilstate(th, ~bitmask(GCSpause));
	mem_gcuntilstate(th, bitmask(GCSpause));

	// If we had been in generational mode, keep it in propagate phase
	if (origmode == GC_GENMODE) {
		mem_gcuntilstate(th, bitmask(GCSmark));
	}

	// Reset to original mode
	vm(th)->gcmode = origmode;
	mem_setpause(th, vm(th)->totalbytes + vm(th)->gcdebt);
}


/** Start garbage collection */
void mem_gcstart(Value th) {
	mem_gcsetdebt(th, 0);
	vm(th)->gcrunning = 1;
}

/** Stop garbage collection */
void mem_gcstop(Value th) {
	vm(th)->gcrunning = 0;
}

/** change GC mode to generational (GC_GENMODE) or incremental (GC_FULLMODE) */
void mem_gcsetmode(Value th, int mode) {
	if (mode == vm(th)->gcmode) return;  /* nothing to change */

	// Change to generational mode
	if (mode == GC_GENMODE) {
		/* make sure gray lists are consistent */
		mem_gcuntilstate(th, bitmask(GCSmark));
		vm(th)->gcestimate = gettotalbytes(th);
		vm(th)->gcmode = GC_GENMODE;
	}
	// change to incremental mode
	else {  
		/* sweep all objects to turn them back to white
			(as white has not changed, nothing extra will be collected) */
		vm(th)->gcmode = GC_FULLMODE;
		mem_sweepbegin(th);
		mem_gcuntilstate(th, ~GCSsweepphases);
	}
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
