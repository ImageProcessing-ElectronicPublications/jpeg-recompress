/*
    Find ZF point of compress a JPEG file while attempting to keep visual quality the same
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

const char *COMMENT = "Compressed by jpeg-fzpoint";

// Number of binary search steps
int attempts = 6;

// Min/max JPEG quality
int jpegMin = 40;
int jpegMax = 100;
int shRadius = 2;

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

static void setRadius(command_t *self)
{
    shRadius = atoi(self->arg);
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
    command_option(&cmd, "-n", "--min [arg]", "Minimum JPEG quality [2]", setMinimum);
    command_option(&cmd, "-x", "--max [arg]", "Maximum JPEG quality [100]", setMaximum);
    command_option(&cmd, "-A", "--radius [arg]", "Sharpen radius [2]", setRadius);
    command_option(&cmd, "-l", "--loops [arg]", "Set the number of runs to attempt [6]", setAttempts);
    command_option(&cmd, "-a", "--accurate", "Favor accuracy over speed", setAccurate);
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
                fprintf(stderr, "File already processed by jpeg-zfpoint!\n");
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
    float metric, maxmetric, qmetric, cmpMin, cmpMax, cmpQ;
    compressedSize = encodeJpeg(&compressed, original, width, height, JCS_RGB, max, 0, 1, subsample);
    compressedGraySize = decodeJpeg(compressed, compressedSize, &compressedGray, &width, &height, JCS_GRAYSCALE);
    maxmetric = metric_corsharp(originalGray, compressedGray, width, height, shRadius);
    maxmetric = cor_sigma(maxmetric);
    qmetric = maxmetric / (float)max;
    cmpMax = 0;
    compressedSize = encodeJpeg(&compressed, original, width, height, JCS_RGB, min, 0, 1, subsample);
    compressedGraySize = decodeJpeg(compressed, compressedSize, &compressedGray, &width, &height, JCS_GRAYSCALE);
    metric = metric_corsharp(originalGray, compressedGray, width, height, shRadius);
    metric = cor_sigma(metric);
    cmpMin = qmetric * (float)min - metric;
    for (int attempt = attempts - 1; attempt >= 0; --attempt)
    {
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
        metric = metric_corsharp(originalGray, compressedGray, width, height, shRadius);
        metric = cor_sigma(metric);
        cmpQ = qmetric * (float)quality - metric;
        info("zfpoint");

        if (attempt)
        {
            info(" at q=%i (%i - %i): dM %f\n", quality, min, max, cmpQ);
        } else {
            info(" at q=%i: dM %f\n", quality, cmpQ);
        }

        if (cmpMin < cmpMax)
        {
            min = MIN(quality + 1, max);
            cmpMin = cmpQ;
        } else {
            max = MAX(quality - 1, min);
            cmpMax = cmpQ;
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
