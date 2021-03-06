#ifndef _PARSE_H_
#define _PARSE_H_

//TODO: switch to enum
#define FRM_UNKNOWN 0    //unknown format    U
#define FRM_ISOBITMAP 1  //primary and secondary bitmaps
#define FRM_BITMAP 2     //fixed length bitmap
#define FRM_SUBFIELDS 3  //the contents is split to subfields     SF
#define FRM_TLV1 4       //TLV subfields, 1 byte tag format      TLV1
#define FRM_TLV2 5       //TLV subfields, 2 bytes tag format     TLV2
#define FRM_TLV3 6       //TLV subfields, 3 bytes tag format     TLV3
#define FRM_TLV4 7       //TLV subfields, 4 bytes tag format     TLV4
#define FRM_TLVEMV 8     //TLV subfields, EMV tag format         TLVE
#define FRM_EBCDIC 9    // EBCDIC EEEEE
#define FRM_BCD 10   //  0x012345  CCCCC  BCD
#define FRM_BIN 11   //  12345     BBBBB  BIN
#define FRM_ASCII 12  // "12345"   LLLLL  ASC
#define FRM_FIXED 13     // Fixed length   F12345
#define FRM_HEX	14	// 0x0123FD -> "0123FD"
#define FRM_BCDSF 15	// The field is first converted from BCD to ASCII, and then is split into subfields
#define FRM_BITSTR 16	//Same as BITMAP but does not define subfields
#define FRM_EMVL 17     // Length format for EMV tags
#define FRM_TLVDS 18

#include <stdlib.h>

extern int debug;

typedef struct fldformat
{
	//unsigned int number;
	unsigned int lengthFormat;
	unsigned int lengthLength;
	unsigned short lengthInclusive;
	unsigned int maxLength;
	int addLength;
	unsigned int dataFormat;
	unsigned int tagFormat;
	char *description;
	char *data;
	unsigned int maxFields;
	unsigned int fields;
	struct fldformat **fld;
	struct fldformat *altformat;
} fldformat;

typedef struct field
{
	char* data;  //parsed data
	char* tag;   //parsed TLV tag name
	unsigned int start;  //start position inside the message binary data relative to the parent field
	unsigned int blength;  //length of the field inside the message binary data (including length length)
	unsigned int length;  //parsed data length
	unsigned int fields;  //number of subfields
	fldformat *frm;  //field format
	struct field **fld;  //array of subfields
	unsigned int altformat;  //altformat number
} field;

fldformat *load_format(char*, fldformat *frmroot=NULL);
void emptyFormat(fldformat*);
void freeFormat(fldformat*);
void emptyField(field*);
void freeField(field*);
void copyFormat(fldformat *to, fldformat *from);
void mirrorFormat(fldformat *to, fldformat *from);
field *parse_message(char*, unsigned int, fldformat*);
unsigned int build_message(char*, unsigned int, field*);
unsigned int get_length(field*);
int parse_field_length(char*, unsigned int, fldformat*);
int is_empty(field*);
void print_message(field*);

int change_format(field*, fldformat*);
const char* get_field(field *fld, int n0=-1, int n1=-1, int n2=-1, int n3=-1, int n4=-1, int n5=-1, int n6=-1, int n7=-1, int n8=-1, int n9=-1);
char* add_field(field *fld, int n0=-1, int n1=-1, int n2=-1, int n3=-1, int n4=-1, int n5=-1, int n6=-1, int n7=-1, int n8=-1, int n9=-1);
int field_format(field *fld, int altformat, int n0=-1, int n1=-1, int n2=-1, int n3=-1, int n4=-1, int n5=-1, int n6=-1, int n7=-1, int n8=-1, int n9=-1);
void remove_field(field *fld,  int n0, int n1=-1, int n2=-1, int n3=-1, int n4=-1, int n5=-1, int n6=-1, int n7=-1, int n8=-1, int n9=-1);
int has_field(field *fld, int n0=-1, int n1=-1, int n2=-1, int n3=-1, int n4=-1, int n5=-1, int n6=-1, int n7=-1, int n8=-1, int n9=-1);
char* add_tag(const char *tag, field *fld, int n0=-1, int n1=-1, int n2=-1, int n3=-1, int n4=-1, int n5=-1, int n6=-1, int n7=-1, int n8=-1, int n9=-1);
const char* get_tag(field *fld, int n0=-1, int n1=-1, int n2=-1, int n3=-1, int n4=-1, int n5=-1, int n6=-1, int n7=-1, int n8=-1, int n9=-1);

#endif

