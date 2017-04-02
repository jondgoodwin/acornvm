/** Yielder/Process/Thread type methods and properties
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/** Return true if thread is done */
int context_isdone(Value th) {
	pushValue(th, th(getLocal(th, 0))->flags1 & ThreadDone? aTrue : aFalse);
	return 1;
}

/** Reset a yielder */
int context_reset(Value th) {
	// Re-initialize thread info that resets it
	ThreadInfo *context = (ThreadInfo*) getLocal(th, 0);
	context->flags1 &= ~(ThreadDone);
	context->yieldTo = aNull;
	CallInfo *cf = context->curmethod = &context->entrymethod;
	cf->nresults = 0;
	cf->ip = ((BMethodInfo*) cf->method)->code; // Start with first instruction

	// Return the thread
	setTop(th, 1);
	return 1;
}

/** Run a yielder with no parms nor return values */
int context_call(Value th) {
	setTop(th, 1);
	getCall(th, 0, BCVARRET);
	return getTop(th);
}

/** Number of frames in a context */
int context_frames(Value th) {
	ThreadInfo *context = (ThreadInfo*) getLocal(th, 0);
	CallInfo *ci = context->curmethod;
	// Heisenberg perspective: Do not include frame for this call
	int nframes = context==th? 0 : 1;
	while ((ci=ci->previous) != NULL)
		nframes++;
	pushValue(th, anInt(nframes));
	return 1;
}

CallInfo* context_getCI(Value th, Value context, Aint frame) {
	CallInfo *ci = ((ThreadInfo*)context)->curmethod;
	// Heisenberg perspective: Do not include frame for this call
	if (context==th)
		frame++;
	while (frame-- && ci!=NULL)
		ci=ci->previous;
	return ci;
}

/** Retrieve method at designated frame */
int context_method(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th, 1)))
		return 0;
	CallInfo *ci = context_getCI(th, getLocal(th, 0), toAint(getLocal(th,1)));
	if (ci==NULL)
		return 0;
	pushValue(th, ci->method);
	return 1;
}

/** Retrieve size of stack at designated frame */
int context_stacksize(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th, 1)))
		return 0;
	CallInfo *ci = context_getCI(th, getLocal(th, 0), toAint(getLocal(th,1)));
	if (ci==NULL)
		return 0;
	pushValue(th, anInt(ci->end - ci->begin));
	return 1;
}

/** Retrieve value of stack at designated frame and position */
int context_stack(Value th) {
	if (getTop(th)<3 || !isInt(getLocal(th, 1)) || !isInt(getLocal(th,2)))
		return 0;
	CallInfo *ci = context_getCI(th, getLocal(th, 0), toAint(getLocal(th,1)));
	if (ci==NULL)
		return 0;
	Aint pos = toAint(getLocal(th,2));
	pushValue(th, pos>=(ci->end-ci->begin)? aNull : (ci->begin)[pos]);
	return 1;
}

/** Initialize the execution context types */
void core_thread_init(Value th) {
	vmlit(TypeYieldc) = pushType(th, vmlit(TypeType), 4);
		pushSym(th, "Yielder");
		popProperty(th, 0, "_name");
		vmlit(TypeYieldm) = pushMixin(th, vmlit(TypeType), aNull, 16);
			pushSym(th, "*Yielder");
			popProperty(th, 1, "_name");
			pushCMethod(th, context_isdone);
			popProperty(th, 1, "done?");
			pushCMethod(th, context_reset);
			popProperty(th, 1, "Reset");
			pushCMethod(th, context_call);
			popProperty(th, 1, "Begin");
			pushCMethod(th, context_call);
			popProperty(th, 1, "End");
			pushCMethod(th, context_frames);
			popProperty(th, 1, "frames");
			pushCMethod(th, context_method);
			popProperty(th, 1, "method");
			pushCMethod(th, context_stacksize);
			popProperty(th, 1, "stacksize");
			pushCMethod(th, context_stack);
			popProperty(th, 1, "stack");
		popProperty(th, 0, "traits");

	popGloVar(th, "Yielder");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
