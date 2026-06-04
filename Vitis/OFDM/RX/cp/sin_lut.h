#ifndef SIN_LUT_H
#define SIN_LUT_H

#include "ofdm_cfo_sync.h"

// 256-point Sine LUT for index i = 0 to 255, representing sin(2 * pi * i / 256)
static const data_t sin_lut[256] = {
    (data_t)0.00000000f, (data_t)0.02454123f, (data_t)0.04906767f, (data_t)0.07356456f, (data_t)0.09801714f, (data_t)0.12241067f, (data_t)0.14673047f, (data_t)0.17096189f,
    (data_t)0.19509032f, (data_t)0.21910124f, (data_t)0.24298018f, (data_t)0.26671276f, (data_t)0.29028468f, (data_t)0.31368174f, (data_t)0.33688985f, (data_t)0.35989504f,
    (data_t)0.38268343f, (data_t)0.40524131f, (data_t)0.42755509f, (data_t)0.44961133f, (data_t)0.47139674f, (data_t)0.49289819f, (data_t)0.51410274f, (data_t)0.53499762f,
    (data_t)0.55557023f, (data_t)0.57580819f, (data_t)0.59569930f, (data_t)0.61523159f, (data_t)0.63439328f, (data_t)0.65317284f, (data_t)0.67155895f, (data_t)0.68954054f,
    (data_t)0.70710678f, (data_t)0.72424708f, (data_t)0.74095113f, (data_t)0.75720885f, (data_t)0.77301045f, (data_t)0.78834643f, (data_t)0.80320753f, (data_t)0.81758481f,
    (data_t)0.83146961f, (data_t)0.84485357f, (data_t)0.85772861f, (data_t)0.87008694f, (data_t)0.88192126f, (data_t)0.89322430f, (data_t)0.90398929f, (data_t)0.91420975f,
    (data_t)0.92387953f, (data_t)0.93299280f, (data_t)0.94154407f, (data_t)0.94952818f, (data_t)0.95694034f, (data_t)0.96377607f, (data_t)0.97003125f, (data_t)0.97570213f,
    (data_t)0.98078528f, (data_t)0.98527764f, (data_t)0.98917651f, (data_t)0.99247953f, (data_t)0.99518473f, (data_t)0.99729046f, (data_t)0.99879546f, (data_t)0.99969882f,
    (data_t)0.999969f, (data_t)0.99969882f, (data_t)0.99879546f, (data_t)0.99729046f, (data_t)0.99518473f, (data_t)0.99247953f, (data_t)0.98917651f, (data_t)0.98527764f,
    (data_t)0.98078528f, (data_t)0.97570213f, (data_t)0.97003125f, (data_t)0.96377607f, (data_t)0.95694034f, (data_t)0.94952818f, (data_t)0.94154407f, (data_t)0.93299280f,
    (data_t)0.92387953f, (data_t)0.91420975f, (data_t)0.90398929f, (data_t)0.89322430f, (data_t)0.88192126f, (data_t)0.87008694f, (data_t)0.85772861f, (data_t)0.84485357f,
    (data_t)0.83146961f, (data_t)0.81758481f, (data_t)0.80320753f, (data_t)0.78834643f, (data_t)0.77301045f, (data_t)0.75720885f, (data_t)0.74095113f, (data_t)0.72424708f,
    (data_t)0.70710678f, (data_t)0.68954054f, (data_t)0.67155895f, (data_t)0.65317284f, (data_t)0.63439328f, (data_t)0.61523159f, (data_t)0.59569930f, (data_t)0.57580819f,
    (data_t)0.55557023f, (data_t)0.53499762f, (data_t)0.51410274f, (data_t)0.49289819f, (data_t)0.47139674f, (data_t)0.44961133f, (data_t)0.42755509f, (data_t)0.40524131f,
    (data_t)0.38268343f, (data_t)0.35989504f, (data_t)0.33688985f, (data_t)0.31368174f, (data_t)0.29028468f, (data_t)0.26671276f, (data_t)0.24298018f, (data_t)0.21910124f,
    (data_t)0.19509032f, (data_t)0.17096189f, (data_t)0.14673047f, (data_t)0.12241067f, (data_t)0.09801714f, (data_t)0.07356456f, (data_t)0.04906767f, (data_t)0.02454123f,
    (data_t)-0.00000000f, (data_t)-0.02454123f, (data_t)-0.04906767f, (data_t)-0.07356456f, (data_t)-0.09801714f, (data_t)-0.12241067f, (data_t)-0.14673047f, (data_t)-0.17096189f,
    (data_t)-0.19509032f, (data_t)-0.21910124f, (data_t)-0.24298018f, (data_t)-0.26671276f, (data_t)-0.29028468f, (data_t)-0.31368174f, (data_t)-0.33688985f, (data_t)-0.35989504f,
    (data_t)-0.38268343f, (data_t)-0.40524131f, (data_t)-0.42755509f, (data_t)-0.44961133f, (data_t)-0.47139674f, (data_t)-0.49289819f, (data_t)-0.51410274f, (data_t)-0.53499762f,
    (data_t)-0.55557023f, (data_t)-0.57580819f, (data_t)-0.59569930f, (data_t)-0.61523159f, (data_t)-0.63439328f, (data_t)-0.65317284f, (data_t)-0.67155895f, (data_t)-0.68954054f,
    (data_t)-0.70710678f, (data_t)-0.72424708f, (data_t)-0.74095113f, (data_t)-0.75720885f, (data_t)-0.77301045f, (data_t)-0.78834643f, (data_t)-0.80320753f, (data_t)-0.81758481f,
    (data_t)-0.83146961f, (data_t)-0.84485357f, (data_t)-0.85772861f, (data_t)-0.87008694f, (data_t)-0.88192126f, (data_t)-0.89322430f, (data_t)-0.90398929f, (data_t)-0.91420975f,
    (data_t)-0.92387953f, (data_t)-0.93299280f, (data_t)-0.94154407f, (data_t)-0.94952818f, (data_t)-0.95694034f, (data_t)-0.96377607f, (data_t)-0.97003125f, (data_t)-0.97570213f,
    (data_t)-0.98078528f, (data_t)-0.98527764f, (data_t)-0.98917651f, (data_t)-0.99247953f, (data_t)-0.99518473f, (data_t)-0.99729046f, (data_t)-0.99879546f, (data_t)-0.99969882f,
    (data_t)-1.00000000f, (data_t)-0.99969882f, (data_t)-0.99879546f, (data_t)-0.99729046f, (data_t)-0.99518473f, (data_t)-0.99247953f, (data_t)-0.98917651f, (data_t)-0.98527764f,
    (data_t)-0.98078528f, (data_t)-0.97570213f, (data_t)-0.97003125f, (data_t)-0.96377607f, (data_t)-0.95694034f, (data_t)-0.94952818f, (data_t)-0.94154407f, (data_t)-0.93299280f,
    (data_t)-0.92387953f, (data_t)-0.91420975f, (data_t)-0.90398929f, (data_t)-0.89322430f, (data_t)-0.88192126f, (data_t)-0.87008694f, (data_t)-0.85772861f, (data_t)-0.84485357f,
    (data_t)-0.83146961f, (data_t)-0.81758481f, (data_t)-0.80320753f, (data_t)-0.78834643f, (data_t)-0.77301045f, (data_t)-0.75720885f, (data_t)-0.74095113f, (data_t)-0.72424708f,
    (data_t)-0.70710678f, (data_t)-0.68954054f, (data_t)-0.67155895f, (data_t)-0.65317284f, (data_t)-0.63439328f, (data_t)-0.61523159f, (data_t)-0.59569930f, (data_t)-0.57580819f,
    (data_t)-0.55557023f, (data_t)-0.53499762f, (data_t)-0.51410274f, (data_t)-0.49289819f, (data_t)-0.47139674f, (data_t)-0.44961133f, (data_t)-0.42755509f, (data_t)-0.40524131f,
    (data_t)-0.38268343f, (data_t)-0.35989504f, (data_t)-0.33688985f, (data_t)-0.31368174f, (data_t)-0.29028468f, (data_t)-0.26671276f, (data_t)-0.24298018f, (data_t)-0.21910124f,
    (data_t)-0.19509032f, (data_t)-0.17096189f, (data_t)-0.14673047f, (data_t)-0.12241067f, (data_t)-0.09801714f, (data_t)-0.07356456f, (data_t)-0.04906767f, (data_t)-0.02454123f
};

#endif // SIN_LUT_H
