#include "zardos.h"

static ScheduleTick scheduleKeys[ZARDOS_HEAP_SIZE+1];
static byte scheduleValues[ZARDOS_HEAP_SIZE+1];
volatile ScheduleTick scheduleCurTick;

//#define scheduleCurTick (scheduleKeys[0])
#define scheduleSize (scheduleValues[0])
#include "zardos_arch.h"

enum {
	ZARDOS_RUN = 0,
	ZARDOS_SUSPEND
};

static byte eventBuffer[ZARDOS_EVENT_BUFFER_SIZE+2];
static byte eventBufferDontCompact;
static ZInterrupt intBuffer[ZARDOS_INT_BUFFER_SIZE];
static int intBufferWrite;
static int intBufferRead;
static int zardosRunning = ZARDOS_SUSPEND;
#define TOP_BUFFER_INDEX 0
#define FREE_QUEUE_INDEX 1
#define topEventBuffer (eventBuffer[TOP_BUFFER_INDEX])
#define freeEventQueue (eventBuffer[FREE_QUEUE_INDEX])

#define callZEvent(X) \
	((ZEvent)X)->call((ZEvent)X)

#define callZEventFromHandle(X) \
	callZEvent(((ZEvent)&eventBuffer[X]))

#define ZEventCallPointer(X) \
	(((ZEvent)&eventBuffer[X]))->call

static void runCompaction(void);


/* initialiseZardos is called at some point during device
   initialisation to set-up the memory buffer and the
   scheduling heap. It takes no parameters.
*/
void initializeZardos(void) {
	zardosSleep();
	eventBuffer[2] = 0x00;
	eventBuffer[3] = ZARDOS_EVENT_BUFFER_SIZE-2;
	topEventBuffer = 0x02;
	freeEventQueue = 0x00;
	scheduleSize = 0x00;
	intBufferRead = intBufferWrite = 0;
	eventBufferDontCompact = 0;
}

ScheduleTick getCurTick(void) {
	return scheduleCurTick;
}

/* scheduleZInterrupt is the procedure used to insert a
** ZInterrupt for immediate execution using a ring buffer
**  approach.
** event - the subroutine to run (takes NO ARGUMENTS AND
**           RETURNS NOTHING).
*/
void scheduleZInterrupt(ZInterrupt event) {
	intBuffer[intBufferWrite++] = event;
	if (intBufferWrite == ZARDOS_INT_BUFFER_SIZE)
		intBufferWrite = 0;
	//TODO check if overflow has occurred
}

void runZInterrupts(void) {
	while (intBufferRead != intBufferWrite) {
		intBuffer[intBufferRead++]();
		if (intBufferRead == ZARDOS_INT_BUFFER_SIZE)
			intBufferRead = 0;
	}
}

/* scheduleZEvent is the procedure used to insert a ZEvent on-to the
   heap. The time in the future, plus the ZEvent are provided as
   parameters. If 8-bit scheduling is enabled, then events will be
   able to be scheduled 127 ticks in the future. If 16-bit scheduling
   in enabled then events may scheduled 32767 ticks in the future.
*/
void scheduleZEvent(ScheduleTick eventTick, void* event) {
	int idx = ++scheduleSize;
	//ScheduleTick mask;
	byte loc = ((byte*)event)-eventBuffer;
	eventTick += scheduleCurTick;
	
	//mask = scheduleCurTick & TICK_MASK;
	//eventTick ^= mask;
	//while (eventTick < (scheduleKeys[idx>>1]^mask) && idx > 1) {
	while ((scheduleKeys[idx>>1]-eventTick) < TICK_MASK && idx > 1) {
		scheduleKeys[idx] = scheduleKeys[idx>>1];
		scheduleValues[idx] = scheduleValues[idx>>1];
		idx >>= 1;
	}
	//scheduleKeys[idx] = eventTick ^ mask;
	scheduleKeys[idx] = eventTick;
	scheduleValues[idx] = loc;
}

static void doFlash(void) {
	errorFlash(50);
}

/* schedulePop is an internal procedure that is called only from zardosTick.
   it checks to see whether the top ZEvent on the stack is scheduled for the
   current, or previous tick, and if so, it removes it from the heap and
   returns the ZEvent in question. The heap is left in a clean way after
   removal. The removed ZEvent is then considered unstable and no collection
   should be run before it is freed or put back on the heap.
*/
static inline byte schedulePop(ScheduleTick curTick) {
	ScheduleTick cidx, idx;
	byte ret = scheduleValues[1];
	ScheduleTick last;

	if (scheduleSize == 0 || (scheduleKeys[1]-curTick) > 0)
		return 0;

	if ((scheduleKeys[1]-curTick) < 0)
		scheduleZInterrupt(doFlash);
	last = scheduleKeys[scheduleSize];

	idx = 1;
	cidx = 2;
	while(cidx <= scheduleSize) {
		if (cidx < scheduleSize && (scheduleKeys[cidx] - scheduleKeys[cidx+1]) < TICK_MASK)
			++cidx;
		if ((scheduleKeys[cidx] - last) < TICK_MASK)
			break;
		else {
	 		scheduleKeys[idx] = scheduleKeys[cidx];
			scheduleValues[idx] = scheduleValues[cidx];
			idx = cidx;
			cidx <<= 1;
		}
	}
	if (idx != scheduleSize) {
		scheduleKeys[idx] = scheduleKeys[scheduleSize];
		scheduleValues[idx] = scheduleValues[scheduleSize];
	}
	scheduleSize--;
	return ret;
}

/* zardosTick is called by the interrupt handler of the main program
** and is used to advance the zardos schedule to the next task
*/
void zardosTick(void) {
	if (zardosRunning == ZARDOS_RUN)
		scheduleCurTick++;
}

/* zardosSleep is used to suspend the main zardos process so that
** all scheduled operations are made to wait
*/
void zardosSleep(void) {
	zardosRunning = ZARDOS_SUSPEND;
}

/* zardosWake is used to wake up ZardOS from suspension
*/
void zardosWake(void) {
	zardosRunning = ZARDOS_RUN;
}

/* zardosTask is the main run-time of ZardOS. It is called at some point
   in the main loop of the master program. It will check to see whether
   any events require handling for the current tick or a previous tick
   that was not properly finished. If no ZEvents are scheduled for the
   current tick then zardOS will assume that spare cycles are available
   to collect the heap and will do so.
*/
void zardosTask(void) {
	byte flag = 0;
	byte handle;
	runZInterrupts();
	while((handle=schedulePop(scheduleCurTick))!=0) {
		flag = 1;
		callZEventFromHandle(handle);
	}
	if (!flag)
		runCompaction();
}

/* zardosLoop is the main run-time of ZardOS. It is run after all the hardware
   set-up is done and will basically just run ZardOS forever. Seriously, just
   look at that fucking procedure: Not exactly rocket science is it? No. It's
   an eternal loop and if you need a giant multi-line comment to understand that,
   then you're a godamned idiot.
*/
void zardosLoop(void) {
	zardosWake();
	while (!0)
		zardosTask();
}

/* allocateZEvent is a procedure that allocates a new ZEvent.
   A new ZEvent is extremely unstable: If the collector is called
   when an allocated ZEvent is not present on the heap then no
   guarantee is made that the ZEvent will not be destroyed by
   the compaction process.
   allocateZEvent should not be called directly. The macro
   newZEvent is provided to allocate a new ZEvent.
*/
void* allocateZEvent(byte size) {
	byte allocatePos = topEventBuffer;
	byte lastPos = TOP_BUFFER_INDEX;
	do {
		if (size < eventBuffer[allocatePos+1]) {
			eventBuffer[allocatePos+1]-=size;
			return (void*)&eventBuffer[allocatePos+eventBuffer[allocatePos+1]];
		} else if (size == eventBuffer[allocatePos+1]) {
			eventBuffer[lastPos] = eventBuffer[allocatePos];
			return (void*)&eventBuffer[allocatePos];
		} else {
			lastPos = allocatePos;
			allocatePos = eventBuffer[allocatePos];
		}
	} while (allocatePos);
	return 0X00;
}

/* deallocateZEvent frees a ZEvent that is no longer required. Unlike
   the "free" command, the size of the object to be freed is explicitly
   required for the function.
   This procedure should not be called directly. The macro freeZEvent
   is provided to deallocate a ZEvent.
*/
void deallocateZEvent(void* event, byte size) {
	byte loc = ((byte*)event)-eventBuffer;
	eventBuffer[loc] = freeEventQueue;
	eventBuffer[loc+1] = size;
	freeEventQueue = loc;
}


/* sortQueue is an internal procedure that sorts the free queue
   so that higher memory appears first.
   ASSUMES that there are at least two elements on the queue
*/
static inline void sortQueue(void) {
	byte curPos, nextPos, nextNextPos;
	byte flag = 1;

	while (flag) {
		flag = 0;
		curPos = FREE_QUEUE_INDEX;
		nextPos = freeEventQueue;

		while ((nextNextPos = eventBuffer[nextPos])) {
			if (nextPos < nextNextPos) {
				eventBuffer[curPos] = nextNextPos;
				eventBuffer[nextPos] = eventBuffer[nextNextPos];
				eventBuffer[nextNextPos] = nextPos;
				flag = 1;
				curPos = nextNextPos;
			} else if (nextNextPos + eventBuffer[nextNextPos+1] == nextPos){
				eventBuffer[nextNextPos+1] += eventBuffer[nextPos+1];
				eventBuffer[curPos] = nextNextPos;
				nextPos = nextNextPos;
			} else {
				curPos = nextPos;
				nextPos = nextNextPos;
			}
		}
	}
}

/* runCollection is an internal procedure that moves newly
   freed data from the free queue onto the free memory region,
   coalescing contiguous blocks as it goes.
   runCollection will sortQueue the free queue if this is necessary
*/
static void runCollection(void) {
	byte hp, lp, tp;
	byte lastp;
	//are there even any freed events?
	if (!freeEventQueue)
		return;

	//Sort the eventBuffer if there are more than one element on the queue
	if (eventBuffer[freeEventQueue])
		sortQueue();

	if (freeEventQueue > topEventBuffer) {
		lp = topEventBuffer;
		hp = topEventBuffer = freeEventQueue;
	} else {
		hp = topEventBuffer;
		lp = freeEventQueue;
	}
	freeEventQueue = 0;
	lastp = TOP_BUFFER_INDEX;
	while(lp) {
		if (lp + eventBuffer[lp+1] == hp) {
			eventBuffer[lp+1] += eventBuffer[hp+1];
			eventBuffer[lastp] = lp;
			hp = eventBuffer[hp];
		} else if (eventBuffer[hp] < lp) {
			tp = eventBuffer[hp];
			eventBuffer[hp] = lp;
			lastp = hp;
			hp = lp;
			lp = tp;
			continue;
		} else {
			lastp = hp;
			hp = eventBuffer[hp];
		}
		if (hp < lp) {
			tp = hp;
			hp = lp;
			lp = tp;
		}
	}
}

/* runCompaction is an internal procedure that compacts the entire task
   memory region such that only a single block of memory remains, with
   allocated memory being shifted up to the higher regions of memory.
*/
static void runCompaction(void) {
	byte nextPos, nextNextPos;
	
	runCollection();
	
	if (eventBufferDontCompact)
		return;

	nextNextPos = 0;
	nextPos = topEventBuffer;
	while (nextPos != 0 && nextNextPos < 5) {
		nextNextPos ++;
		nextPos = eventBuffer[nextPos];
	}
	if (nextNextPos < 5)
		return;
	nextNextPos = topEventBuffer;
	while ((nextNextPos = eventBuffer[nextPos=nextNextPos])) {
		byte fromPos, size;
		fromPos = nextNextPos + eventBuffer[nextNextPos+1];

		size = scheduleSize;
		while (size) {
			if (scheduleValues[size]>=fromPos && scheduleValues[size]<nextPos) {
				scheduleValues[size] += eventBuffer[nextPos+1];
			}
			--size;
		}
		eventBuffer[nextNextPos+1]+=eventBuffer[nextPos+1];
		size = nextPos - fromPos;
		fromPos = nextPos;
		nextPos = nextPos + eventBuffer[nextPos+1];
		while (size--) 
			eventBuffer[--nextPos] = eventBuffer[--fromPos];
	}
	topEventBuffer = nextPos;
}
