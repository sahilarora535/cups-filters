/**
 * This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @brief Convert PWG Raster to a PDF/PCLm file
 * @file rastertopdf.cpp
 * @author Neil 'Superna' Armstrong <superna9999@gmail.com> (C) 2010
 * @author Tobias Hoffmann <smilingthax@gmail.com> (c) 2012
 * @author Till Kamppeter <till.kamppeter@gmail.com> (c) 2014
 * @author Sahil Arora <sahilarora.535@gmail.com> (c) 2017
 */

#include <config.h>
#include <string.h>
#include <limits>
#include <cups/cups.h>
#include <cups/raster.h>
#include <cupsfilters/colormanager.h>
#include <cupsfilters/image.h>

#include <vector>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>

#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_Buffer.hh>
#ifdef QPDF_HAVE_PCLM
#include <qpdf/Pl_RunLength.hh>
#include <qpdf/Pl_DCT.hh>
#endif

#ifdef USE_LCMS1
#include <lcms.h>
#define cmsColorSpaceSignature icColorSpaceSignature
#define cmsSetLogErrorHandler cmsSetErrorHandler
#define cmsSigXYZData icSigXYZData
#define cmsSigLuvData icSigLuvData
#define cmsSigLabData icSigLabData
#define cmsSigYCbCrData icSigYCbCrData
#define cmsSigYxyData icSigYxyData
#define cmsSigRgbData icSigRgbData
#define cmsSigHsvData icSigHsvData
#define cmsSigHlsData icSigHlsData
#define cmsSigCmyData icSigCmyData
#define cmsSig3colorData icSig3colorData
#define cmsSigGrayData icSigGrayData
#define cmsSigCmykData icSigCmykData
#define cmsSig4colorData icSig4colorData
#define cmsSig2colorData icSig2colorData
#define cmsSig5colorData icSig5colorData
#define cmsSig6colorData icSig6colorData
#define cmsSig7colorData icSig7colorData
#define cmsSig8colorData icSig8colorData
#define cmsSig9colorData icSig9colorData
#define cmsSig10colorData icSig10colorData
#define cmsSig11colorData icSig11colorData
#define cmsSig12colorData icSig12colorData
#define cmsSig13colorData icSig13colorData
#define cmsSig14colorData icSig14colorData
#define cmsSig15colorData icSig15colorData
#define cmsSaveProfileToMem _cmsSaveProfileToMem
#else
#include <lcms2.h>
#endif

#define DEFAULT_PDF_UNIT 72   // 1/72 inch

#define PROGRAM "rastertopclm"

#define dprintf(format, ...) fprintf(stderr, "DEBUG2: (" PROGRAM ") " format, __VA_ARGS__)

#define iprintf(format, ...) fprintf(stderr, "INFO: (" PROGRAM ") " format, __VA_ARGS__)

// Compression method for providing data to PCLm Streams.
typedef enum {
  DCT_DECODE = 0,
  FLATE_DECODE,
  RLE_DECODE
} CompressionMethod;

// Color conversion function
typedef unsigned char *(*convertFunction)(unsigned char *src,
  unsigned char *dst, unsigned int pixels);

// Bit conversion function
typedef unsigned char *(*bitFunction)(unsigned char *src,
  unsigned char *dst, unsigned int pixels);

// PDF color conversion function
typedef void (*pdfConvertFunction)(struct pdf_info * info);

cmsHPROFILE         colorProfile = NULL;     // ICC Profile to be applied to PDF
int                 cm_disabled = 0;         // Flag rasied if color management is disabled 
cm_calibration_t    cm_calibrate;            // Status of CUPS color management ("on" or "off")
convertFunction     conversion_function;     // Raster color conversion function
bitFunction         bit_function;            // Raster bit function


#ifdef USE_LCMS1
static int lcmsErrorHandler(int ErrorCode, const char *ErrorText)
{
  fprintf(stderr, "ERROR: %s\n",ErrorText);
  return 1;
}
#else
static void lcmsErrorHandler(cmsContext contextId, cmsUInt32Number ErrorCode,
   const char *ErrorText)
{
  fprintf(stderr, "ERROR: %s\n",ErrorText);
}
#endif



// Bit conversion functions

unsigned char *invertBits(unsigned char *src, unsigned char *dst, unsigned int pixels)
{ 
    unsigned int i;

    // Invert black to grayscale...
    for (i = pixels, dst = src; i > 0; i --, dst ++)
      *dst = ~*dst;

    return dst;
}	

unsigned char *noBitConversion(unsigned char *src, unsigned char *dst, unsigned int pixels)
{
    return src;
}

// Color conversion functions

unsigned char *rgbToCmyk(unsigned char *src, unsigned char *dst, unsigned int pixels)
{
    cupsImageRGBToCMYK(src,dst,pixels);
    return dst;
}
unsigned char *whiteToCmyk(unsigned char *src, unsigned char *dst, unsigned int pixels)
{
    cupsImageWhiteToCMYK(src,dst,pixels);
    return dst;
}

unsigned char *cmykToRgb(unsigned char *src, unsigned char *dst, unsigned int pixels)
{
    cupsImageCMYKToRGB(src,dst,pixels);
    return dst;
}

unsigned char *whiteToRgb(unsigned char *src, unsigned char *dst, unsigned int pixels)
{
    cupsImageWhiteToRGB(src,dst,pixels);
    return dst;
}

unsigned char *rgbToWhite(unsigned char *src, unsigned char *dst, unsigned int pixels)
{
    cupsImageRGBToWhite(src,dst,pixels);
    return dst;
}

unsigned char *cmykToWhite(unsigned char *src, unsigned char *dst, unsigned int pixels)
{
    cupsImageCMYKToWhite(src,dst,pixels);
    return dst;
}

unsigned char *noColorConversion(unsigned char *src,
  unsigned char *dst, unsigned int pixels)
{
    return src;
}

/**
 * 'split_strings()' - Split a string to a vector of strings given some delimiters
 * O - std::vector of std::string after splitting
 * I - input string to be split
 * I - string containing delimiters
 */
static std::vector<std::string>
split_strings(std::string const &str, std::string delimiters = ",")
{
  std::vector<std::string> vec(0);
  std::string value = "";
  bool push_flag = false;

  for (size_t i = 0; i < str.size(); i ++)
  {
    if (push_flag && !(value.empty()))
    {
      vec.push_back(value);
      push_flag = false;
      value.clear();
    }

    if (delimiters.find(str[i]) != std::string::npos)
      push_flag = true;
    else
      value += str[i];
  }
  if (!value.empty())
    vec.push_back(value);
  return vec;
}

/**
 * 'num_digits()' - Calculates the number of digits in an integer
 * O - number of digits in the input integer
 * I - the integer whose digits needs to be calculated
 */
int num_digits(int n)
{
  if (n == 0) return 1;
  int digits = 0;
  while (n)
  {
    ++digits;
    n /= 10;
  }
  return digits;
}

/**
 * 'int_to_fwstring()' - Convert a number to fixed width string by padding with zeroes
 * O - converted string
 * I - the integee which needs to be converted to string
 * I - width of string required
 */
std::string int_to_fwstring(int n, int width)
{
  int num_zeroes = width - num_digits(n);
  if (num_zeroes < 0)
    num_zeroes = 0;
  return std::string(num_zeroes, '0') + QUtil::int_to_string(n);
}

void die(const char * str)
{
    fprintf(stderr, "ERROR: (" PROGRAM ") %s\n", str);
    exit(1);
}


//------------- PDF ---------------

struct pdf_info
{
    pdf_info() 
      : pagecount(0),
        width(0),height(0),
        line_bytes(0),
        bpp(0), bpc(0),
        pclm_num_strips(0),
        pclm_strip_height_preferred(16),  /* default strip height */
        pclm_strip_height(0),
        pclm_strip_height_supported(1, 16),
        pclm_compression_method_preferred(0),
        pclm_source_resolution_supported(0),
        pclm_source_resolution_default(""),
        pclm_raster_back_side(""),
        pclm_strip_data(0),
        render_intent(""),
        color_space(CUPS_CSPACE_K),
        page_width(0),page_height(0)
    {
    }

    QPDF pdf;
    QPDFObjectHandle page;
    unsigned pagecount;
    unsigned width;
    unsigned height;
    unsigned line_bytes;
    unsigned bpp;
    unsigned bpc;
    unsigned                  pclm_num_strips;
    unsigned                  pclm_strip_height_preferred;
    std::vector<unsigned>     pclm_strip_height;
    std::vector<unsigned>     pclm_strip_height_supported;
    std::vector<CompressionMethod> pclm_compression_method_preferred;
    std::vector<std::string>  pclm_source_resolution_supported;
    std::string               pclm_source_resolution_default;
    std::string               pclm_raster_back_side;
    std::vector< PointerHolder<Buffer> > pclm_strip_data;
    std::string render_intent;
    cups_cspace_t color_space;
    PointerHolder<Buffer> page_data;
    double page_width,page_height;
};

int create_pdf_file(struct pdf_info * info)
{
    try {
        info->pdf.emptyPDF();
    } catch (...) {
        return 1;
    }
    return 0;
}

QPDFObjectHandle makeIntegerBox(int x1, int y1, int x2, int y2)
{
    QPDFObjectHandle ret = QPDFObjectHandle::newArray();
    ret.appendItem(QPDFObjectHandle::newInteger(x1));
    ret.appendItem(QPDFObjectHandle::newInteger(y1));
    ret.appendItem(QPDFObjectHandle::newInteger(x2));
    ret.appendItem(QPDFObjectHandle::newInteger(y2));
    return ret;
}


// PDF color conversion functons...

void modify_pdf_color(struct pdf_info * info, int bpp, int bpc, convertFunction fn)
{
    unsigned old_bpp = info->bpp;
    unsigned old_bpc = info->bpc;
    double old_ncolor = old_bpp/old_bpc;

    unsigned old_line_bytes = info->line_bytes;

    double new_ncolor = (bpp/bpc);

    info->line_bytes = (unsigned)old_line_bytes*(new_ncolor/old_ncolor);
    info->bpp = bpp;
    info->bpc = bpc;
    conversion_function = fn; 

    return;
}

void convertPdf_NoConversion(struct pdf_info * info)
{
    conversion_function = noColorConversion;
    bit_function = noBitConversion;
}

void convertPdf_Cmyk8ToWhite8(struct pdf_info * info)
{
    modify_pdf_color(info, 8, 8, cmykToWhite);
    bit_function = noBitConversion;
}

void convertPdf_Rgb8ToWhite8(struct pdf_info * info)
{
    modify_pdf_color(info, 8, 8, rgbToWhite);
    bit_function = noBitConversion;
}

void convertPdf_Cmyk8ToRgb8(struct pdf_info * info)
{
    modify_pdf_color(info, 24, 8, cmykToRgb);
    bit_function = noBitConversion;
}

void convertPdf_White8ToRgb8(struct pdf_info * info)
{
    modify_pdf_color(info, 24, 8, whiteToRgb);
    bit_function = invertBits;
}

void convertPdf_Rgb8ToCmyk8(struct pdf_info * info)
{
    modify_pdf_color(info, 32, 8, rgbToCmyk);
    bit_function = noBitConversion;
}

void convertPdf_White8ToCmyk8(struct pdf_info * info)
{
    modify_pdf_color(info, 32, 8, whiteToCmyk);
    bit_function = invertBits;
}

void convertPdf_InvertColors(struct pdf_info * info)
{
    conversion_function = noColorConversion;
    bit_function = invertBits;
}


#define PRE_COMPRESS


/**
 * 'makePclmStrips()' - return an std::vector of QPDFObjectHandle, each containing the
 *                      stream data of the various strips which make up a PCLm page.
 * O - std::vector of QPDFObjectHandle
 * I - QPDF object
 * I - number of strips per page
 * I - std::vector of PointerHolder<Buffer> containing data for each strip
 * I - strip width
 * I - strip height
 * I - color space
 * I - bits per component
 */
std::vector<QPDFObjectHandle>
makePclmStrips(QPDF &pdf, unsigned num_strips,
               std::vector< PointerHolder<Buffer> > &strip_data,
               std::vector<CompressionMethod> &compression_methods,
               unsigned width, std::vector<unsigned>& strip_height, cups_cspace_t cs, unsigned bpc)
{
    std::vector<QPDFObjectHandle> ret(num_strips);
    for (size_t i = 0; i < num_strips; i ++)
      ret[i] = QPDFObjectHandle::newStream(&pdf);

    // Strip stream dictionary
    std::map<std::string,QPDFObjectHandle> dict;

    dict["/Type"]=QPDFObjectHandle::newName("/XObject");
    dict["/Subtype"]=QPDFObjectHandle::newName("/Image");
    dict["/Width"]=QPDFObjectHandle::newInteger(width);
    dict["/BitsPerComponent"]=QPDFObjectHandle::newInteger(bpc);

    J_COLOR_SPACE color_space;
    unsigned components;
    /* Write "/ColorSpace" dictionary based on raster input */
    switch(cs) {
      case CUPS_CSPACE_K:
      case CUPS_CSPACE_SW:
        dict["/ColorSpace"]=QPDFObjectHandle::newName("/DeviceGray");
        color_space = JCS_GRAYSCALE;
        components = 1;
        break;
      case CUPS_CSPACE_RGB:
      case CUPS_CSPACE_SRGB:
      case CUPS_CSPACE_ADOBERGB:
        dict["/ColorSpace"]=QPDFObjectHandle::newName("/DeviceRGB");
        color_space = JCS_RGB;
        components = 3;
        break;
      default:
        fputs("DEBUG: Color space not supported.\n", stderr); 
        return std::vector<QPDFObjectHandle>(num_strips, QPDFObjectHandle());
    }

    // We deliver already compressed content (instead of letting QPDFWriter do it)
    // to avoid using excessive memory. For that we first get preferred compression
    // method to pre-compress content for strip streams.

    // Use the compression method with highest priority of the available methods
    // __________________
    // Priority | Method
    // ------------------
    // 0        | DCT
    // 1        | FLATE
    // 2        | RLE
    // ------------------
    CompressionMethod compression = compression_methods.front();
    for (std::vector<CompressionMethod>::iterator it = compression_methods.begin();
         it != compression_methods.end(); ++it)
      compression = compression > *it ? compression : *it;

    // write compressed stream data
    for (size_t i = 0; i < num_strips; i ++)
    {
      dict["/Height"]=QPDFObjectHandle::newInteger(strip_height[i]);
      ret[i].replaceDict(QPDFObjectHandle::newDictionary(dict));
      Pl_Buffer psink("psink");
      if (compression == FLATE_DECODE)
      {
        Pl_Flate pflate("pflate", &psink, Pl_Flate::a_deflate);
        pflate.write(strip_data[i]->getBuffer(), strip_data[i]->getSize());
        pflate.finish();
        ret[i].replaceStreamData(PointerHolder<Buffer>(psink.getBuffer()),
                              QPDFObjectHandle::newName("/FlateDecode"),QPDFObjectHandle::newNull());
      }
      else if (compression == RLE_DECODE)
      {
        Pl_RunLength prle("prle", &psink, Pl_RunLength::a_encode);
        prle.write(strip_data[i]->getBuffer(),strip_data[i]->getSize());
        prle.finish();
        ret[i].replaceStreamData(PointerHolder<Buffer>(psink.getBuffer()),
                              QPDFObjectHandle::newName("/RunLengthDecode"),QPDFObjectHandle::newNull());
      }
      else if (compression == DCT_DECODE)
      {
        Pl_DCT pdct("pdct", &psink, width, strip_height[i], components, color_space);
        pdct.write(strip_data[i]->getBuffer(),strip_data[i]->getSize());
        pdct.finish();
        ret[i].replaceStreamData(PointerHolder<Buffer>(psink.getBuffer()),
                              QPDFObjectHandle::newName("/DCTDecode"),QPDFObjectHandle::newNull());
      }
    }
    return ret;
}

void finish_page(struct pdf_info * info)
{
      // Finish previous PCLm page
      if (info->pclm_num_strips == 0)
        return;

      for (size_t i = 0; i < info->pclm_strip_data.size(); i ++)
        if(!info->pclm_strip_data[i].getPointer())
          return;

      std::vector<QPDFObjectHandle> strips = makePclmStrips(info->pdf, info->pclm_num_strips, info->pclm_strip_data, info->pclm_compression_method_preferred, info->width, info->pclm_strip_height, info->color_space, info->bpc);
      for (size_t i = 0; i < info->pclm_num_strips; i ++)
        if(!strips[i].isInitialized()) die("Unable to load strip data");

      // add it
      for (size_t i = 0; i < info->pclm_num_strips; i ++)
        info->page.getKey("/Resources").getKey("/XObject")
                  .replaceKey("/Image" +
                              int_to_fwstring(i,num_digits(info->pclm_num_strips - 1)),
                              strips[i]);

    // draw it
    std::string content;
      std::string res = info->pclm_source_resolution_default;

      // resolution is in dpi, so remove the last three characters from
      // resolution string to get resolution integer
      unsigned resolution_integer = std::stoi(res.substr(0, res.size() - 3));
      double d = (double)DEFAULT_PDF_UNIT / resolution_integer;
      content.append(QUtil::double_to_string(d) + " 0 0 " + QUtil::double_to_string(d) + " 0 0 cm\n");
      unsigned yAnchor = info->height;
      for (unsigned i = 0; i < info->pclm_num_strips; i ++)
      {
        yAnchor -= info->pclm_strip_height[i];
        content.append("/P <</MCID 0>> BDC q\n");
        content.append(QUtil::int_to_string(info->width) + " 0 0 " +
                        QUtil::int_to_string(info->pclm_strip_height[i]) +
                        " 0 " + QUtil::int_to_string(yAnchor) + " cm\n");
        content.append("/Image" +
                       int_to_fwstring(i, num_digits(info->pclm_num_strips - 1)) +
                       " Do Q\n");
      }

    QPDFObjectHandle page_contents = info->page.getKey("/Contents");
      page_contents.getArrayItem(0).replaceStreamData(content, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());

    // bookkeeping
    info->page_data = PointerHolder<Buffer>();
    info->pclm_strip_data.clear();
}


/* Perform modifications to PDF if color space conversions are needed */      
int prepare_pdf_page(struct pdf_info * info, unsigned width, unsigned height, unsigned bpl, 
                     unsigned bpp, unsigned bpc, std::string render_intent, cups_cspace_t color_space)
{
#define IMAGE_CMYK_8   (bpp == 32 && bpc == 8)
#define IMAGE_CMYK_16  (bpp == 64 && bpc == 16)
#define IMAGE_RGB_8    (bpp == 24 && bpc == 8)
#define IMAGE_RGB_16   (bpp == 48 && bpc == 16)
#define IMAGE_WHITE_1  (bpp == 1 && bpc == 1)
#define IMAGE_WHITE_8  (bpp == 8 && bpc == 8)
#define IMAGE_WHITE_16 (bpp == 16 && bpc == 16)    

    int error = 0;
    pdfConvertFunction fn = convertPdf_NoConversion;
    cmsColorSpaceSignature css;

    /* Register available raster information into the PDF */
    info->width = width;
    info->height = height;
    info->line_bytes = bpl;
    info->bpp = bpp;
    info->bpc = bpc;
    info->render_intent = render_intent;
    info->color_space = color_space;
      info->pclm_num_strips = (height / info->pclm_strip_height_preferred) +
                              (height % info->pclm_strip_height_preferred ? 1 : 0);
      info->pclm_strip_height.resize(info->pclm_num_strips);
      info->pclm_strip_data.resize(info->pclm_num_strips);
      for (size_t i = 0; i < info->pclm_num_strips; i ++)
      {
        info->pclm_strip_height[i] = info->pclm_strip_height_preferred < height ?
                                     info->pclm_strip_height_preferred : height;
        height -= info->pclm_strip_height[i];
      }

    /* Invert grayscale by default */
    if (color_space == CUPS_CSPACE_K)
      fn = convertPdf_InvertColors;

    if (colorProfile != NULL) {
      css = cmsGetColorSpace(colorProfile);

      // Convert image and PDF color space to an embedded ICC Profile color space
      switch(css) {
        // Convert PDF to Grayscale when using a gray profile
        case cmsSigGrayData:
          if (color_space == CUPS_CSPACE_CMYK)
            fn = convertPdf_Cmyk8ToWhite8;
          else if (color_space == CUPS_CSPACE_RGB) 
            fn = convertPdf_Rgb8ToWhite8;
          else              
            fn = convertPdf_InvertColors;
          info->color_space = CUPS_CSPACE_K;
          break;
        // Convert PDF to RGB when using an RGB profile
        case cmsSigRgbData:
          if (color_space == CUPS_CSPACE_CMYK) 
            fn = convertPdf_Cmyk8ToRgb8;
          else if (color_space == CUPS_CSPACE_K) 
            fn = convertPdf_White8ToRgb8;
          info->color_space = CUPS_CSPACE_RGB;
          break;
        // Convert PDF to CMYK when using an RGB profile
        case cmsSigCmykData:
          if (color_space == CUPS_CSPACE_RGB)
            fn = convertPdf_Rgb8ToCmyk8;
          else if (color_space == CUPS_CSPACE_K) 
            fn = convertPdf_White8ToCmyk8;
          info->color_space = CUPS_CSPACE_CMYK;
          break;
        default:
          fputs("DEBUG: Unable to convert PDF from profile.\n", stderr);
          colorProfile = NULL;
          error = 1;
      }
      // Perform conversion of an image color space 
    } 

   if (!error)
     fn(info);

   return error;
}

int add_pdf_page(struct pdf_info * info, int pagen, unsigned width,
		 unsigned height, int bpp, int bpc, int bpl, std::string render_intent,
		 cups_cspace_t color_space, unsigned xdpi, unsigned ydpi)
{
    try {
        finish_page(info); // any active

        prepare_pdf_page(info, width, height, bpl, bpp, 
                         bpc, render_intent, color_space);

        if (info->height > (std::numeric_limits<unsigned>::max() / info->line_bytes)) {
            die("Page too big");
        }
          // reserve space for PCLm strips
          for (size_t i = 0; i < info->pclm_num_strips; i ++)
            info->pclm_strip_data[i] = PointerHolder<Buffer>(new Buffer(info->line_bytes*info->pclm_strip_height[i]));

        QPDFObjectHandle page = QPDFObjectHandle::parse(
            "<<"
            "  /Type /Page"
            "  /Resources <<"
            "    /XObject << >> "
            "  >>"
            "  /MediaBox null "
            "  /Contents null "
            ">>");

        // Convert to pdf units
        info->page_width=((double)info->width/xdpi)*DEFAULT_PDF_UNIT;
        info->page_height=((double)info->height/ydpi)*DEFAULT_PDF_UNIT;
          page.replaceKey("/Contents",
            QPDFObjectHandle::newArray(std::vector<QPDFObjectHandle>(1, QPDFObjectHandle::newStream(&info->pdf))));

          // box with dimensions rounded off to the nearest integer
          page.replaceKey("/MediaBox",makeIntegerBox(0,0,info->page_width + 0.5,info->page_height + 0.5));
    
        info->page = info->pdf.makeIndirectObject(page); // we want to keep a reference
        info->pdf.addPage(info->page, false);
    } catch (std::bad_alloc &ex) {
        die("Unable to allocate page data");
    } catch (...) {
        return 1;
    }

    return 0;
}

int close_pdf_file(struct pdf_info * info)
{
    try {
        finish_page(info); // any active

        QPDFWriter output(info->pdf,NULL);
//        output.setMinimumPDFVersion("1.4");
          output.setPCLm(true);
        output.write();
    } catch (...) {
        return 1;
    }

    return 0;
}

void pdf_set_line(struct pdf_info * info, unsigned line_n, unsigned char *line)
{
    //dprintf("pdf_set_line(%d)\n", line_n);

    if(line_n > info->height)
    {
        dprintf("Bad line %d\n", line_n);
        return;
    }

        // copy line data into appropriate pclm strip
        size_t strip_num = line_n / info->pclm_strip_height_preferred;
        unsigned line_strip = line_n - strip_num*info->pclm_strip_height_preferred;
        memcpy(((info->pclm_strip_data[strip_num])->getBuffer() + (line_strip*info->line_bytes)),
               line, info->line_bytes);
}

int convert_raster(cups_raster_t *ras, unsigned width, unsigned height,
		   int bpp, int bpl, struct pdf_info * info)
{
    // We should be at raster start
    int i;
    unsigned cur_line = 0;
    unsigned char *PixelBuffer, *ptr = NULL, *buff;

    PixelBuffer = (unsigned char *)malloc(bpl);
    buff = (unsigned char *)malloc(info->line_bytes);

    do
    {
        // Read raster data...
        cupsRasterReadPixels(ras, PixelBuffer, bpl);

#if !ARCH_IS_BIG_ENDIAN

	if (info->bpc == 16)
	{
	  // Swap byte pairs for endianess (cupsRasterReadPixels() switches
	  // from Big Endian back to the system's Endian)
	  for (i = bpl, ptr = PixelBuffer; i > 0; i -= 2, ptr += 2)
	  {
	    unsigned char swap = *ptr;
	    *ptr = *(ptr + 1);
	    *(ptr + 1) = swap;
	  }
	}
#endif /* !ARCH_IS_BIG_ENDIAN */

        // perform bit operations if necessary
        bit_function(PixelBuffer, ptr,  bpl);

        // write lines and color convert when necessary
 	pdf_set_line(info, cur_line, conversion_function(PixelBuffer, buff, width));
	++cur_line;
    }
    while(cur_line < height);

    free(buff);
    free(PixelBuffer);

    return 0;
}

/* Obtain a source profile name using color qualifiers from raster file */
const char * getIPPColorProfileName(const char * media_type, cups_cspace_t cs, unsigned dpi)
{
    std::string mediaType = "";
    std::string resolution = "";
    std::string colorModel = "";
   
    std::string iccProfile = "";

    // ColorModel
    switch (cs) {
        case CUPS_CSPACE_RGB:
            colorModel = "rgb";
            break;
        case CUPS_CSPACE_SRGB:
            colorModel = "srgb";
            break;
        case CUPS_CSPACE_ADOBERGB:
            colorModel = "adobergb";
            break;
        case CUPS_CSPACE_K:
            colorModel = "gray";
            break;
        case CUPS_CSPACE_CMYK:
            colorModel = "cmyk";
            break;
        default:
            colorModel = "";
            break;
     }
 
    if (media_type != NULL)
      mediaType = media_type;
    if (dpi > 0)
      resolution = dpi;

    // Requires color space and media type qualifiers
    if (resolution != "" || colorModel != "")
      return 0;

    // profile-uri reference: "http://www.server.com/colorModel-Resolution-mediaType.icc
    if (mediaType != "")          
      iccProfile = colorModel + "-" + resolution + ".icc";
    else 
      iccProfile = colorModel + "-" + resolution + "-" + mediaType + ".icc";

    return iccProfile.c_str();
}

int main(int argc, char **argv)
{
    int fd, Page;
    struct pdf_info pdf;
    FILE * input = NULL;
    cups_raster_t	*ras;		/* Raster stream for printing */
    cups_page_header2_t	header;		/* Page header from file */
    ppd_file_t		*ppd;		/* PPD file */
    ppd_attr_t    *attr;  /* PPD attribute */
    int			num_options;	/* Number of options */
    const char*         profile_name;	/* IPP Profile Name */
    cups_option_t	*options;	/* Options */

    // Make sure status messages are not buffered...
    setbuf(stderr, NULL);

    cmsSetLogErrorHandler(lcmsErrorHandler);

    if (argc < 6 || argc > 7)
    {
        fprintf(stderr, "Usage: %s <job> <user> <job name> <copies> <option> [file]\n", argv[0]);
        return 1;
    }

    /* Determine the output format via an environment variable set by a wrapper
        script */
  
    num_options = cupsParseOptions(argv[5], 0, &options);  

    /* support the CUPS "cm-calibration" option */ 
    cm_calibrate = cmGetCupsColorCalibrateMode(options, num_options);

      cm_disabled = 1;

    // Open the PPD file...
    ppd = ppdOpenFile(getenv("PPD"));

    if (ppd)
    {
      ppdMarkDefaults(ppd);
      cupsMarkOptions(ppd, num_options, options);
    }
    else
    {
      ppd_status_t	status;		/* PPD error */
      int		linenum;	/* Line number */

      fputs("DEBUG: The PPD file could not be opened.\n", stderr);

      status = ppdLastError(&linenum);
      
      fprintf(stderr, "DEBUG: %s on line %d.\n", ppdErrorString(status), linenum);
	fprintf(stderr, "ERROR: PCLm output only possible with PPD file.\n");
	return 1;
    }

    // Open the page stream...
    if (argc == 7)
    {
        input = fopen(argv[6], "rb");
        if (input == NULL) die("Unable to open PWG Raster file");
    }
    else
        input = stdin;

    // Get fd from file
    fd = fileno(input);

    // Transform
    ras = cupsRasterOpen(fd, CUPS_RASTER_READ);

    // Process pages as needed...
    Page = 0;

    // Create PDF file
    if (create_pdf_file(&pdf) != 0)
      die("Unable to create PDF file");

    /* Get PCLm attributes from PPD */
    if (ppd)
    {
      char *attr_name = (char *)"cupsPclmStripHeightPreferred";
      if ((attr = ppdFindAttr(ppd, attr_name, NULL)) != NULL)
      {
        fprintf(stderr, "DEBUG: PPD PCLm attribute \"%s\" with value \"%s\"\n",
            attr_name, attr->value);
        pdf.pclm_strip_height_preferred = atoi(attr->value);
      }
      else
        pdf.pclm_strip_height_preferred = 16; /* default strip height */

      attr_name = (char *)"cupsPclmStripHeightSupported";
      if ((attr = ppdFindAttr(ppd, attr_name, NULL)) != NULL)
      {
        fprintf(stderr, "DEBUG: PPD PCLm attribute \"%s\" with value \"%s\"\n",
            attr_name, attr->value);
        pdf.pclm_strip_height_supported.clear();  // remove default value = 16
        std::vector<std::string> vec = split_strings(attr->value, ",");
        for (size_t i = 0; i < vec.size(); i ++)
          pdf.pclm_strip_height_supported.push_back(atoi(vec[i].c_str()));
        vec.clear();
      }

      attr_name = (char *)"cupsPclmRasterBackSide";
      if ((attr = ppdFindAttr(ppd, attr_name, NULL)) != NULL)
      {
        fprintf(stderr, "DEBUG: PPD PCLm attribute \"%s\" with value \"%s\"\n",
            attr_name, attr->value);
        pdf.pclm_raster_back_side = attr->value;
      }

      attr_name = (char *)"cupsPclmSourceResolutionDefault";
      if ((attr = ppdFindAttr(ppd, attr_name, NULL)) != NULL)
      {
        fprintf(stderr, "DEBUG: PPD PCLm attribute \"%s\" with value \"%s\"\n",
            attr_name, attr->value);
        pdf.pclm_source_resolution_default = attr->value;
      }

      attr_name = (char *)"cupsPclmSourceResolutionSupported";
      if ((attr = ppdFindAttr(ppd, attr_name, NULL)) != NULL)
      {
        fprintf(stderr, "DEBUG: PPD PCLm attribute \"%s\" with value \"%s\"\n",
            attr_name, attr->value);
        pdf.pclm_source_resolution_supported = split_strings(attr->value, ",");
      }

      attr_name = (char *)"cupsPclmCompressionMethodPreferred";
      if ((attr = ppdFindAttr(ppd, attr_name, NULL)) != NULL)
      {
        fprintf(stderr, "DEBUG: PPD PCLm attribute \"%s\" with value \"%s\"\n",
            attr_name, attr->value);
        std::vector<std::string> vec = split_strings(attr->value, ",");

        // get all compression methods supported by the printer
        for (std::vector<std::string>::iterator it = vec.begin();
             it != vec.end(); ++it)
        {
          std::string compression_method = *it;
          for (char& x: compression_method)
            x = tolower(x);
          if (compression_method == "flate")
            pdf.pclm_compression_method_preferred.push_back(FLATE_DECODE);
          else if (compression_method == "rle")
            pdf.pclm_compression_method_preferred.push_back(RLE_DECODE);
          else if (compression_method == "jpeg")
            pdf.pclm_compression_method_preferred.push_back(DCT_DECODE);
        }

      }
      // If the compression methods is none of the above or is erreneous
      // use FLATE as compression method and show a warning.
      if (pdf.pclm_compression_method_preferred.empty())
      {
        fprintf(stderr, "WARNING: (rastertopclm) Unable parse PPD attribute \"%s\". Using FLATE for encoding image streams.\n", attr_name);
        pdf.pclm_compression_method_preferred.push_back(FLATE_DECODE);
      }
    }

    while (cupsRasterReadHeader2(ras, &header))
    {
      // Write a status message with the page number
      Page ++;
      fprintf(stderr, "INFO: Starting page %d.\n", Page);

      // Use "profile=profile_name.icc" to embed 'profile_name.icc' into the PDF
      // for testing. Forces color management to enable.
      if (colorProfile != NULL)       
        fprintf(stderr, "DEBUG: TEST ICC Profile specified (color management forced ON): \n[%s]\n", profile_name);

      // Add a new page to PDF file
      if (add_pdf_page(&pdf, Page, header.cupsWidth, header.cupsHeight,
		       header.cupsBitsPerPixel, header.cupsBitsPerColor, 
		       header.cupsBytesPerLine, header.cupsRenderingIntent, 
                       header.cupsColorSpace, header.HWResolution[0],
		       header.HWResolution[1]) != 0)
	die("Unable to start new PDF page");

      // Write the bit map into the PDF file
      if (convert_raster(ras, header.cupsWidth, header.cupsHeight,
			 header.cupsBitsPerPixel, header.cupsBytesPerLine, 
			 &pdf) != 0)
	die("Failed to convert page bitmap");
    }

    close_pdf_file(&pdf); // will output to stdout

    if (colorProfile != NULL) {
      cmsCloseProfile(colorProfile);
    }

    cupsFreeOptions(num_options, options);

    cupsRasterClose(ras);

    if (fd != 0)
      close(fd);

    return (Page == 0);
}
