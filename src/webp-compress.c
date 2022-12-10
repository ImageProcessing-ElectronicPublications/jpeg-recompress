/*
    Recompress a JPEG file while attempting to keep visual quality the same
    by using structural similarity (SSIM) as a metric. Does a binary search
    between JPEG quality 40 and 95 to find the best match. Also makes sure
    that huffman tables are optimized if they weren't already.
*/

#include <webp/encode.h>
#include <webp/decode.h>
#include <getopt.h>
#include "jmetrics.h"

const char *COMMENT = "Compressed by webp-compress";

void usage(char *progname)
{
    printf("usage: %s [options] input.[jpg|ppm] output.webp\n\n", progname);
    printf("options:\n\n");
    printf("  -c, --no-copy                disable copying files that will not be compressed\n");
    printf("  -d, --defish [arg]           set defish strength [0.0]\n");
    printf("  -f, --force                  force process\n");
    printf("  -h, --help                   output program help\n");
    printf("  -l, --loops [arg]            set the number of runs to attempt [8]\n");
    printf("  -m, --method [arg]           set comparison method to one of:\n");
    printf("                               'mpe', 'psnr', 'ssim', 'ms-ssim', 'smallfry', 'ssimfry',\n");
    printf("                               'shbad', 'nhw', 'ssimshb', 'cor', 'corsh', 'sum' [sum]\n");
    printf("  -n, --min [arg]              minimum quality [1]\n");
    printf("  -q, --quality [arg]          set a quality preset: low, medium, high, veryhigh [medium]\n");
    printf("  -r, --ppm                    parse input as PPM\n");
    printf("  -s, --strip                  strip metadata\n");
    printf("  -t, --target [arg]           set target quality [0.76]\n");
    printf("  -x, --max [arg]              maximum quality [99]\n");
    printf("  -z, --zoom [arg]             set defish zoom [1.0]\n");
    printf("  -Q, --quiet                  only print out errors\n");
    printf("  -T, --input-filetype [arg]   set input file type to one of 'auto', 'jpeg', 'ppm' [auto]\n");
    printf("  -V, --version                output program version\n");
}

int main (int argc, char **argv)
{
    int method = SUMMET;

    // Number of binary search steps
    int attempts = 8;

    float target = 0.0f;
    int preset = MEDIUM;

    // Min/max JPEG quality
    int qMin = 1;
    int qMax = 99;

    int force = 0;

    // Defish the image?
    float defishStrength = 0.0f;
    float defishZoom = 1.0f;

    // Input format
    enum filetype inputFiletype = FILETYPE_AUTO;

    // Whether to copy files that cannot be compressed
    int copyFiles = 1;

    // Quiet mode (less output)
    int quiet = 0;

    unsigned char *buf, *original, *originalGray = NULL, *tmpImage;
    unsigned char *compressed = NULL, *compressedGray;
    long bufSize = 0, originalSize = 0, originalGraySize = 0;
    long compressedGraySize = 0;
    unsigned long compressedSize = 0, saved;
    uint8_t *decodedImage = NULL;
    int width, height, min, max, attempt, quality;
    int jpegcs, rgb_stride, err, ok, percent, wSize;
    float metric, umetric;
    char *inputPath, *outputPath;
    FILE *file;
    WebPConfig config;
    WebPPicture pic;
    WebPMemoryWriter wrt;
    WebPMemoryWriterInit(&wrt);

    const char *optstring = "cd:fhl:m:n:q:rt:x:z:QT:V";
    static const struct option opts[] =
    {
        { "defish", required_argument, 0, 'd' },
        { "force", no_argument, 0, 'f' },
        { "help", no_argument, 0, 'h' },
        { "input-filetype", required_argument, 0, 'T' },
        { "loops", required_argument, 0, 'l' },
        { "max", required_argument, 0, 'x' },
        { "method", required_argument, 0, 'm' },
        { "min", required_argument, 0, 'n' },
        { "no-copy", no_argument, 0, 'c' },
        { "no-progressive", no_argument, 0, 'p' },
        { "ppm", no_argument, 0, 'r' },
        { "quality", required_argument, 0, 'q' },
        { "quiet", no_argument, 0, 'Q' },
        { "target", required_argument, 0, 't' },
        { "version", no_argument, 0, 'V' },
        { "zoom", required_argument, 0, 'z' },
        { 0, 0, 0, 0 }
    };
    int opt, longind = 0;

    char *progname = "webp-compress";

    while ((opt = getopt_long(argc, argv, optstring, opts, &longind)) != -1)
    {
        switch (opt)
        {
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
        case 'm':
            method = parseMethod(optarg);
            break;
        case 'n':
            qMin = atoi(optarg);
            break;
        case 'q':
            preset = parseQuality(optarg);
            break;
        case 'r':
            inputFiletype = FILETYPE_PPM;
            break;
        case 't':
            target = atof(optarg);
            break;
        case 'x':
            qMax = atoi(optarg);
            break;
        case 'z':
            defishZoom = atof(optarg);
            break;
        case 'T':
            if (inputFiletype != FILETYPE_AUTO)
            {
                error("multiple file types specified for the input file");
                return 1;
            }
            inputFiletype = parseInputFiletype(optarg);
            break;
        case 'Q':
            quiet = 1;
            break;
        case 'V':
            version();
            return 0;
        };
    }

    if (argc - optind != 2)
    {
        usage(progname);
        return 255;
    }

    inputPath = argv[optind];
    outputPath = argv[optind + 1];

    if (method == UNKNOWN)
    {
        error("invalid method!");
        usage(progname);
        return 255;
    }

    // No target passed, use preset!
    if (target < 0.001f)
    {
        target = setTargetFromPreset(preset);
    }

    // if (!WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, 50)) {
    if (!WebPConfigPreset(&config, WEBP_PRESET_PHOTO, 50)) {
        error("could not initialize WebP configuration");
        return 1;
    }
    if (!WebPPictureInit(&pic)) {
        error("could not initialize WebP picture");
        return 1;
    }
    pic.writer = WebPMemoryWrite;
    pic.custom_ptr = (void*)&wrt;

    /* Read the input into a buffer. */
    bufSize = readFile(inputPath, (void **) &buf);
    if (!bufSize)
    {
        WebPMemoryWriterClear(&wrt);
        return 1;
    }

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
        WebPMemoryWriterClear(&wrt);
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

    // WebP image dimensions
    pic.width = width;
    pic.height = height;
    rgb_stride = width * 3;

    // Convert RGB input into Y
    originalGraySize = grayscale(original, &originalGray, width, height);

    if (!originalSize || !originalGraySize)
    {
        error("could not create the original grayscale image");
        WebPMemoryWriterClear(&wrt);
        WebPPictureFree(&pic);
    }

    if (qMin > qMax)
    {
        error("maximum JPEG quality must not be smaller than minimum quality!");
        return 1;
    }

    min = qMin;
    max = qMax;
    for (attempt = attempts - 1; attempt >= 0; --attempt)
    {
         quality = min + (max - min) / 2;

        /* Terminate early once bisection interval is a singleton. */
        if (min == max)
            attempt = 0;

        WebPMemoryWriterClear(&wrt);

        ok = WebPPictureImportRGB(&pic, original, rgb_stride);
        if (!ok) {
            error("could not import RGB image to WebP");
            WebPMemoryWriterClear(&wrt);
            free(original);
            free(originalGray);
            return 1;
        }

        // Recompress to a new quality level
        config.quality = (float)quality;
        ok = WebPEncode(&config, &pic);
        if (!ok)
        {
            error("could not encode image to WebP");
            WebPMemoryWriterClear(&wrt);
            WebPPictureFree(&pic); // must be called independently of the 'ok' result
            free(original);
            free(originalGray);
            return 1;
        }
        compressedSize = wrt.size;

        // Decode the just encoded buffer
        decodedImage = WebPDecodeRGB(wrt.mem, wrt.size, &width, &height);
        if (decodedImage == NULL) {
            error("unable to decode buffer that was just encoded!");
            WebPMemoryWriterClear(&wrt);
            WebPPictureFree(&pic);
            free(originalGray);
            return 1;
        }

        // Convert RGB input into Y
        compressedGraySize = grayscale(decodedImage, &compressedGray, width, height);

        // Free the decoded RGB image
        WebPFree(decodedImage);

        if (!compressedGraySize)
        {
            error("unable to decode file that was just encoded!");
            WebPMemoryWriterClear(&wrt);
            WebPPictureFree(&pic);
            free(originalGray);
            return 1;
        }

        if (!attempt)
            info(quiet, "Final optimized ");

        // Measure quality difference
        metric = MetricCalc(method, originalGray, compressedGray, width, height, 1, 2);
        umetric = RescaleMetric(method, metric);
        info(quiet, MetricName(method));

        if (attempt)
            info(quiet, " at q=%i (%i - %i): UM %f\n", quality, min, max, umetric);
        else
            info(quiet, " at q=%i: UM %f\n", quality, umetric);

        if (umetric < target)
        {
            if (compressedSize >= bufSize)
            {
                free(compressed);
                free(compressedGray);

                if (copyFiles)
                {
                    info(quiet, "Output file would be larger than input!\n");
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
                    error("output file would be larger than input!");
                    free(buf);
                    return 1;
                }
            }
            min = MIN(quality + 1, max);
        }
        else
        {
            max = MAX(quality - 1, min);
        }

        // If we aren't done yet, then free the image data
        if (attempt)
        {
            WebPPictureFree(&pic);
        }
    }

    free(buf);

    // Calculate and show savings, if any
    percent = compressedSize * 100 / bufSize;
    saved = (bufSize > compressedSize) ? (bufSize - compressedSize) : 0;
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
        WebPMemoryWriterClear(&wrt);
        return 1;
    }

    /* Write image data. */
    wSize = fwrite(wrt.mem, wrt.size, 1, file);

    WebPMemoryWriterClear(&wrt);

    if (wSize != 1) {
        fclose(file);
        error("could not write to output file: %s", outputPath);

        return 1;
    }

    err = fclose(file);
    if (err) {
        error("could not close the output file: %s", outputPath);
        return 1;
    }

    free(original);
    free(originalGray);

    return 0;
}
