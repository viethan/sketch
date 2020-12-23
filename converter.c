#include "displayfull.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

#define PGM 1
#define SK 0

enum { DX = 0, DY = 1, TOOL = 2,
       DATA = 3,
       NONE = 0, LINE = 1,
     BLOCK = 2, COLOUR = 3, TARGETX = 4, TARGETY = 5,
     };

enum { NONE_ins = 0x80, LINE_ins = 0x81, BLOCK_ins = 0x82, 
  COLOUR_ins = 0x83, TARGETX_ins = 0x84, TARGETY_ins = 0x85,
  DATA_ins = 0xC0, DX_ins = 0x00, DY_ins = 0x40,
  };


typedef struct state {
  int x, y, tx, ty;
  unsigned int colour, data, tool;
} state;

typedef struct image {
  unsigned long size; // size of the byte sequence
  unsigned char *bytes; // dem bytes
} image;

typedef unsigned char byte;


// I/O functions
void writeFile(image *thisImage, char *filename, int filetype);
unsigned char *readFile(FILE *fp, unsigned long *length);

// main functions
void solve(char *filename);
void convert2sk(FILE *fp, char *filename);
void convert2pgm(FILE *fp, char *filename);
void freeEverything(unsigned char *input, image *thisImage);

// pgm -> sk functions
bool verifyPGM(unsigned char *input, unsigned long length);
image *newSKImage();
void processPGM(image *thisImage, unsigned char *input);
void execute(image *thisImage, state *curr_state, bool xVSy);
void detectColours(unsigned char *input, bool colours[65536]);
void setColour(image *thisImage, unsigned int rgba);
unsigned int gray2rgba(unsigned int gray);
bool absOrRel(int curr, int prev);
void relativeJump(image *thisImage, state *curr_state, bool xVSy);
void absoluteJump(image *thisImage, state *curr_state, bool xVSy);
int maxGray(unsigned char *input, unsigned long length);
unsigned long startGray(unsigned char *input, unsigned long length);
void turnToolOff(image *thisImage);
void turnToolOn(image *thisImage);
bool inColumn(int column, int gray, unsigned char *input);

// sk -> pgm functions
bool verifySK(unsigned char *input, unsigned long length);
image *newPGMImage();
void processSK(image *thisImage, unsigned char *input, unsigned long length);
void obeyTOOL(unsigned char map[200][200], state *s, byte op);
void obeyDX(unsigned char map[200][200], state *s, byte op);
void obeyDY(unsigned char map[200][200], state *s, byte op);
void obeyDATA(unsigned char map[200][200], state *s, byte op);
void obeyDraw(unsigned char map[200][200], state *s);
void lineFun(unsigned char map[200][200], state *s);
void diagonalLine(unsigned char map[200][200], state *s);
void blockFun(unsigned char map[200][200], state *s);
int getOpcode(byte b);
int getOperand(byte b);
int rgba2gray(unsigned int data);
void pasteBytes(image *thisImage, unsigned char map[200][200]);

// test functions
void assert(int line, bool b);
void test();

void testSetColour();
void testGray2Rgba();
void testAbsOrRel();
void testObeyData();
void testGetOpcode();
void testGetOperand();
void testRgba2Gray();



int main(int argc, char **argv) {
  if (argc == 1) test();
  else if (argc == 2) solve(argv[1]);
  else {
    fprintf(stderr, "Use \'./converter file\' for converting.\nUse \'./converter\' for testing.\n");
    exit(1);
  }

  return 0;
}

// ---------------------------------------------------------
// write the bytes into a new file
// if we transfer it into a pgm file we also include the header
void writeFile(image *thisImage, char *filename, int filetype) {
  FILE *ofp;
  char *new_filename;

  if (filetype == PGM) new_filename = strcat(filename, ".pgm"); 
  else new_filename = strcat(filename, ".sk"); 
  ofp = fopen(new_filename, "wb");

  if (ofp == NULL) { fprintf(stderr, "Error: Cannot write image.\n"); exit(1); }
  if (filetype == PGM) fprintf(ofp,"P5 200 200 255\n"); //Write header
  fwrite(thisImage->bytes, 1, thisImage->size, ofp); 
  fclose(ofp);

  printf("File %s has been written.\n", new_filename);
}

// transfer the file into an array and return it
unsigned char *readFile(FILE *fp, unsigned long *length) {
  unsigned char *s;

  fseek(fp, 0, SEEK_END);
  *length = ftell(fp);
  s = (unsigned char *)malloc(*length * sizeof(unsigned char));
  fseek(fp, 0, SEEK_SET);
  fread(s, *length, 1, fp);

  fclose(fp);
  return s;
}

// ---------------------------------------------------------

// detect whether it's a pgm -> sk or sk -> pgm
// and make the appropriate function call
void solve(char *filename) {
  FILE *fp;

  fp = fopen(filename, "rb");
  if (fp == NULL) { fprintf(stderr, "Error: Cannot read image.\n"); exit(1); }

  if (!(strcmp(filename + (strlen(filename) - 3), ".sk"))) {
    filename[strcspn(filename, ".")] = '\0';
    convert2pgm(fp, filename);
  }
  else if (!(strcmp(filename + (strlen(filename) - 4), ".pgm"))) {
    filename[strcspn(filename, ".")] = '\0';
    convert2sk(fp, filename);
  }
  else { fprintf(stderr, "Error: incorrect filetype.\n"); exit(1); }
}

// verify the PGM file
// convert it to an sk file and write it into a new file
void convert2sk(FILE *fp, char *filename) {
  unsigned long length;
  unsigned char *input = readFile(fp, &length);

  if (!(verifyPGM(input, length))) { fprintf(stderr, "Error: Corrupted PGM file.\n"); exit(1); } 
  image *thisImage = newSKImage();
  processPGM(thisImage, input + startGray(input, length));
  writeFile(thisImage, filename, SK);
  freeEverything(input, thisImage);
}

// verify the sk file
// convert it to a pgm file and write it into a new file
void convert2pgm(FILE *fp, char *filename) {
  unsigned long length;
  unsigned char *input = readFile(fp, &length);

  if (!(verifySK(input, length))) { fprintf(stderr, "Error: Corrupted SK file.\n"); exit(1); } 
  image *thisImage = newPGMImage();
  processSK(thisImage, input, length);
  writeFile(thisImage, filename, PGM);
  freeEverything(input, thisImage);
}

void freeEverything(unsigned char *input, image *thisImage) {
  free(input);
  free(thisImage->bytes);
  free(thisImage);
}

// ---------------------------------------------------------

// verify that this is a valid PGM file
bool verifyPGM(unsigned char *input, unsigned long length) {
  unsigned long i;
  int maxVal = 0, valSize, val;

  // check that file is large enough
  if (length == 4015) return false;
  // check that magic number is P5
  if (input[0] != 'P' || input[1] != '5') return false;
  // check that width and height are both 200
  if (input[3] != '2' || input[4] != '0' || input[5] != '0' || \
    input[7] != '2' || input[8] != '0' || input[9] != '0') return false;

  maxVal = maxGray(input, length);
  i = startGray(input, length) - 1;
  // check maximum gray value
  if (!(0 < maxVal && maxVal < 256)) return false;
  valSize = 1;
  // check for correct whitespace in the "header"
  if (!(isspace(input[2]) && isspace(input[6]) && isspace(input[10]) && isspace(input[i++]))) return false;
  // check grayscale values are <= maxVal
  for (; i < length; i += valSize) {
    val = 0;
    for (int j = 0; j < valSize; j++) val = val * 10 + (input[i + j] - '0');
    if (val > maxVal) return false;
  }
  return true;
}

// initialise a new sk image struct
image *newSKImage() {
  image *thisImage;

  thisImage = (image *)malloc(sizeof(struct image));
  thisImage->size = 0;
  thisImage->bytes = (unsigned char *)malloc(1000000 * sizeof(unsigned char));

  return thisImage;
}

// the actual pgm -> sk conversion
void processPGM(image *thisImage, unsigned char *input) {
  bool colours[65536], found_sequence;
  state *s = (state *)malloc(sizeof(state));
  *s = (state) {0, 0, 0, 0, 0, 0, LINE};

  // detect all the colours in the image
  memset(colours, 0, 65536 * sizeof(bool));
  detectColours(input, colours);

  turnToolOff(thisImage);
  for (unsigned int gray = 0; gray < 65536; gray++) {
    if (colours[gray]) {

      // for all the colours, change to it just once
      s->colour = gray;
      setColour(thisImage, gray2rgba(gray));

      for (int columns = 0; columns < 200;  columns++) {
        
        // starting from the 0th column, detect which columns have pixels in that colour
        if (inColumn(columns, gray, input)) {
          s->tx = columns;
          execute(thisImage, s, 1);
          s->x = columns;

          // move along the column, jumping over the pixels not in that colour
          found_sequence = false;
          for (int rows = 0; rows < 200; rows++) {
            if(input[rows * 200 + columns] == s->colour && found_sequence == false) {
              s->ty = rows;
              execute(thisImage, s, 0);
              s->y = rows;
              found_sequence = true;
            }

            // move and draw at the same time, taking full advantage of DY's RLE mechanism
            else if (input[rows * 200 + columns] != s->colour && found_sequence == true) {
              s->ty = rows;
              turnToolOn(thisImage);
              execute(thisImage, s, 0);
              turnToolOff(thisImage);
              s->y = rows;
              found_sequence = false;
            }
          }

          // check whether there's a valid sequence at the end of the column
          if (found_sequence) {
            s->ty = 200;
            turnToolOn(thisImage);
            execute(thisImage, s, 0);
            s->y = 200;
            turnToolOff(thisImage);
          }
        } 
      }
    colours[gray] = false;
    }
  }
  free(s);
}

// make a jump (DX, DY, TARGETX or TARGETY)
// choose the one with least bytes used
// if s->tool = LINE, also draw
void execute(image *thisImage, state *curr_state, bool xVSy) {
  unsigned int target, current;

  if (xVSy) {
    target = curr_state->tx;
    current = curr_state->x;
  }
  else {
    target = curr_state->ty;
    current = curr_state->y;
  }
  
  if (absOrRel(target, current))
    relativeJump(thisImage, curr_state, xVSy);
  else
    absoluteJump(thisImage, curr_state, xVSy);
}

// Detects all the gray shades used in the image
void detectColours(unsigned char *input, bool colours[65536]) {
  for (int i = 0; i < 200; i++)
    for (int j = 0; j < 200; j++)
      colours[input[i * 200 + j]] = true;
}

// 6 DATA instructions to set the correct rgba value
// 1 COLOUR instruction
void setColour(image *thisImage, unsigned int rgba) {
  thisImage->bytes[thisImage->size++] = DATA_ins | ((rgba >> 30) & 0x3F);
  thisImage->bytes[thisImage->size++] = DATA_ins | ((rgba >> 24) & 0x3F);
  thisImage->bytes[thisImage->size++] = DATA_ins | ((rgba >> 18) & 0x3F);
  thisImage->bytes[thisImage->size++] = DATA_ins | ((rgba >> 12) & 0x3F);
  thisImage->bytes[thisImage->size++] = DATA_ins | ((rgba >> 6) & 0x3F);
  thisImage->bytes[thisImage->size++] = DATA_ins | (rgba & 0x3F);

  thisImage->bytes[thisImage->size++] = COLOUR_ins;
}

// convert a gray value into an rgba value
unsigned int gray2rgba(unsigned int gray) {
    return 255 + ((gray << 8) & (255 << 8)) +((gray << 16) & (255 << 16)) +((gray << 24) & (UINT32_C(255) << 24));
}  

// calculate which type of jump is least costly
// returns false for absolute jump
// returns true for relative jump
bool absOrRel(int curr, int prev) {
  int nr_commands_absolute = 0, nr_commands_relative;

  if (curr < 64) // bytes for DATA
    nr_commands_absolute += 1; 
  else
    nr_commands_absolute += 2;
  
  nr_commands_absolute += 2; // bytes for TARGETY and DY
  nr_commands_relative = ceil((curr - prev) / 31.0);

  return (nr_commands_relative <= nr_commands_absolute);
}

// determine if we're going forward (positive) or backwards (negative)
// make the jump with the necessary amount of DX or DY commands
void relativeJump(image *thisImage, state *curr_state, bool xVSy) {
  int substractor, val, distance, opcode;
  bool loop = true;

  if (xVSy) {
    distance = curr_state->tx - curr_state->x;
    opcode = DX_ins;
  }
  else {
    distance = curr_state->ty - curr_state->y;
    opcode = DY_ins;
  }

  if (distance == 0) return;
  if (distance < 0) substractor = 32; // negative
  else substractor = 31; // positive

  while (loop) {
    if ((substractor == 31 && distance >= 31) || (substractor == 32 && distance <= -32)) {
      val = substractor == 31? 31 : -32;

      thisImage->bytes[thisImage->size++] = opcode | (val & 0x3F);
      distance -= val;
    } 
    else {
      if (distance != 0) { // make sure that we don't add a byte for nothing
        thisImage->bytes[thisImage->size++] = opcode | (distance & 0x3F);    
      }
      loop = false;
    } 
  }
  if (xVSy) thisImage->bytes[thisImage->size++] = DY_ins ; // Execute 
}

// set DATA to either TX or TY
// calls TARGETX/TARGETY
void absoluteJump(image *thisImage, state *curr_state, bool xVSy) {
  unsigned int target, opcode;

  if (xVSy) {
    target = curr_state->tx;
    opcode = TARGETX_ins;
  }
  else {
    target = curr_state->ty;
    opcode = TARGETY_ins;
  }

  if (target >= 64)
    thisImage->bytes[thisImage->size++] = DATA_ins | ((target >> 6) & 0x3F);

  thisImage->bytes[thisImage->size++] = DATA_ins | (target & 0x3F);
  thisImage->bytes[thisImage->size++] = opcode;
  thisImage->bytes[thisImage->size++] = DY_ins; // execute
}

// determinate the max gray value as specified in the pgm header
int maxGray(unsigned char *input, unsigned long length) {
  unsigned long i;
  int maxVal = 0;

  i = 11;
  for (; isspace(input[i]) == 0 && i < length; i++)
    maxVal = maxVal * 10 + (input[i] - '0');
  return maxVal;
}

// determine the index where the gray bytes start to appear
unsigned long startGray(unsigned char *input, unsigned long length) {
  unsigned long i;
  for (i = 11; i < length && isspace(input[i]) == 0; i++)
    ;

  if (i != length) return (i + 1);
  else { fprintf(stderr, "Error: Can't find the gray values"); exit(1); }
  return -1;
}

// TOOL = NONE
void turnToolOff(image *thisImage) {
  thisImage->bytes[thisImage->size++] = NONE_ins;
}

// TOOL = LINE
void turnToolOn(image *thisImage) {
  thisImage->bytes[thisImage->size++] = LINE_ins;
}

// check if the gray value is present in the column
bool inColumn(int column, int gray, unsigned char *input) {
  for (int row = 0; row < 200; row++)
    if (input[row * 200 + column] == gray)
      return true;
  return false;
}

// ---------------------------------------------------------

// verify that this is a valid sk file
bool verifySK(unsigned char *input, unsigned long length) {
  int opcode;

  for (unsigned long i = 0; i < length; i++) {
    opcode = getOpcode(input[i]);
    if (opcode != TOOL && opcode != DX && opcode != DY && opcode != DATA)
      return false;  
  }

  return true;
}

// initialise a new pgm image struct
image *newPGMImage() {
  image *thisImage;

  thisImage = (image *)malloc(sizeof(struct image));
  thisImage->size = 0;
  thisImage->bytes = (unsigned char *)malloc(40015 * sizeof(unsigned char));

  return thisImage;
}

// the actual sk -> pgm conversion
// fill a 2d array that's 200x200 with gray values
void processSK(image *thisImage, unsigned char *input, unsigned long length) {
  int op, opcode;
  unsigned char map[200][200];
  state *s = (state *)malloc(sizeof(state));
  *s = (state) {0, 0, 0, 0, 0xFF, 0, LINE};
  memset(map, 0, 200 * 200 * sizeof(unsigned char));
  
  // take each sk instruction and call the appropriate function
  for (int i = 0; i < length; i++) {
    op = input[i];
    opcode = getOpcode(op);

    switch (opcode) {
      case TOOL:
        obeyTOOL(map, s, op);
        break;
      case DX:
        obeyDX(map, s, op);
        break;
      case DY:
        obeyDY(map, s, op);
        break;
      case DATA:
        obeyDATA(map, s, op);
        break;
    }
  }

  pasteBytes(thisImage, map);
  free(s);
}


/* from now on it's what you'd expect to also see in sketch.c */


void obeyTOOL(unsigned char map[200][200], state *s, byte op) {
  int operand = getOperand(op);

  switch(operand) {
    case NONE:
    case LINE:
    case BLOCK:
      s->tool = operand;
      break;
    case COLOUR:
      s->colour = rgba2gray(s->data);
      break;
    case TARGETX:
      s->tx = s->data;
      break;
    case TARGETY:
      s->ty = s->data;
      break;
  }
  s->data = 0;
}

void obeyDX(unsigned char map[200][200], state *s, byte op) {
  int operand = getOperand(op);
  
  s->tx += operand;
}

void obeyDY(unsigned char map[200][200], state *s, byte op) {
  int operand = getOperand(op);
  
  s->ty += operand;
  if (s->tool == LINE || s->tool == BLOCK) obeyDraw(map, s);
  s->x = s->tx;
  s->y = s->ty;
}

void obeyDATA(unsigned char map[200][200], state *s, byte op) {
  int operand = getOperand(op);
  
  s->data = (s->data << 6) | (operand & 0x3F);
}

void obeyDraw(unsigned char map[200][200], state *s) {
  switch(s->tool) {
    case LINE:
      lineFun(map, s); 
      break;
    case BLOCK:
      blockFun(map, s);
      break;
  }
}

void lineFun(unsigned char map[200][200], state *s) {
  if (s->x == s->tx && s->y != s->ty) { // vertical line
    int y = s->y < s->ty ? s->y : s->ty;
    int ty = s->y > s->ty? s->y : s->ty;

    for (int i = y; i < ty; i++)
      map[i][s->x] = s->colour;
  }
  else if (s->y == s->ty && s->x != s->tx) { // horizontal line
    int x = s->x < s->tx ? s->x : s->tx;
    int tx = s->x > s->tx? s->x : s->tx;

    for (int i = x; i < tx; i++)
      map[s->y][i] = s->colour;
  }
  else diagonalLine(map, s);
}

// draw diagonal lines
// we first find the length of the line using the Pythagorean theorem
// then we calculate the incrementors that we're going to use
void diagonalLine(unsigned char map[200][200], state *s) {
  double x, y, addx, addy;
  double length;

  length = sqrt((s->tx - s->x)*(s->tx - s->x) + (s->ty - s->y)*(s->ty - s->y));
  addx = (s->tx - s->x) / length;
  addy = (s->ty - s->y) / length;
  x = s->x;
  y = s->y;

  for(double i = 0; i < length; i++) {
      map[(int)y][(int)x] = s->colour;
      x += addx;
      y += addy;
  }
}

void blockFun(unsigned char map[200][200], state *s) {
  for (int rows = s->y; rows < s->ty; rows++)
    for (int columns = s->x; columns < s->tx; columns++)
      map[rows][columns] = s->colour;
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

// convert a rgba value into a gray value
int rgba2gray(unsigned int data) {
  int R, G, B;

  B = (data >> 8) & 0xFF;
  G = (data >> 16) & 0xFF;
  R = (data >> 24) & 0xFF;
  return round(0.299 * R +  0.587 * G + 0.114 * B);
}

void pasteBytes(image *thisImage, unsigned char map[200][200]) {
  for (int rows = 0; rows < 200; rows++)
    for (int columns = 0; columns < 200; columns++)
      thisImage->bytes[thisImage->size++] = map[rows][columns];
}

// ---------------------------------------------------------
// A replacement for the library assert function.
void assert(int line, bool b) {
  if (b) return;
  printf("The test on line %d fails.\n", line);
  exit(1);
}

void test() {
  testSetColour();
  testGray2Rgba();
  testAbsOrRel();
  testObeyData();
  testGetOpcode();
  testGetOperand();
  testRgba2Gray();

  printf("All tests passed\n");
}


void testSetColour() {
  image *thisImage = newSKImage();

  setColour(thisImage, 0x121212FF);
  assert(__LINE__, strncmp((const char *)thisImage->bytes, "\xC0\xD2\xC4\xE1\xCB\xFF\x83", 7) == 0);
  thisImage->size = 0;
  setColour(thisImage, 0x272727FF);
  assert(__LINE__, strncmp((const char *)thisImage->bytes, "\xC0\xE7\xC9\xF2\xDF\xFF\x83", 7) == 0);
  thisImage->size = 0;
  setColour(thisImage, 0xAAAAAAFF);
  assert(__LINE__, strncmp((const char *)thisImage->bytes, "\xC2\xEA\xEA\xEA\xEB\xFF\x83", 7) == 0);
  thisImage->size = 0;
  setColour(thisImage, 0xFFFFFFFF);
  assert(__LINE__, strncmp((const char *)thisImage->bytes, "\xC3\xFF\xFF\xFF\xFF\xFF\x83", 7) == 0);
  thisImage->size = 0;
  setColour(thisImage, 0x0D0D0DFF);
  assert(__LINE__, strncmp((const char *)thisImage->bytes, "\xC0\xCD\xC3\xD0\xF7\xFF\x83", 7) == 0);

  free(thisImage->bytes);
  free(thisImage);
}

void testGray2Rgba() {
  assert(__LINE__, gray2rgba(0xFF) == 0xFFFFFFFF);
  assert(__LINE__, gray2rgba(0xAA) == 0xAAAAAAFF);
  assert(__LINE__, gray2rgba(0x11) == 0x111111FF);
  assert(__LINE__, gray2rgba(0xC0) == 0xC0C0C0FF);
  assert(__LINE__, gray2rgba(0xBB) == 0xBBBBBBFF);
  assert(__LINE__, gray2rgba(0xEE) == 0xEEEEEEFF);
  assert(__LINE__, gray2rgba(0x13) == 0x131313FF);
  assert(__LINE__, gray2rgba(0x55) == 0x555555FF);
  assert(__LINE__, gray2rgba(0x47) == 0x474747FF);
  assert(__LINE__, gray2rgba(0x32) == 0x323232FF);
  assert(__LINE__, gray2rgba(0x81) == 0x818181FF);
  assert(__LINE__, gray2rgba(0x99) == 0x999999FF);
  assert(__LINE__, gray2rgba(0xDD) == 0xDDDDDDFF);
  assert(__LINE__, gray2rgba(0x00) == 0x000000FF);
  assert(__LINE__, gray2rgba(0x01) == 0x010101FF);
  assert(__LINE__, gray2rgba(0xCC) == 0xCCCCCCFF);
  assert(__LINE__, gray2rgba(0x02) == 0x020202FF);
  assert(__LINE__, gray2rgba(0x77) == 0x777777FF);
  assert(__LINE__, gray2rgba(0x07) == 0x070707FF);
}

void testAbsOrRel() {
  assert(__LINE__, absOrRel(27, 20) == true);
  assert(__LINE__, absOrRel(37, 20) == true);
  assert(__LINE__, absOrRel(47, 20) == true);
  assert(__LINE__, absOrRel(57, 20) == true);
  assert(__LINE__, absOrRel(67, 20) == true);
  assert(__LINE__, absOrRel(77, 20) == true);
  assert(__LINE__, absOrRel(87, 20) == true);
  assert(__LINE__, absOrRel(97, 20) == true);
  assert(__LINE__, absOrRel(107, 20) == true);
  assert(__LINE__, absOrRel(117, 20) == true);
  assert(__LINE__, absOrRel(127, 20) == true);
  assert(__LINE__, absOrRel(137, 20) == true);
  assert(__LINE__, absOrRel(147, 20) == false);
  assert(__LINE__, absOrRel(157, 20) == false);
  assert(__LINE__, absOrRel(167, 20) == false);
  assert(__LINE__, absOrRel(177, 20) == false);
  assert(__LINE__, absOrRel(187, 20) == false);
  assert(__LINE__, absOrRel(197, 20) == false);
  assert(__LINE__, absOrRel(199, 20) == false);
}

void testObeyData() {
  unsigned char map[200][200];
  state *s = (state *)malloc(sizeof(state));
  *s = (state) {0, 0, 0, 0, 0, 0, LINE};

  obeyDATA(map, s, 0x32);
  assert(__LINE__, s->data == 0x00000032);
  obeyDATA(map, s, 0x64);
  assert(__LINE__, s->data == 0xCA4);
  obeyDATA(map, s, 0xFF);
  assert(__LINE__, s->data == 0x3293F);
  obeyDATA(map, s, 0x00);
  assert(__LINE__, s->data == 0xCA4FC0);
  obeyDATA(map, s, 0x77);
  assert(__LINE__, s->data == 0x3293F037);
  obeyDATA(map, s, 0xAA);
  assert(__LINE__, s->data == 0xA4FC0DEA);
  obeyDATA(map, s, 0xBB);
  assert(__LINE__, s->data == 0x3F037ABB);
  obeyDATA(map, s, 0xCC);
  assert(__LINE__, s->data == 0xC0DEAECC);
  obeyDATA(map, s, 0xDD);
  assert(__LINE__, s->data == 0x37ABB31D);
  obeyDATA(map, s, 0x01);
  assert(__LINE__, s->data == 0xEAECC741);

  free(s);
}

void testGetOpcode() {
  assert(__LINE__, getOpcode(0xC0) == DATA);
  assert(__LINE__, getOpcode(0xC5) == DATA);
  assert(__LINE__, getOpcode(0xC7) == DATA);
  assert(__LINE__, getOpcode(0xD0) == DATA);
  assert(__LINE__, getOpcode(0xD5) == DATA);
  assert(__LINE__, getOpcode(0xD7) == DATA);
  assert(__LINE__, getOpcode(0xE0) == DATA);
  assert(__LINE__, getOpcode(0x80) == TOOL);
  assert(__LINE__, getOpcode(0x81) == TOOL);
  assert(__LINE__, getOpcode(0x82) == TOOL);
  assert(__LINE__, getOpcode(0x83) == TOOL);
  assert(__LINE__, getOpcode(0x40) == DY);
  assert(__LINE__, getOpcode(0x45) == DY);
  assert(__LINE__, getOpcode(0x50) == DY);
  assert(__LINE__, getOpcode(0x60) == DY);
  assert(__LINE__, getOpcode(0x70) == DY);
  assert(__LINE__, getOpcode(0x10) == DX);
  assert(__LINE__, getOpcode(0x20) == DX);
  assert(__LINE__, getOpcode(0x30) == DX);
}

void testGetOperand() {
  assert(__LINE__, getOperand(0x32) == -14);
  assert(__LINE__, getOperand(0x64) == -28);
  assert(__LINE__, getOperand(0x20) == -32);
  assert(__LINE__, getOperand(0x25) == -27);
  assert(__LINE__, getOperand(0x45) == 5);
  assert(__LINE__, getOperand(0x75) == -11);
  assert(__LINE__, getOperand(0x85) == 5);
  assert(__LINE__, getOperand(0x95) == 21);
  assert(__LINE__, getOperand(0xA5) == -27);
  assert(__LINE__, getOperand(0xB5) == -11);
  assert(__LINE__, getOperand(0xC5) == 5);
  assert(__LINE__, getOperand(0xD5) == 21);
  assert(__LINE__, getOperand(0xE5) == -27);
  assert(__LINE__, getOperand(0xF5) == -11);
  assert(__LINE__, getOperand(0xFF) == -1);
  assert(__LINE__, getOperand(0x00) == 0x00);
  assert(__LINE__, getOperand(0x01) == 0x01);
  assert(__LINE__, getOperand(0x02) == 0x02);
  assert(__LINE__, getOperand(0x03) == 0x03);
}

void testRgba2Gray() {
  assert(__LINE__, rgba2gray(0xFFFFFFFF) == 0xFF);
  assert(__LINE__, rgba2gray(0xAAAAAAFF) == 0xAA);
  assert(__LINE__, rgba2gray(0x111111FF) == 0x11);
  assert(__LINE__, rgba2gray(0xC0C0C0FF) == 0xC0);
  assert(__LINE__, rgba2gray(0xBBBBBBFF) == 0xBB);
  assert(__LINE__, rgba2gray(0xEEEEEEFF) == 0xEE);
  assert(__LINE__, rgba2gray(0x131313FF) == 0x13);
  assert(__LINE__, rgba2gray(0x555555FF) == 0x55);
  assert(__LINE__, rgba2gray(0x474747FF) == 0x47);
  assert(__LINE__, rgba2gray(0x323232FF) == 0x32);
  assert(__LINE__, rgba2gray(0x818181FF) == 0x81);
  assert(__LINE__, rgba2gray(0x999999FF) == 0x99);
  assert(__LINE__, rgba2gray(0xDDDDDDFF) == 0xDD);
  assert(__LINE__, rgba2gray(0x000000FF) == 0x00);
  assert(__LINE__, rgba2gray(0x010101FF) == 0x01);
  assert(__LINE__, rgba2gray(0xCCCCCCFF) == 0xCC);
  assert(__LINE__, rgba2gray(0x020202FF) == 0x02);
  assert(__LINE__, rgba2gray(0x777777FF) == 0x77);
  assert(__LINE__, rgba2gray(0x070707FF) == 0x07);
}
