A simple program that decodes one wire bus data bytes sent by a one wire bus master or slave device. Input a .csv file containing the waveform data from the one wire bus interaction you want to decode. I created this tool to help develop the F370 eeprom writer.

To use this program
---------------------------------
1. Capture a one wire bus interaction between a master and slave device with an oscilloscope.
2. Output the waveform as a .csv file from the oscilloscope.
3. Create an /OWB directory and a /OWB/DATA directory wherever you want.
4. Place the .csv file in "user specified location for OWB folder"/OWB/DATA.
5. Place the 'decoder.cpp' file in the /OWB directory.
6. Compile the code with the following on Linux: g++ decoder.cpp -o byteDecoder (or any other way you like to compile c++ code)
7. In the same directory as the binary file, run the program with ./byteDecoder
