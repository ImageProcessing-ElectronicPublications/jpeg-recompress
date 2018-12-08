/*
    Recompress a JPEG file while attempting to keep visual quality the same
    by using structural similarity (SSIM) as a metric. Does a binary search
    between JPEG quality 40 and 95 to find the best match. Also makes sure
    that huffman tables are optimized if they weren't already.
*/
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <iqa.h>
#include <smallfry.h>

#include "commander.h"
#include "edit.h"
#include "util.h"

#ifdef _WIN32
    #include <io.h>
    #include <fcntl.h>
#endif

const char *COMMENT = "Compressed by jpeg-recompress";

// Comparison method
enum METHOD
{
    UNKNOWN,
    SSIM,
    MS_SSIM,
    SMALLFRY,
    SSIMFRY,
    SHARPENBAD,
    SUMMET,
    MPE
};

int method = SUMMET;

// Number of binary search steps
int attempts = 6;

// Target quality (SSIM) value
enum QUALITY_PRESET 
{
    LOW,
    MEDIUM,
    HIGH,
    VERYHIGH
};

float target = 0.0;
int preset = MEDIUM;

// Min/max JPEG quality
int jpegMin = 40;
int jpegMax = 98;

// Strip metadata from the file?
int strip = 0;

// Disable progressive mode?
int noProgressive = 0;

// Defish the image?
float defishStrength = 0.0;
float defishZoom = 1.0;

// Input format
enum filetype inputFiletype = FILETYPE_AUTO;

// Whether to copy files that cannot be compressed
int copyFiles = 1;

// Whether to favor accuracy over speed
int accurate = 0;

// Chroma subsampling method
int subsample = SUBSAMPLE_DEFAULT;

// Quiet mode (less output)
int quiet = 0;

static void setAttempts(command_t *self)
{
    attempts = atoi(self->arg);
}

static void setTarget(command_t *self)
{
    target = atof(self->arg);
}

static void setQuality(command_t *self)
{
    if (!strcmp("low", self->arg)) {
        preset = LOW;
    } else if (!strcmp("medium", self->arg)) {
        preset = MEDIUM;
    } else if (!strcmp("high", self->arg)) {
        preset = HIGH;
    } else if (!strcmp("veryhigh", self->arg)) {
        preset = VERYHIGH;
    } else {
        fprintf(stderr, "Unknown quality preset '%s'!\n", self->arg);
    }
}

static void setMethod(command_t *self)
{
    if (!strcmp("ssim", self->arg)) {
        method = SSIM;
    } else if (!strcmp("ms-ssim", self->arg)) {
        method = MS_SSIM;
    } else if (!strcmp("smallfry", self->arg)) {
        method = SMALLFRY;
    } else if (!strcmp("ssimfry", self->arg)) {
        method = SSIMFRY;
    } else if (!strcmp("shbad", self->arg)) {
        method = SHARPENBAD;
    } else if (!strcmp("mpe", self->arg)) {
        method = MPE;
    } else if (!strcmp("sum", self->arg)) {
        method = SUMMET;
    } else {
        method = UNKNOWN;
    }
}

static void setNoProgressive(command_t *self)
{
    noProgressive = 1;
}

static void setMinimum(command_t *self)
{
    jpegMin = atoi(self->arg);
}

static void setMaximum(command_t *self)
{
    jpegMax = atoi(self->arg);
}

static void setStrip(command_t *self)
{
    strip = 1;
}

static void setDefish(command_t *self)
{
    defishStrength = atof(self->arg);
}

static void setZoom(command_t *self)
{
    defishZoom = atof(self->arg);
}

static void setInputFiletype(command_t *self)
{
    if (!strcmp("auto", self->arg))
        inputFiletype = FILETYPE_AUTO;
    else if (!strcmp("jpeg", self->arg))
        inputFiletype = FILETYPE_JPEG;
    else if (!strcmp("ppm", self->arg))
        inputFiletype = FILETYPE_PPM;
    else
        inputFiletype = FILETYPE_UNKNOWN;
}

static void setPpm(command_t *self)
{
    inputFiletype = FILETYPE_PPM;
}

static void setCopyFiles(command_t *self)
{
    copyFiles = 0;
}

static void setAccurate(command_t *self)
{
    accurate = 1;
}

static void setTargetFromPreset()
{
    switch (preset) {
        case LOW:
            target = 0.5;
            break;
        case MEDIUM:
            target = 0.76;
            break;
        case HIGH:
            target = 0.93;
            break;
        case VERYHIGH:
            target = 0.99;
            break;
    }
}
static float RescaleMetric(int currentmethod, float value)
{
    float kametric = 1.0;
    float kbmetric = 0.0;
    switch (currentmethod) {
        case SSIM:
            kametric = 251.0;
            kbmetric = -250.0;
            value *= value;
            break;
        case MS_SSIM:
            kametric = 2.08;
            kbmetric = -1.0;
            value *= value;
            break;
        case SMALLFRY:
            kametric = 0.105;
            kbmetric = -10.0;
            break;
        case MPE:
            kametric = -1.16;
            kbmetric = 1.94;
            value = sqrt(value);
            break;
    }
    value *= kametric;
    value += kbmetric;
    return value;
}

static void setSubsampling(command_t *self)
{
    if (!strcmp("default", self->arg)) {
        subsample = SUBSAMPLE_DEFAULT;
    } else if (!strcmp("disable", self->arg)) {
        subsample = SUBSAMPLE_444;
    } else {
        fprintf(stderr, "Unknown sampling method '%s', using default!\n", self->arg);
    }
}

static void setQuiet(command_t *self)
{
    quiet = 1;
}

// Open a file for writing
FILE *openOutput(char *name)
{
    if (strcmp("-", name) == 0) {
        #ifdef _WIN32
            setmode(fileno(stdout), O_BINARY);
        #endif

        return stdout;
    } else {
        return fopen(name, "wb");
    }
}

// Logs an informational message, taking quiet mode into account
void info(const char *format, ...)
{
    va_list argptr;

    if (!quiet) {
        va_start(argptr, format);
        vfprintf(stderr, format, argptr);
        va_end(argptr);
    }
}

int main (int argc, char **argv)
{
    unsigned char *buf;
    long bufSize = 0;
    unsigned char *original;
    long originalSize = 0;
    unsigned char *originalGray = NULL;
    long originalGraySize = 0;
    unsigned char *compressed = NULL;
    unsigned long compressedSize = 0;
    unsigned char *compressedGray;
    long compressedGraySize = 0;
    unsigned char *tmpImage;
    int width, height;
    unsigned char *metaBuf;
    unsigned int metaSize = 0;
    FILE *file;

    // Parse commandline options
    command_t cmd;
    command_init(&cmd, argv[0], VERSION);
    cmd.usage = "[options] input.jpg compressed-output.jpg";
    command_option(&cmd, "-t", "--target [arg]", "Set target quality [0.76]", setTarget);
    command_option(&cmd, "-q", "--quality [arg]", "Set a quality preset: low, medium, high, veryhigh [medium]", setQuality);
    command_option(&cmd, "-n", "--min [arg]", "Minimum JPEG quality [40]", setMinimum);
    command_option(&cmd, "-x", "--max [arg]", "Maximum JPEG quality [98]", setMaximum);
    command_option(&cmd, "-l", "--loops [arg]", "Set the number of runs to attempt [6]", setAttempts);
    command_option(&cmd, "-a", "--accurate", "Favor accuracy over speed", setAccurate);
    command_option(&cmd, "-m", "--method [arg]", "Set comparison method to one of 'mpe', 'ssim', 'ms-ssim', 'smallfry', 'ssimfry', 'shbad', 'sum' [sum]", setMethod);
    command_option(&cmd, "-s", "--strip", "Strip metadata", setStrip);
    command_option(&cmd, "-d", "--defish [arg]", "Set defish strength [0.0]", setDefish);
    command_option(&cmd, "-z", "--zoom [arg]", "Set defish zoom [1.0]", setZoom);
    command_option(&cmd, "-r", "--ppm", "Parse input as PPM", setPpm);
    command_option(&cmd, "-c", "--no-copy", "Disable copying files that will not be compressed", setCopyFiles);
    command_option(&cmd, "-p", "--no-progressive", "Disable progressive encoding", setNoProgressive);
    command_option(&cmd, "-S", "--subsample [arg]", "Set subsampling method. Valid values: 'default', 'disable'. [default]", setSubsampling);
    command_option(&cmd, "-T", "--input-filetype [arg]", "Set input file type to one of 'auto', 'jpeg', 'ppm' [auto]", setInputFiletype);
    command_option(&cmd, "-Q", "--quiet", "Only print out errors.", setQuiet);
    command_parse(&cmd, argc, argv);

    if (cmd.argc < 2)
    {
        command_help(&cmd);
        return 255;
    }

    if (method == UNKNOWN)
    {
        fprintf(stderr, "Invalid method!");
        command_help(&cmd);
        return 255;
    }

    // No target passed, use preset!
    if (!target)
    {
        setTargetFromPreset();
    }
    
    char *inputPath = (char *) cmd.argv[0];
    char *outputPath = cmd.argv[1];

    /* Read the input into a buffer. */
    bufSize = readFile(inputPath, (void **) &buf);

    /* Detect input file type. */
    if (inputFiletype == FILETYPE_AUTO)
        inputFiletype = detectFiletypeFromBuffer(buf, bufSize);

    /*
     * Read original image and decode. We need the raw buffer contents and its
     * size to obtain meta data and the original file size later.
     */
    originalSize = decodeFileFromBuffer(buf, bufSize, &original, inputFiletype, &width, &height, JCS_RGB);
    if (!originalSize)
    {
        fprintf(stderr, "invalid input file: %s\n", cmd.argv[0]);
        return 1;
    }

    if (defishStrength)
    {
        info("Defishing...\n");
        tmpImage = malloc(width * height * 3);
        defish(original, tmpImage, width, height, 3, defishStrength, defishZoom);
        free(original);
        original = tmpImage;
    }

    // Convert RGB input into Y
    originalGraySize = grayscale(original, &originalGray, width, height);

    if (inputFiletype == FILETYPE_JPEG)
    {
        // Read metadata (EXIF / IPTC / XMP tags)
        if (getMetadata(buf, bufSize, &metaBuf, &metaSize, COMMENT))
        {
            if (copyFiles)
            {
                info("File already processed by jpeg-recompress!\n");
                file = openOutput(outputPath);
                if (file == NULL)
                {
                    fprintf(stderr, "Could not open output file.");
                    return 1;
                }

                fwrite(buf, bufSize, 1, file);
                fclose(file);

                free(buf);

                return 0;
            } else {
                fprintf(stderr, "File already processed by jpeg-recompress!\n");
                free(buf);
                return 2;
            }
        }
    }

    if (strip)
    {
        // Pretend we have no metadata
        metaSize = 0;
    } else {
        info("Metadata size is %ukb\n", metaSize / 1024);
    }

    if (!originalSize || !originalGraySize) { return 1; }

    if (jpegMin > jpegMax)
    {
        fprintf(stderr, "Maximum JPEG quality must not be smaller than minimum JPEG quality!\n");
        return 1;
    }

    // Do a binary search to find the optimal encoding quality for the
    // given target SSIM value.
    int min = jpegMin, max = jpegMax;
    for (int attempt = attempts - 1; attempt >= 0; --attempt)
    {
        float metric, umetric;
        int quality = min + (max - min) / 2;
        int progressive = attempt ? 0 : !noProgressive;
        int optimize = accurate ? 1 : (attempt ? 0 : 1);

        // Recompress to a new quality level, without optimizations (for speed)
        compressedSize = encodeJpeg(&compressed, original, width, height, JCS_RGB, quality, progressive, optimize, subsample);

        // Load compressed luma for quality comparison
        compressedGraySize = decodeJpeg(compressed, compressedSize, &compressedGray, &width, &height, JCS_GRAYSCALE);

        if (!compressedGraySize)
        {
          fprintf(stderr, "Unable to decode file that was just encoded!\n");
          return 1;
        }

        if (!attempt)
        {
            info("Final optimized ");
        }

        // Measure quality difference
        switch (method)
        {
            case SSIM:
                metric = iqa_ssim(originalGray, compressedGray, width, height, width, 0, 0);
                umetric = RescaleMetric(method, metric);
                info("ssim");
                break;
            case MS_SSIM:
                metric = iqa_ms_ssim(originalGray, compressedGray, width, height, width, 0);
                umetric = RescaleMetric(method, metric);
                info("ms-ssim");
                break;
            case SMALLFRY:
                metric = smallfry_metric(originalGray, compressedGray, width, height);
                umetric = RescaleMetric(method, metric);
                info("smallfry");
                break;
            case SHARPENBAD:
                metric = sharpenbad_metric(originalGray, compressedGray, width, height);
                umetric = RescaleMetric(method, metric);
                info("sharpenbad");
                break;
            case MPE:
                metric = meanPixelError(originalGray, compressedGray, width, height, 1);
                umetric = RescaleMetric(method, metric);
                info("mpe");
                break;
            case SSIMFRY:
                metric = iqa_ssim(originalGray, compressedGray, width, height, width, 0, 0);
                umetric = RescaleMetric(SSIM, metric);
                metric = smallfry_metric(originalGray, compressedGray, width, height);
                umetric += RescaleMetric(SMALLFRY, metric);
                umetric /= 2.0;
                info("ssimfry");
                break;
            case SUMMET: default:
                metric = iqa_ssim(originalGray, compressedGray, width, height, width, 0, 0);
                umetric = RescaleMetric(SSIM, metric);
                metric = smallfry_metric(originalGray, compressedGray, width, height);
                umetric += RescaleMetric(SMALLFRY, metric);
                metric = sharpenbad_metric(originalGray, compressedGray, width, height);
                umetric += RescaleMetric(SHARPENBAD, metric);
                umetric /= 3.0;
                info("sum");
                break;
        }

        if (attempt)
        {
            info(" at q=%i (%i - %i): UM %f\n", quality, min, max, umetric);
        } else {
            info(" at q=%i: UM %f\n", quality, umetric);
        }

        if (umetric < target)
        {
            if (compressedSize >= bufSize)
            {
                free(compressed);
                free(compressedGray);

                if (copyFiles)
                {
                    info("Output file would be larger than input!\n");
                    file = openOutput(cmd.argv[1]);
                    if (file == NULL)
                    {
                        fprintf(stderr, "Could not open output file.");
                        return 1;
                    }

                    fwrite(buf, bufSize, 1, file);
                    fclose(file);

                    free(buf);

                    return 0;
                } else {
                    fprintf(stderr, "Output file would be larger than input!\n");
                    free(buf);
                    return 1;
                }
            }

            min = MIN(quality + 1, max);
        } else {
            max = MAX(quality - 1, min);
        }

        // If we aren't done yet, then free the image data
        if (attempt)
        {
            free(compressed);
            free(compressedGray);
        }
    }

    free(buf);

    // Calculate and show savings, if any
    int percent = (compressedSize + metaSize) * 100 / bufSize;
    unsigned long saved = (bufSize > compressedSize) ? bufSize - compressedSize - metaSize : 0;
    info("New size is %i%% of original (saved %lu kb)\n", percent, saved / 1024);

    if (compressedSize >= bufSize)
    {
        fprintf(stderr, "Output file is larger than input, aborting!\n");
        return 1;
    }

    // Open output file for writing
    file = openOutput(cmd.argv[1]);
    if (file == NULL)
    {
        fprintf(stderr, "Could not open output file.");
        return 1;
    }

    /* Check that the metadata starts with a SOI marker. */
    if (!checkJpegMagic(compressed, compressedSize))
    {
        fprintf(stderr, "Missing SOI marker, aborting!\n");
        return 1;
    }

    /* Make sure APP0 is recorded immediately after the SOI marker. */
    if (compressed[2] != 0xff || compressed[3] != 0xe0)
    {
        fprintf(stderr, "Missing APP0 marker, aborting!\n");
        return 1;
    }

    /* Write SOI marker and APP0 metadata to the output file. */
    int app0_len = (compressed[4] << 8) + compressed[5];
    fwrite(compressed, 4 + app0_len, 1, file);

    /*
     * Write comment (COM metadata) so we know not to reprocess this file in
     * the future if it gets passed in again.
     */
    fputc(0xff, file);
    fputc(0xfe, file);
    fputc(0x00, file);
    fputc(strlen(COMMENT) + 2, file);
    fwrite(COMMENT, strlen(COMMENT), 1, file);

    /* Write additional metadata markers. */
    if (inputFiletype == FILETYPE_JPEG && !strip)
    {
        fwrite(metaBuf, metaSize, 1, file);
    }

    /* Write image data. */
    fwrite(compressed + 4 + app0_len, compressedSize - 4 - app0_len, 1, file);
    fclose(file);

    /* Cleanup. */
    command_free(&cmd);

    if (inputFiletype == FILETYPE_JPEG && !strip)
    {
        free(metaBuf);
    }

    free(compressed);
    free(original);
    free(originalGray);

    return 0;
}
