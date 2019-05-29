# EVS_CODEC
ENHANCE VOICE SERVICE

EVS inputs a 16-bit encoded pcm file with one frame for 20ms and supports four sample rate samples
(8 000, 16 000, 32 000 and 48 000 samples/s). 
The encoding bit rate can be 5.9, 7.2, 8.0, 9.6, 13.2. , 16.4, 24.4, 32.0, 48.0, 64.0, 96.0 or 128.0 kbit/s. 
There are 3 options for the encoder, LP based coding, frequency domain and inactive coding. 
The encoder selection of each frame is determined according to the closed-loop analysis result of the input signal. 
For LP based coding, there are six encoding modes, namely the Unvoiced Coding (UC) mode, Voiced Coding (VC) mode, 
Transition Coding (TC) mode, Audio Coding (AC) mode, Inactive Coding (IC) mode and Generic Coding (GC) mode. 
