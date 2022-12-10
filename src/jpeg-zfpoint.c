/*
    Find ZF point of compress a JPEG file while attempting to keep visual quality the same
    by using structural similarity (SSIM) as a metric. Does a binary search
    between JPEG quality 40 and 95 to find the best match. Also makes sure
    that huffman tables are optimized if they weren't already.
*/

#include <getopt.h>
#include "jmetrics.h"

const char *COMMENT = "Find ZF point of compress a JPEG";

void usage(char *progname)
{
    printf("usage: %s [options] input.jpg output.jpg\n\n", progname);
    printf("options:\n\n");
    printf("  -a, --accurate               favor accuracy over speed\n");
    printf("  -c, --no-copy                disable copying files that will not be compressed\n");
    printf("  -d, --defish [arg]           set defish strength [0.0]\n");
    printf("  -f, --force                  force process\n");
    printf("  -h, --help                   output program help\n");
    printf("  -l, --loops [arg]            set the number of runs to attempt [6]\n");
    printf("  -n, --min [arg]              minimum JPEG quality [40]\n");
    printf("  -p, --no-progressive         disable progressive encoding\n");
    printf("  -r, --ppm                    parse input as PPM\n");
    printf("  -s, --strip                  strip metadata\n");
    printf("  -x, --max [arg]              maximum JPEG quality [100]\n");
    printf("  -z, --zoom [arg]             set defish zoom [1.0]\n");
    printf("  -A, --radius [arg]           sharpen radius [2]\n");
    printf("  -Q, --quiet                  only print out errors\n");
    printf("  -S, --subsample [arg]        set subsampling method to one of 'default', 'disable' [default]\n");
    printf("  -T, --input-filetype [arg]   set input file type to one of 'auto', 'jpeg', 'ppm' [auto]\n");
    printf("  -V, --version                output program version\n");
    printf("  -Y, --ycbcr [arg]            YCbCr jpeg colorspace: 0 - source, >0 - YCrCb, <0 - RGB\n");
}

int main (int argc, char **argv)
{
    // Number of binary search steps
    int attempts = 6;

    // Min/max JPEG quality
    int jpegMin = 40;
    int jpegMax = 100;
    int shRadius = 2;

    int force = 0;
    int ycbcr = 0;
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

    unsigned char *buf, *original, *originalGray = NULL, *tmpImage;
    unsigned char *compressed = NULL, *compressedGray, *metaBuf;
    long bufSize = 0, originalSize = 0, originalGraySize = 0;
    long compressedGraySize = 0;
    unsigned long compressedSize = 0, saved;
    int width, height, min, max, attempt, percent, app0_len;
    int jpegcs, jpegcst, quality, progressive, optimize;
    unsigned int metaSize = 0;
    float metric, maxmetric, qmetric, cmpMin, cmpMax, cmpQ;
    char *inputPath, *outputPath;
    FILE *file;

    const char *optstring = "acd:fhl:n:prsx:z:A:S:T:QVY:";
    static const struct option opts[] =
    {
        { "accurate", no_argument, 0, 'a' },
        { "defish", required_argument, 0, 'd' },
        { "force", no_argument, 0, 'f' },
        { "help", no_argument, 0, 'h' },
        { "input-filetype", required_argument, 0, 'T' },
        { "loops", required_argument, 0, 'l' },
        { "min", required_argument, 0, 'n' },
        { "max", required_argument, 0, 'x' },
        { "no-copy", no_argument, 0, 'c' },
        { "no-progressive", no_argument, 0, 'p' },
        { "ppm", no_argument, 0, 'r' },
        { "quiet", no_argument, 0, 'Q' },
        { "radius", required_argument, 0, 'A' },
        { "strip", no_argument, 0, 's' },
        { "subsample", required_argument, 0, 'S' },
        { "version", no_argument, 0, 'V' },
        { "ycbcr", required_argument, 0, 'Y' },
        { "zoom", required_argument, 0, 'z' },
        { 0, 0, 0, 0 }
    };
    int opt, longind = 0;

    char *progname = "jpeg-zfpoint";

    while ((opt = getopt_long(argc, argv, optstring, opts, &longind)) != -1)
    {
        switch (opt)
        {
        case 'a':
            accurate = 1;
            break;
        case 'c':
            copyFiles = 0;
            break;
        case 'd':
            defishStrength = atof(optarg);
            break;
        case 'f':
            force = 1;
            break;
        case 'h':
            usage(progname);
            return 0;
        case 'l':
            attempts = atoi(optarg);
            break;
        case 'n':
            jpegMin = atoi(optarg);
            break;
        case 'p':
            noProgressive = 1;
            break;
        case 'r':
            inputFiletype = FILETYPE_PPM;
            break;
        case 's':
            strip = 1;
            break;
        case 'x':
            jpegMax = atoi(optarg);
            break;
        case 'z':
            defishZoom = atof(optarg);
            break;
        case 'A':
            shRadius = atof(optarg);
            break;
        case 'Q':
            quiet = 1;
            break;
        case 'S':
            subsample = parseSubsampling(optarg);
            break;
        case 'T':
            if (inputFiletype != FILETYPE_AUTO)
            {
                error("multiple file types specified for the input file");
                return 1;
            }
            inputFiletype = parseInputFiletype(optarg);
            break;
        case 'V':
            version();
            return 0;
        case 'Y':
            ycbcr = atoi(optarg);
            break;
        };
    }

    if (argc - optind != 2)
    {
        usage(progname);
        return 255;
    }

    inputPath = argv[optind];
    outputPath = argv[optind + 1];

    /* Read the input into a buffer. */
    bufSize = readFile(inputPath, (void **) &buf);

    /* Detect input file type. */
    if (inputFiletype == FILETYPE_AUTO)
        inputFiletype = detectFiletypeFromBuffer(buf, bufSize);

    /*
     * Read original image and decode. We need the raw buffer contents and its
     * size to obtain meta data and the original file size later.
     */
    originalSize = decodeFileFromBuffer(buf, bufSize, &original, inputFiletype, &width, &height, &jpegcs, JCS_RGB);
    if (!originalSize)
    {
        error("invalid input file: %s", inputPath);
        return 1;
    }

    if (defishStrength)
    {
        info(quiet, "Defishing...\n");
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
        if (getMetadata(buf, bufSize, &metaBuf, &metaSize, COMMENT) && !force)
        {
            if (copyFiles)
            {
                info(quiet, "File already processed by jpeg-zfpoint!\n");
                file = openOutput(outputPath);
                if (file == NULL)
                {
                    error("could not open output file: %s", outputPath);
                    return 1;
                }

                fwrite(buf, bufSize, 1, file);
                fclose(file);

                free(buf);

                return 0;
            }
            else
            {
                error("file already processed by jpeg-zfpoint!");
                free(buf);
                return 2;
            }
        }
    }

    if (strip)
        metaSize = 0;
    else
        info(quiet, "Metadata size is %ukb\n", metaSize / 1024);

    if (!originalSize || !originalGraySize)
        return 1;

    if (ycbcr < 0)
        jpegcs = JCS_RGB;
    if (ycbcr > 0)
        jpegcs = JCS_YCbCr;

    if (jpegMin > jpegMax)
    {
        error("maximum JPEG quality must not be smaller than minimum JPEG quality!");
        return 1;
    }

    // Find ZF point.
    min = jpegMin;
    max = jpegMax;
    compressedSize = encodeJpeg(&compressed, original, width, height, JCS_RGB, max, jpegcs, 0, 1, subsample);
    compressedGraySize = decodeJpeg(compressed, compressedSize, &compressedGray, &width, &height, &jpegcst, JCS_GRAYSCALE);
    maxmetric = metric_corsharp(originalGray, compressedGray, width, height, shRadius);
    maxmetric = cor_sigma(maxmetric);
    qmetric = maxmetric / (float)max;
    cmpMax = 0;
    compressedSize = encodeJpeg(&compressed, original, width, height, JCS_RGB, min, jpegcs, 0, 1, subsample);
    compressedGraySize = decodeJpeg(compressed, compressedSize, &compressedGray, &width, &height, &jpegcst, JCS_GRAYSCALE);
    metric = metric_corsharp(originalGray, compressedGray, width, height, shRadius);
    metric = cor_sigma(metric);
    cmpMin = qmetric * (float)min - metric;
    for (attempt = attempts - 1; attempt >= 0; --attempt)
    {
        quality = min + (max - min) / 2;
        progressive = attempt ? 0 : !noProgressive;
        optimize = accurate ? 1 : (attempt ? 0 : 1);

        // Recompress to a new quality level, without optimizations (for speed)
        compressedSize = encodeJpeg(&compressed, original, width, height, JCS_RGB, quality, jpegcs, progressive, optimize, subsample);

        // Load compressed luma for quality comparison
        compressedGraySize = decodeJpeg(compressed, compressedSize, &compressedGray, &width, &height, &jpegcst, JCS_GRAYSCALE);

        if (!compressedGraySize)
        {
            fprintf(stderr, "Unable to decode file that was just encoded!\n");
            return 1;
        }

        if (!attempt)
        {
            info(quiet, "Final optimized ");
        }

        // Measure quality difference
        metric = metric_corsharp(originalGray, compressedGray, width, height, shRadius);
        metric = cor_sigma(metric);
        cmpQ = qmetric * (float)quality - metric;
        info(quiet, "zfpoint");

        if (attempt)
            info(quiet, " at q=%i (%i - %i): dM %f\n", quality, min, max, cmpQ);
        else
            info(quiet, " at q=%i: dM %f\n", quality, cmpQ);

        if (cmpMin < cmpMax)
        {
            min = MIN(quality + 1, max);
            cmpMin = cmpQ;
        }
        else
        {
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
    percent = (compressedSize + metaSize) * 100 / bufSize;
    saved = (bufSize > (compressedSize + metaSize)) ? (bufSize - compressedSize - metaSize) : 0;
    info(quiet, "New size is %i%% of original (saved %lu kb)\n", percent, saved / 1024);

    if (compressedSize >= bufSize && !force)
    {
        error("output file is larger than input, aborting!");
        return 1;
    }

    // Open output file for writing
    file = openOutput(outputPath);
    if (file == NULL)
    {
        error("could not open output file");
        return 1;
    }

    /* Check that the metadata starts with a SOI marker. */
    if (!checkJpegMagic(compressed, compressedSize))
    {
        error("missing SOI marker, aborting!");
        return 1;
    }

    /* Make sure APP0 is recorded immediately after the SOI marker. */
    if (compressed[2] != 0xff || (compressed[3] != 0xe0 && compressed[3] != 0xee))
    {
        error("missing APP0 marker, aborting!");
        return 1;
    }

    /* Write SOI marker and APP0 metadata to the output file. */
    app0_len = (compressed[4] << 8) + compressed[5];
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
        fwrite(metaBuf, metaSize, 1, file);

    /* Write image data. */
    fwrite(compressed + 4 + app0_len, compressedSize - 4 - app0_len, 1, file);
    fclose(file);

    if (inputFiletype == FILETYPE_JPEG && !strip)
        free(metaBuf);

    free(compressed);
    free(original);
    free(originalGray);

    return 0;
}
