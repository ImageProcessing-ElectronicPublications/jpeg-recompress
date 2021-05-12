/*
    Compare two JPEG images. Several methods are available. PSNR, SSIM,
    and MS_SSIM require the images to be exactly the same size, while
    FAST works on any sized image.

    FAST compares two images and returns the difference on a
    scale of 0 - 99, where 0 would mean the images are identical. The
    comparison should be immune to exposure, saturation, white balance,
    scaling, minor crops and similar modifications.

    If the difference is 10 or less than the images are very likely
    different versions of the same image (e.g. a thumbnail or black
    and white edit) or just slightly different images. It is possible
    to get false positives, in which case a slower PSNR or SSIM
    comparison will help.
*/

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <iqa.h>
#include <smallfry.h>

#include "edit.h"
#include "hash.h"
#include "util.h"

// Comparison method
enum METHOD
{
    UNKNOWN,
    FAST,
    PSNR,
    SSIM,
    MS_SSIM,
    SMALLFRY,
    SHARPENBAD,
    COR,
    CORSHARP
};

int method = FAST;

/* Long command line options. */
enum longopts
{
    OPT_SHORT = 1000
};

int printPrefix = 1;

// Hash size when method is FAST
int size = 16;
int radius = 2;

// Use PPM input?
enum filetype inputFiletype1 = FILETYPE_AUTO;
enum filetype inputFiletype2 = FILETYPE_AUTO;

static enum METHOD parseMethod(const char *s)
{
    if (!strcmp("fast", s))
        return FAST;
    if (!strcmp("psnr", s))
        return PSNR;
    if (!strcmp("ssim", s))
        return SSIM;
    if (!strcmp("ms-ssim", s))
        return MS_SSIM;
    if (!strcmp("smallfry", s))
        return SMALLFRY;
    if (!strcmp("shbad", s))
        return SHARPENBAD;
    if (!strcmp("cor", s))
        return COR;
    if (!strcmp("corsh", s))
        return CORSHARP;
    return UNKNOWN;
}

static enum filetype parseInputFiletype(const char *s)
{
    if (!strcmp("auto", s))
        return FILETYPE_AUTO;
    if (!strcmp("jpeg", s))
        return FILETYPE_JPEG;
    if (!strcmp("ppm", s))
        return FILETYPE_PPM;
    return FILETYPE_UNKNOWN;
}

int compareFastFromBuffer(unsigned char *imageBuf1, long bufSize1, unsigned char *imageBuf2, long bufSize2)
{
    unsigned char *hash1, *hash2;

    // Generate hashes
    if (jpegHashFromBuffer(imageBuf1, bufSize1, &hash1, size))
    {
        error("error hashing image 1!");
        return 1;
    }

    if (jpegHashFromBuffer(imageBuf2, bufSize2, &hash2, size))
    {
        error("error hashing image 2!");
        return 1;
    }

    // Compare and print out hamming distance
    printf("%u\n", hammingDist(hash1, hash2, size * size) * 100 / (size * size));

    // Cleanup
    free(hash1);
    free(hash2);

    return 0;
}

int compareFromBuffer(unsigned char *imageBuf1, long bufSize1, unsigned char *imageBuf2, long bufSize2)
{
    unsigned char *image1, *image2, *image1Gray = NULL, *image2Gray = NULL;
    int width1, width2, height1, height2;
    int format, components;
    float diff;

    // Set requested pixel format
    switch (method) {
        case PSNR:
            format = JCS_RGB;
            components = 3;
            break;
        case SSIM: case MS_SSIM: default:
            format = JCS_GRAYSCALE;
            components = 1;
            break;
    }

    // Decode files
    if (!decodeFileFromBuffer(imageBuf1, bufSize1, &image1, inputFiletype1, &width1, &height1, format))
    {
        error("invalid input reference file");
        return 1;
    }

    if (1 == components && FILETYPE_PPM == inputFiletype1)
    {
        grayscale(image1, &image1Gray, width1, height1);
        free(image1);
        image1 = image1Gray;
    }

    if (!decodeFileFromBuffer(imageBuf2, bufSize2, &image2, inputFiletype2, &width2, &height2, format))
    {
        error("invalid input query file");
        return 1;
    }

    if (1 == components && FILETYPE_PPM == inputFiletype2)
    {
        grayscale(image2, &image2Gray, width2, height2);
        free(image2);
        image2 = image2Gray;
    }

    // Ensure width/height are equal
    if (width1 != width2 || height1 != height2)
    {
        error("images must be identical sizes for selected method!");
        return 1;
    }

    // Calculate and print comparison
    switch (method)
    {
        case PSNR:
            diff = iqa_psnr(image1, image2, width1, height1, width1 * components);
            if (printPrefix)
                printf("PSNR: ");
            printf("%f\n", diff);
            break;
        case SMALLFRY:
            diff = metric_smallfry(image1, image2, width1, height1);
            if (printPrefix)
                printf("SMALLFRY: ");
            printf("%f\n", diff);
            break;
        case SHARPENBAD:
            diff = metric_sharpenbad(image1, image2, width1, height1);
            if (printPrefix)
                printf("SHARPENBAD: ");
            printf("%f\n", diff);
            break;
        case COR:
            diff = metric_cor(image1, image2, width1, height1);
            if (printPrefix)
                printf("COR: ");
            printf("%f\n", diff);
            break;
        case CORSHARP:
            diff = metric_corsharp(image1, image2, width1, height1, radius);
            if (printPrefix)
                printf("CORSHARP: ");
            printf("%f\n", diff);
            break;
        case MS_SSIM:
            diff = iqa_ms_ssim(image1, image2, width1, height1, width1 * components, 0);
            if (printPrefix)
                printf("MS-SSIM: ");
            printf("%f\n", diff);
            break;
        case SSIM: default:
            diff = iqa_ssim(image1, image2, width1, height1, width1 * components, 0, 0);
            if (printPrefix)
                printf("SSIM: ");
            printf("%f\n", diff);
            break;
    }

    // Cleanup
    free(image1);
    free(image2);

    return 0;
}

void usage(void)
{
    printf("usage: %s [options] image1.jpg image2.jpg\n\n", progname);
    printf("options:\n\n");
    printf("  -V, --version                output program version\n");
    printf("  -h, --help                   output program help\n");
    printf("  -s, --size [arg]             set fast comparison image hash size\n");
    printf("  -m, --method [arg]           set comparison method to one of 'fast', 'psnr', 'ssim', 'ms-ssim', 'smallfry', 'shbad', 'cor', 'corsh' [fast]\n");
    printf("  -A, --radius [arg]           set aperture size (for corsh)\n");
    printf("  -r, --ppm                    parse first input as PPM instead of JPEG\n");
    printf("  -T, --input-filetype [arg]   set first input file type to one of 'auto', 'jpeg', 'ppm' [auto]\n");
    printf("  -U, --second-filetype [arg]  set second input file type to one of 'auto', 'jpeg', 'ppm' [auto]\n");
    printf("      --short                  do not prefix output with the name of the used method\n");
}

int main (int argc, char **argv)
{
    const char *optstring = "VhS:m:A:rT:U:";
    static const struct option opts[] = {
        { "version", no_argument, 0, 'V' },
        { "help", no_argument, 0, 'h' },
        { "size", required_argument, 0, 'S' },
        { "method", required_argument, 0, 'm' },
        { "radius", required_argument, 0, 'A' },
        { "ppm", no_argument, 0, 'r' },
        { "input-filetype", required_argument, 0, 'T' },
        { "second-filetype", required_argument, 0, 'U' },
        { "short", no_argument, 0, OPT_SHORT },
        { 0, 0, 0, 0 }
    };
    int opt, longind = 0;

    progname = "jpeg-compare";

    while ((opt = getopt_long(argc, argv, optstring, opts, &longind)) != -1)
    {
        switch (opt)
        {
            case 'V':
                version();
                return 0;
            case 'h':
                usage();
                return 0;
            case 's':
                size = atoi(optarg);
                break;
            case 'm':
                method = parseMethod(optarg);
                break;
            case 'A':
                radius = atoi(optarg);
                break;
            case 'r':
                if (inputFiletype1 != FILETYPE_AUTO)
                {
                    error("multiple file types specified for input file 1");
                    return 1;
                }
                inputFiletype1 = FILETYPE_PPM;
                break;
            case 'T':
                if (inputFiletype1 != FILETYPE_AUTO) {
                    error("multiple file types specified for input file 1");
                    return 1;
                }
                inputFiletype1 = parseInputFiletype(optarg);
                break;
            case 'U':
                if (inputFiletype2 != FILETYPE_AUTO) {
                    error("multiple file types specified for input file 2");
                    return 1;
                }
                inputFiletype2 = parseInputFiletype(optarg);
                break;
            case OPT_SHORT:
                printPrefix = 0;
                break;
        };
    }

    if (argc - optind != 2)
    {
        usage();
        return 255;
    }

    // Read the images
    unsigned char *imageBuf1, *imageBuf2;
    long bufSize1, bufSize2;

    char *fileName1 = argv[optind];
    char *fileName2 = argv[optind + 1];

    bufSize1 = readFile(fileName1, (void **)&imageBuf1);
    if (!bufSize1)
    {
        error("failed to read file: %s", fileName1);
        return 1;
    }

    bufSize2 = readFile(fileName2, (void **)&imageBuf2);
    if (!bufSize2)
    {
        error("failed to read file: %s", fileName2);
        return 1;
    }

    /* Detect input file types. */
    if (inputFiletype1 == FILETYPE_AUTO)
        inputFiletype1 = detectFiletypeFromBuffer(imageBuf1, bufSize1);
    if (inputFiletype2 == FILETYPE_AUTO)
        inputFiletype2 = detectFiletypeFromBuffer(imageBuf2, bufSize2);

    // Calculate and print output
    switch (method)
    {
        case FAST:
            if (inputFiletype1 != FILETYPE_JPEG || inputFiletype2 != FILETYPE_JPEG)
            {
                error("fast comparison only works with JPEG files!");
                return 255;
            }
            return compareFastFromBuffer(imageBuf1, bufSize1, imageBuf2, bufSize2);
        case PSNR:
        case SSIM:
        case MS_SSIM:
        case SMALLFRY:
        case SHARPENBAD:
        case COR:
        case CORSHARP:
            return compareFromBuffer(imageBuf1, bufSize1, imageBuf2, bufSize2);
        default:
            error("unknown comparison method!");
            return 255;
    }

    // Cleanup resources
    free(imageBuf1);
    free(imageBuf2);
}