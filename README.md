![GitHub release (latest by date)](https://img.shields.io/github/v/release/ImageProcessing-ElectronicPublications/jpeg-recompress)
![GitHub Release Date](https://img.shields.io/github/release-date/ImageProcessing-ElectronicPublications/jpeg-recompress)
![GitHub repo size](https://img.shields.io/github/repo-size/ImageProcessing-ElectronicPublications/jpeg-recompress)
![GitHub all releases](https://img.shields.io/github/downloads/ImageProcessing-ElectronicPublications/jpeg-recompress/total)
![GitHub](https://img.shields.io/github/license/ImageProcessing-ElectronicPublications/jpeg-recompress)

# JPEG Recompress

Utilities for archiving photos for saving to long term storage or serving over the web. The goals are:

 * Use a common, well supported format (JPEG)
 * Minimize storage space and cost
 * Identify duplicates / similar photos

Approach:

 * Command line utilities and scripts
 * Simple options and useful help
 * Good quality output via sane defaults

Contributions to this project are very welcome.

## Download

You can download the latest source and binary releases from the [JPEG Recompress releases page](https://github.com/ImageProcessing-ElectronicPublications/jpeg-recompress/releases).

## Utilities

The following utilities are part of this project. All of them accept a `--help` parameter to see the available options.

### jpeg-recompress

Compress JPEGs by re-encoding to the smallest JPEG quality while keeping _perceived_ visual quality the same and by making sure huffman tables are optimized. This is a __lossy__ operation, but the images are visually identical and it usually saves 30-70% of the size for JPEGs coming from a digital camera, particularly DSLRs. By default all EXIF/IPTC/XMP and color profile metadata is copied over, but this can be disabled to save more space if desired.

There is no need for the input file to be a JPEG. In fact, you can use `jpeg-recompress` as a replacement for `cjpeg` by using PPM input and the `--ppm` option.

The better the quality of the input image is, the better the output will be.

Some basic photo-related editing options are available, such as removing fisheye lens distortion.

#### Demo

Below are two 100% crops of [Nikon's D3x Sample Image 2](http://static.nikonusa.com/D3X_gallery/index.html). The left shows the original image from the camera, while the others show the output of `jpeg-recompress` with the `medium` quality setting and various comparison methods. By default SSIM is used, which lowers the file size by **88%**. The recompression algorithm chooses a JPEG quality of 80. By comparison the `veryhigh` quality setting chooses a JPEG quality of 93 and saves 70% of the file size.

![JPEG recompression comparison](https://cloud.githubusercontent.com/assets/106826/3633843/5fde26b6-0eff-11e4-8c98-f18dbbf7b510.png)

Why are they different sizes? The default quality settings are set to average out to similar visual quality over large data sets. They may differ on individual photos (like above) because each metric considers different parts of the image to be more or less important for compression.

#### Image Comparison Metrics

The following metrics are available when using `jpeg-recompress`. SSIM is the default.

Name        | Option        | Description
----------- | ------------- | -----------
MPE         | `-m mpe`      | Mean pixel error (as used by [imgmin](https://github.com/rflynn/imgmin))
PSNR        | `-m psnr`     | [Peak signal-to-noise ratio](https://en.wikipedia.org/wiki/Peak_signal-to-noise_ratio)
SSIM        | `-m ssim`     | [Structural similarity](http://en.wikipedia.org/wiki/Structural_similarity)
MS-SSIM*    | `-m ms-ssim`  | Multi-scale structural similarity (slow!) ([2008 paper](http://foulard.ece.cornell.edu/publications/dmr_hvei2008_paper.pdf))
SmallFry    | `-m smallfry` | Linear-weighted BBCQ-like ([original project](https://github.com/dwbuiten/smallfry), [2011 BBCQ paper](http://spie.org/Publications/Proceedings/Paper/10.1117/12.872231) -> [LibSmallFry](https://github.com/ImageProcessing-ElectronicPublications/libsmallfry))
SharpenBad  | `-m shbad`    | Sharpen discrepancies ([LibSmallFry](https://github.com/ImageProcessing-ElectronicPublications/libsmallfry))
Correlation | `-m cor`      | Correlation ([LibSmallFry](https://github.com/ImageProcessing-ElectronicPublications/libsmallfry))
CorSharpen  | `-m corsh`    | Correlation sharpen ([LibSmallFry](https://github.com/ImageProcessing-ElectronicPublications/libsmallfry))
NHW         | `-m nhw`      | NHW convolutional metric ([original project](https://github.com/rcanut/NHW_Neatness_Metrics) -> [LibSmallFry](https://github.com/ImageProcessing-ElectronicPublications/libsmallfry))
1 pair      | `-m ssimfry`  | `(ssim + smallfry) / 2`
2 pair      | `-m ssimshb`  | `(ssim + shbad) / 2`
SUMMARY     | `-m sum`      | `(ssim + smallfry + shbad + nhw) / 4` **DEFAULT**

**Note**: The SmallFry algorithm may be [patented](http://www.jpegmini.com/main/technology) so use with caution.

"Universal Scale" of metrics (UM):
```
0.0 ... (DIRTY) ... 0.5 ... (LOW) ... 0.75 ... (MEDIUM) ... 0.875 ... (SUBHIGH) ... 0.9375 ... (HIGH) ... 0.96875 ... (VERYHIGH) ... 1.0
```
Trends:
```
UM = 1.16*sqrt(PNSR)-6.52
UM = -0.92*sqrt(MPE)+1.81
UM = 2.25*cor_sigma(cor_sigma(cor_sigma(SSIM)))-0.2
UM = 1.74*cor_sigma(cor_sigma(MS_SSIM))+0.1
UM = 0.0755*SMALLFRY-7.0
UM = 2.37*sqrt(sqrt(sqrt(1.0/NHW)))-1.06
UM = 3.0*cor_sigma(cor_sigma(COR))-1.5
UM = 2.25*cor_sigma(cor_sigma(CORSH))-0.75
UM = 1.45*SHARPENBAD-0.15

cor_sigma(M) = 1.0-sqrt(1.0-M*M)
```

#### Subsampling

The JPEG format allows for subsampling of the color channels to save space. For each 2x2 block of pixels per color channel (four pixels total) it can store four pixels (all of them), two pixels or a single pixel. By default, the JPEG encoder subsamples the non-luma channels to two pixels (often referred to as 4:2:0 subsampling). Most digital cameras do the same because of limitations in the human eye. This may lead to unintended behavior for specific use cases (see #12 for an example), so you can use `--subsample disable` to disable this subsampling.

#### Example Commands

```bash
# Default settings
jpeg-recompress image.jpg compressed.jpg

# High quality example settings
jpeg-recompress --quality high --min 60 image.jpg compressed.jpg

# Slow high quality settings (3-4x slower than above, slightly more accurate)
jpeg-recompress --accurate --quality high --min 60 image.jpg compressed.jpg

# Use SmallFry instead of SSIM
jpeg-recompress --method smallfry image.jpg compressed.jpg

# Use 4:4:4 sampling (disables subsampling).
jpeg-recompress --subsample disable image.jpg compressed.jpg

# Remove fisheye distortion (Tokina 10-17mm on APS-C @ 10mm)
jpeg-recompress --defish 2.6 --zoom 1.2 image.jpg defished.jpg

# Read from stdin and write to stdout with '-' as the filename
jpeg-recompress - - <image.jpg >compressed.jpg

# Convert RAW to JPEG via PPM from stdin
dcraw -w -q 3 -c IMG_1234.CR2 | jpeg-recompress --ppm - compressed.jpg

# Disable progressive mode (not recommended)
jpeg-recompress --no-progressive image.jpg compressed.jpg

# Disable all output except for errors
jpeg-recompress --quiet image.jpg compressed.jpg
```

### jpeg-compare

Compare two JPEG photos to judge how similar they are. The `fast` comparison method returns an integer from 0 to 99, where 0 is identical. PSNR, SSIM, and MS-SSIM return floats but require images to be the same dimensions.

```bash
# Do a fast compare of two images
jpeg-compare image1.jpg image2.jpg

# Calculate PSNR
jpeg-compare --method psnr image1.jpg image2.jpg

# Calculate SSIM
jpeg-compare --method ssim image1.jpg image2.jpg
```

### jpeg-hash

Create a hash of an image that can be used to compare it to other images quickly.

```bash
jpeg-hash image.jpg
```

### jpeg-zfpoint

Compress JPEG files by re-encoding them to the lowest JPEG quality using the peculiarity jpeg (zero point) quantization feature.

### webp-compress

Compress JPEGs by re-encoding to the smallest WEBP quality while keeping perceived visual quality the same.

This is a lossy operation, but the images are visually identical and it usually saves >50% of the size for JPEGs coming from a digital camera, particularly DSLRs.

All EXIF/IPTC/XMP and color profile metadata are not preserved!

Some basic photo-related editing options are available, such as removing fisheye lens distortion.

## Building

### Dependencies
 * [libiqa](https://github.com/ImageProcessing-ElectronicPublications/libiqa)
 * [libsmallfry](https://github.com/ImageProcessing-ElectronicPublications/libsmallfry)
 * [jpeg8](https://www.ijg.org)
 * [webp](https://developers.google.com/speed/webp/)

#### Debian

Debian users can install via `apt-get`:

```bash
sudo apt-get install build-essential autoconf pkg-config nasm libtool libjpeg8-dev
git clone https://github.com/ImageProcessing-ElectronicPublications/libiqa.git
cd libiqa
make
sudo make install
cd ..
git clone https://github.com/ImageProcessing-ElectronicPublications/libsmallfry.git
cd libsmallfry
make
sudo make install
cd ..
```

### Compiling (Linux and Mac OS X)

The `Makefile` should work as-is on Ubuntu and Mac OS X. Other platforms may need to set the location of `libjpeg.a` or make other tweaks.

```bash
make
```

### Installation

Install the binaries into `/usr/local/bin`:

```bash
sudo make install
```

## Links / Alternatives

* https://github.com/rflynn/imgmin
* https://news.ycombinator.com/item?id=803839

## License

* JPEG-Recompress is copyright &copy; 2015 Daniel G. Taylor
* Image Quality Assessment (IQA) is copyright 2011, Tom Distler (http://tdistler.com)
* SmallFry is copyright 2014, Derek Buitenhuis (https://github.com/dwbuiten)

All are released under an MIT license.

http://dgt.mit-license.org/
