// Basic program skeleton for a Sketch File (.sk) Viewer
#include "displayfull.h"
#include "sketch.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------

long int binaryLength(display *d) {
  FILE *fp;
  long length;

  fp = fopen(getName(d), "rb");
  fseek(fp, 0, SEEK_END);
  length = ftell(fp);
  fclose(fp);

  return length;
}

unsigned char *binaryString(display *d) {
  FILE *fp;
  long length;
  unsigned char *s;

  fp = fopen(getName(d), "rb");
  length = binaryLength(d);
  s = (unsigned char *)malloc(length);
  fseek(fp, 0, SEEK_SET);
  fread(s, length, 1, fp);
  fclose(fp);

  return s;
}

void reset(state *s, unsigned char *v) {
  s->x = 0;
  s->y = 0;
  s->tx = 0;
  s->ty = 0;
  s->tool = LINE;
  s->data = 0;
  s->end = false;

  free(v);
}

void obeyNextFrame(display *d, state *s) {
  unsigned char *v;
  long int length = binaryLength(d);
  bool found = false;

  v = binaryString(d);
  for (int i = 0; i < length && !found; ++i)
    if (v[i] == 0x88 && s->start < i) {
      s->start = i;
      found = true;
      s->end = true;
    }

  free(v);
}

void obeyTOOL(display *d, state *s, byte op) {
  int operand = getOperand(op);

  switch(operand) {
    case NONE:
    case LINE:
    case BLOCK:
      s->tool = operand;
      break;
    case COLOUR:
      colour(d, s->data);
      break;
    case TARGETX:
      s->tx = s->data;
      break;
    case TARGETY:
      s->ty = s->data;
      break;
    case SHOW:
      show(d);
      break;
    case PAUSE:
      pause(d, s->data);
      break;
    case NEXTFRAME:
      obeyNextFrame(d, s);
      break;
  }
  s->data = 0;
}

void obeyDX(display *d, state *s, byte op) {
  int operand = getOperand(op);
  
  s->tx += operand;
}

void obeyDraw(display *d, state *s) {
  switch(s->tool) {
    case LINE:
      line(d, s->x, s->y, s->tx, s->ty);
      break;
    case BLOCK:
      block(d, s->x, s->y, s->tx - s->x, s->ty - s->y);
      break;
  }
}

void obeyDY(display *d, state *s, byte op) {
  int operand = getOperand(op);
  
  s->ty += operand;
  if (s->tool == LINE || s->tool == BLOCK) obeyDraw(d, s);
  s->x = s->tx;
  s->y = s->ty;
}

void obeyDATA(display *d, state *s, byte op) {
  int operand = getOperand(op);
  
  s->data = (s->data << 6) | (operand & 0x3F);
}

// ---------------------------------------------------------------------------



// Allocate memory for a drawing state and initialise it
state *newState() {
  state *new;

  new = (state *)malloc(sizeof(state));
  *new = (state) {0, 0, 0, 0, LINE, 0, 0, false};

  return new;
}

// Release all memory associated with the drawing state
void freeState(state *s) {
  free(s);
}

// Extract an opcode from a byte (two most significant bits).
int getOpcode(byte b) {
  return b >> 6;
}

// Extract an operand (-32..31) from the rightmost 6 bits of a byte.
int getOperand(byte b) {
  int val = b & 0x3F;

  if (val >> 5) val = val | 0xFFFFFFC0; // extend sign
  return val;
}

// Execute the next byte of the command sequence.
void obey(display *d, state *s, byte op) {
  int opcode;

  opcode = getOpcode(op);
  switch (opcode) {
    case TOOL:
      obeyTOOL(d, s, op);
      break;
    case DX:
      obeyDX(d, s, op);
      break;
    case DY:
      obeyDY(d, s, op);
      break;
    case DATA:
      obeyDATA(d, s, op);
      break;
  }
}

// Draw a frame of the sketch file. For basic and intermediate sketch files
// this means drawing the full sketch whenever this function is called.
// For advanced sketch files this means drawing the current frame whenever
// this function is called.
bool processSketch(display *d, void *data, const char pressedKey) {

    //TO DO: OPEN, PROCESS/DRAW A SKETCH FILE BYTE BY BYTE, THEN CLOSE IT
    //NOTE: CHECK DATA HAS BEEN INITIALISED... if (data == NULL) return (pressedKey == 27);
    //NOTE: TO GET ACCESS TO THE DRAWING STATE USE... state *s = (state*) data;
    //NOTE: TO GET THE FILENAME... char *filename = getName(d);
    //NOTE: DO NOT FORGET TO CALL show(d); AND TO RESET THE DRAWING STATE APART FROM
    //      THE 'START' FIELD AFTER CLOSING THE FILE

  state *s = (state *) data;
  if (data == NULL) return (pressedKey == 27);

  unsigned char *v;
  long int length = binaryLength(d);
  v = binaryString(d);

  int i = s->start != 0 ? s->start + 1 : 0; 
  for (; i < length && !(s->end); ++i)
    obey(d, s, v[i]);

  if (s->end == false) s->start = 0;
  show(d);
  reset(s, v);
  return (pressedKey == 27);
}

// View a sketch file in a 200x200 pixel window given the filename
void view(char *filename) {
  display *d = newDisplay(filename, 200, 200);
  state *s = newState();
  run(d, s, processSketch);
  freeState(s);
  freeDisplay(d);
}

// Include a main function only if we are not testing (make sketch),
// otherwise use the main function of the test.c file (make test).
#ifndef TESTING
int main(int n, char *args[n]) {
  if (n != 2) { // return usage hint if not exactly one argument
    printf("Use ./sketch file\n");
    exit(1);
  } else view(args[1]); // otherwise view sketch file in argument
  return 0;
}
#endif
