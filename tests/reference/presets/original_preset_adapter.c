/* Test-only hardware boundary shims. Original storage functions are included
 * unchanged, with their original notice. Derived work: see LICENSE-LXR.txt.
 * FatFs writes target byte arrays, UART queries receive deterministic replies.
 * The original UART parser, filesystem, hardware timing and AVR ABI are NOT
 * emulated. Count-pointer width adaptation is explicit (AVR int16/host int32). */
#include "original_preset_adapter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include P17_ORIGINAL_MENU_HEADER
#include P17_ORIGINAL_FRONT_HEADER

/* Suppress hardware headers; retain real menu/protocol/parameter/step headers. */
#define _FATFS 6502
#define UART_H_
#define LCD_ROUTINES_H
#define TIMEBASE_H_
#define DIN_H_
#define DOUT_H_
typedef struct { int unused; } FATFS;
typedef struct { int unused; } FIL;
typedef unsigned int UINT;
typedef int FRESULT;
#define FR_OK 0
#define FR_DISK_ERR 1
#define FA_CREATE_ALWAYS 1
#define FA_WRITE 2
#define FA_OPEN_EXISTING 4
#define FA_READ 8
uint8_t parameter_values[NUM_PARAMS], parameters2[END_OF_SOUND_PARAMETERS];
uint8_t menu_currentPresetNr[NUM_PRESET_LOCATIONS], menu_shownPattern;
volatile uint8_t frontParser_newSeqDataAvailable;
volatile StepData frontParser_stepData;
volatile MidiMsg frontParser_midiMsg;
uint8_t frontPanel_sysexMode;
volatile uint16_t time_sysTick;
static uint8_t io_bytes[65536];
static size_t io_size, io_at;
static char io_name[16];
static struct P17Message messages[1024];
static size_t message_count;
static unsigned query_mode, query_byte, query_number;
static void require_adapter(int ok) { if (!ok) abort(); }
static void count_result(void* p, size_t width, unsigned value) {
    if (width == sizeof(uint16_t)) { uint16_t v=(uint16_t)value; memcpy(p,&v,width); }
    else { unsigned int v=value; require_adapter(width==sizeof(v)); memcpy(p,&v,width); }
}
static FRESULT io_write(const void* p, size_t n, void* count, size_t width) {
    require_adapter(n<=sizeof(io_bytes)-io_at);
    memcpy(io_bytes+io_at,p,n); io_at+=n; if(io_at>io_size)io_size=io_at;
    count_result(count,width,(unsigned)n); return FR_OK;
}
static FRESULT io_read(void* p, size_t n, void* count, size_t width) {
    if(n>io_size-io_at)n=io_size-io_at;
    memcpy(p,io_bytes+io_at,n);io_at+=n;count_result(count,width,(unsigned)n);return FR_OK;
}
static FRESULT io_open(const char* path, unsigned flags) {
    require_adapter(strlen(path)<sizeof(io_name));strcpy(io_name,path);io_at=0;
    if(flags&FA_CREATE_ALWAYS)io_size=0;
    return FR_OK;
}
static FRESULT io_close(void* file) {(void)file;return FR_OK;}
static FRESULT io_mount(unsigned drive,void* fs) {(void)drive;(void)fs;return FR_OK;}
#define f_mount(d,f) io_mount(d,f)
#define f_open(f,p,m) ((void)(f),io_open(p,m))
#define f_close(f) io_close(f)
#define f_write(f,p,n,c) ((void)(f),io_write(p,n,c,sizeof(*(c))))
#define f_read(f,p,n,c) ((void)(f),io_read(p,n,c,sizeof(*(c))))
void lcd_clear(void) {}
void lcd_home(void) {}
void lcd_setcursor(unsigned a,unsigned b) {(void)a;(void)b;}
void lcd_string(const char* p) {(void)p;}
#define lcd_string_F lcd_string
void menu_repaintAll(void) {}
void menu_switchPage(uint8_t page) {(void)page;}
void led_setActiveVoice(uint8_t voice) {(void)voice;}
void menu_sendAllGlobals(void) {}
void din_readNextInput(void) {}
void dout_updateOutputs(void) {}
void uart_checkAndParse(void) {}
static char* p17_itoa(int v,char* p,int base) {(void)base;sprintf(p,"%d",v);return p;}
#define itoa p17_itoa
void frontPanel_sendData(uint8_t status,uint8_t a,uint8_t b) {
    require_adapter(message_count<sizeof(messages)/sizeof(messages[0]));
    messages[message_count++]=(struct P17Message){status,a,b};
}
static void reply(unsigned n) {
    if(query_mode==SYSEX_REQUEST_STEP_DATA) {
        frontParser_stepData=(StepData){(uint8_t)(n*17u+0x80u),(uint8_t)(n%128u),
            (uint8_t)((n*3u)%128u),(uint8_t)(n%228u),(uint8_t)((n+7u)%128u),
            (uint8_t)((n+11u)%228u),(uint8_t)((n+13u)%128u)};
    } else if(query_mode==SYSEX_REQUEST_MAIN_STEP_DATA) {
        unsigned word=(0x1234u+n*257u)&65535u;
        frontParser_stepData.volume=(uint8_t)(word>>8);
        frontParser_stepData.prob=(uint8_t)word;
        frontParser_stepData.note=(uint8_t)(n%16u);
    } else {
        frontParser_stepData.volume=(uint8_t)((n+1u)%8u);
        frontParser_stepData.prob=(uint8_t)(n+2u);
    }
    frontParser_newSeqDataAvailable=1;
}
void frontPanel_sendByte(uint8_t byte) {
    if(byte==SYSEX_START){frontParser_midiMsg.status=SYSEX_START;query_mode=0;query_byte=0;return;}
    if(byte==SYSEX_END){query_mode=0;return;}
    if(query_mode==0){query_mode=byte;query_byte=0;return;}
    if(query_mode==SYSEX_REQUEST_PATTERN_DATA){reply(byte);return;}
    if(query_mode==SYSEX_REQUEST_STEP_DATA||query_mode==SYSEX_REQUEST_MAIN_STEP_DATA) {
        if(query_byte++==0)query_number=(unsigned)byte<<7;
        else {query_number|=byte;query_byte=0;reply(query_number);}
    }
}
/* The runner copies only these unchanged functions from pinned menu.c and
 * records their exact source ranges/hashes: menu_init and numtostrpu. */
void paramToModTargetInit(void);
#include P17_ORIGINAL_MENU_FRAGMENT
#include P17_ORIGINAL_PRESET_SOURCE

void p17_original_contract(struct P17OriginalContract* r) {
    *r=(struct P17OriginalContract){END_OF_SOUND_PARAMETERS,PAR_BEGINNING_OF_GLOBALS,
        NUM_PARAMS,NUM_PARAMS-PAR_BEGINNING_OF_GLOBALS,sizeof(StepData),
        {offsetof(StepData,volume),offsetof(StepData,prob),offsetof(StepData,note),
         offsetof(StepData,param1Nr),offsetof(StepData,param1Val),offsetof(StepData,param2Nr),offsetof(StepData,param2Val)},
         NUM_TRACKS,NUM_PATTERN,STEPS_PER_PATTERN,FILE_VERSION,getNumModTargets()};
}
void p17_original_defaults(uint8_t* values,size_t cap) {
    menu_init();require_adapter(cap>=NUM_PARAMS);memcpy(values,parameter_values,NUM_PARAMS);
}
size_t p17_original_write(unsigned format,const uint8_t* values,size_t count,
 const uint8_t name[8],uint8_t* output,size_t cap,char filename[16]) {
    require_adapter(count==NUM_PARAMS);memcpy(parameter_values,values,count);
    memcpy(preset_currentName,name,8);frontParser_midiMsg.status=0;query_mode=0;
    if(format==P17_SOUND)preset_saveDrumset(7,0);
    else if(format==P17_PATTERN)preset_savePattern(7);
    else if(format==P17_ALL)preset_saveAll(7,1);
    else if(format==P17_PERFORMANCE)preset_saveAll(7,0);
    else preset_saveGlobals();
    require_adapter(cap>=io_size);memcpy(output,io_bytes,io_size);strcpy(filename,io_name);return io_size;
}
int p17_original_read_sound(const uint8_t* data,size_t size,int morph,uint8_t* base,uint8_t* target) {
    require_adapter(size<=sizeof(io_bytes));memcpy(io_bytes,data,size);io_size=size;io_at=0;
    memset(parameter_values,0,sizeof(parameter_values));memset(parameters2,0,sizeof(parameters2));
    memset(parameter_values,41,END_OF_SOUND_PARAMETERS);message_count=0;
    int result=preset_loadDrumset(7,(uint8_t)morph);
    memcpy(base,parameter_values,END_OF_SOUND_PARAMETERS);memcpy(target,parameters2,END_OF_SOUND_PARAMETERS);return result;
}
size_t p17_original_messages(struct P17Message* output,size_t cap) {
    require_adapter(cap>=message_count);memcpy(output,messages,message_count*sizeof(*output));return message_count;
}
unsigned p17_original_target_parameter(unsigned i) {
    return i<getNumModTargets()?pgm_read_word(&modTargets[i].param):65535u;
}
/* Each enum ordinal is compiled from the actual AVR definitions. The generated
 * list binds IDs0..227 independently of plugin descriptors/parameter_map.md. */
unsigned p17_original_parameter_enum(unsigned id) {
    static const unsigned ids[]={
#include P17_ORIGINAL_ENUM_LIST
    };
    return id<sizeof(ids)/sizeof(ids[0])?ids[id]:65535u;
}
