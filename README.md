fractal.sk - 99592 bytes

Colouring is the most expensive instruction (6 DATA + 1 COLOUR = 7 bytes).

DY is the most effective instruction (moves + draws at the same time) and provides RLE.

171 colours; 16580 colour changes.

Most efficient colouring requires least efficient movement.

Most efficient movement requires least efficient colouring.

Best compression requires both.

Plan.

Set every colour ONCE. 

Find the columns which contain pixels in that colour.

Draw with DY.

Jump over pixels if needed.

The program assumes a pgm file of size 200x200 and max gray of 255.

sk -> pgm fills a 200x200 matrix with grays. Works for intermediate.
