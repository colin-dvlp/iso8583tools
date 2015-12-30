#include <cstdio>
#include <cstdlib> //for exit(). TODO: Remove exit() and introduce exceptions
#include <sstream>

#include "parser.h"

using namespace std;

string to_string(unsigned int);

field::field(const string &str)
{
	fill_default();
	frm=new fldformat();
	deletefrm=true;
	firstfrm=frm;
	data=str;
}

field::field(fldformat *format, const string &str)
{
	fill_default();
	frm=format;
	firstfrm=frm;
	data=str;
}

field::field(const std::string &filename, const string &str)
{
	fill_default();
	frm=new fldformat(filename);
	deletefrm=true;
	firstfrm=frm;
	data=str;
}

//copy constructor
field::field(const field &from)
{
	data=from.data;
	start=from.start;
	blength=from.blength;
	flength=from.flength;
	deletefrm=from.deletefrm;
	frm=from.frm;
	if(deletefrm)
	{
		firstfrm=new fldformat(*from.firstfrm);
		frm=firstfrm;
		for(fldformat *tmpfrm=from.firstfrm; tmpfrm!=NULL; tmpfrm=tmpfrm->get_altformat(), frm=frm->get_altformat())
			if(tmpfrm==from.frm)
				break;
	}
	else
		firstfrm=from.firstfrm;
	subfields=from.subfields;
	altformat=from.altformat;
}

field::~field(void)
{
	clear();
	if(deletefrm)
		delete frm;
}

void field::fill_default(void)
{
	data.clear();
	start=0;
	blength=0;
	flength=0;
	frm=NULL;
	subfields.clear();
	altformat=0;
	deletefrm=false;
	firstfrm=frm;
}

void field::clear(void)
{
	fldformat *tmpfrm=frm, *tmpfirstfrm=firstfrm;
	unsigned int tmpaltformat=altformat;
	bool delfrm=deletefrm;
	fill_default();
	deletefrm=delfrm;
	firstfrm=tmpfirstfrm; //make formats immune to clear()
	frm=tmpfrm;
	altformat=tmpaltformat;
}

//forks the field. All data and subfields are also copied so that all pointers except frm will have new values to newly copied data but non-pointers will have same values
//If field is not empty, the format is retained
field& field::operator= (const field &from)
{
	fldformat *tmpfrm=frm, *tmpfirstfrm=firstfrm;
	bool keepfrm=false;

	if(this==&from)
		return *this;

	if(!empty())
		keepfrm=true;

	clear();

	data=from.data;
	start=from.start;
	blength=from.blength;
	flength=from.flength;
	deletefrm=from.deletefrm;
	frm=from.frm;
	if(deletefrm)
	{
		firstfrm=new fldformat(*from.firstfrm);
		frm=firstfrm;
		for(fldformat *tmpfrm=from.firstfrm; tmpfrm!=NULL; tmpfrm=tmpfrm->get_altformat(), frm=frm->get_altformat())
			if(tmpfrm==from.frm)
				break;
	}
	else
		firstfrm=from.firstfrm;
	subfields=from.subfields;
	altformat=from.altformat;

	if(keepfrm)
		set_frm(tmpfirstfrm, tmpfrm);

	return *this;
}

//relink data from another field. The old field will become empty
void field::moveFrom(field &from)
{
	if(this==&from)
		return;

	*this=from;
	from.clear();
}

void field::swap(field &from)
{
	if(this==&from)
		return;

	field tmpfld;
	tmpfld.moveFrom(from);
	from.moveFrom(*this);
	moveFrom(tmpfld);
}

int field::compare (const field& other) const
{
	if(empty() && other.empty())
		return 0;

	switch(frm->dataFormat)
	{
		case fldformat::fld_subfields:
		case fldformat::fld_bcdsf:
		case fldformat::fld_tlv:
			break;
		default:
			return data.compare(other.data);
	}

	for(const_iterator i=begin(), j=other.begin(); i!=end() && j!=other.end(); ++i, ++j)
	{
		while((!i->second.frm || i->second.frm->dataFormat==fldformat::fld_isobitmap || i->second.frm->dataFormat==fldformat::fld_bitmap) && i!=end()) //skip subfields without format and bitmaps
			++i;
		while((!j->second.frm || j->second.frm->dataFormat==fldformat::fld_isobitmap || j->second.frm->dataFormat==fldformat::fld_bitmap) && j!=other.end())
			++j;
		if(i==end() && j==other.end())
			break;
		if(i==end())
			return 1;
		if(j==other.end())
			return -1;
		if(i->first<j->first)
			return 1;
		if(j->first<i->first)
			return -1;
		int r=i->second.compare(j->second);
		if(r)
			return r;
	}
	return 0;
}

void field::print_message(string numprefix) const
{
	if(!numprefix.empty())
		printf("%s ", numprefix.c_str());

	printf("%s", frm->get_description().c_str());

	if(!data.empty())
		printf(" (%d): [%s]\n", flength, data.c_str());
	else
		printf(":\n");

	switch(frm->dataFormat)
	{
		case fldformat::fld_subfields:
		case fldformat::fld_bcdsf:
		case fldformat::fld_tlv:
			for(const_iterator i=begin(); i!=end(); ++i)
				i->second.print_message(numprefix + to_string(i->first) + ".");
			break;
		default:
			break;
	}
}

bool field::empty(void) const
{
	if(frm->empty())
		return true;

	switch(frm->dataFormat)
	{
		case fldformat::fld_subfields:
		case fldformat::fld_bcdsf:
		case fldformat::fld_tlv:
			break;
		case fldformat::fld_isobitmap:
		case fldformat::fld_bitmap:
			return false;
		default:
			return data.empty();
	}

	if(subfields.empty())
		return true;

	for(const_iterator i=begin(); i!=end(); ++i)
		if(i->second.frm && i->second.frm->dataFormat!=fldformat::fld_isobitmap && i->second.frm->dataFormat!=fldformat::fld_bitmap && !i->second.empty())
			return false;

	return true;
}

// Switches to the next applicable altformat.
// Returns true on success, false if no applicable altformats left after the current
bool field::switch_altformat(void)
{
	fldformat *frmtmp=frm;

	for(unsigned int i=altformat+1; (frmtmp=frmtmp->get_altformat())!=NULL; i++)
		if(change_format(frmtmp))
		{
			altformat=i;
			return true;
		}

	return false;
}

// Rewinds to the first applicable altformat.
// Returns true on success, false if none formats are applicable (which is an error)
bool field::reset_altformat(void)
{
	fldformat *frmtmp=firstfrm;

	for(unsigned int i=0; frmtmp!=NULL; frmtmp=frmtmp->get_altformat(), i++)
		if(change_format(frmtmp))
		{
			altformat=i;
			return true;
		}

	return false;
}

// Assigns a new format to the field. Not to be used to switch to an altformat because it assumes the new format to be the root of altformat, so the information about the first altformat is lost and reset_altformat() would not reset to original altformat, use switch_altformat() instead.
// If frmaltnew is not null, it must be an altformat of frmnew
bool field::set_frm(fldformat *frmnew, fldformat *frmaltnew)
{
	fldformat *frmtmp=frmnew;

	for(unsigned int i=0; frmtmp!=NULL; frmtmp=frmtmp->get_altformat(), i++)
		if(change_format(frmtmp))
		{
			altformat=i;
			break;
		}

	if(!frmtmp)
		return false;

	if(deletefrm)
	{
		delete firstfrm;
		deletefrm=false;
	}

	firstfrm=frmnew;

	if(frmaltnew)
	{
		frmtmp=frm;
		for(unsigned int i=altformat; frmtmp!=NULL; frmtmp=frmtmp->get_altformat(), i++)
			if(frmtmp==frmaltnew)
			{
				altformat=i;
				return change_format(frmaltnew);
			}
	}

	return true;
}

// Internal function to change current format, not to be called directly
// If the new format does not suit the already present field tree, the function will restore the original format. The function guarantees consistency of the resulting field format, however it is not guaranteed that all subfields will remain on the same altformats in case of failure.
bool field::change_format(fldformat *frmnew)
{
	iterator i;
	fldformat *frmold=frm;

	if(!frmnew)
		return false;

	if(frm == frmnew)
		return true;

	frm=frmnew;

	for(i=begin(); i!=end(); i++)
		if(!frmnew->sfexist(i->first) || !i->second.change_format(&frmnew->sf(i->first)))
			break;

	if(i!=end())
	{
		if(debug)
			printf("Error: Unable to change field format (%d). Reverting.\n", i->first);

		for(frm=frmold; i!=begin(); i--)
			if(!frmold->sfexist(i->first) || !i->second.change_format(&frmold->sf(i->first)))
				if(debug)
					printf("Error: Unable to revert\n");

		if(!frmold->sfexist(i->first) || !i->second.change_format(&frmold->sf(i->first)))
			if(debug)
				printf("Error: Unable to revert\n");

		return false;
	}

	return true;
}

//a segfault-safe accessor function. Returns a pointer to the field contents. If the field does not exist, it would be created. If it cannot be created, a valid pointer to a dummy array is returned.
string& field::add_field(int n0, int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8, int n9)
{
	static string def="";
	int n[]={n0, n1, n2, n3, n4, n5, n6, n7, n8, n9};
	unsigned int i;
	field *curfld=this;

	for(i=0; i<sizeof(n)/sizeof(n[0]); i++)
	{
		if(n[i]==-1)
			break;

		if(!curfld || !curfld->frm)
			return def;

		curfld=&curfld->sf(n[i]);
	}

	if(!curfld || !curfld->frm)
		return def;

	return curfld->data;
}

int field::field_format(unsigned int newaltformat, int n0, int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8, int n9)
{
	int n[]={n0, n1, n2, n3, n4, n5, n6, n7, n8, n9};
	unsigned int i;
	fldformat *tmpfrm;
	field *curfld=this;

	for(i=0; i<sizeof(n)/sizeof(n[0]); i++)
	{
		if(n[i]==-1)
			break;

		if(!curfld || !curfld->frm)
			return 2;

		tmpfrm=curfld->frm;

		curfld=&curfld->sf(n[i]);
	}

	if(!curfld || !curfld->frm)
		return 2;

	if(i==0)
	{
		if(curfld->altformat>=newaltformat || newaltformat==0) //no need or unable to go back in the list
		{
			if(newaltformat==0 && curfld->altformat==1)
				curfld->altformat=0;

			return 3;
		}

		if(newaltformat==1 && curfld->altformat==0)
		{
			curfld->altformat=1;
			return 0;
		}

		tmpfrm=curfld->frm;

		for(i=curfld->altformat==0?1:curfld->altformat; i<newaltformat; i++)
			if(!tmpfrm->altformat)
				return 4;
			else
				tmpfrm=tmpfrm->altformat;

		if(curfld->change_format(tmpfrm))
			curfld->altformat=newaltformat;
		else
			return 1;

		return 0;
	}

	tmpfrm=&tmpfrm->sf(n[i-1]);

	for(i=1; i<altformat; i++)
		if(!tmpfrm->altformat)
			return 4;
		else
			tmpfrm=tmpfrm->altformat;

	if(curfld->change_format(tmpfrm))
		curfld->altformat=newaltformat;
	else
		return 1;

	return 0;
} 

//a segfault-safe accessor function. Return a pointer to the fields contents. If the field does not exist, returns a valid pointer to an empty string. The field structure is not modified.
const string& field::get_field(int n0, int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8, int n9)
{
	static const string def="";
	int n[]={n0, n1, n2, n3, n4, n5, n6, n7, n8, n9};
	unsigned int i;
	field *curfld=this;

	for(i=0; i<sizeof(n)/sizeof(n[0]); i++)
	{
		if(n[i]==-1)
			break;

		if(!curfld || !curfld->sfexist(n[i]))
			return def;

		curfld=&curfld->sf(n[i]);
	}

	if(!curfld)
		return def;

	return curfld->data;
}

void field::remove_field(int n0, int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8, int n9)
{
	int n[]={n0, n1, n2, n3, n4, n5, n6, n7, n8, n9};
	unsigned int i;
	field *curfld=this;

	for(i=0; i<sizeof(n)/sizeof(n[0])-1; i++)
	{
		if(n[i+1]==-1)
			break;

		if(!curfld || !curfld->sfexist(n[i]))
			return;

		curfld=&curfld->sf(n[i]);
	}

	if(!curfld || !curfld->sfexist(n[i]))
		return;

	subfields.erase(n[i]);
	return;
}

//returns false if fields does not exists or has no subfields or empty.
//otherwise, returns true
bool field::has_field(int n0, int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8, int n9)
{
	int n[]={n0, n1, n2, n3, n4, n5, n6, n7, n8, n9};
	unsigned int i;
	field *curfld=this;

	for(i=0; i<sizeof(n)/sizeof(n[0]); i++)
	{
		if(n[i]==-1)
			break;

		if(!curfld || !curfld->sfexist(n[i]))
			return false;

		curfld=&curfld->sf(n[i]);
	}

	if(!curfld)
		return false;

	switch(curfld->frm->dataFormat)
	{
		case fldformat::fld_subfields:
		case fldformat::fld_tlv:
		case fldformat::fld_bcdsf:
			return subfields.empty();
		default:
			return data.empty();
	}
}

//returns reference to subfield. If it does not exists, it will be added.
field& field::sf(int n0, int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8, int n9)
{
	if(n0 < 0)
	{
		printf("Error: Wrong subfield number: %d\n", n0);
		exit(1);
	}

	if(!sfexist(n0))
	{
		if(!frm->sfexist(n0))
		{
			printf("Error: Wrong format for subfield number: %d\n", n0);
			exit(1);
		}

		if(subfields[n0].empty())
			subfields[n0].set_frm(&frm->sf(n0));
	}

	if(n1<0)
		return subfields[n0];
	else
		return subfields[n0].sf(n1, n2, n3, n4, n5, n6, n7, n8, n9);
}

bool field::sfexist(int n0, int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8, int n9) const
{
	if(n0 < 0)
		return false;

	const_iterator it = subfields.find(n0);

	if(it == subfields.end())
		return false;

	if(n1<0)
		return true;
	else
		return it->second.sfexist(n1, n2, n3, n4, n5, n6, n7, n8, n9);
}

string to_string(unsigned int n)
{
	ostringstream ss;
	ss << n;
	return ss.str();
}
