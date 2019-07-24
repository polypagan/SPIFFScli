# SPIFFScli
All the trivial UNIX file functions applied to SPIFFS

from the code:

// instruct user
void usage() {

  Serial << "L)ist" << endl
         << "R)emove <filename>" << endl
         << "M)ove <file1> <file2>" << endl
         << "D)ump <filename>" << endl
         << "C)p <file1> <file2>" << endl
         << "T)ouch <filename>" << endl;
}

Command parser ignores any characters after first alphabetic, until space or tab seen.
Next "word" is filename. '/' is automagically prefixed, or ignored if also typed 
(since root is the only available directory).

D)ump is much like hexdump -C

issues, questions, suggestions, improvements, forks, all welcome.
