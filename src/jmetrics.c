#include "jmetrics.h"

#define INPUT_BUFFER_SIZE 102400

float clamp(float low, float value, float high)
{
    return (value < low) ? low : ((value > high) ? high : value);
}

float waverage4(float x1, float x2, float x3, float x4)
{
    float xm, dx1, dx2 ,dx3, dx4, dxs, w1, w2, w3, w4, ws;
    xm = x1 + x2 + x3 + x4;
    xm *= 0.25f;
    dx1 = (x1 - xm) * (x1 - xm);
    dx2 = (x2 - xm) * (x2 - xm);
    dx3 = (x3 - xm) * (x3 - xm);
    dx4 = (x4 - xm) * (x4 - xm);
    dxs = dx1 + dx2 + dx3 + dx4;
    dxs *= 0.25f;
    if (dxs > 0.0f)
    {
        w1 = dxs / (dxs + dx1);
        w2 = dxs / (dxs + dx2);
        w3 = dxs / (dxs + dx3);
        w4 = dxs / (dxs + dx4);
        ws = w1 + w2 + w3 + w4;
        if (ws > 0.0f)
            xm = (w1 * x1 + w2 * x2 + w3 * x3 + w4 * x4) / ws;
    }
    return xm;
}

int interpolate(const unsigned char *image, int width, int components, float x, float y, int offset)
{
    int stride = width * components;
    float px = x - floor(x);
    float py = y - floor(y);
    int x1 = floor(x);
    int x2 = ceil(x);
    int y1 = floor(y);
    int y2 = ceil(y);

    float top = (float) image[y1 * stride + x1 * components + offset] * (1.0 - px) +
                (float) image[y1 * stride + x2 * components + offset] * px;
    float bot = (float) image[y2 * stride + x1 * components + offset] * (1.0 - px) +
                (float) image[y2 * stride + x2 * components + offset] * px;

    return (top * (1.0 - py)) + (bot * py);
}

float meanPixelError(const unsigned char *original, const unsigned char *compressed, int width, int height, int components)
{
    float pme = 0.0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            for (int z = 0; z < components; z++)
            {
                int offset = y * width * components + x * components + z;
                pme += abs(original[offset] - compressed[offset]);
            }
        }
    }

    return pme / (width * height * components);
}

void defish(const unsigned char *input, unsigned char *output, int width, int height, int components, float strength, float zoom)
{
    const int cx = width / 2;
    const int cy = height / 2;
    const float len = sqrt(width * width + height * height);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float dx = (cx - x) * zoom;
            float dy = (cy - y) * zoom;
            float r = sqrt(dx * dx + dy * dy) / len * strength;
            float theta = 1.0;

            if (r != 0.0)
            {
                theta = atan(r) / r;
            }

            dx = clamp(0.0, (float) width / 2.0 - theta * dx, width);
            dy = clamp(0.0, (float) height / 2.0 - theta * dy, height);

            for (int z = 0; z < components; z++)
            {
                output[y * width * components + x * components + z] = interpolate(input, width, components, dx, dy, z);
            }
        }
    }
}

long grayscale(const unsigned char *input, unsigned char **output, int width, int height)
{
    int stride = width * 3;

    *output = malloc(width * height);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            // Y = 0.299R + 0.587G + 0.114B
            (*output)[y * width + x] = input[y * stride + x * 3] * 0.299 +
                                       input[y * stride + x * 3 + 1] * 0.587 +
                                       input[y * stride + x * 3 + 2] * 0.114 + 0.5;
        }
    }

    return width * height;
}

void scale(unsigned char *image, int width, int height, unsigned char **newImage, int newWidth, int newHeight)
{
    *newImage = malloc((unsigned long) newWidth * newHeight);

    for (int y = 0; y < newHeight; y++)
    {
        for (int x = 0; x < newWidth; x++)
        {
            int oldY = (float) y / newHeight * height + 0.5;
            int oldX = (float) x / newWidth * width + 0.5;

            (*newImage)[y * newWidth + x] = image[oldY * width + oldX];

        }
    }
}

void genHash(unsigned char *image, int width, int height, unsigned char **hash)
{
    *hash = malloc((unsigned long) width * height);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int pos = y * width + x;

            (*hash)[pos] = image[pos] < image[pos + 1];
        }
    }
}

int jpegHash(const char *filename, unsigned char **hash, int size)
{
    unsigned char *image;
    unsigned long imageSize = 0;
    unsigned char *scaled;
    int width, height;

    imageSize = decodeFile(filename, &image, FILETYPE_JPEG, &width, &height, JCS_GRAYSCALE);

    if (!imageSize)
        return 1;

    scale(image, width, height, &scaled, size, size);
    free(image);
    genHash(scaled, size, size, hash);
    free(scaled);

    return 0;
}

int jpegHashFromBuffer(unsigned char *imageBuf, long bufSize, unsigned char **hash, int size)
{
    unsigned char *image;
    unsigned long imageSize = 0;
    unsigned char *scaled;
    int width, height, jpegcs;

    imageSize = decodeFileFromBuffer(imageBuf, bufSize, &image, FILETYPE_JPEG, &width, &height, &jpegcs, JCS_GRAYSCALE);

    if (!imageSize)
        return 1;

    scale(image, width, height, &scaled, size, size);
    free(image);
    genHash(scaled, size, size, hash);
    free(scaled);

    return 0;
}

unsigned int hammingDist(const unsigned char *hash1, const unsigned char *hash2, int hashLength)
{
    unsigned int dist = 0;

    for (unsigned int x = 0; x < hashLength; x++)
    {
        if (hash1[x] != hash2[x])
        {
            dist++;
        }
    }

    return dist;
}

/* Print program version to stdout. */
void version(void)
{
    printf("%s\n", JMVERSION);
}

/* Print an error message. */
void error(const char *format, ...)
{
    va_list arglist;

    va_start(arglist, format);
    fprintf(stderr, "%s: ", "ERROR");
    vfprintf(stderr, format, arglist);
    fputc('\n', stderr);
    va_end(arglist);
}

long readFile(char *name, void **buffer)
{
    FILE *file;
    size_t fileLen = 0;
    size_t bytesRead = 0;

    unsigned char chunk[INPUT_BUFFER_SIZE];

    // Open file
    if (strcmp("-", name) == 0)
    {
        file = stdin;

#ifdef _WIN32
        // We need to use binary mode on Windows otherwise we get
        // corrupted files. See this issue:
        // https://github.com/danielgtaylor/jpeg-archive/issues/14
        setmode(fileno(stdin), O_BINARY);
#endif
    }
    else
    {
        file = fopen(name, "rb");

        if (!file)
        {
            error("unable to open file: %s", name);
            return 0;
        }
    }

    *buffer = malloc(sizeof chunk);
    while ((bytesRead = fread(chunk, 1, sizeof chunk, file)) > 0)
    {
        unsigned char *reallocated = realloc(*buffer, fileLen + bytesRead);
        if (reallocated)
        {
            *buffer = reallocated;
            memmove((unsigned char *)(*buffer) + fileLen, chunk, bytesRead);
            fileLen += bytesRead;
        }
        else
        {
            error("only able to read %zu bytes!", fileLen);
            free(*buffer);
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return fileLen;
}

int checkJpegMagic(const unsigned char *buf, unsigned long size)
{
    return (size >= 2 && buf[0] == 0xff && buf[1] == 0xd8);
}

unsigned long decodeJpeg(unsigned char *buf, unsigned long bufSize, unsigned char **image, int *width, int *height, int *jpegcs, int pixelFormat)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    int row_stride;
    JSAMPARRAY buffer;

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_decompress(&cinfo);

    // Set the source
    jpeg_mem_src(&cinfo, buf, bufSize);

    // Read header and set custom parameters
    jpeg_read_header(&cinfo, TRUE);

    cinfo.out_color_space = pixelFormat;

    // Start decompression
    jpeg_start_decompress(&cinfo);

    *width = cinfo.output_width;
    *height = cinfo.output_height;
    *jpegcs = cinfo.jpeg_color_space;

    // Allocate temporary row
    row_stride = (*width) * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)
             ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    // Allocate image pixel buffer
    *image = malloc(row_stride * (*height));

    // Read image row by row
    int row = 0;
    while (cinfo.output_scanline < cinfo.output_height)
    {
        (void) jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy((void *)((*image) + row_stride * row), buffer[0], row_stride);
        row++;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return row_stride * (*height);
}

unsigned long encodeJpeg(unsigned char **jpeg, unsigned char *buf, int width, int height, int pixelFormat, int quality, int jpegcs, int progressive, int optimize, int subsample)
{
    long unsigned int jpegSize = 0;
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride = width * (pixelFormat == JCS_RGB ? 3 : 1);

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);

    // Set destination
    jpeg_mem_dest(&cinfo, jpeg, &jpegSize);

    // Set options
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = pixelFormat == JCS_RGB ? 3 : 1;
    cinfo.in_color_space = pixelFormat;

    /*
    // Mozjpeg:
        if (!optimize)
        {
            // Not optimizing for space, so use a much faster compression
            // profile. This is about twice as fast and can be used when
            // testing visual quality *before* doing the final encoding.
            // Note: This *must* be set before calling `jpeg_set_defaults`
            // as it modifies how that call works.
            if (jpeg_c_int_param_supported(&cinfo, JINT_COMPRESS_PROFILE))
                jpeg_c_set_int_param(&cinfo, JINT_COMPRESS_PROFILE, JCP_FASTEST);
        }
    */

    jpeg_set_defaults(&cinfo);

    /*
    // Mozjpeg:
        if (!optimize)
        {
            // Disable trellis quantization if we aren't optimizing. This saves
            // a little processing.
            if (jpeg_c_bool_param_supported(&cinfo, JBOOLEAN_TRELLIS_QUANT))
                jpeg_c_set_bool_param(&cinfo, JBOOLEAN_TRELLIS_QUANT, FALSE);
            if (jpeg_c_bool_param_supported(&cinfo, JBOOLEAN_TRELLIS_QUANT_DC))
                jpeg_c_set_bool_param(&cinfo, JBOOLEAN_TRELLIS_QUANT_DC, FALSE);
        }
    */

    if (optimize)
        cinfo.optimize_coding = TRUE;

    if (optimize && !progressive)
    {
        cinfo.scan_info = NULL;
        cinfo.num_scans = 0;
        /*
        // Mozjpeg:
                // Moz defaults, disable progressive
                if (jpeg_c_bool_param_supported(&cinfo, JBOOLEAN_OPTIMIZE_SCANS))
                    jpeg_c_set_bool_param(&cinfo, JBOOLEAN_OPTIMIZE_SCANS, FALSE);
        */
    }

    if (!optimize && progressive)
    {
        // No moz defaults, set scan progression
        jpeg_simple_progression(&cinfo);
    }

    if (subsample == SUBSAMPLE_444)
    {
        cinfo.comp_info[0].h_samp_factor = 1;
        cinfo.comp_info[0].v_samp_factor = 1;
        cinfo.comp_info[1].h_samp_factor = 1;
        cinfo.comp_info[1].v_samp_factor = 1;
        cinfo.comp_info[2].h_samp_factor = 1;
        cinfo.comp_info[2].v_samp_factor = 1;
    }

    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_set_colorspace (&cinfo, jpegcs);

    // Start the compression
    jpeg_start_compress(&cinfo, TRUE);

    // Process scanlines one by one
    while (cinfo.next_scanline < cinfo.image_height)
    {
        row_pointer[0] = &buf[cinfo.next_scanline * row_stride];
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return jpegSize;
}

int checkPpmMagic(const unsigned char *buf, unsigned long size)
{
    return (size >= 2 && buf[0] == 'P' && buf[1] == '6');
}

unsigned long decodePpm(unsigned char *buf, unsigned long bufSize, unsigned char **image, int *width, int *height)
{
    unsigned long pos = 0, imageDataSize;
    int depth;

    if (!checkPpmMagic(buf, bufSize))
    {
        error("not a valid PPM format image!");
        return 0;
    }

    // Read to first newline
    while (buf[pos++] != '\n' && pos < bufSize);

    // Discard for any comment and empty lines
    while ((buf[pos] == '#' || buf[pos] == '\n') && pos < bufSize)
    {
        while (buf[pos] != '\n')
        {
            pos++;
        }
        pos++;
    }

    if (pos >= bufSize)
    {
        error("not a valid PPM format image!");
        return 0;
    }

    // Read width/height
    sscanf((const char *) buf + pos, "%d %d", width, height);

    // Go to next line
    while (buf[pos++] != '\n' && pos < bufSize);

    if (pos >= bufSize)
    {
        error("not a valid PPM format image!");
        return 0;
    }

    // Read bit depth
    sscanf((const char*) buf + pos, "%d", &depth);

    if (depth != 255)
    {
        error("unsupported bit depth: %d", depth);
        return 0;
    }

    // Go to next line
    while (buf[pos++] != '\n' && pos < bufSize);

    // Width * height * red/green/blue
    imageDataSize = (*width) * (*height) * 3;
    if (pos + imageDataSize != bufSize)
    {
        error("incorrect image size: %lu vs. %lu", bufSize, pos + imageDataSize);
        return 0;
    }

    // Allocate image pixel buffer
    *image = malloc(imageDataSize);

    // Copy pixel data
    memcpy((void *) *image, (void *) buf + pos, imageDataSize);

    return (*width) * (*height);
}

enum filetype detectFiletype(const char *filename)
{
    unsigned char *buf = NULL;
    long bufSize = 0;
    bufSize = readFile((char *)filename, (void **)&buf);
    enum filetype ret = detectFiletypeFromBuffer(buf, bufSize);
    free(buf);
    return ret;
}

enum filetype detectFiletypeFromBuffer(unsigned char *buf, long bufSize)
{
    if (checkJpegMagic(buf, bufSize))
        return FILETYPE_JPEG;

    if (checkPpmMagic(buf, bufSize))
        return FILETYPE_PPM;

    return FILETYPE_UNKNOWN;
}

unsigned long decodeFile(const char *filename, unsigned char **image, enum filetype type, int *width, int *height, int pixelFormat)
{
    unsigned char *buf = NULL;
    int jpegcs;
    long bufSize = 0;
    bufSize = readFile((char *)filename, (void **)&buf);
    unsigned long ret = decodeFileFromBuffer(buf, bufSize, image, type, width, height, &jpegcs, pixelFormat);
    free(buf);
    return ret;
}

unsigned long decodeFileFromBuffer(unsigned char *buf, long bufSize, unsigned char **image, enum filetype type, int *width, int *height, int *jpegcs, int pixelFormat)
{
    switch (type)
    {
    case FILETYPE_PPM:
        *jpegcs = JCS_RGB;
        return decodePpm(buf, bufSize, image, width, height);
    case FILETYPE_JPEG:
        return decodeJpeg(buf, bufSize, image, width, height, jpegcs, pixelFormat);
    default:
        return 0;
    }
}

int getMetadata(const unsigned char *buf, unsigned int bufSize, unsigned char **meta, unsigned int *metaSize, const char *comment)
{
    unsigned int pos = 0;
    unsigned int totalSize = 0;
    unsigned int offsets[20];
    unsigned int sizes[20];
    unsigned int count = 0;

    // Read through all the file markers
    while (pos < bufSize && count < 20)
    {
        unsigned int marker = (buf[pos] << 8) + buf[pos + 1];

        //printf("Marker %x at %u\n", marker, pos);

        if (marker == 0xffda /* SOS */)
        {
            // This is the end, so stop!
            break;
        }
        else if (marker == 0xffdd /* DRI */)
        {
            pos += 2 + 4;
        }
        else if (marker >= 0xffd0 && marker <= 0xffd9 /* RST0+x */)
        {
            pos += 2;
        }
        else
        {
            // Marker has a custom size, read it in
            int size = (buf[pos + 2] << 8) + buf[pos + 3];
            //printf("Size is %i (%x)\n", size, size);

            // Save APP0+x and COM markers
            if ((marker >= 0xffe1 && marker <= 0xffef) || marker == 0xfffe)
            {
                if (marker == 0xfffe && comment != NULL && !strncmp(comment, (char *) buf + pos + 4, strlen(comment)))
                {
                    return 1;
                }

                totalSize += size + 2;
                offsets[count] = pos;
                sizes[count] = size + 2;
                count++;
            }

            pos += 2 + size;
        }
    }

    // Allocate the metadata buffer
    *meta = malloc(totalSize);
    *metaSize = totalSize;

    // Copy over all the metadata into the new buffer
    pos = 0;
    for (int x = 0; x < count; x++)
    {
        memcpy(((*meta) + pos), (buf + offsets[x]), sizes[x]);
        pos += sizes[x];
    }

    return 0;
}

// Open a file for writing
FILE *openOutput(char *name)
{
    if (strcmp("-", name) == 0)
    {
#ifdef _WIN32
        setmode(fileno(stdout), O_BINARY);
#endif

        return stdout;
    }
    else
    {
        return fopen(name, "wb");
    }
}

// Logs an informational message, taking quiet mode into account
void info(int quiet, const char *format, ...)
{
    va_list argptr;

    if (!quiet)
    {
        va_start(argptr, format);
        vfprintf(stderr, format, argptr);
        va_end(argptr);
    }
}

enum filetype parseInputFiletype(const char *s)
{
    if (!strcmp("auto", s))
        return FILETYPE_AUTO;
    if (!strcmp("jpeg", s))
        return FILETYPE_JPEG;
    if (!strcmp("ppm", s))
        return FILETYPE_PPM;
    return FILETYPE_UNKNOWN;
}

int parseSubsampling(const char *s)
{
    if (!strcmp("default", s))
        return SUBSAMPLE_DEFAULT;
    else if (!strcmp("disable", s))
        return SUBSAMPLE_444;

    error("unknown sampling method: %s", s);
    return SUBSAMPLE_DEFAULT;
}

enum QUALITY_PRESET parseQuality(const char *s)
{
    if (!strcmp("low", s))
        return LOW;
    else if (!strcmp("medium", s))
        return MEDIUM;
    else if (!strcmp("high", s))
        return HIGH;
    else if (!strcmp("veryhigh", s))
        return VERYHIGH;

    error("unknown quality preset: %s", s);
    return MEDIUM;
}

float setTargetFromPreset(int preset)
{
    float target = 0.76f;
    switch (preset)
    {
    case LOW:
        target = 0.5f;
        break;
    case MEDIUM:
        target = 0.76f;
        break;
    case HIGH:
        target = 0.93f;
        break;
    case VERYHIGH:
        target = 0.99f;
        break;
    }
    return target;
}

enum METHOD parseMethod(const char *s)
{
    if (!strcmp("fast", s))
        return FAST;
    if (!strcmp("mpe", s))
        return MPE;
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
    if (!strcmp("ssimfry", s))
        return SSIMFRY;
    if (!strcmp("ssimshb", s))
        return SSIMSHBAD;
    if (!strcmp("sum", s))
        return SUMMET;
    if (!strcmp("cor", s))
        return COR;
    if (!strcmp("corsh", s))
        return CORSHARP;
    return UNKNOWN;
}

float RescaleMetric(int currentmethod, float value)
{
    switch (currentmethod)
    {
    case PSNR:
        value = sqrt(value);
        value *= 0.9f;
        value -= 4.8f;
        break;
    case MPE:
        value = -sqrt(value);
        value *= 1.15f;
        value += 1.9f;
        break;
    case SSIM:
        value = cor_sigma(value);
        value = cor_sigma(value);
        value = cor_sigma(value);
        value *= 1.8f;
        break;
    case MS_SSIM:
        value = cor_sigma(value);
        value = cor_sigma(value);
        value *= 1.8f;
        value += 0.3f;
        break;
    case SMALLFRY:
        value *= 0.105f;
        value -= 10.0f;
        break;
    case COR:
        value = cor_sigma(value);
        value = cor_sigma(value);
        value *= 3.0f;
        value -= 1.5f;
        break;
    case CORSHARP:
        value = cor_sigma(value);
        value = cor_sigma(value);
        value *= 2.25f;
        value -= 0.75f;
        break;
    }
    return value;
}

char* MetricName(int currentmethod)
{
    char *value = "UNKNOW";
    switch (currentmethod)
    {
    case FAST:
        value = "FAST";
        break;
    case PSNR:
        value = "PSNR";
        break;
    case MPE:
        value = "MPE";
        break;
    case SSIM:
        value = "SSIM";
        break;
    case MS_SSIM:
        value = "MS-SSIM";
        break;
    case SMALLFRY:
        value = "SMALLFRY";
        break;
    case COR:
        value = "COR";
        break;
    case CORSHARP:
        value = "CORSHARP";
        break;
    case SHARPENBAD:
        value = "SHARPENBAD";
        break;
    case SSIMFRY:
        value = "SSIMFRY";
        break;
    case SSIMSHBAD:
        value = "SSIMSHBAD";
        break;
    case SUMMET:
        value = "SUM";
        break;
    }
    return value;
}

float MetricCalc(int method, unsigned char *image1, unsigned char *image2, int width, int height, int components, int radius)
{
    float diff, tmetric, tm1, tm2, tm3, tm4;

    // Calculate and print comparison
    switch (method)
    {
    case PSNR:
        diff = iqa_psnr(image1, image2, width, height, width * components);
        break;
    case MPE:
        diff = meanPixelError(image1, image2, width, height, 1);
        break;
    case SMALLFRY:
        diff = metric_smallfry(image1, image2, width, height);
        break;
    case SHARPENBAD:
        diff = metric_sharpenbad(image1, image2, width, height);
        break;
    case COR:
        diff = metric_cor(image1, image2, width, height);
        break;
    case CORSHARP:
        diff = metric_corsharp(image1, image2, width, height, radius);
        break;
    case MS_SSIM:
        diff = iqa_ms_ssim(image1, image2, width, height, width * components, 0);
        break;
    case SSIM:
        diff = iqa_ssim(image1, image2, width, height, width * components, 0, 0);
        break;
    case SSIMFRY:
        tmetric = iqa_ssim(image1, image2, width, height, width * components, 0, 0);
        diff = RescaleMetric(SSIM, tmetric);
        tmetric = metric_smallfry(image1, image2, width, height);
        diff += RescaleMetric(SMALLFRY, tmetric);
        diff *= 0.5f;
        break;
    case SSIMSHBAD:
        tmetric = iqa_ssim(image1, image2, width, height, width * components, 0, 0);
        diff = RescaleMetric(SSIM, tmetric);
        tmetric = metric_sharpenbad(image1, image2, width, height);
        diff += RescaleMetric(SHARPENBAD, tmetric);
        diff *= 0.5f;
        break;
    case SUMMET:
    default:
        tmetric = iqa_ssim(image1, image2, width, height, width * components, 0, 0);
        tm1 = RescaleMetric(SSIM, tmetric);
        tmetric = metric_smallfry(image1, image2, width, height);
        tm2 = RescaleMetric(SMALLFRY, tmetric);
        tmetric = metric_sharpenbad(image1, image2, width, height);
        tm3 = RescaleMetric(SHARPENBAD, tmetric);
        tmetric = metric_corsharp(image1, image2, width, height, radius);
        tm4 = RescaleMetric(CORSHARP, tmetric);
        diff = waverage4(tm1, tm2, tm3, tm4);
        break;
    }
    return diff;
}

int compareFastFromBuffer(unsigned char *imageBuf1, long bufSize1, unsigned char *imageBuf2, long bufSize2, int printPrefix, int size)
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
    if (printPrefix)
        printf("%s: ", MetricName(FAST));
    printf("%u\n", hammingDist(hash1, hash2, size * size) * 100 / (size * size));

    // Cleanup
    free(hash1);
    free(hash2);

    return 0;
}

int compareFromBuffer(int method, unsigned char *imageBuf1, long bufSize1, unsigned char *imageBuf2, long bufSize2, int printPrefix, int umscale, int radius, enum filetype inputFiletype1, enum filetype inputFiletype2)
{
    unsigned char *image1, *image2, *image1Gray = NULL, *image2Gray = NULL;
    int width1, width2, height1, height2;
    int format, components, jpegcs;
    float diff;

    // Set requested pixel format
    switch (method)
    {
    case PSNR:
        format = JCS_RGB;
        components = 3;
        break;
    case SSIM:
    case MS_SSIM:
    case SMALLFRY:
    case SHARPENBAD:
    case COR:
    case CORSHARP:
    default:
        format = JCS_GRAYSCALE;
        components = 1;
        break;
    }

    // Decode files
    if (!decodeFileFromBuffer(imageBuf1, bufSize1, &image1, inputFiletype1, &width1, &height1, &jpegcs, format))
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

    if (!decodeFileFromBuffer(imageBuf2, bufSize2, &image2, inputFiletype2, &width2, &height2, &jpegcs, format))
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
    diff = MetricCalc(method, image1, image2, width1, height1, components, radius);
    if (umscale)
        diff = RescaleMetric(method, diff);
    if (printPrefix)
        printf("%s: ", MetricName(method));
    printf("%f", diff);
    if (umscale)
        printf(" (UM)\n");
    else
        printf("\n");

    // Cleanup
    free(image1);
    free(image2);

    return 0;
}
