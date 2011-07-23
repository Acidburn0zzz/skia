/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SkPDFDocument_DEFINED
#define SkPDFDocument_DEFINED

#include "SkPDFTypes.h"
#include "SkRefCnt.h"
#include "SkTDArray.h"
#include "SkTScopedPtr.h"

class SkPDFCatalog;
class SkPDFDevice;
class SkPDFPage;
class SkWSteam;

/** \class SkPDFDocument

    A SkPDFDocument assembles pages together and generates the final PDF file.
*/
class SkPDFDocument {
public:
    enum Flags {
        kNoCompression_Flag = 0x01,  //!< mask disable stream compression.
        kNoEmbedding_Flag   = 0x02,  //!< mask do not embed fonts.

        kDraftMode_Flags    = 0x03,
    };
    /** Create a PDF document.
     */
    explicit SK_API SkPDFDocument(Flags flags = (Flags)0);
    SK_API ~SkPDFDocument();

    /** Output the PDF to the passed stream.  It is an error to call this (it
     *  will return false and not modify stream) if no pages have been added
     *  or there are pages missing (i.e. page 1 and 3 have been added, but not
     *  page 2).
     *
     *  @param stream    The writable output stream to send the PDF to.
     */
    SK_API bool emitPDF(SkWStream* stream);

    /** Sets the specific page to the passed PDF device. If the specified
     *  page is already set, this overrides it. Returns true if successful.
     *  Will fail if the document has already been emitted.
     *
     *  @param pageNumber The position to add the passed device (1 based).
     *  @param pdfDevice  The page to add to this document.
     */
    SK_API bool setPage(int pageNumber, SkPDFDevice* pdfDevice);

    /** Append the passed pdf device to the document as a new page.  Returns
     *  true if successful.  Will fail if the document has already been emitted.
     *
     *  @param pdfDevice The page to add to this document.
     */
    SK_API bool appendPage(SkPDFDevice* pdfDevice);

    /** Get the list of pages in this document.
     */
    SK_API const SkTDArray<SkPDFPage*>& getPages();

private:
    SkTScopedPtr<SkPDFCatalog> fCatalog;
    int64_t fXRefFileOffset;

    SkTDArray<SkPDFPage*> fPages;
    SkTDArray<SkPDFDict*> fPageTree;
    SkRefPtr<SkPDFDict> fDocCatalog;
    SkTDArray<SkPDFObject*> fPageResources;
    int fSecondPageFirstResourceIndex;

    SkRefPtr<SkPDFDict> fTrailerDict;

    /** Output the PDF header to the passed stream.
     *  @param stream    The writable output stream to send the header to.
     */
    void emitHeader(SkWStream* stream);

    /** Get the size of the header.
     */
    size_t headerSize();

    /** Output the PDF footer to the passed stream.
     *  @param stream    The writable output stream to send the footer to.
     *  @param objCount  The number of objects in the PDF.
     */
    void emitFooter(SkWStream* stream, int64_t objCount);
};

#endif
