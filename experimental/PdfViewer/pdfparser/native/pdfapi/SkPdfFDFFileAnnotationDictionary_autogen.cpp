#include "SkPdfFDFFileAnnotationDictionary_autogen.h"


#include "SkPdfNativeDoc.h"
int64_t SkPdfFDFFileAnnotationDictionary::Page(SkPdfNativeDoc* doc) {
  SkPdfNativeObject* ret = get("Page", "");
  if (doc) {ret = doc->resolveReference(ret);}
  if ((ret != NULL && ret->isInteger()) || (doc == NULL && ret != NULL && ret->isReference())) return ret->intValue();
  // TODO(edisonn): warn about missing default value for optional fields
  return 0;
}

bool SkPdfFDFFileAnnotationDictionary::has_Page() const {
  return get("Page", "") != NULL;
}

