#pragma once

// Text-to-speech on the XH-S3E-AI board speaker (I2S amp on BCLK 15, WS 16, DOUT 7)
// with SAM, the 1982 "Software Automatic Mouth". English only, robot voice.
void speak_init(void);
void speak(const char *text);
