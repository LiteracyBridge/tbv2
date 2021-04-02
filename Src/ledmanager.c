// TBookV2  ledmanager.c
//   Apr 2018

#include "tbook.h"
#include "ledMgr.h"

const	bool								swapCOLORS	= false;   // custom build for swapped LED keypad?	
const int									LED_EVT 		=	1;
static osEventFlagsId_t		osFlag_LedThr;		// osFlag to signal an LED change to LedThread 
bool 											ledMgrActive = false;

typedef enum {	LED_BOTH=0, LED_RED=1, LED_GREEN=2, LED_OFF=3 } LEDcolor;
typedef struct ledShade { // define proportions of Both,Red,Green,Off for time-multiplexing LED
	uint8_t dly[4];		// # msec of Both,Red,Green,Off
} ledShade;
				// B  R  G  n
ledShade gShd =  { 0, 0, 1, 8 };		// G  on 10%
ledShade G_Shd = { 0, 0, 4, 6 };		// G  on 40%
ledShade rShd =  { 0, 1, 0, 8 };		// R  on 10%
ledShade R_Shd = { 0, 4, 0, 6 };		// R  on 40%
ledShade oShd =  { 1, 0, 0, 8 };    // RG on 10%
ledShade O_Shd = { 4, 0, 2, 4 };		// RG on 40%  G on 20%
ledShade OffShd ={ 0, 0, 0, 255 };	// special marker value-- no multi-plexing required

const int MAX_SEQ_STEPS = 10;  //avoid NONSTANDARD C
typedef struct ledSeq {
	// definition of sequence
	uint8_t 	nSteps;					// # of shades displayed in sequence
	bool			repeat;					// true, if it loops
	int				repeatCnt;
	ledShade	shade[MAX_SEQ_STEPS];	// shades to display
	int 			msec[MAX_SEQ_STEPS];	// duration of each
	// state during display
	uint8_t 	nextStep;	// next shade to display
	ledShade	currShade;	// shade currently being displayed
	uint8_t 	shadeStep;	// 0=Both, 1=Red, 2=Grn, 3=Off
	int	 	delayLeft;	// msec remaining to display shade
} ledSeq;

static ledSeq ledA, ledB, ledC;		// three static-ly allocated copies
// currSeq always == either bgSeq or fgSeq
//  changes made by filling prepSeq, then swapping it with bgSeq or fgSeq
//  and/or replacing currSeq with bgSeq or fgSeq
static ledSeq *			currSeq = NULL;
static ledSeq *			bgSeq 	= &ledA;
static ledSeq *			fgSeq 	= &ledB;
static ledSeq *			prepSeq = &ledC;
static osThreadAttr_t 	thread_attr;

void 								LED_Init( GPIO_ID led ){		// configure GPIO for output PUSH_PULL slow
	gConfigOut( led );
	gSet( led, 0 ); 
}

ledShade						asShade( char c ){						// R r G g O o Y y __ => ledShade 
	switch (c){
		case 'g': return gShd;
		case 'G': return G_Shd;
		
		case 'r': return rShd;
		case 'R': return R_Shd;
		
		case 'o': return oShd;
		case 'O': return O_Shd;
		
		default:
		case '_': return OffShd;
	}
}
void								seqAddShade( ledSeq *seq, ledShade shade, int ms ){				// add ms of shd to seq
	short step = seq->nSteps;
	if ( step==MAX_SEQ_STEPS ) return;
	if ( ms==0 ) return;
	seq->shade[ step ] = shade;
	seq->msec[ step ] = ms;
	seq->nSteps = step+1;
}
void								convertSeq( ledSeq *seq, const char *def ){		// fill 'seq' based on def e.g. "R20G100_10"
	seq->nextStep = 0;
	seq->shadeStep = 0;
	seq->delayLeft = 0;
	
	seq->nSteps = 0;
	seq->repeat = false;
	seq->repeatCnt = 0;
	short i = 0;
	if ( def==NULL ) return;
	
	//  convert string of Color char, duration in 1/10ths sec to color[] & msec[] for shades
	//  "R G8" => shade R for 0.1sec, G for 0.8sec, repeat=false
	//  "R20 G O _10" => R 2.0sec, G 0.1, O 0.1, off 1.0, repeat=false
	//  "R!" =>  R 0.1, repeat=true			// stays till changed
	while( def[i] != 0 ){
		ledShade shd = asShade( def[i] );
		short tsec = 0;
		i++;
		if (def[i]==' ') i++;
		while ( isdigit( def[i] )){
			tsec = tsec*10 + (def[i]-'0');
			i++;
		}
		if ( def[i]==' ' ) i++;
		if ( def[i]=='!' ){ 
			seq->repeat = true;
			i++;
		}
		if ( tsec==0 ) tsec = 1;
		seqAddShade( seq, shd, tsec*100 ); 
	}
}
void 								ledSet( LEDcolor led ){
	if ( swapCOLORS ) 
		led = led==LED_GREEN? LED_RED : led==LED_RED? LED_GREEN : led;

	switch ( led ){
		case LED_RED:   
			gSet( gRED, 1 );	gSet( gGREEN, 0 );	break;
		case LED_GREEN: 
			gSet( gRED, 0 );	gSet( gGREEN, 1 );	break;
		case LED_BOTH:  
			gSet( gRED, 1 );	gSet( gGREEN, 1 );	break;
		case LED_OFF:   
			gSet( gRED, 0 );	gSet( gGREEN, 0 );	break;
	} 
}

void 								handlePowerEvent( int powerEvent ){
	switch (powerEvent){
		case POWER_UP:
			//dbgLog("Led Power up \n");
			LED_Init( gGREEN );		
			LED_Init( gRED );
#if defined( STM3210E_EVAL )
			LED_Init( gORANGE ); 
			LED_Init( gBLUE );  
#endif
		break;
		case POWER_DOWN:
			gUnconfig( gGREEN );		
			gUnconfig( gRED );
#if defined( STM3210E_EVAL )
			gUnconfig( gORANGE ); 
			gUnconfig( gBLUE );  
#endif
			//dbgLog("Led Power Down \n");
		break;
	}
}
//
//******** ledThread -- update LEDs according to current sequence
volatile static uint32_t wkup = 0;
void								ledThread( void *arg ){	
	dbgLog( "4 inThr: 0x%x 0x%x \n", &arg, &arg + LED_STACK_SIZE );
	while ( true ){
		ledSeq *c = currSeq;		// get consistent copy & use it

		if ( c->delayLeft <= 0 ){		// move to next shade
			if ( c->nextStep >= c->nSteps ){
				if ( c->repeat ){
					c->nextStep = 0;
					c->repeatCnt++;
				} else {
					currSeq = bgSeq;	// finished fgSeq, switch to bg
					continue;			// loop back to start that seq
				}
			}
			c->currShade = c->shade[ c->nextStep ];
			c->shadeStep = 0;
			c->delayLeft = c->msec[ c->nextStep ];
			c->nextStep++;
		}
		int delay = 0, step = c->shadeStep;
		LEDcolor color;
		if ( c->currShade.dly[3] == 255 && c->currShade.dly[0]==0 && c->currShade.dly[1]==0 && c->currShade.dly[2]==0 ){ // == OffShd?
			color = LED_OFF;
			delay = c->delayLeft;		// no need to do time multiplexing
		} else {
			while ( delay==0 ){  // get next non-zero color in this shade
				color = (LEDcolor) step;
				delay = c->currShade.dly[ step ];
				step++;
				if ( step>3 ) step = 0;
			}
		}
		ledSet( color );
		c->shadeStep = step;
		if ( delay > c->delayLeft ) delay = c->delayLeft;
		c->delayLeft -= delay;		// amount of this shade to go
		wkup = osEventFlagsWait( osFlag_LedThr, LED_EVT, osFlagsWaitAny, delay );  // wait for delay or ledFg/ledBg wakeup
	}
}
//PUBLIC
void 								ledFg( const char *def ){						// install 'def' as foreground pattern
	if ( def==NULL || def[0]==0 ){  	// "" => switch to background pattern
		currSeq = bgSeq;
		dbgLog( "9 ledFg: off \n", def );
	} else {
		dbgLog( "9 ledFg: %s \n", def );
		convertSeq( prepSeq, def );	// convert into prep
		ledSeq *sv = fgSeq;			// swap prep with fg
		fgSeq = prepSeq;
		prepSeq = sv;			
		currSeq = fgSeq;			// make fg current as atomic action -- since thread is watching it
		osEventFlagsSet( osFlag_LedThr, LED_EVT );		// wakeup for ledThread
	}
}
void								ledBg( const char *def ){						// install 'def' as background pattern
	dbgLog( "9 ledBg: %s \n", def );
	if (def==NULL)	// NULL = turn off
		def = "_100!";
	convertSeq( prepSeq, def );		// convert into prep
	prepSeq->repeat = true;
	
	ledSeq *sv = bgSeq;			// swap prep with bg
	bgSeq = prepSeq;
	prepSeq = sv;			
	
	if ( currSeq != fgSeq ){   // fg seq not active -- switch to new bg
		currSeq = bgSeq;
		osEventFlagsSet( osFlag_LedThr, LED_EVT );		// wakeup ledThread
	}
}
void 								initLedManager(){				// initialize & spawn LED thread
	handlePowerEvent( POWER_UP );
//	registerPowerEventHandler( handlePowerEvent );

	ledBg( "_49G" );  // 0.1sec G flash every 5 sec
	
	osFlag_LedThr = osEventFlagsNew(NULL);	// os flag ID -- used so ledFg & ledBg can wakeup LedThread
	if ( osFlag_LedThr == NULL)
		tbErr( "osFlag_LedThr alloc failed" );	
	
	thread_attr.name = "led_thread";
	thread_attr.stack_size = LED_STACK_SIZE;
	Dbg.thread[3] = (osRtxThread_t *) osThreadNew( ledThread, NULL, &thread_attr );
	if ( Dbg.thread[3] == NULL )
		tbErr( "ledThread spawn failed" );	
	
	dbgLog( "4 LedMgr OK \n" );
	ledMgrActive = true;				// for tbUtil flashCode
}
// ledmanager.c 
