/** Manage memory garbage collection.
 * @file
 * 
 * The garbage collector implements a tri-color, incremental, single- (or dual-) generation
 * mark-and-sweep algorithm.
 *
 * All pointer-based Values (e.g., string, symbol) share a common header
 * at the start of the allocated area. The implementation code for such pointer-based Values 
 * is interwoven with the garbage collection routines in this file.
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
#define KGC_NORMAL	0		//!< gc is in normal (non-generational) collection
#define KGC_EMERGENCY	1	//!< gc was forced by an allocation failure
#define KGC_GEN		2		//!< generational collection

// Garbage collector states
#define GCSmark	0			//!< Mark stage of collection
#define GCSatomic	1		//!< Atomic marking stage of collection
#define GCSsweepsymbol	2	//!< Sweeping symbol table
#define GCSsweep	3		//!< General purpose sweep stage
#define GCSpause	4		//!< Pause stage during collection

/** true during all sweep stages */
#define GCSsweepphases (bitmask(GCSsweepsymbol) | bitmask(GCSsweep))

/** Initialize the global state for garbage collection */
void mem_init(VmInfo *vm) {
	vm->gcdebt = 0;
	vm->totalbytes = sizeof(VmInfo);
	vm->gcpause = AVM_GCPAUSE;
	vm->gcmajorinc = AVM_GCMAJOR;
	vm->gcstepmul = AVM_GCMUL;

	vm->gcrunning = 0;
	vm->gcmode = KGC_NORMAL;
	vm->gcstate = GCSpause;
	vm->currentwhite = bit2mask(WHITE0BIT, FIXEDBIT);

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
/** Return true if ... */
#define isdeadm(ow,m)	(!(((m) ^ WHITEBITS) & (ow)))
/** Return true if object is dead: */
#define isdead(th, v)	isdeadm(otherwhite(th), (v)->marked)



/** Is collection mode set to generational? */
#define isgenerational(th)	(vm(th)->gcmode == KGC_GEN)


/* ====================================================================== */

/** @file
 * Mark Stage
 * ----------
 *
 * Marking begins with the root object (the VM). We traverse from root 
 * recursively to all the current white Values it references, 
 * to change their color to black or gray:
 * - We will mark an object gray if it contains any Values. It will be 
 *   place on the top of the gray chain for later such processing.
 * - We will mark an object black if it contains no Values to check
 *   or if its Values have all been checked and marked. 
 *
 * To reduce lag spikes, marking is done incrementally, typically a single object.
 * Thus, objects containing value references to other objects are first marked
 * gray, and on the next iteration marked black as their references are traversed.
 *
 * At the end of all marking (including 'atomic' finalization),
 * mem_gcmemtrav measures how much memory is in all objects marked black.
 * The current white color will be flipped.
*/


/** Mark a current white object to black or gray (ASSUMES valid white object).
 * (In most cases, use macro mem_markobj to call this only if it is a valid white object).
 * Object is marked black for simple objects without embedded Values, updating gcmemtrav.
 * Object is marked gray for collections with embedded Values, 
 * and added to a gray chain for later marking by mem_marktopgray. */
void mem_markobjraw(Value th, MemInfo *mem) {
	VmInfo* vm = vm(th);
	assert(vm->gcstate == GCSmark);
	Aint size = 0;
	white2gray(mem);
	switch (mem->enctyp) {

	// Mark to black all symbols, strings, and functions (which have no embedded Values).
	// size is used to update gcmemtrav is size of black-marked mrmory areas.
	case SymEnc: {
		size = sym_memsize(sym_size(mem));
		break;
	}

    // We mark too gray the collections have have embedded values
    // Push it on top of gray list for later handling by mem_marktopgray()
	case StrEnc:
	case ArrEnc:
	case TblEnc: 
	case PartEnc:
	case FuncEnc:
	case ThrEnc:
	case VmEnc:
	{
		((MemInfoGray*)mem) -> graylink = vm->gray;
		vm->gray = (MemInfoGray*)mem;
		return;
	}

	// Should never get here
	default: 
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
	case StrEnc: strMark(th, (StrInfo*) o); return;
	case ArrEnc: arrMark(th, (ArrInfo*) o);	return;
	case TblEnc: tblMark(th, (TblInfo*)o); return;
	case PartEnc: partMark(th, (PartInfo*)o); return;
	case FuncEnc: funcMark(th, (FuncInfo *)o); return;
	case ThrEnc: thrMark(th, (ThreadInfo *)o); return;
	case VmEnc: vmMark(th, (VmInfo *)o); return;

	// Should never get here
	default: assert(0); return;
	}
}

/** Mark all gray objects in the gray list */
void mem_markallgray(Value th) {
	while (vm(th)->gray)
		mem_marktopgray(th);
}

/** Mark everything that cannot be interrupted by ongoing object changes */
Aint mem_markatomic(Value th) {
	assert(!iswhite(vm(th))); // Root object is white
	Aint work = -(Aint) vm(th)->gcmemtrav;  // start counting work

	// Nothing atomic to run at the moment! (Later the finalizers?)
	
	work += vm(th)->gcmemtrav;  /* complete counting */
	return work;  /* estimate of memory marked by 'atomic' */
}

/** Start garbage collection cycle by initializing the mark cycle */
void mem_markbegin(Value th) {
	// Reset gray object lists
	vm(th)->gray = vm(th)->grayagain = NULL;

	// Mark root object = virtual machine
	mem_markobj(th, vm(th));
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
 * Full collection may be needed sweep any unreferenced objects after this.
*/

/** Free memory allocated to an unreferenced object.
 * This uses encoding-specific macros that understand the allocated structures. */
void mem_sweepfree(Value th, MemInfo *mb) {
	switch (mb->enctyp) {
	case SymEnc: symFree(th, (SymInfo*)mb); break;
	case StrEnc: strFree(th, (StrInfo*)mb); break;
	case ArrEnc: arrFree(th, (ArrInfo *)mb); break;
	case TblEnc: tblFree(th, (TblInfo *)mb); break;
	case PartEnc: partFree(th, (PartInfo*)mb); break;
	case FuncEnc: funcFree(th, (FuncInfo*)mb); break;
	case ThrEnc: thrFree(th, (ThreadInfo *)mb); break;
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
	vm(th)->gcstate = GCSsweepsymbol;

	// Manually fix color of vm & main thread who are not in allocated chain
	vm(th)->marked = (vm(th)->marked & maskcolors) | currentwhite(th);
	vm(th)->main_thr.marked = (vm(th)->main_thr.marked & maskcolors) | currentwhite(th);

	// prepare to sweep objects
	vm(th)->sweepstrgc = 0;
	vm(th)->sweepgc = mem_sweepToLive(th, &vm(th)->objlist, &n);
	return n;
}

/** Clean up after sweep by collapsing buffers, as needed */
void mem_sweepcleanup(Value th) {
	// do not change sizes in emergency
	if (vm(th)->gcmode == KGC_EMERGENCY) return;

	// Shrink symbol table, if usage is grown too small
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
	vm->gcstate = KGC_NORMAL;
	// mem_sweepwholelist(th, &vm->finobj);  /* finalizers can create objs. in 'finobj' */
	mem_sweepwholelist(th, &vm->objlist);
	for (i = 0; i < vm->sym_table.nbrAvail; i++)  /* free all string lists */
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

	// Perform next step based on the current state of the garbage collector
	switch (vm(th)->gcstate) {

	// Pause begins the collection process anew, initializing the marking cycle
	case GCSpause: {
		assert(!isgenerational(th));
		// Start count of memory traversed - include symbol table size as traversed
		vm(th)->gcmemtrav = sym_tblsz(th);
		vm(th)->gcstate = GCSmark;
		mem_markbegin(th);
		return vm(th)->gcmemtrav;
	}

	// Marks gray objects one-at-a-time
	// When all gray objects are marked, do the atomic marking then start sweep phase
	case GCSmark: {
		if (vm(th)->gray) {
			Aint oldtrav = vm(th)->gcmemtrav;
			mem_marktopgray(th);
			return vm(th)->gcmemtrav - oldtrav;  /* memory traversed in this step */
		}
		// If no more gray objects to mark, finalize all marking, flip current white
		// and then start the sweep stage
		else {  
			Aint work;

			// Do atomic (finalization) marking
			vm(th)->gcstate = GCSatomic;
			vm(th)->gcestimate = vm(th)->gcmemtrav;  // save what was counted
			work = mem_markatomic(th);  // add what was traversed by 'atomic'
			vm(th)->gcestimate += work;  // estimate of total memory traversed

			// Begin sweep phase
			vm(th)->currentwhite = otherwhite(th);  // flip current white
			return work + mem_sweepbegin(th) * GCSWEEPCOST;
		}
	}

    // Sweep unreferenced symbols first
    case GCSsweepsymbol: {
		Auint i;
		for (i = 0; i < GCSWEEPMAX && vm(th)->sweepstrgc + i < vm(th)->sym_table.nbrAvail; i++)
			mem_sweepwholelist(th, (MemInfo**) &vm(th)->sym_table.symArray[vm(th)->sweepstrgc + i]);
		vm(th)->sweepstrgc += i;
		if (vm(th)->sweepstrgc >= vm(th)->sym_table.nbrAvail)  /* no more strings to sweep? */
			vm(th)->gcstate = GCSsweep;
		return i * GCSWEEPCOST;
    }

	// Sweep all other unreferenced objects
	case GCSsweep: {
		if (vm(th)->sweepgc) {
			vm(th)->sweepgc = mem_sweeplist(th, vm(th)->sweepgc, GCSWEEPMAX);
			return GCSWEEPMAX*GCSWEEPCOST;
		}
		else {
			/* sweep main thread */
			MemInfo *mt = (MemInfo*) vm(th)->main_thread;
			mem_sweeplist(th, &mt, 1);
			mem_sweepcleanup(th);
			vm(th)->gcstate = GCSpause;  // finish collection
			return GCSWEEPCOST;
		}
	}
    default: assert(0); return 0;
	}
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
		mem_gcuntilstate(th, bitmask(GCSpause));  // run complete (minor) cycle
		vm(th)->gcstate = GCSmark;  // skip restart
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
	if (vm(th)->gcstate == GCSpause)
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


/** Prevent white objects from pointing to black? (during sweep or generational)
 * Macro to tell when main invariant (white objects cannot point to black
 * ones) must be kept. During a non-generational collection, the sweep
 * phase may break the invariant, as objects turned white may point to
 * still-black objects. The invariant is restored when sweep ends and
 * all objects are white again. During a generational collection, the
 * invariant must be kept all times. */
#define keepinvariant(th)	(isgenerational(th) || vm(th)->gcstate <= GCSatomic)

/** Perform a full garbage collection cycle.
 * It will be automatically called in emergency mode if memory fills up.
 * If emergency, do not call finalizers, which could change stack positions
 */
void mem_gcfull(Value th, int isemergency) {

	// Save original gc mode
	int origmode = vm(th)->gcmode;
	assert(origmode != KGC_EMERGENCY);

	// Reset GC mode based on isemergency
	if (isemergency)
		vm(th)->gcmode = KGC_EMERGENCY;
	else {
		vm(th)->gcmode = KGC_NORMAL;
	}

	// If there might be black objects, sweep all objects to turn them back to white
	// (since white has not changed, nothing will be collected)
	if (keepinvariant(th)) {
		mem_sweepbegin(th);
	}
	// finish any pending sweep phase to start a new cycle
	mem_gcuntilstate(th, bitmask(GCSpause));

	// Start and run entire collection
	mem_gcuntilstate(th, ~bitmask(GCSpause));
	mem_gcuntilstate(th, bitmask(GCSpause));

	// If we had been in generational mode, keep it in propagate phase
	if (origmode == KGC_GEN) {
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

/** change GC mode to generational (KGC_GEN) or incremental (KGC_NORMAL) */
void mem_gcsetmode(Value th, int mode) {
	if (mode == vm(th)->gcmode) return;  /* nothing to change */

	// Change to generational mode
	if (mode == KGC_GEN) {
		/* make sure gray lists are consistent */
		mem_gcuntilstate(th, bitmask(GCSmark));
		vm(th)->gcestimate = gettotalbytes(th);
		vm(th)->gcmode = KGC_GEN;
	}
	// change to incremental mode
	else {  
		/* sweep all objects to turn them back to white
			(as white has not changed, nothing extra will be collected) */
		vm(th)->gcmode = KGC_NORMAL;
		mem_sweepbegin(th);
		mem_gcuntilstate(th, ~GCSsweepphases);
	}
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
