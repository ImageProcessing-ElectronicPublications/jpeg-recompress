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
#include "jmetrics.h"

void usage(char *progname)
{
    printf("usage: %s [options] image1.jpg image2.jpg\n\n", progname);
    printf("options:\n\n");
    printf("  -h, --help                   output program help\n");
    printf("  -m, --method [arg]           set comparison method to one of:\n");
    printf("                               'fast', 'mpe', 'psnr', 'mse', 'msef', 'cor', 'ssim', 'ms-ssim', 'vifp1',\n");
    printf("                               'smallfry', 'shbad', 'nhw', 'ssimfry', 'ssimshb', 'sum' [fast]\n");
    printf("  -n, --norm                   UM scale metric\n");
    printf("  -r, --ppm                    parse first input as PPM instead of JPEG\n");
    printf("  -s, --size [arg]             set fast comparison image hash size\n");
    printf("  -T, --input-filetype [arg]   set first input file type to one of 'auto', 'jpeg', 'ppm' [auto]\n");
    printf("  -U, --second-filetype [arg]  set second input file type to one of 'auto', 'jpeg', 'ppm' [auto]\n");
    printf("  -V, --version                output program version\n");
    printf("      --short                  do not prefix output with the name of the used method\n");
}

int main (int argc, char **argv)
{
    int method = FAST;
    int umscale = 0;

    int printPrefix = 1;

    // Hash size when method is FAST
    int size = 16;

    unsigned char *imageBuf1, *imageBuf2;
    long bufSize1, bufSize2;
    char *fileName1, *fileName2;

    // Use PPM input?
    enum filetype inputFiletype1 = FILETYPE_AUTO;
    enum filetype inputFiletype2 = FILETYPE_AUTO;

    const char *optstring = "VhS:m:nrT:U:";
    static const struct option opts[] =
    {
        { "version", no_argument, 0, 'V' },
        { "help", no_argument, 0, 'h' },
        { "size", required_argument, 0, 'S' },
        { "method", required_argument, 0, 'm' },
        { "norm", no_argument, 0, 'n' },
        { "ppm", no_argument, 0, 'r' },
        { "input-filetype", required_argument, 0, 'T' },
        { "second-filetype", required_argument, 0, 'U' },
        { "short", no_argument, 0, OPT_SHORT },
        { 0, 0, 0, 0 }
    };
    int opt, longind = 0;

    char *progname = "jpeg-compare";

    while ((opt = getopt_long(argc, argv, optstring, opts, &longind)) != -1)
    {
        switch (opt)
        {
        case 'V':
            version();
            return 0;
        case 'h':
            usage(progname);
            return 0;
        case 's':
            size = atoi(optarg);
            break;
        case 'm':
            method = parseMethod(optarg);
            break;
        case 'n':
            umscale = 1;
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
            if (inputFiletype1 != FILETYPE_AUTO)
            {
                error("multiple file types specified for input file 1");
                return 1;
            }
            inputFiletype1 = parseInputFiletype(optarg);
            break;
        case 'U':
            if (inputFiletype2 != FILETYPE_AUTO)
            {
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
        usage(progname);
        return 255;
    }

    // Read the images
    fileName1 = argv[optind];
    fileName2 = argv[optind + 1];

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
        return compareFastFromBuffer(imageBuf1, bufSize1, imageBuf2, bufSize2, printPrefix, size);
    case MPE:
    case PSNR:
    case MSE:
    case MSEF:
    case SSIM:
    case MS_SSIM:
    case VIFP1:
    case SMALLFRY:
    case SHARPENBAD:
    case COR:
    case NHW:
    case SSIMFRY:
    case SSIMSHBAD:
    case SUMMET:
        return compareFromBuffer(method, imageBuf1, bufSize1, imageBuf2, bufSize2, printPrefix, umscale, inputFiletype1, inputFiletype2);
    default:
        error("unknown comparison method!");
        return 255;
    }

    // Cleanup resources
    free(imageBuf1);
    free(imageBuf2);
}
