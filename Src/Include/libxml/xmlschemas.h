/*
 * Summary: incomplete XML Schemas structure implementation
 * Description: interface to the XML Schemas handling and schema validity checking, it is incomplete right now.
 *
 * Copy: See Copyright for the status of this software.
 *
 * Author: Daniel Veillard
 */
#ifndef __XML_SCHEMA_H__
#define __XML_SCHEMA_H__

#include <libxml/xmlversion.h>
#ifdef LIBXML_SCHEMAS_ENABLED
#include <libxml/tree.h>

//#ifdef __cplusplus
//extern "C" {
//#endif
/**
 * This error codes are obsolete; not used any more.
 */
typedef enum {
	XML_SCHEMAS_ERR_OK  = 0,
	XML_SCHEMAS_ERR_NOROOT      = 1,
	XML_SCHEMAS_ERR_UNDECLAREDELEM,
	XML_SCHEMAS_ERR_NOTTOPLEVEL,
	XML_SCHEMAS_ERR_MISSING,
	XML_SCHEMAS_ERR_WRONGELEM,
	XML_SCHEMAS_ERR_NOTYPE,
	XML_SCHEMAS_ERR_NOROLLBACK,
	XML_SCHEMAS_ERR_ISABSTRACT,
	XML_SCHEMAS_ERR_NOTEMPTY,
	XML_SCHEMAS_ERR_ELEMCONT,
	XML_SCHEMAS_ERR_HAVEDEFAULT,
	XML_SCHEMAS_ERR_NOTNILLABLE,
	XML_SCHEMAS_ERR_EXTRACONTENT,
	XML_SCHEMAS_ERR_INVALIDATTR,
	XML_SCHEMAS_ERR_INVALIDELEM,
	XML_SCHEMAS_ERR_NOTDETERMINIST,
	XML_SCHEMAS_ERR_CONSTRUCT,
	XML_SCHEMAS_ERR_INTERNAL,
	XML_SCHEMAS_ERR_NOTSIMPLE,
	XML_SCHEMAS_ERR_ATTRUNKNOWN,
	XML_SCHEMAS_ERR_ATTRINVALID,
	XML_SCHEMAS_ERR_VALUE,
	XML_SCHEMAS_ERR_FACET,
	XML_SCHEMAS_ERR_,
	XML_SCHEMAS_ERR_XXX
} xmlSchemaValidError;

/*
 * ATTENTION: Change xmlSchemaSetValidOptions's check
 * for invalid values, if adding to the validation
 * options below.
 */
/**
 * xmlSchemaValidOption:
 *
 * This is the set of XML Schema validation options.
 */
typedef enum {
	XML_SCHEMA_VAL_VC_I_CREATE = 1<<0 // Default/fixed: create an attribute node or an element's text node on the instance.
} xmlSchemaValidOption;
/*
    XML_SCHEMA_VAL_XSI_ASSEMBLE			= 1<<1,
 * assemble schemata using
 * xsi:schemaLocation and
 * xsi:noNamespaceSchemaLocation
 */

/**
 * The schemas related types are kept internal
 */
typedef struct _xmlSchema xmlSchema;
typedef xmlSchema * xmlSchemaPtr;
/**
 * xmlSchemaValidityErrorFunc:
 * @ctx: the validation context
 * @msg: the message
 * @...: extra arguments
 *
 * Signature of an error callback from an XSD validation
 */
typedef void (XMLCDECL *xmlSchemaValidityErrorFunc)(void * ctx, const char * msg, ...) LIBXML_ATTR_FORMAT(2, 3);
/**
 * xmlSchemaValidityWarningFunc:
 * @ctx: the validation context
 * @msg: the message
 * @...: extra arguments
 *
 * Signature of a warning callback from an XSD validation
 */
typedef void (XMLCDECL *xmlSchemaValidityWarningFunc)(void * ctx, const char * msg, ...) LIBXML_ATTR_FORMAT(2, 3);
/**
 * A schemas validation context
 */
//typedef struct _xmlSchemaParserCtxt xmlSchemaParserCtxt;
struct xmlSchemaParserCtxt;
typedef xmlSchemaParserCtxt * xmlSchemaParserCtxtPtr;
typedef struct _xmlSchemaValidCtxt xmlSchemaValidCtxt;
//typedef xmlSchemaValidCtxt * xmlSchemaValidCtxtPtr;
/**
 * xmlSchemaValidityLocatorFunc:
 * @ctx: user provided context
 * @file: returned file information
 * @line: returned line information
 *
 * A schemas validation locator, a callback called by the validator.
 * This is used when file or node informations are not available
 * to find out what file and line number are affected
 *
 * Returns: 0 in case of success and -1 in case of error
 */
typedef int (XMLCDECL *xmlSchemaValidityLocatorFunc)(void * ctx, const char ** file, unsigned long * line);
/*
 * Interfaces for parsing.
 */
XMLPUBFUN xmlSchemaParserCtxtPtr xmlSchemaNewParserCtxt(const char * URL);
XMLPUBFUN xmlSchemaParserCtxtPtr xmlSchemaNewMemParserCtxt(const char * buffer, int size);
XMLPUBFUN xmlSchemaParserCtxtPtr xmlSchemaNewDocParserCtxt(xmlDoc * doc);
XMLPUBFUN void xmlSchemaFreeParserCtxt(xmlSchemaParserCtxtPtr ctxt);
XMLPUBFUN void xmlSchemaSetParserErrors(xmlSchemaParserCtxtPtr ctxt, xmlSchemaValidityErrorFunc err, xmlSchemaValidityWarningFunc warn, void * ctx);
XMLPUBFUN void xmlSchemaSetParserStructuredErrors(xmlSchemaParserCtxtPtr ctxt, xmlStructuredErrorFunc serror, void * ctx);
XMLPUBFUN int xmlSchemaGetParserErrors(xmlSchemaParserCtxtPtr ctxt, xmlSchemaValidityErrorFunc * err, xmlSchemaValidityWarningFunc * warn, void ** ctx);
XMLPUBFUN int xmlSchemaIsValid(xmlSchemaValidCtxt * ctxt);

XMLPUBFUN xmlSchemaPtr xmlSchemaParse(xmlSchemaParserCtxtPtr ctxt);
XMLPUBFUN void FASTCALL xmlSchemaFree(xmlSchema * schema);
#ifdef LIBXML_OUTPUT_ENABLED
	XMLPUBFUN void xmlSchemaDump(FILE * output, xmlSchemaPtr schema);
#endif /* LIBXML_OUTPUT_ENABLED */
/*
 * Interfaces for validating
 */
XMLPUBFUN void xmlSchemaSetValidErrors(xmlSchemaValidCtxt * ctxt, xmlSchemaValidityErrorFunc err, xmlSchemaValidityWarningFunc warn, void * ctx);
XMLPUBFUN void xmlSchemaSetValidStructuredErrors(xmlSchemaValidCtxt * ctxt, xmlStructuredErrorFunc serror, void * ctx);
XMLPUBFUN int xmlSchemaGetValidErrors(xmlSchemaValidCtxt * ctxt, xmlSchemaValidityErrorFunc * err, xmlSchemaValidityWarningFunc * warn, void ** ctx);
XMLPUBFUN int xmlSchemaSetValidOptions(xmlSchemaValidCtxt * ctxt, int options);
XMLPUBFUN void xmlSchemaValidateSetFilename(xmlSchemaValidCtxt * vctxt, const char * filename);
XMLPUBFUN int xmlSchemaValidCtxtGetOptions(xmlSchemaValidCtxt * ctxt);
XMLPUBFUN xmlSchemaValidCtxt * xmlSchemaNewValidCtxt(xmlSchemaPtr schema);
XMLPUBFUN void FASTCALL xmlSchemaFreeValidCtxt(xmlSchemaValidCtxt * ctxt);
XMLPUBFUN int xmlSchemaValidateDoc(xmlSchemaValidCtxt * ctxt, xmlDoc * instance);
XMLPUBFUN int xmlSchemaValidateOneElement(xmlSchemaValidCtxt * ctxt, xmlNode * elem);
XMLPUBFUN int xmlSchemaValidateStream(xmlSchemaValidCtxt * ctxt, xmlParserInputBuffer * input, xmlCharEncoding enc, xmlSAXHandler * sax, void * user_data);
XMLPUBFUN int xmlSchemaValidateFile(xmlSchemaValidCtxt * ctxt, const char * filename, int options);
XMLPUBFUN xmlParserCtxt * xmlSchemaValidCtxtGetParserCtxt(xmlSchemaValidCtxt * ctxt);
/*
 * Interface to insert Schemas SAX validation in a SAX stream
 */
typedef struct _xmlSchemaSAXPlug xmlSchemaSAXPlugStruct;
typedef xmlSchemaSAXPlugStruct * xmlSchemaSAXPlugPtr;

XMLPUBFUN xmlSchemaSAXPlugPtr xmlSchemaSAXPlug(xmlSchemaValidCtxt * ctxt, xmlSAXHandler ** sax, void ** user_data);
XMLPUBFUN int xmlSchemaSAXUnplug(xmlSchemaSAXPlugPtr plug);
XMLPUBFUN void xmlSchemaValidateSetLocator(xmlSchemaValidCtxt * vctxt, xmlSchemaValidityLocatorFunc f, void * ctxt);

//#ifdef __cplusplus
//}
//#endif

#endif /* LIBXML_SCHEMAS_ENABLED */
#endif /* __XML_SCHEMA_H__ */
