/*
    Hash a single JPEG file. The hash is based on tracking gradients
    between pixels in the image. The larger the hash size, the less
    likely you are to get collisions, but the more time it takes to
    calculate.
*/
#include <getopt.h>
#include "jmetrics.h"

void usage(char *progname)
{
    printf("usage: %s [options] image.jpg\n\n", progname);
    printf("options:\n\n");
    printf("  -h, --help                   output program help\n");
    printf("  -s, --size [arg]             set fast comparison image hash size\n");
    printf("  -V, --version                output program version\n");
}

int main (int argc, char **argv)
{
    unsigned char *hash;
    int size = 16;

    const char *optstring = "hs:V";
    static const struct option opts[] =
    {
        { "help", no_argument, 0, 'h' },
        { "size", required_argument, 0, 's' },
        { "version", no_argument, 0, 'V' },
        { 0, 0, 0, 0 }
    };
    int opt, longind = 0;

    char *progname = "jpeg-hash";

    while ((opt = getopt_long(argc, argv, optstring, opts, &longind)) != -1)
    {
        switch (opt)
        {
        case 'h':
            usage(progname);
            return 0;
        case 's':
            size = atoi(optarg);
            break;
        case 'V':
            version();
            return 0;
        };
    }

    if (argc - optind != 1)
    {
        usage(progname);
        return 255;
    }

    // Generate the image hash
    if (jpegHash(argv[optind], &hash, size))
    {
        error("error hashing image!");
        return 1;
    }

    // Print out the hash a string of 1s and 0s
    for (int x = 0; x < size * size; x++)
    {
        printf("%c", hash[x] ? '1' : '0');
    }
    printf("\n");

    // Cleanup
    free(hash);

    return 0;
}
