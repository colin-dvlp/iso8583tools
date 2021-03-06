#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

//unsigned int build_message(char*, unsigned int, field*, fldformat*);
unsigned int build_field(char*, unsigned int, field*);
unsigned int build_field_alt(char*, unsigned int, field*);
unsigned int build_ebcdic(char*, char*, unsigned int);
unsigned int build_hex(char*, char*, unsigned int);
unsigned int build_bcdl(char*, char*, unsigned int);
unsigned int build_bcdr(char*, char*, unsigned int);
unsigned int build_isobitmap(char*, unsigned int, field*, unsigned int);
unsigned int build_bitmap(char*, unsigned int, field*, unsigned int);

unsigned int build_message(char *buf, unsigned int maxlength, field *fld)
{
	return build_field(buf, maxlength, fld);
}

unsigned int get_length(field *fld)
{
	unsigned int lenlen=0;
	unsigned int blength=0;
	unsigned int flength=0;
	unsigned int i, j, pos, sflen, taglength;
	int bitmap_found=-1;
	fldformat *frm;
	fldformat tmpfrm;

	if(!fld)
	{
		if(debug)
			printf("Error: No fld\n");
		return 0;
	}

	frm=fld->frm;

	if(!frm)
	{
		if(debug)
			printf("Error: No frm\n");
		return 0;
	}

	if(fld->data)
		flength=strlen(fld->data);
//	else if(frm->lengthFormat==FRM_FIXED)
//		flength=frm->maxLength;

	if(frm->lengthFormat==FRM_EMVL)
		if(frm->dataFormat==FRM_BCD || frm->dataFormat==FRM_HEX)
			lenlen=(flength+1)/2>127?2:1;
		else
			lenlen=flength>127?2:1;
	else
		lenlen=frm->lengthLength;

	switch(frm->dataFormat)
	{
		case FRM_SUBFIELDS:
			pos=lenlen;
			for(i=0; i < fld->fields; i++)
			{
				if(pos==frm->maxLength+lenlen)
					break;

				if(bitmap_found!=-1 && frm->fld[bitmap_found]->dataFormat!=FRM_ISOBITMAP && frm->fld[bitmap_found]->maxLength < i-bitmap_found)
					break;

				if(fld->fld[i] && !frm->fld[i])
					return 0;

				if(!frm->fld[i])
					continue;

				if((fld->fld[i] || frm->fld[i]->dataFormat==FRM_ISOBITMAP || frm->fld[i]->dataFormat==FRM_BITMAP) && (bitmap_found==-1 || frm->fld[bitmap_found]->dataFormat==FRM_ISOBITMAP || frm->fld[bitmap_found]->maxLength > i-bitmap_found-1))
				{
					if(frm->fld[i]->dataFormat==FRM_ISOBITMAP)
					{
						bitmap_found=i;
						sflen=((fld->fields-i-1)/64+1)*8;
					}
					else if(frm->fld[i]->dataFormat==FRM_BITMAP)
					{
						bitmap_found=i;
						sflen=(frm->fld[i]->maxLength+7)/8;
					}
					else if(is_empty(fld->fld[i]))
						continue;
					else
						sflen=get_length(fld->fld[i]);
					
					if(!sflen)
						return 0;
					pos+=sflen;
				}
			}

			flength=pos-lenlen;
			blength=pos-lenlen;
			
			break;

		case FRM_BCDSF:
			memset(&tmpfrm, 0, sizeof(fldformat));
			mirrorFormat(&tmpfrm, frm);
			tmpfrm.lengthFormat=FRM_UNKNOWN;
			tmpfrm.lengthLength=0;
			tmpfrm.dataFormat=FRM_SUBFIELDS;
			fld->frm=&tmpfrm;

			flength=get_length(fld);

			fld->frm=frm;

			if(!flength)
				return 0;

			blength=(flength+1)/2;

			break;

		case FRM_TLV1:
		case FRM_TLV2:
		case FRM_TLV3:
		case FRM_TLV4:
		case FRM_TLVEMV:

			if(!frm->fld[0])
				return 0;

			pos=lenlen;
			taglength=frm->dataFormat-FRM_TLV1+1;

			for(i=0; i<frm->maxFields; i++)
			{
				if(!fld->fld[i])
					break;

				if(!fld->fld[i]->tag)
					return 0;

				if(is_empty(fld->fld[i]))
					continue;

				if(frm->dataFormat==FRM_TLVEMV)
				{
					if(strlen(fld->fld[i]->tag)!=2 && strlen(fld->fld[i]->tag)!=4)
						return 0;

					taglength=strlen(fld->fld[i]->tag)/2;
				}
			
				pos+=taglength;
				
				sflen=get_length(fld->fld[i]);
				if(!sflen)
					return 0;
				pos+=sflen;
			}

			blength=pos-lenlen;

			break;

		case FRM_TLVDS:
			pos=lenlen;
			if(frm->tagFormat==FRM_BCD || frm->tagFormat==FRM_HEX)
				taglength=1;
			else
				taglength=2;

			for(i=0; i<frm->fields; i++)
			{
				if(!fld->fld[i])
					continue;

				if(!frm->fld[i])
					return 0;

				if(is_empty(fld->fld[i]))
					continue;

				pos+=taglength;
				
				sflen=get_length(fld->fld[i]);

				if(!sflen)
					return 0;
				pos+=sflen;
			}

			blength=pos-lenlen;
			break;

		case FRM_ISOBITMAP:
			blength=(flength/64)*8;
			break;

		case FRM_BITMAP:
		case FRM_BITSTR:
			blength=(flength+7)/8;
			break;

		case FRM_HEX:
		case FRM_BCD:
			blength=(flength+1)/2;
			break;

		case FRM_BIN:
		case FRM_ASCII:
		case FRM_EBCDIC:
			blength=flength;
			break;

		default:
			if(debug)
				printf("Error: Unknown data format\n");
			return 0;
	}

	blength+=frm->addLength;

	//printf("get_length: %s length %d\n", frm->description, lenlen+blength);

	return lenlen+blength;
}

unsigned int build_field(char *buf, unsigned int maxlength, field *fld)
{
	fldformat *frm;
	unsigned int length;

	if(!buf)
	{
		if(debug)
			printf("Error: No buf\n");
		return 0;
	}

	if(!fld)
	{
		if(debug)
			printf("Error: No fld\n");
		return 0;
	}

	frm=fld->frm;

	if(!frm)
	{
		if(debug)
			printf("Error: No frm\n");
		return 0;
	}

	if(fld->altformat)   //if altformat is forced by field_format()
		return build_field_alt(buf, maxlength, fld);  //then trying to build the field with it

	fld->altformat=1;            //if not,
	length=build_field_alt(buf, maxlength, fld);   //then try the first format
	if(length)
		return length;

	if(debug)
		printf("Info: Retrying with an alternate format\n");

	for(; frm->altformat; frm=frm->altformat)   //if that failed,
	{
		fld->altformat++;            //then iterate through remaining altformats
		if(change_format(fld, frm->altformat))  //that we are able to change to
		{
			length=build_field_alt(buf, maxlength, fld);
			if(length)
				return length;
		}
	}

	return 0;
}

unsigned int build_field_alt(char *buf, unsigned int maxlength, field *fld)
{
	unsigned int lenlen=0;
	unsigned int blength=0;
	unsigned int flength=0;
	unsigned int mlength=0;
	char lengthbuf[7];
	unsigned int i, j, pos, sflen, taglength;
	int bitmap_found=-1;
	fldformat *frm;
	fldformat tmpfrm;

	if(!buf)
	{
		if(debug)
			printf("Error: No buf\n");
		return 0;
	}

	if(!fld)
	{
		if(debug)
			printf("Error: No fld\n");
		return 0;
	}

	frm=fld->frm;

	if(!frm)
	{
		if(debug)
			printf("Error: No frm\n");
		return 0;
	}

//	printf("Building %s\n", frm->description);

	if(frm->data && fld->data && strcmp(frm->data, fld->data))
	{
		if(debug)
			printf("Error: Format mandatory data (%s) does not match field data (%s) for %s\n", frm->data, fld->data, frm->description);
		return 0;
	}

	if(fld->data)
		flength=strlen(fld->data);
//	else if(frm->lengthFormat==FRM_FIXED)
//		flength=frm->maxLength;

	if(frm->lengthFormat==FRM_EMVL)
		if(frm->dataFormat==FRM_BCD || frm->dataFormat==FRM_HEX)
			lenlen=(flength+1)/2>127?2:1;
		else
			lenlen=flength>127?2:1;
	else
		lenlen=frm->lengthLength;

	switch(frm->dataFormat)
	{
		case FRM_SUBFIELDS:
			pos=lenlen;
			for(i=0; i < fld->fields; i++)
			{
				if(pos==frm->maxLength+lenlen)
					break;

				if(bitmap_found!=-1 && frm->fld[bitmap_found]->dataFormat!=FRM_ISOBITMAP && frm->fld[bitmap_found]->maxLength < i-bitmap_found)
					break;

				if(fld->fld[i] && !frm->fld[i])
				{
					if(debug)
						printf("Error: No format for subfield %d\n", i);
					return 0;
				}

				if(!frm->fld[i])
					continue;

				if((fld->fld[i] || frm->fld[i]->dataFormat==FRM_ISOBITMAP || frm->fld[i]->dataFormat==FRM_BITMAP) && (bitmap_found==-1 || frm->fld[bitmap_found]->dataFormat==FRM_ISOBITMAP || frm->fld[bitmap_found]->maxLength > i-bitmap_found-1))
				{
					if(pos>maxlength)
						return 0;

					if(frm->fld[i]->dataFormat==FRM_ISOBITMAP)
					{
						bitmap_found=i;
						sflen=build_isobitmap(buf+pos, maxlength-pos, fld, i);
					}
					else if(frm->fld[i]->dataFormat==FRM_BITMAP)
					{
						bitmap_found=i;
						sflen=build_bitmap(buf+pos, maxlength-pos, fld, i);
					}
					else if(is_empty(fld->fld[i]))
					{
						if(debug)
							printf("Warning: No subfields for subfield %d\n", i);
						continue;
					}
					else
						sflen=build_field(buf+pos, maxlength-pos, fld->fld[i]);
					
					if(!sflen)
					{
						if(debug)
							printf("Error: unable to build subfield %d: %s\n", i, frm->fld[i]->description);
						return 0;
					}
					pos+=sflen;
				}
			}

			flength=pos-lenlen;
			blength=pos-lenlen;
			mlength=blength;

			break;

		case FRM_BCDSF:
			fld->data=(char*)malloc(maxlength*2+1);
			
			tmpfrm.lengthFormat=FRM_UNKNOWN;
			tmpfrm.lengthLength=0;
			tmpfrm.maxLength=frm->maxLength;
			tmpfrm.dataFormat=FRM_SUBFIELDS;
			tmpfrm.tagFormat=0;
			tmpfrm.description=frm->description;
			tmpfrm.data=frm->data;
			tmpfrm.maxFields=frm->maxFields;
			tmpfrm.fields=frm->fields;
			tmpfrm.fld=frm->fld;

			fld->frm=&tmpfrm;

			flength=build_field(fld->data, maxlength*2, fld);

			fld->frm=frm;

			if(!flength)
				return 0;

			blength=(flength+1)/2;
			
			if(frm->lengthFormat==FRM_FIXED)
				mlength=flength;
			else
				mlength=blength;

			if(lenlen+blength>maxlength)
				return 0;
			
			if(!build_bcdl(fld->data, buf+lenlen, flength))
			{
				if(debug)
					printf("Error: Not BCD subfield\n");
				return 0;
			}

			free(fld->data);
			fld->data=0;
			
			break;

		case FRM_TLV1:
		case FRM_TLV2:
		case FRM_TLV3:
		case FRM_TLV4:
		case FRM_TLVEMV:

			if(!frm->fld[0])
			{
				if(debug)
					printf("Error: No tlv subfield format\n");
				return 0;
			}

			pos=lenlen;
			taglength=frm->dataFormat-FRM_TLV1+1;

			for(i=0; i<frm->maxFields && pos<maxlength; i++)
			{
				if(!fld->fld[i])
					break;

				if(!fld->fld[i]->tag)
				{
					if(debug)
						printf("Error: Tag length is too small\n");
					return 0;
				}

				if(is_empty(fld->fld[i]))
					continue;

				sflen=strlen(fld->fld[i]->tag);

				if(frm->dataFormat==FRM_TLVEMV)
				{
					if(sflen!=2 && sflen!=4)
						return 0;

					taglength=sflen/2;
				}

				if(pos+taglength>maxlength)
				{
					if(debug)
						printf("Error: Not enough length for TLV tag\n");
					return 0;
				}

				switch(frm->tagFormat)
				{
					case FRM_EBCDIC:
						if(sflen<taglength)
							return 0;
						build_ebcdic(fld->fld[i]->tag, buf+pos, sflen);
						break;
					case FRM_HEX:
						if(sflen<taglength*2)
							return 0;
						build_hex(fld->fld[i]->tag, buf+pos, sflen);
						break;
					case FRM_BCD:
						if(sflen<taglength*2)
							return 0;
						build_bcdl(fld->fld[i]->tag, buf+pos, sflen);
						break;
					case FRM_ASCII:
					default:
						if(sflen<taglength)
							return 0;
						memcpy(buf+pos, fld->fld[i]->tag, sflen);
				}

				pos+=taglength;

				sflen=build_field(buf+pos, maxlength-pos, fld->fld[i]);
				if(!sflen)
				{
					if(debug)
						printf("Error: unable to build TLV subfield\n");
					return 0;
				}
				pos+=sflen;
			}

			mlength=pos-lenlen;
			blength=pos-lenlen;

			break;

		case FRM_TLVDS:
			pos=lenlen;

			if(frm->tagFormat==FRM_BCD || frm->tagFormat==FRM_HEX)
				taglength=1;
			else
				taglength=2;

			for(i=0; i<frm->fields && pos<maxlength; i++)
			{
				if(!fld->fld[i])
					continue;

				if(!frm->fld[i])
				{
					if(debug)
						printf("Error: No format for tag %d.\n", i);
					return 0;
				}

				if(pos+taglength>maxlength)
				{
					if(debug)
						printf("Error: Not enough length for TLV tag\n");
					return 0;
				}

				if(is_empty(fld->fld[i]))
					continue;

				switch(frm->tagFormat)
				{
					case FRM_EBCDIC:
						if(taglength+1<=snprintf(lengthbuf, taglength+1, "%u", i) || taglength<strlen(lengthbuf))
						{
							if(debug)
								printf("Error: TLV tag number is too big (%d)\n", i);
							return 0;
						}

						memcpy(buf + pos + taglength - strlen(lengthbuf), lengthbuf, strlen(lengthbuf));
						if(!build_ebcdic(lengthbuf, buf + pos + taglength - strlen(lengthbuf), strlen(lengthbuf)))
							return 0;

						for(j=0; j < taglength - strlen(lengthbuf); j++)
							buf[pos+j]=0xF0;
						break;

					case FRM_HEX:
						if(taglength*2+1<=snprintf(lengthbuf, taglength*2+1, "%X", i) || taglength*2<strlen(lengthbuf))
						{
							if(debug)
								printf("Error: TLV tag number is too big (%d)\n", i);
							return 0;
						}

						if(!build_hex(lengthbuf, buf + pos + taglength - (strlen(lengthbuf)+1)/2, strlen(lengthbuf)))
							return 0;

						for(j=0; j < taglength - (strlen(lengthbuf)+1)/2; j++)
							buf[pos+j]='\0';
						break;
					case FRM_BCD:
						
						if(taglength*2+1<=snprintf(lengthbuf, taglength*2+1, "%u", i) || taglength*2<strlen(lengthbuf))
						{
							if(debug)
								printf("Error: TLV tag number is too big (%d)\n", i);
							return 0;
						}

						if(!build_bcdr(lengthbuf, buf + pos + taglength - (strlen(lengthbuf)+1)/2, strlen(lengthbuf)))
							return 0;

						for(j=0; j < taglength - (strlen(lengthbuf)+1)/2; j++)
							buf[pos+j]='\0';
						break;
					case FRM_ASCII:
					default:			
						if(taglength+1<=snprintf(lengthbuf, taglength+1, "%u", i) || taglength<strlen(lengthbuf))
						{
							if(debug)
								printf("Error: TLV tag number is too big (%d)\n", i);
							return 0;
						}

						memcpy(buf + pos + taglength - strlen(lengthbuf), lengthbuf, strlen(lengthbuf));

						for(j=0; j < taglength - strlen(lengthbuf); j++)
							buf[pos+j]='0';
				}

				pos+=taglength;

				sflen=build_field(buf+pos, maxlength-pos, fld->fld[i]);
				if(!sflen)
				{
					if(debug)
						printf("Error: unable to build TLV subfield\n");
					return 0;
				}
				pos+=sflen;
			}

			mlength=pos-lenlen;
			blength=pos-lenlen;

			break;


		case FRM_ISOBITMAP:
			if(flength==0)
				return 0;

			for(i=0; i<flength/64+1; i++)
			{
				if(i!=flength/64)
					buf[i*64]=0x80;

				if((i+1)*8>maxlength)
					return 0;

				memset(buf+i*8, '\0', 8);

				for(j=1; j<64; j++)
					if(fld->data[j-1]=='1')
						buf[i*8+j/8]|=1<<(7-j%8);
			}

			blength=i*8;
			mlength=0;

			break;

		case FRM_BITMAP:
		case FRM_BITSTR:
			blength=(flength+7)/8;
			mlength=flength;

			if(lenlen+blength>maxlength)
				return 0;

			memset(buf+lenlen, '\0', blength);
			for(i=0; i<flength;i++)
				if(fld->data[i]=='1')
					buf[lenlen + i/8]|=1<<(7-i%8);

			break;

		case FRM_BIN:
		case FRM_ASCII:
			blength=flength;
			mlength=flength;
			if(lenlen+blength>maxlength)
				return 0;
			memcpy(buf+lenlen, fld->data, blength);
			break;

		case FRM_HEX:
			blength=(flength+1)/2;
			mlength=blength;
			if(lenlen+blength>maxlength)
				return 0;
			if(!build_hex(fld->data, buf+lenlen, flength))
				return 0;

			break;

		case FRM_BCD:
			blength=(flength+1)/2;
			mlength=flength;
			if(lenlen+blength>maxlength)
				return 0;
			if(!build_bcdr(fld->data, buf+lenlen, flength))
				return 0;

			break;

		case FRM_EBCDIC:
			blength=flength;
			mlength=flength;
			if(lenlen+blength>maxlength)
				return 0;
			build_ebcdic(fld->data, buf+lenlen, blength);
			break;

		default:
			if(debug)
				printf("Error: Unknown data format\n");
			return 0;
	}

	if(frm->lengthInclusive)
		mlength+=lenlen;

	mlength+=frm->addLength;

	switch(frm->lengthFormat)
	{
		case FRM_FIXED:
			if(frm->maxLength != mlength)
			{
				if(debug)
					printf("Error: Bad length for fixed-length field! %d (field) != %d (format) for %s\n", mlength, frm->maxLength, frm->description);
				return 0;
			}
			break;

		case FRM_BIN:
			if(lenlen>4)
				for(i=0; i < lenlen-4; i++)
					buf[i]='\0';

			for(i=0; i<(lenlen>4?4:lenlen); i++)
				buf[(lenlen>4?4:lenlen)-i-1]=((unsigned char *)(&mlength))[i];
			break;

		case FRM_EMVL:
			if(lenlen>1)
				buf[0]=127+lenlen;
			
			buf[lenlen-1]=mlength;
			break;

		case FRM_BCD:
			if(lenlen*2>=sizeof(lengthbuf) || lenlen*2+1<=snprintf(lengthbuf, lenlen*2+1, "%u", mlength) || lenlen*2<strlen(lengthbuf))
			{
				if(debug)
					printf("Error: Length of length is too small (%d)\n", lenlen);
				return 0;
			}

			if(!build_bcdr(lengthbuf, buf + lenlen - (strlen(lengthbuf)+1)/2, strlen(lengthbuf)))
				return 0;

			for(i=0; i < lenlen - (strlen(lengthbuf)+1)/2; i++)
				buf[i]='\0';

			break;

		case FRM_ASCII:
			if(lenlen>=sizeof(lengthbuf) || lenlen+1<=snprintf(lengthbuf, lenlen+1, "%u", mlength) || lenlen<strlen(lengthbuf))
			{
				if(debug)
					printf("Error: Length of length is too small\n");
				return 0;
			}

			memcpy(buf + lenlen - strlen(lengthbuf), lengthbuf, strlen(lengthbuf));

			for(i=0; i < lenlen - strlen(lengthbuf); i++)
				buf[i]='0';

			break;
	
		case FRM_EBCDIC:
			if(lenlen>=sizeof(lengthbuf) || lenlen+1<=snprintf(lengthbuf, lenlen+1, "%u", mlength) || lenlen<strlen(lengthbuf))
			{
				if(debug)
					printf("Error: Length of length is too small (%d)\n", lenlen);
				return 0;
			}

			if(!build_ebcdic(lengthbuf, buf + lenlen - strlen(lengthbuf), strlen(lengthbuf)))
				return 0;

			for(i=0; i < lenlen - strlen(lengthbuf); i++)
				buf[i]=0xF0;

			break;

		case FRM_UNKNOWN:
			break;

		default:
			if(frm->dataFormat!=FRM_ISOBITMAP)
			{
				if(debug)
					printf("Error: Unknown length format\n");
				return 0;
			}
	}

	//printf("build_field: %s , length %d\n", frm->description, lenlen +blength);

	return lenlen+blength;
}

unsigned int build_ebcdic(char *from, char *to, unsigned int len)
{
	unsigned int i;
	const unsigned char ascii2ebcdic[257]="\0\x001\x002\x003\x037\x02D\x02E\x02F\x016\x005\x025\x00B\x00C\x00D\x00E\x00F\x010\x011\x012\x013\x03C\x03D\x032\x026\x018\x019\x03F\x027\x022\x01D\x01E\x01F\x040\x05A\x07F\x07B\x05B\x06C\x050\x07D\x04D\x05D\x05C\x04E\x06B\x060\x04B\x061\x0F0\x0F1\x0F2\x0F3\x0F4\x0F5\x0F6\x0F7\x0F8\x0F9\x07A\x05E\x04C\x07E\x06E\x06F\x07C\x0C1\x0C2\x0C3\x0C4\x0C5\x0C6\x0C7\x0C8\x0C9\x0D1\x0D2\x0D3\x0D4\x0D5\x0D6\x0D7\x0D8\x0D9\x0E2\x0E3\x0E4\x0E5\x0E6\x0E7\x0E8\x0E9\x0BA\x0E0\x0BB\x0B0\x06D\x079\x081\x082\x083\x084\x085\x086\x087\x088\x089\x091\x092\x093\x094\x095\x096\x097\x098\x099\x0A2\x0A3\x0A4\x0A5\x0A6\x0A7\x0A8\x0A9\x0C0\x04F\x0D0\x0A1\x007\x020\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x0FF\x040\x040\x04A\x0B1\x040\x0B2\x06A\x040\x040\x0C3\x040\x040\x05F\x040\x0D9\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x040\x07F\x040\x040\x040\x040\x064\x065\x062\x066\x063\x067\x09C\x068\x074\x071\x072\x073\x078\x075\x076\x077\x040\x069\x0ED\x0EE\x0EB\x0EF\x0EC\x040\x080\x0FD\x0FE\x0FB\x0FC\x0AD\x040\x059\x044\x045\x042\x046\x043\x047\x09E\x048\x054\x051\x052\x053\x058\x055\x056\x057\x040\x049\x0CD\x0CE\x0CF\x0CB\x0CC\x040\x070\x0DD\x0DE\x0DB\x0DC\x08D\x040\x0DF";

	for(i=0; i<len; i++)
		to[i]=ascii2ebcdic[(unsigned char)from[i]];
	
	return len;
}

unsigned int build_bcdr(char *from, char *to, unsigned int len)
{
	unsigned int i;
	unsigned int u=len/2*2==len?0:1;
	unsigned char t;
	unsigned int separator_found=0;

	to[0]='\0';
	for(i=0; i<(len+1)/2; i++)
	{
		if(i!=0 || u==0)
		{
			t=(unsigned char)from[i*2-u];
			if(17<len && len<38 && !separator_found && t=='^')     //making one exception for track2
			{
				separator_found=1;
				to[i]=0xD0;
			}
			else if(t>='0' && t<='9')
				to[i]=(t-'0')<<4;
			else
			{
				if(debug)
					printf("Error: build_bcdr: The string is not BCD\n");
				return 0;
			}
		}

		t=(unsigned char)from[i*2+1-u];
		if(17<len && len<38 && !separator_found && t=='^')     //making one exception for track2
		{
			separator_found=1;
			to[i]|=0xD;
		}
		else if(t>='0' && t<='9')
			to[i]|=t-'0';
		else
		{
			if(debug)
				printf("Error: build_bcdr: The string is not BCD\n");
			return 0;
		}
	}
	return (len+1)/2;
}

unsigned int build_bcdl(char *from, char *to, unsigned int len)
{
	unsigned int i;
	unsigned int u=len/2*2==len?0:1;
	unsigned char t;
	unsigned int separator_found=0;

	to[0]='\0';
	for(i=0; i<(len+1)/2; i++)
	{
		t=(unsigned char)from[i*2];
		if(17<len && len<38 && !separator_found && t=='^')     //making one exception for track2
		{
			separator_found=1;
			to[i]=0xD0;
		}
		else if(t>='0' && t<='9')
			to[i]=(t-'0')<<4;
		else
		{
			if(debug)
				printf("Error: build_bcdl: The string is not BCD ('%s')\n", from);
			return 0;
		}

		if(u==0 || i!=(len+1)/2-1)
		{
			t=(unsigned char)from[i*2+1];
			if(17<len && len<38 && !separator_found && t=='^')     //making one exception for track2
			{
				separator_found=1;
				to[i]|=0xD;
			}
			else if(t>='0' && t<='9')
				to[i]|=t-'0';
			else
			{
				if(debug)
					printf("Error: build_bcdl: The string is not BCD ('%s')\n", from);
				return 0;
			}
		}
	}
	return (len+1)/2;
}

unsigned int build_hex(char *from, char *to, unsigned int len)
{
	unsigned int i;
	unsigned char t;
	unsigned int u=len/2*2==len?0:1;

	to[0]='\0';
	for(i=0; i<(len+1)/2; i++)
	{
		if(i!=0 || u==0)
		{
			t=(unsigned char)from[i*2-u];
			if(t>='0' && t<='9')
				to[i]=(t-'0')<<4;
			else if(t>='A' && t<='F')
				to[i]=(t-'A'+0xA)<<4;
			else if(t>='a' && t<='f')
				to[i]=(t-'a'+0xA)<<4;
			else
			{
				if(debug)
					printf("Error: build_hex: The string is not HEX\n");
				return 0;
			}
		}

		t=(unsigned char)from[i*2+1-u];
		if(t>='0' && t<='9')
			to[i]|=t-'0';
		else if(t>='A' && t<='F')
			to[i]|=t-'A'+0xA;
		else if(t>='a' && t<='f')
			to[i]|=t-'a'+0xA;
		else
		{
			if(debug)
				printf("Error: build_hex: The string is not BCD\n");
			return 0;
		}
	}
	return (len+1)/2;
}

unsigned int build_isobitmap(char *buf, unsigned int maxlength, field *fld, unsigned int index)
{
	unsigned int i, j;

	//printf("ISOBITMAP: %d %d %d\n", fld->fields, index, (fld->fields-index-1)/64+1);

	for(i=0; i<(fld->fields-index-1)/64+1; i++)
	{
		if((i+1)*8>maxlength)
			return 0;

		memset(buf+i*8, '\0', 8);

		if(i!=(fld->fields-index-1)/64)
			buf[i*64]=0x80;

		for(j=1; j<64; j++)
			if(fld->fld[i*64+j+index] && !is_empty(fld->fld[i*64+j+index]))
				buf[i*8+j/8]|=1<<(7-j%8);
	}
	
	return i*8;
}

unsigned int build_bitmap(char *buf, unsigned int maxlength, field *fld, unsigned int index)
{
	unsigned int i, blength, flength;

	flength=fld->frm->fld[index]->maxLength;

	blength=(flength+7)/8;

	if(blength>maxlength)
		return 0;

	memset(buf, '\0', blength);

	for(i=0; i<flength && i<fld->fields-index-1; i++)
		if(fld->fld[index+1+i] && !is_empty(fld->fld[index+1+i]))
			buf[i/8]|=1<<(7-i%8);

	return blength;
}

int is_empty(field *fld)
{
	int i;

	switch(fld->frm->dataFormat)
	{
		case FRM_SUBFIELDS:
		case FRM_BCDSF:
		case FRM_TLV1:
		case FRM_TLV2:
		case FRM_TLV3:
		case FRM_TLV4:
		case FRM_TLVEMV:
		case FRM_TLVDS:
			break;
		case FRM_ISOBITMAP:
		case FRM_BITMAP:
			return 0;
		default:
			if(!fld->data || !fld->data[0])
				return 1;
			else
				return 0;
	}

	if(!fld->fld)
		return 1;

	for(i=0; i<fld->frm->fields; i++)
		if(fld->fld[i] && fld->fld[i]->frm->dataFormat!=FRM_ISOBITMAP && fld->fld[i]->frm->dataFormat!=FRM_BITMAP && !is_empty(fld->fld[i]))
			return 0;

	return 1;
}

