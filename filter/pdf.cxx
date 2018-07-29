/*
 * Copyright 2012 Canonical Ltd.
 * Copyright 2013 ALT Linux, Andrew V. Stepanov <stanv@altlinux.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "pdf.h"

#include <vector>
#include <string>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>

// #include <PDFDoc.h>
// #include <GlobalParams.h>
// #include <Form.h>
// #include <Gfx.h>
// #include <GfxFont.h>
// #include <Page.h>
// #include <PDFDocEncoding.h>
// #ifdef HAVE_CPP_POPPLER_VERSION_H
// #include "cpp/poppler-version.h"
// #endif

// extern "C" {
// #include <embed.h>
// #include <sfnt.h>
// }

// #include <fontconfig/fontconfig.h>

/*
 * Useful reference:
 *
 * http://www.gnupdf.org/Indirect_Object
 * http://www.gnupdf.org/Introduction_to_PDF
 * http://blog.idrsolutions.com/2011/05/understanding-the-pdf-file-format-%E2%80%93-pdf-xref-tables-explained
 * http://labs.appligent.com/pdfblog/pdf-hello-world/
*/

// static EMB_PARAMS *get_font(const char *font);

// static const char *emb_pdf_escape_name(const char *name, int len);

// static int utf8_to_utf16(const char *utf8, unsigned short **out_ptr);

// static const char* get_next_wide(const char *utf8, int *unicode_out);

// extern "C" {
//     static int pdf_embed_font(
//             pdf_t *doc,
//             Page *page,
//             const char *typeface);
// }

// static void fill_font_stream(
//         const char *buf,
//         int len,
//         void *context);

// static Object *make_fontdescriptor_dic(pdf_t *doc,
//         EMB_PARAMS *emb,
//         EMB_PDF_FONTDESCR *fdes,
//         Ref fontfile_obj_ref);

// static Object *make_font_dic(pdf_t *doc,
//         EMB_PARAMS *emb,
//         EMB_PDF_FONTDESCR *fdes,
//         EMB_PDF_FONTWIDTHS *fwid,
//         Ref fontdescriptor_obj_ref);

// static Object *make_cidfont_dic(pdf_t *doc,
//         EMB_PARAMS *emb,
//         const char *fontname,
//         Ref fontdescriptor_obj_ref);

// /* Font to use to fill form */
// static EMB_PARAMS *Font;


/**
 * 'makeRealBox()' - Return a QPDF array object of real values for a box.
 * O - QPDFObjectHandle for an array
 * I - Dimensions of the box in a float array
 */
static QPDFObjectHandle makeRealBox(float values[4])
{
  QPDFObjectHandle ret = QPDFObjectHandle::newArray();
  for (int i = 0; i < 4; ++i) {
    ret.appendItem(QPDFObjectHandle::newReal(values[i]));
  }
  return ret;
}


/**
 * 'pdf_load_template()' - Load an existing PDF file and do initial parsing
 *                         using QPDF.
 * I - Filename to open
 */
extern "C" pdf_t * pdf_load_template(const char *filename)
{
  QPDF *pdf = new QPDF();
  pdf->processFile(filename);
  unsigned pages = (pdf->getAllPages()).size();

  if (pages != 1) {
    fprintf(stderr, "Error: PDF template must contain exactly 1 page: %s\n",
            filename);
    delete pdf;
    return NULL;
  }

  return pdf;
}


/**
 * 'pdf_free()' - Free pointer used by PDF object
 * I - Pointer to PDF object
 */
extern "C" void pdf_free(pdf_t *pdf)
{
  delete pdf;
}


/**
 * 'pdf_prepend_stream' - Prepend a stream to the contents of a specified
 *                        page in PDF file.
 * I - Pointer to QPDF object
 * I - page number of page to prepend stream to
 * I - buffer containing data to be prepended
 * I - length of buffer
 */
extern "C" void pdf_prepend_stream(pdf_t *pdf,
                                   unsigned page_num,
                                   char const *buf,
                                   size_t len)
{
  std::vector<QPDFObjectHandle> pages = pdf->getAllPages();
  if (pages.empty() || page_num > pages.size()) {
    fprintf(stderr, "ERROR: Unable to prepend stream to requested PDF page\n");
    return;
  }

  QPDFObjectHandle page = pages[page_num - 1];

  // get page contents stream / array  
  QPDFObjectHandle contents = page.getKey("/Contents");
  if (!contents.isStream() && !contents.isArray())
  {
    fprintf(stderr, "ERROR: Malformed PDF.\n");
    return;
  }

  // prepare the new stream which is to be prepended
  PointerHolder<Buffer> stream_data = PointerHolder<Buffer>(new Buffer(len));
  memcpy(stream_data->getBuffer(), buf, len);
  QPDFObjectHandle stream = QPDFObjectHandle::newStream(pdf, stream_data);
  stream = pdf->makeIndirectObject(stream);

  // if the contents is an array, prepend the new stream to the array,
  // else convert the contents to an array and the do the same.
  if (contents.isStream())
  {
    QPDFObjectHandle old_streamdata = contents;
    contents = QPDFObjectHandle::newArray();
    contents.appendItem(old_streamdata);
  }

  contents.insertItem(0, stream);
  page.replaceKey("/Contents", contents);
}


// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
// static Object name_object(const char *s)
// {
//     return Object(new GooString(s));
// }
// #else
// static Object * name_object(const char *s)
// {
//     Object *o = new Object();
//     o->initName((char *)s);
//     return o;
// }
// #endif

// /*
//  * Create new PDF integer type object.
//  */
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
// static Object int_object(int i)
// {
//     return Object(i);
// }
// #else
// static Object * int_object(int i)
// {
//     Object *o = new Object();
//     o->initInt(i);
//     return o;
// }
// #endif

// static Object * get_resource_dict(XRef *xref,
//                                   Dict *pagedict,
//                                   Object *resdict,
//                                   Ref *resref)
// {
//     Object res;

//     /* TODO resource dict can also be inherited */
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     res = pagedict->lookupNF("Resources");
//     if (res.isNull())
// #else
//     if (!pagedict->lookupNF("Resources", &res))
// #endif
//         return NULL;

//     if (res.isRef()) {
//         *resref = res.getRef();
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         *resdict = xref->fetch(resref->num, resref->gen);
// #else
//         xref->fetch(resref->num, resref->gen, resdict);
// #endif
//     }
//     else if (res.isDict()) {
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         *resdict = res.copy();
// #else
//         res.copy(resdict);
// #endif
//         resref->num = 0;
//     }
//     else
//         resdict = NULL;

// #if POPPLER_VERSION_MAJOR <= 0 && POPPLER_VERSION_MINOR < 58
//     res.free();
// #endif
//     return resdict;
// }


/**
 * 'pdf_add_type1_font()' - Add the specified type1 fontface to the specified
 *                          page in a PDF document.
 * I - QPDF object
 * I - page number of the page to which the font is to be added
 * I - name of the font to be added
 */
extern "C" void pdf_add_type1_font(pdf_t *pdf,
                                   unsigned page_num,
                                   const char *name)
{
  std::vector<QPDFObjectHandle> pages = pdf->getAllPages();
  if (pages.empty() || page_num > pages.size()) {
    fprintf(stderr, "ERROR: Unable to add type1 font to requested PDF page\n");
    return;
  }

  QPDFObjectHandle page = pages[page_num - 1];

  QPDFObjectHandle resources = page.getKey("/Resources");
  if (!resources.isDictionary())
  {
    fprintf(stderr, "ERROR: Malformed PDF.\n");
    return;
  }

  QPDFObjectHandle font = QPDFObjectHandle::newDictionary();
  font.replaceKey("/Type", QPDFObjectHandle::newName("/Font"));
  font.replaceKey("/Subtype", QPDFObjectHandle::newName("/Type1"));
  font.replaceKey("/BaseFont",
                  QPDFObjectHandle::newName(std::string("/") + std::string(name)));

  QPDFObjectHandle fonts = resources.getKey("/Font");
  if (fonts.isNull())
  {
    fonts = QPDFObjectHandle::newDictionary();
  }
  else if (!fonts.isDictionary())
  {
    fprintf(stderr, "ERROR: Can't recognize Font resource in PDF template.\n");
    return;
  }

  font = pdf->makeIndirectObject(font);
  fonts.replaceKey("/bannertopdf-font", font);
  resources.replaceKey("/Font", fonts);
}


/**
 * 'dict_lookup_rect()' - Lookup for an array of rectangle dimensions in a QPDF
 *                        dictionary object. If it is found, store the values in
 *                        an array and return true, else return false.
 * O - True / False, depending on whether the key is found in the dictionary
 * I - PDF dictionary object
 * I - Key to lookup for
 * I - array to store values if key is found
 */
static bool dict_lookup_rect(QPDFObjectHandle object,
                             std::string const& key,
                             float rect[4])
{
  // preliminary checks
  if (!object.isDictionary() || !object.hasKey(key))
    return false;

  // check if the key is array or some other type
  QPDFObjectHandle value = object.getKey(key);
  if (!value.isArray())
    return false;
  
  // get values in a vector and assign it to rect
  std::vector<QPDFObjectHandle> array = value.getArrayAsVector();
  for (int i = 0; i < 4; ++i) {
    // if the value in the array is not real, we have an invalid array 
    if (!array[i].isReal() && !array[i].isInteger())
      return false;
    
    rect[i] = array[i].getNumericValue();
  }

  return array.size() == 4;
}


// static void dict_set_rect(XRef *xref,
//                           Object *dict,
//                           const char *key,
//                           float rect[4])
// {
//     Object array;
//     int i;

// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     array = Object(new Array(xref));

//     for (i = 0; i < 4; i++) {
//         array.arrayAdd(Object(static_cast<double>(rect[i])));
//     }

//     dict->dictSet(key, std::move(array));
// #else
//     array.initArray(xref);

//     for (i = 0; i < 4; i++) {
//         Object el;
//         el.initReal(rect[i]);
//         array.arrayAdd(&el);
//     }

//     dict->dictSet(key, &array);
// #endif
// }


/**
 * 'fit_rect()' - Update the scale of the page using old media box dimensions
 *                and new media box dimensions.
 * I - Old media box
 * I - New media box
 * I - Pointer to scale which needs to be updated
 */
static void fit_rect(float oldrect[4],
                     float newrect[4],
                     float *scale)
{
  float oldwidth = oldrect[2] - oldrect[0];
  float oldheight = oldrect[3] - oldrect[1];
  float newwidth = newrect[2] - newrect[0];
  float newheight = newrect[3] - newrect[1];

  *scale = newwidth / oldwidth;
  if (oldheight * *scale > newheight)
    *scale = newheight / oldheight;
}


/**
 * 'pdf_resize_page()' - Resize page in a PDF with the given dimensions.
 * I - Pointer to QPDF object
 * I - Page number
 * I - Width of page to set
 * I - Length of page to set
 * I - Scale of page to set
 */
extern "C" void pdf_resize_page (pdf_t *pdf,
                                 unsigned page_num,
                                 float width,
                                 float length,
                                 float *scale)
{
  std::vector<QPDFObjectHandle> pages = pdf->getAllPages();
  if (pages.empty() || page_num > pages.size()) {
    fprintf(stderr, "ERROR: Unable to resize requested PDF page\n");
    return;
  }

  QPDFObjectHandle page = pages[page_num - 1];
  float new_mediabox[4] = { 0.0, 0.0, width, length };
  float old_mediabox[4];
  QPDFObjectHandle media_box;

  if (!dict_lookup_rect(page, "/MediaBox", old_mediabox)) {
    fprintf(stderr, "ERROR: pdf doesn't contain a valid mediabox\n");
    return;
  }

  fit_rect(old_mediabox, new_mediabox, scale);
  media_box = makeRealBox(new_mediabox);

  page.replaceKey("/ArtBox", media_box);
  page.replaceKey("/BleedBox", media_box);
  page.replaceKey("/CropBox", media_box);
  page.replaceKey("/MediaBox", media_box);
  page.replaceKey("/TrimBox", media_box);
}


/**
 * 'pdf_duplicate_page()' - Duplicate a specified pdf page in a PDF
 * I - Pointer to QPDF object
 * I - page number of the page to be duplicated
 * I - number of copies to be duplicated
 */
extern "C" void pdf_duplicate_page (pdf_t *pdf,
                                    unsigned page_num,
                                    unsigned count)
{
  std::vector<QPDFObjectHandle> pages = pdf->getAllPages();
  if (pages.empty() || page_num > pages.size()) {
    fprintf(stderr, "ERROR: Unable to duplicate requested PDF page\n");
    return;
  }

  QPDFObjectHandle page = pages[page_num - 1];
  for (unsigned i = 0; i < count; ++i)
  {
    page = pdf->makeIndirectObject(page);
    pdf->addPage(page, false);
  }
}


/**
 * 'pdf_write()' - Write the contents of PDF object to an already open FILE*.
 * I - pointer to QPDF structure
 * I - File pointer to write to
 */
extern "C" void pdf_write(pdf_t *pdf, FILE *file)
{
  QPDFWriter output(*pdf, "pdf_write", file, false);
  output.write();
}



/*
 * 'lookup_opt()' - Get value according to key in the options list.
 * I - pointer to the opt_t type list
 * I - key to be found in the list
 * O - character string which corresponds to the value of the key or
 *     NULL if key is not found in the list.
 */
const char *lookup_opt(opt_t *opt, const char *key) {
    if ( ! opt || ! key ) {
        return NULL;
    }

    while (opt) {
        if (opt->key && opt->val) {
            if ( strcmp(opt->key, key) == 0 ) {
                return opt->val;
            }
        }
        opt = opt->next;
    }

    return NULL;
}


/*
 * 'pdf_fill_form()' -  1. Lookup in PDF template file for form.
 *                      2. Lookup for form fields' names.
 *                      3. Fill recognized fields with information.
 * I - Pointer to the QPDF structure
 * I - Pointer to the opt_t type list
 * O - status of form fill - 0 for failure, 1 for success
 */
extern "C" int pdf_fill_form(pdf_t *doc, opt_t *opt)
{
    // initialize AcroFormDocumentHelper and PageDocumentHelper objects
    // to work with forms in the PDF
    QPDFAcroFormDocumentHelper afdh(*doc);
    QPDFPageDocumentHelper pdh(*doc);

    // check if the PDF has a form or not
    if ( !afdh.hasAcroForm() ) {
        fprintf(stderr, "PDF template file doesn't have form. It's okay.\n");
        return 0;
    }

    // get the first page from the PDF to fill the form. Since this
    // is a banner file,it must contain only a single page, and that
    // check has already been performed in the `pdf_load_template()` function
    std::vector<QPDFPageObjectHelper> pages = pdh.getAllPages();
    if (pages.empty()) {
        fprintf(stderr, "Can't get page from PDF tamplate file.\n");
        return 0;
    }
    QPDFPageObjectHelper page = pages.front();

    // get the annotations in the page
    std::vector<QPDFAnnotationObjectHelper> annotations =
                  afdh.getWidgetAnnotationsForPage(page);

    for (std::vector<QPDFAnnotationObjectHelper>::iterator annot_iter =
                     annotations.begin();
                 annot_iter != annotations.end(); ++annot_iter) {
        // For each annotation, find its associated field. If it's a
        // text field, we try to set its value. This will automatically
        // update the document to indicate that appearance streams need
        // to be regenerated. At the time of this writing, qpdf doesn't
        // have any helper code to assist with appearance stream generation,
        // though there's nothing that prevents it from being possible.
        QPDFFormFieldObjectHelper ffh =
            afdh.getFieldForAnnotation(*annot_iter);
        if (ffh.getFieldType() == "/Tx") {
            // Lookup the options setting for value of this field and fill the
            // value accordingly. This will automatically set
            // /NeedAppearances to true.
            const char *name = ffh.getFullyQualifiedName().c_str();
            const char *fill_with = lookup_opt(opt, name);
            if (! fill_with) {
                fprintf(stderr, "Lack information for widget: %s.\n", name);
                fill_with = "N/A";
            }
            fprintf(stderr, "Fill widget name %s with value %s.\n", name, fill_with);
            ffh.setV(fill_with);
        }
    }

    // status 1 notifies that the function successfully filled all the
    // identifiable fields in the form
    return 1;
}


// /*
//  * 1. Lookup in PDF template file for form.
//  * 2. Lookup for form fields' names.
//  * 3. Fill recognized fields with information.
//  */
// extern "C" int pdf_fill_form(pdf_t *doc, opt_t *opt)
// {
//     XRef *xref = doc->getXRef();
//     Catalog *catalog = doc->getCatalog();
//     Catalog::FormType form_type = catalog->getFormType();
//     if ( form_type == Catalog::NoForm ) {
//         fprintf(stderr, "PDF template file doesn't have form. It's okay.\n");
//         return 0;
//     }

//     Page *page = catalog->getPage(1);
//     if ( !page ) {
//         fprintf(stderr, "Can't get page from PDF tamplate file.\n");
//         return 0;
//     }
//     Object pageobj;
//     Ref pageref = page->getRef();
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     pageobj = xref->fetch(pageref.num, pageref.gen);
// #else
//     xref->fetch(pageref.num, pageref.gen, &pageobj);
// #endif

//     const char *font_size = lookup_opt(opt, "banner-font-size");
//     if ( ! font_size ) {
//         /* Font size isn't specified use next one. */
//         font_size = "14";
//     }

//     /* Embed font into PDF */
//     const char *font = lookup_opt(opt, "banner-font");
//     if ( ! font ) {
//         /* Font isn't specified use next one. */
//         font = "FreeMono";
//     }
//     int res = pdf_embed_font(doc, page, font);
//     if ( ! res ) {
//         fprintf(stderr, "Can't integrate %s font into PDF file.\n", font);
//         return 0;
//     }

//     /* Page's resources dictionary */
//     Object resdict;
//     Ref resref;
  
//     get_resource_dict(xref, pageobj.getDict(), &resdict, &resref);

//     FormPageWidgets *widgets = page->getFormWidgets();
//     if ( !widgets ) {
//         fprintf(stderr, "Can't get page's widgets.\n");
//         return 0;
//     }
//     int num_widgets = widgets->getNumWidgets();

//     /* Go through widgets and fill them as necessary */
//     for (int i=0; i < num_widgets; ++i)
//     {
//         FormWidget *fm = widgets->getWidget(i);

//         /* Take into consideration only Text widgets */
//         if ( fm->getType() != formText ) {
//             continue;
//         }

//         FormWidgetText *fm_text = static_cast<FormWidgetText*>(fm);

//         /* Ignore R/O widget */
//         if ( fm_text->isReadOnly() ) {
//             continue;
//         }

//         FormField *ff = fm_text->getField();
//         GooString *field_name;
//         field_name = ff->getFullyQualifiedName();
//         if ( ! field_name )
//             field_name = ff->getPartialName();
//         if ( ! field_name ) {
//             fprintf(stderr, "Ignore widget #%d (unknown name)\n", i);
//             continue;
//         }

//         const char *name = field_name->getCString();
//         const char *fill_with = lookup_opt(opt, name);
//         if ( ! fill_with ) {
//             fprintf(stderr, "Lack information for widget: %s.\n", name);
//             fill_with = "N/A";
//         }

//         fprintf(stderr, "Fill widget name %s with value %s.\n", name, fill_with);

//         unsigned short *fill_with_w;
//         int len = utf8_to_utf16(fill_with, &fill_with_w);
//         if ( !len ) {
//             fprintf(stderr, "Bad data for widget: %s.\n", name);
//             continue;
//         }

//         GooString *content = new GooString((char*)fill_with_w, len);
//         fm_text->setContent(content);

//         /* Object for form field */
//         Object *field_obj = ff->getObj();
//         Ref field_ref = ff->getRef();

//         /* Construct appearance object in form of: "/stanv_font 12 Tf" */
//         GooString *appearance = new GooString();
//         appearance->append("/stanv_font ");
//         appearance->append(font_size);
//         appearance->append(" Tf");

//         /* Modify field's appearance */
//         Object appearance_obj;
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         field_obj->getDict()->set("DA", Object(appearance));
// #else
//         appearance_obj.initString(appearance);
//         field_obj->getDict()->set("DA", &appearance_obj);
// #endif

//         /*
//          * Create /AP - entry stuff.
//          * This is right way to display characters other then Latin1
//          */

//         /* UTF8 '/0' ending string */
//         const char *ptr_text = fill_with;

//         GooString *ap_text = new GooString("<");
//         while ( *ptr_text ) {
//             int unicode;
//             /* Get next character in Unicode */
//             ptr_text = get_next_wide(ptr_text, &unicode);
//             const unsigned short gid = emb_get(Font, unicode);
//             char text[5];
//             memset(text, 0, sizeof(text));
//             sprintf(text,"%04x", gid);
//             ap_text->append(text, 4);
//         }
//         ap_text->append("> Tj\n");

//         /* Create empty string for stream */
//         GooString *appearance_stream = new GooString();

//         /* Orde has matter */
//         appearance_stream->append("/Tx BMC \n");
//         appearance_stream->append("BT\n");	// Begin text object
//         appearance_stream->append("/stanv_font ");
//         appearance_stream->append(font_size);
//         appearance_stream->append(" Tf\n");
//         appearance_stream->append("2 12.763 Td\n");
//         appearance_stream->append(ap_text);
//         appearance_stream->append("ET\n");
//         appearance_stream->append("EMC\n");

//         Object appearance_stream_dic;
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         appearance_stream_dic = Object(new Dict(xref));
// #else
//         appearance_stream_dic.initDict(xref);
// #endif

//         /*
//          * Appearance stream dic.
//          * See: 4.9 Form XObjects
//          * TABLE 4.41 Additional entries specific to a type 1 form dictionary
//          */
//         appearance_stream_dic.dictSet("Type", name_object("XObject"));
//         appearance_stream_dic.dictSet("Subtype", name_object("Form"));
//         appearance_stream_dic.dictSet("FormType", int_object(1));
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         appearance_stream_dic.dictSet("Resources", Object(resref.num, resref.gen));
// #else
//         Object obj_ref_x;
//         obj_ref_x.initRef(resref.num, resref.gen);
//         appearance_stream_dic.dictSet("Resources", &obj_ref_x);
// #endif

//         /* BBox array: TODO. currently out of the head. */
//         Object array;
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         array = Object(new Array(xref));
//         array.arrayAdd(Object(0.0));
//         array.arrayAdd(Object(0.0));
//         array.arrayAdd(Object(237.0));
//         array.arrayAdd(Object(25.0));

//         appearance_stream_dic.dictSet("BBox", std::move(array));
//         appearance_stream_dic.dictSet("Length", Object(appearance_stream->getLength()));

//         MemStream *mem_stream = new MemStream(appearance_stream->getCString(),
//                 0, appearance_stream->getLength(), std::move(appearance_stream_dic));

//         /* Make obj stream */
//         Object stream = Object(static_cast<Stream *>(mem_stream));

//         Ref r = xref->addIndirectObject(&stream);

//         /* Update Xref table */
//         Object obj_ref = Object(r.num, r.gen);

//         /*
//          * Fill Annotation's appearance streams dic /AP
//          * See: 8.4.4 Appearance Streams
//          */
//         Object appearance_streams_dic = Object(new Dict(xref));
//         appearance_streams_dic.dictSet("N", std::move(obj_ref));

//         field_obj->getDict()->set("AP", std::move(appearance_streams_dic));
// #else
//         array.initArray(xref);
//         Object el;
//         el.initReal(0);
//         array.arrayAdd(&el);
//         el.initReal(0);
//         array.arrayAdd(&el);
//         el.initReal(237);
//         array.arrayAdd(&el);
//         el.initReal(25);
//         array.arrayAdd(&el);
//         appearance_stream_dic.dictSet("BBox", &array);
//         appearance_stream_dic.dictSet("Length", int_object(appearance_stream->getLength()));

//         MemStream *mem_stream = new MemStream(appearance_stream->getCString(),
//                 0, appearance_stream->getLength(), &appearance_stream_dic);

//         /* Make obj stream */
//         Object stream;
//         stream.initStream(mem_stream);

//         Ref r;
//         r = xref->addIndirectObject(&stream);

//         /* Update Xref table */
//         Object obj_ref;
//         obj_ref.initRef(r.num, r.gen);

//         /* 
//          * Fill Annotation's appearance streams dic /AP
//          * See: 8.4.4 Appearance Streams
//          */
//         Object appearance_streams_dic;
//         appearance_streams_dic.initDict(xref);
//         appearance_streams_dic.dictSet("N", &obj_ref);

//         field_obj->getDict()->set("AP", &appearance_streams_dic);
// #endif

//         /* Notify poppler about changes */
//         xref->setModifiedObject(field_obj, field_ref);
//     }

//     /*
//      * Adjust form's NeedAppearances flag.
//      * We need to fill form's fields with specified font.
//      * The right way to this is via /AP.
//      *
//      * false - is default value for PDF. See:
//      * PDFReference.pdf - table 8.47 Entries in the interactive form dictionary
//      *
//      * OpenOffice - by default sets it to 'true'.
//      */
//     Object *obj_form = catalog->getAcroForm();
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     obj_form->dictSet("NeedAppearances", Object(gFalse));
// #else
//     Object obj1;
//     obj1.initBool(gFalse);
//     obj_form->dictSet("NeedAppearances", &obj1);
// #endif

//     /* Add AccroForm as indirect obj */
//     Ref ref_form = xref->addIndirectObject(obj_form);

//     /*
//      * So update Catalog object.
//      */
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     Object catObj = xref->getCatalog();
// #else
//     Object* catObj = new Object();
//     catObj = xref->getCatalog(catObj);
// #endif
//     Ref catRef;
//     catRef.gen = xref->getRootGen();
//     catRef.num = xref->getRootNum();
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     catObj.dictSet("AcroForm", Object(ref_form.num, ref_form.gen));
//     xref->setModifiedObject(&catObj, catRef);
// #else
//     Object obj2;
//     obj2.initRef(ref_form.num, ref_form.gen);
//     catObj->dictSet("AcroForm", &obj2);
//     xref->setModifiedObject(catObj, catRef);
// #endif

//     /* Success */
//     return 1;
// }

// /* Embeded font into PDF */
// static int pdf_embed_font(pdf_t *doc,
//         Page *page,
//         const char *typeface) {

//     /* Load font using libfontconfig */
//     Font = get_font(typeface);
//     if ( ! Font ) {
//         fprintf(stderr, "Can't load font: %s\n", typeface);
//         return 0;
//     }

//     /* Font's description */
//     EMB_PDF_FONTDESCR *Fdes = emb_pdf_fontdescr(Font);
//     if ( ! Fdes ) {
//         return 0;
//     }

//     /* Font's widths description */
//     EMB_PDF_FONTWIDTHS *Fwid=emb_pdf_fontwidths(Font);
//     if ( ! Fwid ) {
//         return 0;
//     }

//     /* Create empty string for stream */
//     GooString *font_stream = new GooString();

//     /* Fill stream */
//     const int outlen = emb_embed(Font, fill_font_stream, font_stream);
//     assert( font_stream->getLength() == outlen );

//     /* Get XREF table */
//     XRef *xref = doc->getXRef();

//     /* Font dictionary object for embeded font */
//     Object f_dic;
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     f_dic = Object(new Dict(xref));
// #else
//     f_dic.initDict(xref);
// #endif
//     f_dic.dictSet("Type", name_object("Font"));

//     /* Stream lenght */
//     f_dic.dictSet("Length", int_object(outlen));

//     /* Lenght for EMB_FMT_TTF font type */
//     if ( Font->outtype == EMB_FMT_TTF ) {
//         f_dic.dictSet("Length1", int_object(outlen));
//     }

//     /* Add font subtype */
//     const char *subtype = emb_pdf_get_fontfile_subtype(Font);
//     if ( subtype ) {
//         f_dic.dictSet("Subtype", name_object(copyString(subtype)));
//     }

//     /* Create memory stream font. Add it to font dic. */
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     MemStream *mem_stream = new MemStream(font_stream->getCString(),
//             0, outlen, std::move(f_dic));
//     Object stream = Object(static_cast<Stream *>(mem_stream));
// #else
//     MemStream *mem_stream = new MemStream(font_stream->getCString(),
//             0, outlen, &f_dic);

//     /* Make obj stream */
//     Object stream;
//     stream.initStream(mem_stream);
// #endif

//     Ref r;

//     /* Update Xref table */
//     r = xref->addIndirectObject(&stream);

//     /* Get page object */
//     Object pageobj;
//     Ref pageref = page->getRef();
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     pageobj = xref->fetch(pageref.num, pageref.gen);
// #else
//     xref->fetch(pageref.num, pageref.gen, &pageobj);
// #endif
//     if (!pageobj.isDict()) {
//         fprintf(stderr, "Error: malformed pdf.\n");
//         return 0;
//     }

//     /* Page's resources dictionary */
//     Object resdict;
//     Ref resref;
//     Object *ret = get_resource_dict(xref, pageobj.getDict(), &resdict, &resref);
//     if ( !ret ) {
//         fprintf(stderr, "Error: malformed pdf\n");
// #if POPPLER_VERSION_MAJOR <= 0 && POPPLER_VERSION_MINOR < 58
//         pageobj.free();
// #endif
//         return 0;
//     }

//     /* Dictionary for all fonts in page's resources */
//     Object fonts;

// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     fonts = resdict.dictLookupNF("Font");
// #else
//     resdict.dictLookupNF("Font", &fonts);
// #endif
//     if (fonts.isNull()) {
//         /* Create new one, if doesn't exists */
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         resdict.dictSet("Font", Object(new Dict(xref)));
//         fonts = resdict.dictLookupNF("Font");
// #else
//         fonts.initDict(xref);
//         resdict.dictSet("Font", &fonts);
// #endif
//         fprintf(stderr, "Create new font dict in page's resources.\n");
//     }

//     /*
//      * For embeded font there are 4 inderect objects and 4 reference obj.
//      * Each next point to previsious one.
//      * Last one object goes to Font dic.
//      *
//      * 1. Font stream obj + reference obj
//      * 2. FontDescriptor obj + reference obj
//      * 3. Width resource obj + reference obj
//      * 4. Multibyte resourcse obj + reference obj
//      *
//      */

//     /* r - indirect object refrence to dict with stream */
//     Object *font_desc_resource_dic = make_fontdescriptor_dic(doc, Font, Fdes, r);
//     r = xref->addIndirectObject(font_desc_resource_dic);

//     /* r - indirect object reference to dict font descriptor resource */
//     Object *font_resource_dic = make_font_dic(doc, Font, Fdes, Fwid, r);
//     r = xref->addIndirectObject(font_resource_dic);

//     /* r - widths resource dic */
//     Object *cidfont_resource_dic = make_cidfont_dic(doc, Font, Fdes->fontname, r);
//     r = xref->addIndirectObject(cidfont_resource_dic);

//     /* r - cid resource dic */
//     Object font_res_obj_ref;
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     font_res_obj_ref = Object(r.num, r.gen);
// #else
//     font_res_obj_ref.initRef(r.num, r.gen);
// #endif

//     Object *fonts_dic;
//     Object dereferenced_obj;

//     if ( fonts.isDict() ) {
//         /* "Font" resource is dictionary object */
//         fonts_dic = &fonts;
//     } else if ( fonts.isRef() ) {
//         /* "Font" resource is indirect reference object */
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         dereferenced_obj = xref->fetch(fonts.getRefNum(), fonts.getRefGen());
// #else
//         xref->fetch(fonts.getRefNum(), fonts.getRefGen(), &dereferenced_obj);
// #endif
//         fonts_dic = &dereferenced_obj;
//     }

//     if ( ! fonts_dic->isDict() ) {
//         fprintf(stderr, "Can't recognize Font resource in PDF template.\n");
//         return 0;
//     }

//     /* Add to fonts dic new font */
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     fonts_dic->dictSet("stanv_font", std::move(font_res_obj_ref));
// #else
//     fonts_dic->dictSet("stanv_font", &font_res_obj_ref);
// #endif

//     /* Notify poppler about changes in fonts dic */
//     if ( fonts.isRef() ) {
//         xref->setModifiedObject(fonts_dic, fonts.getRef());
//     }

//     /* Notify poppler about changes in resources dic */
//     xref->setModifiedObject(&resdict, resref);
//     fprintf(stderr, "Resource dict was changed.\n");

// #if POPPLER_VERSION_MAJOR <= 0 && POPPLER_VERSION_MINOR < 58
//     pageobj.free();
// #endif

//     /* Success */
//     return 1;
// }

// /*
//  * Call-back function to fill font stream object.
//  */
// static void fill_font_stream(const char *buf, int len, void *context)
// {
//     GooString *s = (GooString *)context;
//     s->append(buf, len);
// }

// /*
//  * Use libfontconfig to pick up suitable font.
//  * Memory should be freed.
//  * XXX: doesn't work correctly. Need to do some revise.
//  */
// static char *get_font_libfontconfig(const char *font) {
//     FcPattern *pattern = NULL;
//     FcFontSet *candidates = NULL;
//     FcChar8* found_font = NULL;
//     FcResult result;

//     FcInit ();
//     pattern = FcNameParse ((const FcChar8 *)font);

//     /* guide fc, in case substitution becomes necessary */
//     FcPatternAddInteger (pattern, FC_SPACING, FC_MONO);
//     FcConfigSubstitute (0, pattern, FcMatchPattern);
//     FcDefaultSubstitute (pattern);

//     /* Receive a sorted list of fonts matching our pattern */
//     candidates = FcFontSort (0, pattern, FcFalse, 0, &result);
//     FcPatternDestroy (pattern);

//     /* In the list of fonts returned by FcFontSort()
//        find the first one that is both in TrueType format and monospaced */
//     for (int i = 0; i < candidates->nfont; i++) {

//         /* TODO? or just try? */
//         FcChar8 *fontformat=NULL;

//         /* sane default, as FC_MONO == 100 */
//         int spacing=0;
//         FcPatternGetString(
//                 candidates->fonts[i],
//                 FC_FONTFORMAT,
//                 0,
//                 &fontformat);

//         FcPatternGetInteger(
//                 candidates->fonts[i],
//                 FC_SPACING,
//                 0,
//                 &spacing);

//         if ( (fontformat) && (spacing == FC_MONO) ) {
//             if (strcmp((const char *)fontformat, "TrueType") == 0) {
//                 found_font = FcPatternFormat (
//                         candidates->fonts[i],
//                         (const FcChar8 *)"%{file|cescape}/%{index}");

//                 /* Don't take into consideration remain candidates */
//                 break;
//             } else if (strcmp((const char *)fontformat, "CFF") == 0) {

//                 /* TTC only possible with non-cff glyphs! */
//                 found_font = FcPatternFormat (
//                         candidates->fonts[i],
//                         (const FcChar8 *)"%{file|cescape}");

//                 /* Don't take into consideration remain candidates */
//                 break;
//             }
//         }
//     }

//     FcFontSetDestroy (candidates);

//     if ( ! found_font ) {
//         fprintf(stderr,"No viable font found\n");
//         return NULL;
//     }

//     return (char *)found_font;
// }

// /*
//  * Load font file using fontembed file.
//  * Test for requirements.
//  */
// static EMB_PARAMS *load_font(const char *font) {
//     assert(font);

//     OTF_FILE *otf;
//     otf = otf_load(font);
//     if ( ! otf ) {
//         return NULL;
//     }

//     FONTFILE *ff=fontfile_open_sfnt(otf);
//     if ( ! ff ) {
//         return NULL;
//     }

//     EMB_PARAMS *emb=emb_new(ff,
//             EMB_DEST_PDF16,
//             static_cast<EMB_CONSTRAINTS>( /* bad fontembed */
//                 EMB_C_FORCE_MULTIBYTE|
//                 EMB_C_TAKE_FONTFILE|
//                 EMB_C_NEVER_SUBSET));
//     if ( ! emb ) {
//         return NULL;
//     }

//     if ( ! (emb->plan & EMB_A_MULTIBYTE) ) {
//         return NULL;
//     }

//     EMB_PDF_FONTDESCR *fdes=emb_pdf_fontdescr(emb);
//     if ( ! fdes ) {
//         return NULL;
//     }

//     /* Success */
//     return emb;
// }

// /*
//  * Return fontembed library object that corresponds requirements.
//  */
// static EMB_PARAMS *get_font(const char *font)
// {
//     assert(font);

//     char *fontname = NULL;
//     EMB_PARAMS *emb = NULL;

//     /* Font file specified. */
//     if ( (font[0]=='/') || (font[0]=='.') ) {
//         fontname = strdup(font);
//         emb = load_font(fontname);
//     }

//     /* Use libfontconfig. */
//     if ( ! emb ) {
//         fontname = get_font_libfontconfig(font);
//         emb = load_font(fontname);
//     }

//     free(fontname);

//     return emb;
// }

// /*
//  * Was taken from ./fontembed/embed_pdf.c
//  */
// static const char *emb_pdf_escape_name(const char *name, int len)
// {
//     assert(name);
//     if (len==-1) {
//         len=strlen(name);
//     }

//     /* PDF implementation limit */
//     assert(len<=127);

//     static char buf[128*3];
//     int iA,iB;
//     const char hex[]="0123456789abcdef";

//     for (iA=0,iB=0;iA<len;iA++,iB++) {
//         if ( ((unsigned char)name[iA]<33)||((unsigned char)name[iA]>126)||
//                 (strchr("#()<>[]{}/%",name[iA])) ) {
//             buf[iB]='#';
//             buf[++iB]=hex[(name[iA]>>4)&0x0f];
//             buf[++iB]=hex[name[iA]&0xf];
//         } else {
//             buf[iB]=name[iA];
//         }
//     }
//     buf[iB]=0;
//     return buf;
// }

// /*
//  * Construct font description dictionary.
//  * Translated to Poppler function emb_pdf_simple_fontdescr() from
//  * cups-filters/fontembed/embed_pdf.c
//  */
// static Object *make_fontdescriptor_dic(
//         pdf_t *doc,
//         EMB_PARAMS *emb,
//         EMB_PDF_FONTDESCR *fdes,
//         Ref fontfile_obj_ref)
// {
//     assert(emb);
//     assert(fdes);

//     /* Get XREF table */
//     XRef *xref = doc->getXRef();

//     /* Font dictionary for embeded font */
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     Object *dic = new Object(new Dict(xref));
// #else
//     Object *dic = new Object();
//     dic->initDict(xref);
// #endif

//     dic->dictSet("Type", name_object("FontDescriptor"));
//     dic->dictSet(
//             "FontName",
//             name_object(copyString(emb_pdf_escape_name(fdes->fontname,-1))));
//     dic->dictSet("Flags", int_object(fdes->flags));
//     dic->dictSet("ItalicAngle", int_object(fdes->italicAngle));
//     dic->dictSet("Ascent", int_object(fdes->ascent));
//     dic->dictSet("Descent", int_object(fdes->descent));
//     dic->dictSet("CapHeight", int_object(fdes->capHeight));
//     dic->dictSet("StemV", int_object(fdes->stemV));

//     /* FontBox array */
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     Object array = Object(new Array(xref));
//     array.arrayAdd(Object(static_cast<double>(fdes->bbxmin)));
//     array.arrayAdd(Object(static_cast<double>(fdes->bbymin)));
//     array.arrayAdd(Object(static_cast<double>(fdes->bbxmax)));
//     array.arrayAdd(Object(static_cast<double>(fdes->bbymax)));

//     dic->dictSet("FontBBox", std::move(array));
// #else
//     Object array;
//     array.initArray(xref);

//     Object el;

//     el.initReal(fdes->bbxmin);
//     array.arrayAdd(&el);

//     el.initReal(fdes->bbymin);
//     array.arrayAdd(&el);

//     el.initReal(fdes->bbxmax);
//     array.arrayAdd(&el);

//     el.initReal(fdes->bbymax);
//     array.arrayAdd(&el);

//     dic->dictSet("FontBBox", &array);
// #endif

//     if (fdes->xHeight) {
//         dic->dictSet("XHeight", int_object(fdes->xHeight));
//     }

//     if (fdes->avgWidth) {
//         dic->dictSet("AvgWidth", int_object(fdes->avgWidth));
//     }

//     if (fdes->panose) {
//         /* Font dictionary for embeded font */
//         Object style_dic;
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         style_dic = Object(new Dict(xref));
// #else
//         style_dic.initDict(xref);
// #endif

//         GooString *panose_str = new GooString(fdes->panose, 12);
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         style_dic.dictSet("Panose", Object(panose_str));

//         dic->dictSet("Style", std::move(style_dic));
// #else
//         Object panose;

//         panose.initString(panose_str);
//         style_dic.dictSet("Panose", &panose);

//         dic->dictSet("Style", &style_dic);
// #endif
//     }

// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     dic->dictSet(emb_pdf_get_fontfile_key(emb), Object(fontfile_obj_ref.num, fontfile_obj_ref.gen));
// #else
//     Object ref_obj;
//     ref_obj.initRef(fontfile_obj_ref.num, fontfile_obj_ref.gen);
//     dic->dictSet(emb_pdf_get_fontfile_key(emb), &ref_obj);
// #endif

//     return dic;
// }

// static Object *make_font_dic(
//         pdf_t *doc,
//         EMB_PARAMS *emb,
//         EMB_PDF_FONTDESCR *fdes,
//         EMB_PDF_FONTWIDTHS *fwid,
//         Ref fontdescriptor_obj_ref)
// {
//     assert(emb);
//     assert(fdes);
//     assert(fwid);

//     /* Get XREF table */
//     XRef *xref = doc->getXRef();

// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     Object *dic = new Object(new Dict(xref));
// #else
//     Object *dic = new Object();
//     dic->initDict(xref);
// #endif

//     dic->dictSet("Type", name_object("Font"));
//     dic->dictSet(
//             "Subtype",
//             name_object(copyString(emb_pdf_get_font_subtype(emb))));
//     dic->dictSet(
//             "BaseFont",
//             name_object(copyString(emb_pdf_escape_name(fdes->fontname,-1))));

// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     dic->dictSet("FontDescriptor", Object(fontdescriptor_obj_ref.num, fontdescriptor_obj_ref.gen));
// #else
//     Object ref_obj;
//     ref_obj.initRef(fontdescriptor_obj_ref.num, fontdescriptor_obj_ref.gen);
//     dic->dictSet("FontDescriptor", &ref_obj);
// #endif

//     if ( emb->plan & EMB_A_MULTIBYTE ) {
//         assert(fwid->warray);

//         Object CIDSystemInfo_dic;
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         CIDSystemInfo_dic = Object(new Dict(xref));
// #else
//         CIDSystemInfo_dic.initDict(xref);
// #endif

//         Object registry;
//         Object ordering;

//         GooString *str;

//         str = new GooString(copyString(fdes->registry));
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         CIDSystemInfo_dic.dictSet("Registry", Object(str));
// #else
//         registry.initString(str);
//         CIDSystemInfo_dic.dictSet("Registry", &registry);
// #endif

//         str = new GooString(copyString(fdes->ordering));
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         CIDSystemInfo_dic.dictSet("Ordering", Object(str));
// #else
//         ordering.initString(str);
//         CIDSystemInfo_dic.dictSet("Ordering", &ordering);
// #endif

//         CIDSystemInfo_dic.dictSet("Supplement", int_object(fdes->supplement));

// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//         dic->dictSet("CIDSystemInfo", std::move(CIDSystemInfo_dic));
// #else
//         dic->dictSet("CIDSystemInfo", &CIDSystemInfo_dic);
// #endif

//         dic->dictSet("DW", int_object(fwid->default_width));
//     }

//     return dic;
// }


// static Object *make_cidfont_dic(
//         pdf_t *doc,
//         EMB_PARAMS *emb,
//         const char *fontname,
//         Ref fontdescriptor_obj_ref)
// {
//     assert(emb);
//     assert(fontname);

//     /*
//      * For CFF: one of:
//      * UniGB-UCS2-H, UniCNS-UCS2-H, UniJIS-UCS2-H, UniKS-UCS2-H
//      */
//     const char *encoding="Identity-H";
//     const char *addenc="-";

//     if ( emb->outtype == EMB_FMT_TTF ) { // !=CidType0
//         addenc="";
//     }

//     /* Get XREF table */
//     XRef *xref = doc->getXRef();

// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     Object *dic = new Object(new Dict(xref));
// #else
//     Object *dic = new Object();
//     dic->initDict(xref);
// #endif

//     dic->dictSet("Type", name_object("Font"));
//     dic->dictSet("Subtype", name_object("Type0"));


//     GooString * basefont = new GooString();
//     basefont->append(emb_pdf_escape_name(fontname,-1));
//     basefont->append(addenc);
//     basefont->append((addenc[0])?encoding:"");

//     dic->dictSet("BaseFont",
//             name_object(copyString(basefont->getCString())));

//     dic->dictSet("Encoding", name_object(copyString(encoding)));

//     Object obj;
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     obj = Object(fontdescriptor_obj_ref.num, fontdescriptor_obj_ref.gen);
// #else
//     obj.initRef(fontdescriptor_obj_ref.num, fontdescriptor_obj_ref.gen);
// #endif

//     Object array;
// #if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 58
//     array = Object(new Array(xref));
//     array.arrayAdd(std::move(obj));

//     dic->dictSet("DescendantFonts", std::move(array));
// #else
//     array.initArray(xref);
//     array.arrayAdd(&obj);

//     dic->dictSet("DescendantFonts", &array);
// #endif

//     return dic;
// }


// /*
//  * Convert UTF8 to UTF16.
//  * Version for poppler - UTF16BE.
//  *
//  * Reference:
//  * http://stackoverflow.com/questions/7153935/how-to-convert-utf-8-stdstring-to-utf-16-stdwstring
//  */
// static int utf8_to_utf16(const char *utf8, unsigned short **out_ptr)
// {
//     unsigned long *unicode, *head;

//     int strl = strlen(utf8);

//     unicode = head = (unsigned long*) malloc(strl * sizeof(unsigned long));

//     if ( ! head ) {
//         fprintf(stderr,"stanv: 1\n");
//         return 0;
//     }

//     while (*utf8){
//         unsigned long uni;
//         size_t todo;
//         unsigned char ch = *utf8;

//         if (ch <= 0x7F)
//         {
//             uni = ch;
//             todo = 0;
//         }
//         else if (ch <= 0xBF)
//         {
//             /* not a UTF-8 string */
//             return 0;
//         }
//         else if (ch <= 0xDF)
//         {
//             uni = ch&0x1F;
//             todo = 1;
//         }
//         else if (ch <= 0xEF)
//         {
//             uni = ch&0x0F;
//             todo = 2;
//         }
//         else if (ch <= 0xF7)
//         {
//             uni = ch&0x07;
//             todo = 3;
//         }
//         else
//         {
//             /* not a UTF-8 string */
//             return 0;
//         }

//         for (size_t j = 0; j < todo; ++j)
//         {
//             utf8++;
//             if ( ! *utf8 ) {
//                 /* not a UTF-8 string */
//                 return 0;
//             }

//             unsigned char ch = *utf8;

//             if (ch < 0x80 || ch > 0xBF) {
//                 /* not a UTF-8 string */
//                 return 0;
//             }

//             uni <<= 6;
//             uni += ch & 0x3F;
//         }

//         if (uni >= 0xD800 && uni <= 0xDFFF) {
//             /* not a UTF-8 string */
//             return 0;
//         }

//         if (uni > 0x10FFFF) {
//             /* not a UTF-8 string */
//             return 0;
//         }

//         *unicode = uni;
//         unicode++;
//         utf8++;
//     }

//     ssize_t len = sizeof(unsigned short) * (unicode - head + 1) * 2;
//     unsigned short * out = (unsigned short *)malloc(len);

//     if ( ! out ) {
//         return 0;
//     }

//     *out_ptr = out;

//     while ( head != unicode ) {
//         unsigned long uni = *head;

//         if (uni <= 0xFFFF)
//         {
//             *out = (unsigned short)uni;
//             *out = ((0xff & uni) << 8) | ((uni & 0xff00) >> 8);
//         }
//         else
//         {
//             uni -= 0x10000;

//             *out += (unsigned short)((uni >> 10) + 0xD800);
//             *out = ((0xff & uni) << 8) | ((uni & 0xff00) >> 8);
//             out++;
//             *out += (unsigned short)((uni >> 10) + 0xD800);
//             *out = ((0xff & uni) << 8) | ((uni & 0xff00) >> 8);
//         }

//         head++;
//         out++;
//     }

//     return (out - *out_ptr) * sizeof (unsigned short);
// }

// const char *get_next_wide(const char *utf8, int *unicode_out) {

//     unsigned long uni;
//     size_t todo;

//     if ( !utf8 || !*utf8 ) {
//         return utf8;
//     }

//     unsigned char ch = *utf8;

//     *unicode_out = 0;

//     if (ch <= 0x7F)
//     {
//         uni = ch;
//         todo = 0;
//     }
//     else if (ch <= 0xBF)
//     {
//         /* not a UTF-8 string */
//         return utf8;
//     }
//     else if (ch <= 0xDF)
//     {
//         uni = ch&0x1F;
//         todo = 1;
//     }
//     else if (ch <= 0xEF)
//     {
//         uni = ch&0x0F;
//         todo = 2;
//     }
//     else if (ch <= 0xF7)
//     {
//         uni = ch&0x07;
//         todo = 3;
//     }
//     else
//     {
//         /* not a UTF-8 string */
//         return utf8;
//     }

//     for (size_t j = 0; j < todo; ++j)
//     {
//         utf8++;
//         if ( ! *utf8 ) {
//             /* not a UTF-8 string */
//             return utf8;
//         }

//         unsigned char ch = *utf8;

//         if (ch < 0x80 || ch > 0xBF) {
//             /* not a UTF-8 string */
//             return utf8;
//         }

//         uni <<= 6;
//         uni += ch & 0x3F;
//     }

//     if (uni >= 0xD800 && uni <= 0xDFFF) {
//         /* not a UTF-8 string */
//         return utf8;
//     }

//     if (uni > 0x10FFFF) {
//         /* not a UTF-8 string */
//         return utf8;
//     }

//     *unicode_out = (int)uni;
//     utf8++;

//     return utf8;
// }
