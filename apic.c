#include "apic.h"
#include "display.h"
#include "mem_map.h"
#include "interrupt.h"
#include "page_map.h"
#include "util.h"

#define APIC_BASE_MSR 0x1B

#define APIC_EOI_REG 0xB0
#define APIC_SPURIOUS_REG 0x0F0
#define APIC_TIMER_REG 0x320
#define APIC_TIMER_INITIAL_REG 0x380
#define APIC_TIMER_CURRENT_REG 0x390
#define APIC_TIMER_DIVIDE_REG 0x3E0

#define APIC_ENABLE (1 << 11)
#define APIC_LOCAL_ENABLE (1 << 8)
#define APIC_TIMER_MASKED (1 << 16)
#define APIC_TIMER_PERIODIC (1 << 17)

#define APIC_TIMER_DIVIDE_BY_1 0b1011
#define APIC_TIMER_DIVIDE_BY_2 0b0000
#define APIC_TIMER_DIVIDE_BY_4 0b0001
#define APIC_TIMER_DIVIDE_BY_8 0b0010
#define APIC_TIMER_DIVIDE_BY_16 0b0011
#define APIC_TIMER_DIVIDE_BY_32 0b1000
#define APIC_TIMER_DIVIDE_BY_64 0b1001
#define APIC_TIMER_DIVIDE_BY_128 0b1010

static inline uint32_t reg_read(int reg) {
  return *(volatile uint32_t*)((uint64_t)APIC_BASE_ADDR + reg);
}

static inline void reg_write(int reg, uint32_t value) {
  *(volatile uint32_t*)((uint64_t)APIC_BASE_ADDR + reg) = value;
}

ISR_DEFINE(spurious, 0) {
  printf("#SPURIOUS\n");
  reg_write(APIC_EOI_REG, 0);
}

ISR_DEFINE(timer, 0) {
  static int fired = 0;
  if (!(++fired % APIC_TIMER_FREQ)) {
    printf("#TIMER (%d Hz, fired: %d K)\n", APIC_TIMER_FREQ, fired / 1000);
  }
  reg_write(APIC_EOI_REG, 0);
}

static void init_timer(void) {
  set_isr(INT_VECTOR_TIMER, timer_isr_getter());
  reg_write(APIC_TIMER_REG, INT_VECTOR_TIMER);
  reg_write(APIC_TIMER_DIVIDE_REG, APIC_TIMER_DIVIDE_BY_1);

  outb(0x61, (inb(0x61) & 0x0C) | 1); // enable PIT
  outb(0x43, 0xB0);

  reg_write(APIC_TIMER_INITIAL_REG, -1); // wait for 10 ms
  outb(0x42, 0x9C);
  outb(0x42, 0x2E);
  while (!(inb(0x61) & 0x20));

  uint64_t ticked = -1 - reg_read(APIC_TIMER_CURRENT_REG);
  reg_write(APIC_TIMER_REG, APIC_TIMER_PERIODIC | INT_VECTOR_TIMER);
  reg_write(APIC_TIMER_INITIAL_REG,
            (uint32_t)(ticked * 100 / APIC_TIMER_FREQ));

  outb(0x61, inb(0x61) & 0x0C); // disable PIT
}

void init_apic(void) {
  wrmsr(APIC_BASE_MSR, APIC_BASE_ADDR | APIC_ENABLE);
  map_page(APIC_BASE_ADDR, APIC_BASE_ADDR,
           PAGE_MAPPING_WRITE | PAGE_MAPPING_PWT | PAGE_MAPPING_PCD, 0);
  set_isr(INT_VECTOR_SPURIOUS, spurious_isr_getter());
  reg_write(APIC_SPURIOUS_REG, APIC_LOCAL_ENABLE | INT_VECTOR_SPURIOUS);

  init_timer();
}
