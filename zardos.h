#include <stdint.h>
/* The following three defines will set-up all the parameters
   of the ZardOS instance for the target chip
*/

/* 8bit or 16bit schedule ticks. This will define how far in the
   future events can be scheduled.
*/
#ifndef SCHEDULE_8BIT
#define SCHEDULE_16BIT
#endif

/* Heap size defines how many tasks can exist on the heap at any
   time
*/
#ifndef ZARDOS_HEAP_SIZE
#define ZARDOS_HEAP_SIZE 16
#endif

/* The event buffer size determines how much memory is available
   for ZEvents
*/
#ifndef ZARDOS_EVENT_BUFFER_SIZE
#define ZARDOS_EVENT_BUFFER_SIZE 256
#endif

/* The interrupt buffer size determines how much memory is available
   for ZInterrupts.
*/
#ifndef ZARDOS_INT_BUFFER_SIZE
#define ZARDOS_INT_BUFFER_SIZE 8
#endif


typedef void(*ZInterrupt)(void);
typedef uint8_t byte;
typedef uint16_t word;

#ifdef SCHEDULE_16BIT
	typedef int16_t ScheduleTick; 
	#define TICK_MASK 0x8000
#else
	typedef int8_t ScheduleTick;
	#define TICK_MASK 0x80
#endif



void initZardosTimer(void);
void initializeZardos(void);
void scheduleZEvent(ScheduleTick eventTick, void* event);
void* allocateZEvent(byte size);
void deallocateZEvent(void* event, byte size);
void zardosLoop(void);
void zardosTick(void);
void zardosSleep(void);
void zardosWake(void);
ScheduleTick getCurTick(void);
void scheduleZInterrupt(ZInterrupt event);

typedef struct _ZEvent* ZEvent;
typedef void(*ZCallback)(ZEvent);
struct _ZEvent {
	ZCallback call;
};

/* DEF_ZEVENT and END_DEF are used to define a new ZEvent type.
   DEF_ZEVENT is used with a single parameter which will be the
   new type name, existing as a pointer to a ZEvent structure,
   as may be returned by newZEvent. As such, a ZEvent should be
   used with C structure pointer syntax "->" to access
   members. An example is:

   DEF_ZEVENT(BlinkEvent)
      byte onOff;
      byte tick;
   END_DEF

   This defines a new ZEvent called BlinkEvent with two members
   onOff and tick.

*/
#define DEF_ZEVENT(X) \
	typedef struct _##X * X; \
	struct _##X { \
		struct _ZEvent base;

#define END_DEF };

/* newZEvent is used to allocate a new ZEvent. Once allocated the
   ZEvent is unstable whilst it is not in the scheduling heap and
   may be freely garbage collected. The size is automatically
   rounded up to the next multiple of two. An example is:

   BlinkEvent be = newZEvent(BlinkEvent);
*/
#define newZEvent(X) \
	((X)allocateZEvent(((1+sizeof(struct _##X))&0xFE)))

/* freeZEvent is used to deallocate a new ZEvent. Unlike the stdlib
   free procedure, the type of the ZEvent must be specified in the
   invocation, so that the size (rounded to two) can correctly be passed to
   deallocateZEvent. An example is:

   freeZEvent(be, BlinkEvent)
*/
#define freeZEvent(X,Y) \
	deallocateZEvent((void*)X, ((1+sizeof(struct _##Y))&0xFE))

/* setZEventCallback is used to set-up the callback used for a
   particular ZEvent. An extant ZEvent can have its callback
   changed at any time. An example is:

   setZEventCallback(be, BlinkEvent_Call)
*/
#define setZEventCallback(X,Y) \
	((ZEvent)X)->call = ((ZCallback)Y)
