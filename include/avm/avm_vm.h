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
		MemCommonInfoGray;			//!< Common header for value-containing object

		Value global;				//!< VM's "built in" Global hash table

		Value main_thread;			//!< VM's main thread
		struct ThreadInfo main_thr; //!< State space for main thread

		struct SymTable sym_table;	//!< global symbol table
		AuintIdx hashseed;			//!< randomized seed for hashing strings
		Value literals;				//!< array of all built-in symbol and type literals 
		Value stdidx;				//!< Table to convert std symbol to index
		Value *stdsym;				//!< c-array to convert index to std symbol

		// Garbage Collection state
		MemInfo *objlist;			//!< linked list of all collectable objects
		MemInfo **sweepgc;			//!< current position of sweep in list 'objlist'
		MemInfoGray *gray;			//!< list of gray objects
		MemInfoGray *grayagain;		//!< list of objects to be traversed atomically

		Auint totalbytes;			//!< number of bytes currently allocated - GCdebt
		Aint gcdebt;				//!< bytes allocated, not yet compensated by the collector
		Auint gcmemtrav;			//!< cumulative size of all objects marked black
		Auint gcestimate;			//!< an estimate of the non-garbage memory in use

		char gcmode;				//!< Collection mode: Normal, Emergency, Gen
		char gcstate;				//!< state of garbage collector
		char gcrunning;				//!< true if GC is running
		char currentwhite;			//!< Current white color for new objects

		int gcpause;				//!< size of pause between successive GCs 
		int gcmajorinc;				//!< pause between major collections (only in gen. mode)
		int gcstepmul;				//!< GC `granularity' 

		Auint sweepstrgc;  //!< position of sweep in symbol table

	} VmInfo;

	/** Mark all in-use thread values for garbage collection 
	 * Increments how much allocated memory the thread uses. */
	#define vmMark(th, v) \
		{mem_markobj(th, (v)->main_thread); \
		mem_markobj(th, (v)->global); \
		mem_markobj(th, (v)->literals); \
		mem_markobj(th, (v)->stdidx); \
		vm(th)->gcmemtrav += sizeof(VmInfo);}

	/** Point to standard symbol from index */
	#define vmStdSym(th,idx) (vm(th)->stdsym[idx])
	#define nStdSym 256

	/** C index values for all VM literal values used throughout the code
	    for common symbols and core types. They are forever immune from garbage collection
		by being anchored to the VM. */
	enum VmLiterals {
		// Compiler symbols
		SymNull,	//!< 'null'
		SymFalse,	//!< 'false'
		SymTrue,	//!< 'true'
		SymAnd,			//!< 'and'
		SymAsync,		//!< 'async'
		SymBreak,		//!< 'break'
		SymContinue,	//!< 'continue'
		SymDo,			//!< 'do'
		SymEach,		//!< 'each'
		SymElse,		//!< 'else'
		SymElif,		//!< 'elif'
		SymIf,			//!< 'if'
		SymIn,			//!< 'in'
		SymMatch,		//!< 'match'
		SymNew,			//!< 'new'
		SymNot,			//!< 'not'
		SymOr,			//!< 'or'
		SymReturn,		//!< 'return'
		SymSelf,		//!< 'self'
		SymThis,		//!< 'this'
		SymTo,			//!< 'to'
		SymVar,			//!< 'var'
		SymWait,		//!< 'wait'
		SymWhile,		//!< 'while'
		SymWorker,		//!< 'worker'
		SymYield,		//!< 'yield'
		SymLBrace,	//!< '{'
		SymRBrace,	//!< '}'
		SymSemicolon,	//!< ';'

		// Compiler symbols that are also methods
		SymAppend,	//!< '<<'
		SymPlus,	//!< '+'
		SymMinus,	//!< '-'
		SymMult,	//!< '*'
		SymDiv,		//!< '/'

		// Methods that are not compiler symbols
		// Byte-code (and parser) standard methods
		SymParas,	//!< '()'
		SymNeg,		//!< '-@'
		SymNext,	//!< 'next'

		SymFinalizer, //!< '_finalizer' method for CData

		// AST symbols
		SymMethod,	//!< 'method'
		SymAssgn,	//!< '='
		SymColon,	//!< ':'
		SymThisBlock, //!< 'thisblock'
		SymCallProp, //!< 'callprop'
		SymActProp, //!< 'activeprop'
		SymGlobal,  //!< 'global'
		SymLit,		//!< 'lit'

		// Core type type
		TypeType,	//!< Type
		TypeNullc,	//!< Null class
		TypeNullm,	//!< Null mixin
		TypeBoolc,	//!< Float class
		TypeBoolm,	//!< Float mixin
		TypeIntc,	//!< Integer class
		TypeIntm,	//!< Integer mixin
		TypeFloc,	//!< Float class
		TypeFlom,	//!< Float mixin
		TypeMethc,	//!< Method class
		TypeMethm,	//!< Method mixin
		TypeThrc,	//!< Thread class
		TypeThrm,	//!< Thread mixin
		TypeVmc,	//!< Vm class
		TypeVmm,	//!< Vm mixin
		TypeSymc,	//!< Symbol class
		TypeSymm,	//!< Symbol mixin
		TypeTextc,	//!< Text class
		TypeTextm,	//!< Text mixin
		TypeListc,	//!< List class
		TypeListm,	//!< List mixin
		TypeCloc,	//!< Closure class
		TypeClom,	//!< Closure mixin
		TypeIndexc,	//!< Index class
		TypeIndexm,	//!< Index mixin
		TypeResc,	//!< Index class
		TypeResm,	//!< Index mixin
		TypeAll,	//!< All

		//! Number of literals
		nVmLits
	};

	/** The value for the indexed literal */
	#define vmlit(lit) arr_info(vm(th)->literals)->arr[lit]

	/** Lock the Vm */
	void vm_lock(Value th);
	/** Unlock the Vm */
	void vm_unlock(Value th);

	#define logSevere(msg, ...) {vmLog(msg, __VA_ARGS__); exit(1);}
	#define logError(msg, ...) vmLog(msg, __VA_ARGS__)
	#define logWarning(msg, ...) vmLog(msg, __VA_ARGS__)
	#ifdef _DEBUG
	#define logInfo(msg, ...) vmLog(msg, __VA_ARGS__)
	#else
	#define logInfo(msg, ...)
	#endif

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif