#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>

#include "pxa260.h"
#include "pxa260_DMA.h"
#include "pxa260_DSP.h"
#include "pxa260_GPIO.h"
#include "pxa260_IC.h"
#include "pxa260_LCD.h"
#include "pxa260_PwrClk.h"
#include "pxa260_RTC.h"
#include "pxa260_TIMR.h"
#include "pxa260_UART.h"
#include "pxa260I2c.h"
#include "pxa260Timing.h"
#include "../armv5te/cpu.h"
#include "../armv5te/emu.h"
#include "../armv5te/mem.h"
#include "../armv5te/os/os.h"
#include "../armv5te/translate.h"
#include "../tungstenT3Bus.h"
#include "../emulator.h"


#define PXA260_IO_BASE 0x40000000
#define PXA260_MEMCTRL_BASE 0x48000000

#define PXA260_TIMER_TICKS_PER_FRAME (TUNGSTEN_T3_CPU_CRYSTAL_FREQUENCY / EMU_FPS)


uint16_t*    pxa260Framebuffer;
Pxa255pwrClk pxa260PwrClk;
Pxa255ic     pxa260Ic;

static Pxa255lcd  pxa260Lcd;
static Pxa255timr pxa260Timer;
static Pxa255gpio pxa260Gpio;


#include "pxa260Accessors.c.h"

bool pxa260Init(uint8_t** returnRom, uint8_t** returnRam){
   uint32_t mem_offset = 0;
   uint8_t i;

   //set timing callback pointers
   pxa260TimingInit();

   //enable dynarec if available
   do_translate = true;

   mem_and_flags = os_reserve(MEM_MAXSIZE * 2);
   if(!mem_and_flags)
      return false;

   addr_cache_init();
   memset(mem_areas, 0x00, sizeof(mem_areas));

   //regions
   //ROM
   mem_areas[0].base = PXA260_ROM_START_ADDRESS;
   mem_areas[0].size = TUNGSTEN_T3_ROM_SIZE;
   mem_areas[0].ptr = mem_and_flags + mem_offset;
   mem_offset += TUNGSTEN_T3_ROM_SIZE;

   //RAM
   mem_areas[1].base = PXA260_RAM_START_ADDRESS;
   mem_areas[1].size = TUNGSTEN_T3_RAM_SIZE;
   mem_areas[1].ptr = mem_and_flags + mem_offset;
   mem_offset += TUNGSTEN_T3_RAM_SIZE;

   //memory regions that are not directly mapped to a buffer are not added to mem_areas
   //adding them causes SIGSEGVs

   //accessors
   //default
   for(i = 0; i < PXA260_TOTAL_MEMORY_BANKS; i++){
       // will fallback to bad_* on non-memory addresses
       read_byte_map[i] = memory_read_byte;
       read_half_map[i] = memory_read_half;
       read_word_map[i] = memory_read_word;
       write_byte_map[i] = memory_write_byte;
       write_half_map[i] = memory_write_half;
       write_word_map[i] = memory_write_word;
   }

   //PCMCIA0
   for(i = PXA260_START_BANK(PXA260_PCMCIA0_START_ADDRESS); i <= PXA260_END_BANK(PXA260_PCMCIA0_START_ADDRESS, PXA260_PCMCIA0_SIZE); i++){
       read_byte_map[i] = pxa260_pcmcia0_read_byte;
       read_half_map[i] = pxa260_pcmcia0_read_half;
       read_word_map[i] = pxa260_pcmcia0_read_word;
       write_byte_map[i] = pxa260_pcmcia0_write_byte;
       write_half_map[i] = pxa260_pcmcia0_write_half;
       write_word_map[i] = pxa260_pcmcia0_write_word;
   }

   //PCMCIA1
   for(i = PXA260_START_BANK(PXA260_PCMCIA1_START_ADDRESS); i <= PXA260_END_BANK(PXA260_PCMCIA1_START_ADDRESS, PXA260_PCMCIA1_SIZE); i++){
       read_byte_map[i] = pxa260_pcmcia1_read_byte;
       read_half_map[i] = pxa260_pcmcia1_read_half;
       read_word_map[i] = pxa260_pcmcia1_read_word;
       write_byte_map[i] = pxa260_pcmcia1_write_byte;
       write_half_map[i] = pxa260_pcmcia1_write_half;
       write_word_map[i] = pxa260_pcmcia1_write_word;
   }

   //IO
   read_byte_map[PXA260_START_BANK(PXA260_IO_BASE)] = pxa260_io_read_byte;
   read_half_map[PXA260_START_BANK(PXA260_IO_BASE)] = bad_read_half;
   read_word_map[PXA260_START_BANK(PXA260_IO_BASE)] = pxa260_io_read_word;
   write_byte_map[PXA260_START_BANK(PXA260_IO_BASE)] = pxa260_io_write_byte;
   write_half_map[PXA260_START_BANK(PXA260_IO_BASE)] = bad_write_half;
   write_word_map[PXA260_START_BANK(PXA260_IO_BASE)] = pxa260_io_write_word;

   //LCD
   read_byte_map[PXA260_START_BANK(PXA260_LCD_BASE)] = bad_read_byte;
   read_half_map[PXA260_START_BANK(PXA260_LCD_BASE)] = bad_read_half;
   read_word_map[PXA260_START_BANK(PXA260_LCD_BASE)] = pxa260_lcd_read_word;
   write_byte_map[PXA260_START_BANK(PXA260_LCD_BASE)] = bad_write_byte;
   write_half_map[PXA260_START_BANK(PXA260_LCD_BASE)] = bad_write_half;
   write_word_map[PXA260_START_BANK(PXA260_LCD_BASE)] = pxa260_lcd_write_word;

   //MEMCTRL
   read_byte_map[PXA260_START_BANK(PXA260_MEMCTRL_BASE)] = bad_read_byte;
   read_half_map[PXA260_START_BANK(PXA260_MEMCTRL_BASE)] = bad_read_half;
   read_word_map[PXA260_START_BANK(PXA260_MEMCTRL_BASE)] = pxa260_memctrl_read_word;
   write_byte_map[PXA260_START_BANK(PXA260_MEMCTRL_BASE)] = bad_write_byte;
   write_half_map[PXA260_START_BANK(PXA260_MEMCTRL_BASE)] = bad_write_half;
   write_word_map[PXA260_START_BANK(PXA260_MEMCTRL_BASE)] = pxa260_memctrl_write_word;

   //W86L488
   read_byte_map[PXA260_START_BANK(TUNGSTEN_T3_W86L488_START_ADDRESS)] = bad_read_byte;
   read_half_map[PXA260_START_BANK(TUNGSTEN_T3_W86L488_START_ADDRESS)] = pxa260_static_chip_select_2_read_half;
   read_word_map[PXA260_START_BANK(TUNGSTEN_T3_W86L488_START_ADDRESS)] = bad_read_word;
   write_byte_map[PXA260_START_BANK(TUNGSTEN_T3_W86L488_START_ADDRESS)] = bad_write_byte;
   write_half_map[PXA260_START_BANK(TUNGSTEN_T3_W86L488_START_ADDRESS)] = pxa260_static_chip_select_2_write_half;
   write_word_map[PXA260_START_BANK(TUNGSTEN_T3_W86L488_START_ADDRESS)] = bad_write_word;

   *returnRom = mem_areas[0].ptr;
   *returnRam = mem_areas[1].ptr;

   return true;
}

void pxa260Deinit(void){
   if(mem_and_flags){
       // translation_table uses absolute addresses
       flush_translations();
       memset(mem_areas, 0, sizeof(mem_areas));
       os_free(mem_and_flags, MEM_MAXSIZE * 2);
       mem_and_flags = NULL;
   }

   addr_cache_deinit();
}

void pxa260Reset(void){
   /*
   static void emu_reset()
   {
       memset(mem_areas[1].ptr, 0, mem_areas[1].size);

       memset(&arm, 0, sizeof arm);
       arm.control = 0x00050078;
       arm.cpsr_low28 = MODE_SVC | 0xC0;
       cpu_events &= EVENT_DEBUG_STEP;

       sched_reset();
       sched.items[SCHED_THROTTLE].clock = CLOCK_27M;
       sched.items[SCHED_THROTTLE].proc = throttle_interval_event;

       memory_reset();
   }
   */

   //set up extra CPU hardware
   pxa260icInit(&pxa260Ic);
   pxa260pwrClkInit(&pxa260PwrClk);
   pxa260lcdInit(&pxa260Lcd, &pxa260Ic);
   pxa260timrInit(&pxa260Timer, &pxa260Ic);
   pxa260gpioInit(&pxa260Gpio, &pxa260Ic);
   pxa260I2cReset();
   pxa260TimingReset();

   memset(&arm, 0, sizeof arm);
   arm.control = 0x00050078;
   arm.cpsr_low28 = MODE_SVC | 0xC0;
   cycle_count_delta = 0;
   cpu_events = 0;
   //cpu_events &= EVENT_DEBUG_STEP;

   //PC starts at 0x00000000, the first opcode for Palm OS 5 is a jump
}

void pxa260SetRtc(uint16_t days, uint8_t hours, uint8_t minutes, uint8_t seconds){
   //TODO: make this do something
}

uint32_t pxa260StateSize(void){
   uint32_t size = 0;

   return size;
}

void pxa260SaveState(uint8_t* data){
   uint32_t offset = 0;

}

void pxa260LoadState(uint8_t* data){
   uint32_t offset = 0;

}

void pxa260Execute(bool wantVideo){
   uint32_t index;

   pxa260TimingRun(TUNGSTEN_T3_CPU_CRYSTAL_FREQUENCY / EMU_FPS);

   //this needs to run at 3.6864 MHz
   for(index = 0; index < TUNGSTEN_T3_CPU_CRYSTAL_FREQUENCY / EMU_FPS; index++)
      pxa260timrTick(&pxa260Timer);

   //render
   if(likely(wantVideo))
      pxa260lcdFrame(&pxa260Lcd);
}

uint32_t pxa260GetRegister(uint8_t reg){
   return reg_pc(reg);
}

uint64_t pxa260ReadArbitraryMemory(uint32_t address, uint8_t size){
   uint64_t data = UINT64_MAX;//invalid access

   switch(size){
      case 8:
         if(read_byte_map[address >> 26] != bad_read_byte){
            data = read_byte_map[address >> 26](address);
         }
         break;

      case 16:
         if(read_half_map[address >> 26] != bad_read_half){
            data = read_half_map[address >> 26](address);
         }
         break;

      case 32:
         if(read_word_map[address >> 26] != bad_read_word){
            data = read_word_map[address >> 26](address);
         }
         break;
   }

   return data;
}
