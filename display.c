// HW3: VGA display driver (device file "display", major DISPLAY = 2).
// write() copies pixels into the Mode 13h frame buffer,
// ioctl() switches video modes and changes palette colors.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"

#define VGA_SIZE  (320*200)   // Mode 13h: 320x200, one byte per pixel
#define TEXT_SIZE (80*25*2)   // Mode 3: 80x25 cells, two bytes per cell

// 0xA0000 is user memory now; the kernel sees video memory at KERNBASE+0xA0000
static uchar *vgabuf = (uchar*)P2V(0xA0000);
static uchar *textbuf = (uchar*)P2V(0xB8000);
static uchar textsave[TEXT_SIZE];   // console text, kept while in graphics mode
static int offset;                  // where the next write() lands

int
displaywrite(struct file *f, char *buf, int n)
{
  if (offset + n > VGA_SIZE)
    n = VGA_SIZE - offset;
  memmove(vgabuf + offset, buf, n);
  offset += n;
  return n;
}

int
displayioctl(struct file *f, int param, int value)
{
  switch(param){
  case 1:  // change video mode
    if (value == 0x13) {
      memmove(textsave, textbuf, TEXT_SIZE);  // mode switch wipes video memory
      vgaMode13();
      offset = 0;
      return 0;
    }
    if (value == 0x3) {
      vgaMode3();
      memmove(textbuf, textsave, TEXT_SIZE);  // bring the console text back
      return 0;
    }
    return -1;
  case 2:  // set palette: value = index<<24 | r<<16 | g<<8 | b
    vgaSetPalette((value >> 24) & 0xff, (value >> 16) & 0x3f,
                  (value >> 8) & 0x3f, value & 0x3f);
    return 0;
  }
  return -1;
}

void
displayinit(void)
{
  devsw[DISPLAY].write = displaywrite;
  devsw[DISPLAY].ioctl = displayioctl;
  // no read: the display is write-only
}
