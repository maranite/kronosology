/*
 * libxml2_host_stubs.cpp  -  inert host-build stand-ins for the real
 * libxml2 xmlTextReader* API that kontakt_xml.cpp (CKontaktXml) calls into.
 *
 * The real Kronos/Eva runtime links against a real libxml2.so; this project's
 * Makefile links every verify/test_*.cpp binary against the FULL object set
 * (see Makefile's `verify` target), so leaving these symbols genuinely
 * unresolved -- appropriate for a not-yet-reconstructed Eva-internal callee,
 * per this project's established "many Unknown symbol warnings expected"
 * `make link` convention -- would instead break the link step of every OTHER
 * class's verify test too, since this build host has no i386 (-m32) libxml2
 * package installed to link against (only amd64). These bodies do nothing
 * real; CKontaktXml's own host KAT test (verify/test_kontakt_xml.cpp) only
 * exercises the pure/leaf methods that never call through here -- see that
 * file's own header comment.
 */

extern "C" {

struct _xmlTextReader;
typedef struct _xmlTextReader *xmlTextReaderPtr;

xmlTextReaderPtr xmlNewTextReaderFilename(const char *) { return 0; }
void xmlFreeTextReader(xmlTextReaderPtr) {}
int xmlTextReaderRead(xmlTextReaderPtr) { return 0; }
int xmlTextReaderNext(xmlTextReaderPtr) { return 0; }
int xmlTextReaderNodeType(xmlTextReaderPtr) { return 0; }
int xmlTextReaderIsEmptyElement(xmlTextReaderPtr) { return 0; }
int xmlTextReaderDepth(xmlTextReaderPtr) { return 0; }
int xmlTextReaderAttributeCount(xmlTextReaderPtr) { return 0; }
int xmlTextReaderMoveToAttributeNo(xmlTextReaderPtr, int) { return 0; }
int xmlTextReaderGetParserLineNumber(xmlTextReaderPtr) { return 0; }
unsigned char *xmlTextReaderName(xmlTextReaderPtr) { return 0; }
unsigned char *xmlTextReaderValue(xmlTextReaderPtr) { return 0; }
unsigned char *xmlStrdup(const unsigned char *) { return 0; }

static void HostStubXmlFree(void *) {}
void (*xmlFree)(void *) = HostStubXmlFree; /* real symbol is libxml2's own function-pointer variable */

} /* extern "C" */
