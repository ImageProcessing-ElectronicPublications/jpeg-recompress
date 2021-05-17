/*
    Image editing functions
*/
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <iqa.h>
#include <smallfry.h>
#include <sys/types.h>
#include <jpeglib.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#ifndef JMETRICS_H
#define JMETRICS_H

#ifndef JMVERSION
#define JMVERSION "2.3.0"
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Subsampling method, which defines how much of the data from
// each color channel is included in the image per 2x2 block.
// A value of 4 means all four pixels are included, while 2
// means that only two of the four are included (hence the term
// subsampling). Subsampling works really well for photos, but
// can have issues with crisp colored borders (e.g. red text).
enum SUBSAMPLING_METHOD
{
    // Default is 4:2:0
    SUBSAMPLE_DEFAULT,
    // Using 4:4:4 is more detailed and will prevent fine text
    // from getting blurry (e.g. screenshots)
    SUBSAMPLE_444
};

enum filetype
{
    FILETYPE_UNKNOWN,
    FILETYPE_AUTO,
    FILETYPE_JPEG,
    FILETYPE_PPM
};

// Comparison method
enum METHOD
{
    UNKNOWN,
    FAST,
    MPE,
    PSNR,
    SSIM,
    MS_SSIM,
    SMALLFRY,
    SSIMFRY,
    SHARPENBAD,
    SSIMSHBAD,
    SUMMET,
    COR,
    CORSHARP
};

// Target quality (SSIM) value
enum QUALITY_PRESET
{
    LOW,
    MEDIUM,
    HIGH,
    VERYHIGH
};

/* Long command line options. */
enum longopts
{
    OPT_SHORT = 1000
};

/*
    Clamp a value between low and high.
*/
float clamp(float low, float value, float high);

/*
    Bilinear interpolation
*/
int interpolate(const unsigned char *image, int width, int components, float x, float y, int offset);

/*
    Get mean error per pixel rate.
*/
float meanPixelError(const unsigned char *original, const unsigned char *compressed, int width, int height, int components);

/*
    Remove fisheye distortion from an image. The amount of distortion is
    controlled by strength, while zoom controls where the image gets
    cropped. For example, the Tokina 10-17mm ATX fisheye on a Canon APS-C
    body set to 10mm looks good with strength=2.6 and zoom=1.2.
*/
void defish(const unsigned char *input, unsigned char *output, int width, int height, int components, float strength, float zoom);

/*
    Convert an RGB image to grayscale. Assumes 8-bit color components,
    3 color components and a row stride of width * 3.
*/
long grayscale(const unsigned char *input, unsigned char **output, int width, int height);

/*
    Generate an image hash given a filename. This is a convenience
    function which reads the file, decodes it to grayscale,
    scales the image, and generates the hash.
*/
int jpegHash(const char *filename, unsigned char **hash, int size);
int jpegHashFromBuffer(unsigned char *imageBuf, long bufSize, unsigned char **hash, int size);

/*
    Downscale an image with nearest-neighbor interpolation.
    http://jsperf.com/pixel-interpolation/2
*/
void scale(unsigned char *image, int width, int height, unsigned char **newImage, int newWidth, int newHeight);

/*
    Generate an image hash based on gradients.
    http://www.hackerfactor.com/blog/index.php?/archives/529-Kind-of-Like-That.html
*/
void genHash(unsigned char *image, int width, int height, unsigned char **hash);

/*
    Calculate the hamming distance between two hashes.
    http://en.wikipedia.org/wiki/Hamming_distance
*/
unsigned int hammingDist(const unsigned char *hash1, const unsigned char *hash2, int hashLength);

/* Print program version to stdout. */
void version(void);

/* Print an error message. */
void error(const char *format, ...);

/*
    Read a file into a buffer and return the length.
*/
long readFile(char *name, void **buffer);

/*
    Decode a buffer into a JPEG image with the given pixel format.
    Returns the size of the image pixel array.
    See libjpeg.txt for a (very long) explanation.
*/
int checkJpegMagic(const unsigned char *buf, unsigned long size);
unsigned long decodeJpeg(unsigned char *buf, unsigned long bufSize, unsigned char **image, int *width, int *height, int *jpegcs, int pixelFormat);

/*
    Decode buffer into a PPM image.
    Returns the size of the image pixel array.
*/
int checkPpmMagic(const unsigned char *buf, unsigned long size);
unsigned long decodePpm(unsigned char *buf, unsigned long bufSize, unsigned char **image, int *width, int *height);

/*
    Encode a buffer of image pixels into a JPEG.
*/
unsigned long encodeJpeg(unsigned char **jpeg, unsigned char *buf, int width, int height, int pixelFormat, int quality, int jpegcs, int progressive, int optimize, int subsample);

/* Automatically detect the file type of a given file. */
enum filetype detectFiletype(const char *filename);
enum filetype detectFiletypeFromBuffer(unsigned char *buf, long bufSize);

/* Decode an image file with a given format. */
unsigned long decodeFile(const char *filename, unsigned char **image, enum filetype type, int *width, int *height, int pixelFormat);
unsigned long decodeFileFromBuffer(unsigned char *buf, long bufSize, unsigned char **image, enum filetype type, int *width, int *height, int *jpegcs, int pixelFormat);

/*
    Get JPEG metadata (EXIF, IPTC, XMP, etc) and return a buffer
    with just this data, suitable for writing out to a new file.
    Reads in all APP0-APP15 markers as well as COM markers.
    Reads up to 20 markers.

    If comment is not NULL, then returns 1 if the comment is
    encountered, allowing scripts to detect if they have previously
    modified the file.
*/
int getMetadata(const unsigned char *buf, unsigned int bufSize, unsigned char **meta, unsigned int *metaSize, const char *comment);
FILE *openOutput(char *name);
void info(int quiet, const char *format, ...);

enum filetype parseInputFiletype(const char *s);
int parseSubsampling(const char *s);
enum QUALITY_PRESET parseQuality(const char *s);
float setTargetFromPreset(int preset);
enum METHOD parseMethod(const char *s);
float RescaleMetric(int currentmethod, float value);
char* MetricName(int currentmethod);
float MetricCalc(int method, unsigned char *image1, unsigned char *image2, int width, int height, int components, int radius);
int compareFastFromBuffer(unsigned char *imageBuf1, long bufSize1, unsigned char *imageBuf2, long bufSize2, int printPrefix, int size);
int compareFromBuffer(int method, unsigned char *imageBuf1, long bufSize1, unsigned char *imageBuf2, long bufSize2, int printPrefix, int umscale, int radius, enum filetype inputFiletype1, enum filetype inputFiletype2);

#endif
